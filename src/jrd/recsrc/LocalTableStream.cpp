/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2021 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/Savepoint.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/vio_proto.h"
#include "../common/classes/auto.h"

#include "RecordSource.h"

namespace Firebird::Jrd
{



static SavNumber startLocalTableCursorSavepoint(const LocalTableContext& context)
{
	const auto localTableRequest = context.getLocalTableRequest();

	if (!localTableRequest || !localTableRequest->req_auto_trans.hasData())
		return 0;

	return StableCursorSavePoint::startSavepoint(context.getTransaction());
}


LocalTableContext::LocalTableContext(thread_db* tdbb, Request* request,
		const DeclareLocalTableNode* table, bool outerDecl)
	: m_tdbb(tdbb),
	  m_request(request),
	  m_localTableRequest(table ? request->getLocalTableRequest(outerDecl) : request),
	  m_oldTransaction(tdbb->getTransaction()),
	  m_transaction(request->req_transaction),
	  m_oldFrameId(tdbb->tdbb_temp_frame_id),
	  m_frameId(0)
{
	m_oldSnapshot.init();

	if (table && table->useLtt)
	{
		m_transaction = m_localTableRequest->getLocalTableTransaction();
		tdbb->setTransaction(m_transaction);
		m_frameId = m_localTableRequest->getLocalTableInstanceId(tdbb);
		tdbb->tdbb_temp_frame_id = m_frameId;
		m_switched = true;

		Request::AutoTranCtx autoTranCtx;
		if (m_localTableRequest->getLocalTableAutoTranCtx(autoTranCtx))
		{
			m_oldSnapshot = request->req_snapshot;
			request->req_snapshot = autoTranCtx.m_snapshot;
			m_restoreSnapshot = true;
		}
	}
}

LocalTableContext::~LocalTableContext()
{
	if (m_restoreSnapshot)
		m_request->req_snapshot = m_oldSnapshot;

	if (m_switched)
	{
		m_tdbb->tdbb_temp_frame_id = m_oldFrameId;
		m_tdbb->setTransaction(m_oldTransaction);
	}
}


LocalTableScan::LocalTableScan(CompilerScratch* csb, StreamType stream,
		const DeclareLocalTableNode* table, bool outerDecl, const Format* format)
	: RecordStream(csb, stream, format),
	  m_localTable(table),
	  m_outerDecl(outerDecl),
	  m_localImpure(table ? csb->allocImpure<LocalImpure>() : 0)
{
	if (m_localImpure)
		m_impure = m_localImpure;
}

void LocalTableScan::setupLocalTable(thread_db* tdbb, const LocalTableContext& context) const
{
	if (!m_localTable || !m_localTable->useLtt)
		return;

	const auto rpb = &tdbb->getRequest()->req_rpb[m_stream];
	rpb->rpb_relation = m_localTable->getRelation(tdbb, context.getLocalTableRequest());
	rpb->rpb_temp_instance_id = context.getFrameId();
}

void LocalTableScan::initializeLocalTable(const LocalTableContext& context) const
{
	if (!m_localTable)
		return;

	const auto request = context.getLocalTableRequest();
	const auto impure = context.getRequest()->getImpure<LocalImpure>(m_localImpure);
	impure->localTableRequest = request;
	impure->cursorTransaction = nullptr;
	impure->cursorSavepoint = 0;

	if (!m_localTable->useLtt)
		return;

	impure->cursorSavepoint = startLocalTableCursorSavepoint(context);
	if (impure->cursorSavepoint)
		impure->cursorTransaction = context.getTransaction();
}

void LocalTableScan::closeLocalTable(thread_db* tdbb) const
{
	if (!m_localTable || !m_localTable->useLtt || !m_localImpure)
		return;

	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<LocalImpure>(m_localImpure);
	if (!impure->cursorSavepoint)
		return;

	StableCursorSavePoint::releaseSavepoint(tdbb, impure->cursorTransaction, impure->cursorSavepoint);
	impure->cursorTransaction = nullptr;
}

bool LocalTableScan::refetchRecord(thread_db* tdbb) const
{
	if (m_localTable)
		return true;

	return RecordStream::refetchRecord(tdbb);
}

WriteLockResult LocalTableScan::lockRecord(thread_db* tdbb) const
{
	if (m_localTable)
		status_exception::raise(Arg::Gds(isc_record_lock_not_supp));

	return RecordStream::lockRecord(tdbb);
}

void LocalTableScan::nullRecords(thread_db* tdbb) const
{
	if (!m_localTable)
	{
		RecordStream::nullRecords(tdbb);
		return;
	}

	const auto request = tdbb->getRequest();
	LocalTableContext context(tdbb, request, m_localTable, m_outerDecl);
	RecordStream::nullRecords(tdbb);
}


// ----------------------------------
// Data access: local table decorator
// ----------------------------------

LocalTableRecordSource::LocalTableRecordSource(CompilerScratch* csb, StreamType stream,
		RecordSource* next, const DeclareLocalTableNode* table, bool outerDecl)
	: RecordSource(csb),
	  m_stream(stream),
	  m_next(next),
	  m_localTable(table),
	  m_outerDecl(outerDecl)
{
	fb_assert(m_next && m_localTable && m_localTable->useLtt);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = next->getCardinality();
}

void LocalTableRecordSource::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);
	LocalTableContext context(tdbb, request, m_localTable, m_outerDecl);

	const auto rpb = &request->req_rpb[m_stream];
	rpb->rpb_relation = m_localTable->getRelation(tdbb, context.getLocalTableRequest());
	rpb->rpb_temp_instance_id = context.getFrameId();

	impure->localTableRequest = context.getLocalTableRequest();
	impure->cursorTransaction = nullptr;
	impure->cursorSavepoint = 0;

	impure->cursorSavepoint = startLocalTableCursorSavepoint(context);
	if (impure->cursorSavepoint)
		impure->cursorTransaction = context.getTransaction();

	impure->irsb_flags = irsb_open;
	m_next->open(tdbb);
}

