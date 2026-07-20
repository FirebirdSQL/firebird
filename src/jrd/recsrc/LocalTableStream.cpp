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
#include "../dsql/StmtNodes.h"
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/vio_proto.h"
#include "../common/classes/auto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------
// Data access: local table
// ------------------------

LocalTableStream::LocalTableStream(CompilerScratch* csb, StreamType stream, const DeclareLocalTableNode* table,
	bool outerDecl)
	: RecordStream(csb, stream),
	  m_table(table),
	  m_outerDecl(outerDecl)
{
	fb_assert(m_table);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = DEFAULT_CARDINALITY;
}

void LocalTableStream::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	const auto rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;

	const auto localTableRequest = impure->localTableRequest = request->getLocalTableRequest(m_outerDecl);

	if (m_table->useLtt)
	{
		AutoSetRestore<FB_UINT64> autoFrameId(
			&tdbb->tdbb_temp_frame_id, localTableRequest->getLocalTableInstanceId(tdbb));

		rpb->rpb_relation = m_table->getRelation(tdbb, localTableRequest);
	}

	rpb->rpb_number.setValue(BOF_NUMBER);

	// Inside an autonomous transaction block, reading a DLTT uses the parent transaction.
	// The parent's top savepoint already holds a verb action for this relation (from the
	// INSERT that made the data visible), so get_undo_data returns udForceBack, hiding
	// all freshly-inserted rows. Bypass cursor stability for this case only — non-autonomous
	// reads must keep cursor stability so that INSERT...SELECT from the same DLTT does not
	// recurse into rows inserted by the current statement.
	if (m_table->useLtt)
	{
		if (localTableRequest->req_auto_trans.hasData())
			rpb->rpb_stream_flags |= RPB_s_unstable;
		else
			rpb->rpb_stream_flags &= ~RPB_s_unstable;
	}
}

void LocalTableStream::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
		impure->irsb_flags &= ~irsb_open;
}

bool LocalTableStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	const auto request = tdbb->getRequest();
	const auto rpb = &request->req_rpb[m_stream];
	const auto impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (!m_table->useLtt)
	{
		if (!rpb->rpb_record)
			rpb->rpb_record = FB_NEW_POOL(*tdbb->getDefaultPool()) Record(*tdbb->getDefaultPool(), m_format);

		const auto recordBuffer = m_table->getImpure(tdbb, impure->localTableRequest)->recordBuffer;

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

	const auto localTableRequest = impure->localTableRequest;
	const auto transaction = localTableRequest->getLocalTableTransaction();
	AutoSetRestore<FB_UINT64> autoFrameId(
		&tdbb->tdbb_temp_frame_id, localTableRequest->getLocalTableInstanceId(tdbb));

	AutoSetRestore2<jrd_tra*, thread_db> autoTransaction(
		tdbb, &thread_db::getTransaction, &thread_db::setTransaction, transaction);

	const bool found = VIO_next_record(tdbb, rpb, transaction, request->req_pool, DPM_next_all, nullptr);

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
