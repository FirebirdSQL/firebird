/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/intl.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceJrdHelpers.h"
#include "../jrd/optimizer/Optimizer.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ---------------------------
// Data access: procedure scan
// ---------------------------

ProcedureScan::ProcedureScan(CompilerScratch* csb, const string& alias, StreamType stream,
							 const jrd_prc* procedure, const ValueListNode* sourceList,
							 const ValueListNode* targetList, MessageNode* message)
	: RecordStream(csb, stream, procedure->prc_record_format), m_alias(csb->csb_pool, alias),
	  m_procedure(procedure), m_sourceList(sourceList), m_targetList(targetList), m_message(message)
{
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = DEFAULT_CARDINALITY;

	fb_assert(!sourceList == !targetList);

	if (sourceList && targetList)
		fb_assert(sourceList->items.getCount() == targetList->items.getCount());
}

void ProcedureScan::internalOpen(thread_db* tdbb) const
{
	if (!m_procedure->isImplemented())
	{
		status_exception::raise(
			Arg::Gds(isc_proc_pack_not_implemented) <<
				m_procedure->getName().object << m_procedure->getName().getSchemaAndPackage().toString());
	}
	else if (!m_procedure->isDefined())
	{
		status_exception::raise(
			Arg::Gds(isc_prcnotdef) << Arg::Str(m_procedure->getName().toString()) <<
			Arg::Gds(isc_modnotfound));
	}

	const_cast<jrd_prc*>(m_procedure)->checkReload(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;

	// get rid of any lingering record

	delete rpb->rpb_record;
	rpb->rpb_record = NULL;

	ULONG iml;
	const UCHAR* im;

	if (m_sourceList)
	{
		iml = m_message->format->fmt_length;
		im = request->getImpure<UCHAR>(m_message->impureOffset);

		const NestConst<ValueExprNode>* const sourceEnd = m_sourceList->items.end();
		const NestConst<ValueExprNode>* sourcePtr = m_sourceList->items.begin();
		const NestConst<ValueExprNode>* targetPtr = m_targetList->items.begin();

		for (; sourcePtr != sourceEnd; ++sourcePtr, ++targetPtr)
			EXE_assignment(tdbb, *sourcePtr, *targetPtr);
	}
	else
	{
		iml = 0;
		im = NULL;
	}

	Request* const proc_request = m_procedure->getStatement()->findRequest(tdbb);
	impure->irsb_req_handle = proc_request;

	// req_proc_fetch flag used only when fetching rows, so
	// is set at end of open()
	proc_request->req_flags &= ~req_proc_fetch;
	AutoSetRestoreFlag<ULONG> autoSetReqProcSelect(&proc_request->req_flags, req_proc_select, true);

	try
	{
		proc_request->setGmtTimeStamp(request->getGmtTimeStamp());

		TraceProcExecute trace(tdbb, proc_request, request, m_targetList);

		AutoSetRestore<USHORT> autoOriginalTimeZone(
			&tdbb->getAttachment()->att_original_timezone,
			tdbb->getAttachment()->att_current_timezone);

		EXE_start(tdbb, proc_request, request->req_transaction);

		if (iml)
			EXE_send(tdbb, proc_request, 0, iml, im);

		trace.finish(true, ITracePlugin::RESULT_SUCCESS);
	}
	catch (const Exception&)
	{
		close(tdbb);
		throw;
	}

	proc_request->req_flags |= req_proc_fetch;
}

void ProcedureScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		Request* const proc_request = impure->irsb_req_handle;

		if (proc_request)
		{
			EXE_unwind(tdbb, proc_request);
			proc_request->req_flags &= ~req_in_use;
			impure->irsb_req_handle = NULL;
			proc_request->req_attachment = NULL;
		}

		delete [] impure->irsb_message;
		impure->irsb_message = NULL;
	}
}