void LocalTableRecordSource::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		LocalTableContext context(tdbb, request, m_localTable, m_outerDecl);
		m_next->close(tdbb);

		StableCursorSavePoint::releaseSavepoint(tdbb, impure->cursorTransaction, impure->cursorSavepoint);
		impure->cursorTransaction = nullptr;
	}
}

bool LocalTableRecordSource::internalGetRecord(thread_db* tdbb) const
{
	LocalTableContext context(tdbb, tdbb->getRequest(), m_localTable, m_outerDecl);
	return m_next->getRecord(tdbb);
}

bool LocalTableRecordSource::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult LocalTableRecordSource::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void LocalTableRecordSource::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	m_next->getLegacyPlan(tdbb, plan, level);
}

void LocalTableRecordSource::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry,
	unsigned level, bool recurse) const
{
	m_next->getPlan(tdbb, planEntry, level, recurse);
}

void LocalTableRecordSource::markRecursive()
{
	m_next->markRecursive();
}

void LocalTableRecordSource::invalidateRecords(Request* request) const
{
	m_next->invalidateRecords(request);
}

void LocalTableRecordSource::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

bool LocalTableRecordSource::isDependent(const StreamList& streams) const
{
	return m_next->isDependent(streams);
}

void LocalTableRecordSource::nullRecords(thread_db* tdbb) const
{
	LocalTableContext context(tdbb, tdbb->getRequest(), m_localTable, m_outerDecl);
	m_next->nullRecords(tdbb);
}

// ------------------------
// Data access: local table
// ------------------------

LocalTableStream::LocalTableStream(CompilerScratch* csb, StreamType stream, const DeclareLocalTableNode* table,
		bool outerDecl)
	: LocalTableScan(csb, stream, table, outerDecl)
{
	fb_assert(m_localTable);

	m_cardinality = DEFAULT_CARDINALITY;
}

void LocalTableStream::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);
	LocalTableContext context(tdbb, request, m_localTable, m_outerDecl);
	setupLocalTable(tdbb, context);
	initializeLocalTable(context);

	impure->irsb_flags = irsb_open;

	const auto rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;
	rpb->rpb_number.setValue(BOF_NUMBER);
}

void LocalTableStream::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
		impure->irsb_flags &= ~irsb_open;

	closeLocalTable(tdbb);
}

bool LocalTableStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	const auto request = tdbb->getRequest();
	const auto rpb = &request->req_rpb[m_stream];
	const auto impure = request->getImpure<Impure>(m_impure);
	LocalTableContext context(tdbb, request, m_localTable, m_outerDecl);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (!m_localTable->useLtt)
	{
		if (!rpb->rpb_record)
			rpb->rpb_record = FB_NEW_POOL(*tdbb->getDefaultPool()) Record(*tdbb->getDefaultPool(), m_format);

		const auto recordBuffer = m_localTable->getImpure(tdbb, impure->localTableRequest)->recordBuffer;

		while (true)
		{
			rpb->rpb_number.increment();

			if (rpb->rpb_number.getValue() >= recordBuffer->getCount())
			{
				rpb->rpb_number.setValid(false);
				return false;
			}

			if (recordBuffer->fetch(rpb->rpb_number.getValue(), rpb->rpb_record))
			{
				rpb->rpb_number.setValid(true);
				break;
			}
		}

		return true;
	}

	const bool found = VIO_next_record(tdbb, rpb, context.getTransaction(), request->req_pool,
		DPM_next_all, nullptr);

	if (found)
	{
		rpb->rpb_number.setValid(true);
		return true;
	}

	rpb->rpb_number.setValid(false);
	return false;
}

bool LocalTableStream::refetchRecord(thread_db* tdbb) const
{
	return true;
}

WriteLockResult LocalTableStream::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void LocalTableStream::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	//// TODO: Use Local Table name/alias.

	if (!level)
		plan += "(";

	plan += "Local_Table";
	plan += " NATURAL";

	if (!level)
		plan += ")";
}

void LocalTableStream::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "LocalTableStream";

	//// TODO: Use Local Table name/alias.

	planEntry.lines.add().text = "Local Table Full Scan";
	printOptInfo(planEntry.lines);
}


}	// namespace Firebird::Jrd