bool ProcedureScan::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	UserId* invoker = m_procedure->invoker ? m_procedure->invoker : tdbb->getAttachment()->att_ss_user;
	AutoSetRestore<UserId*> userIdHolder(&tdbb->getAttachment()->att_ss_user, invoker);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	const Format* const msg_format = m_procedure->getOutputFormat();
	const ULONG oml = msg_format->fmt_length;
	UCHAR* om = impure->irsb_message;

	if (!om)
		om = impure->irsb_message = FB_NEW_POOL(*tdbb->getDefaultPool()) UCHAR[oml];

	Record* const record = VIO_record(tdbb, rpb, m_format, tdbb->getDefaultPool());

	Request* const proc_request = impure->irsb_req_handle;

	TraceProcFetch trace(tdbb, proc_request);

	AutoSetRestoreFlag<ULONG> autoSetReqProcSelect(&proc_request->req_flags, req_proc_select, true);
	AutoSetRestore<USHORT> autoOriginalTimeZone(
		&tdbb->getAttachment()->att_original_timezone,
		tdbb->getAttachment()->att_current_timezone);

	try
	{
		EXE_receive(tdbb, proc_request, 1, oml, om);

		dsc desc = msg_format->fmt_desc[msg_format->fmt_count - 1];
		desc.dsc_address = (UCHAR*) (om + (IPTR) desc.dsc_address);
		SSHORT eos;
		dsc eos_desc;
		eos_desc.makeShort(0, &eos);
		MOV_move(tdbb, &desc, &eos_desc);

		if (!eos)
		{
			trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
			rpb->rpb_number.setValid(false);
			return false;
		}
	}
	catch (const Exception&)
	{
		trace.fetch(true, ITracePlugin::RESULT_FAILED);
		close(tdbb);
		throw;
	}

	trace.fetch(false, ITracePlugin::RESULT_SUCCESS);

	for (USHORT i = 0; i < m_format->fmt_count; i++)
	{
		assignParams(tdbb, &msg_format->fmt_desc[2 * i], &msg_format->fmt_desc[2 * i + 1],
					 om, &m_format->fmt_desc[i], i, record);
	}

	rpb->rpb_number.setValid(true);
	return true;
}

bool ProcedureScan::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult ProcedureScan::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void ProcedureScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias, false) + " NATURAL";

	if (!level)
		plan += ")";
}

void ProcedureScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "ProcedureScan";

	planEntry.lines.add().text = "Procedure " + printName(tdbb, m_procedure->getName(), m_alias) + " Scan";
	printOptInfo(planEntry.lines);

	planEntry.objectType = obj_procedure;
	planEntry.objectName = m_procedure->getName();

	if (m_alias.hasData() && m_procedure->getName().toString() != m_alias)
		planEntry.alias = m_alias;
}

void ProcedureScan::assignParams(thread_db* tdbb,
								 const dsc* from_desc,
								 const dsc* flag_desc,
								 const UCHAR* msg,
								 const dsc* to_desc,
								 SSHORT to_id,
								 Record* record) const
{
	SSHORT indicator;
	dsc desc2;
	desc2.makeShort(0, &indicator);

	dsc desc1 = *flag_desc;
	desc1.dsc_address = const_cast<UCHAR*>(msg) + (IPTR) flag_desc->dsc_address;

	MOV_move(tdbb, &desc1, &desc2);

	if (indicator)
	{
		record->setNull(to_id);
		const USHORT len = to_desc->dsc_length;
		UCHAR* const p = record->getData() + (IPTR) to_desc->dsc_address;
		switch (to_desc->dsc_dtype)
		{
		case dtype_text:
			/* YYY - not necessarily the right thing to do */
			/* YYY for text formats that don't have trailing spaces */
			if (len)
			{
				const CHARSET_ID chid = DSC_GET_CHARSET(to_desc);
				/*
				CVC: I don't know if we have to check for dynamic-127 charset here.
				If that is needed, the line above should be replaced by the commented code here.
				CHARSET_ID chid = INTL_TTYPE(to_desc);
				if (chid == ttype_dynamic)
					chid = INTL_charset(tdbb, chid);
				*/
				const char pad = chid == ttype_binary ? '\0' : ' ';
				memset(p, pad, len);
			}
			break;

		case dtype_cstring:
			*p = 0;
			break;

		case dtype_varying:
			*reinterpret_cast<SSHORT*>(p) = 0;
			break;

		default:
			if (len)
				memset(p, 0, len);
			break;
		}
	}
	else
	{
		record->clearNull(to_id);
		desc1 = *from_desc;
		desc1.dsc_address = const_cast<UCHAR*>(msg) + (IPTR) desc1.dsc_address;
		desc2 = *to_desc;
		desc2.dsc_address = record->getData() + (IPTR) desc2.dsc_address;
		if (!DSC_EQUIV(&desc1, &desc2, false))
		{
			MOV_move(tdbb, &desc1, &desc2);
			return;
		}

		switch (desc1.dsc_dtype)
		{
		case dtype_short:
			*reinterpret_cast<SSHORT*>(desc2.dsc_address) =
				*reinterpret_cast<SSHORT*>(desc1.dsc_address);
			break;
		case dtype_long:
			*reinterpret_cast<SLONG*>(desc2.dsc_address) =
				*reinterpret_cast<SLONG*>(desc1.dsc_address);
			break;
		case dtype_int64:
			*reinterpret_cast<SINT64*>(desc2.dsc_address) =
				*reinterpret_cast<SINT64*>(desc1.dsc_address);
			break;
		default:
			memcpy(desc2.dsc_address, desc1.dsc_address, desc1.dsc_length);
		}
	}
}
