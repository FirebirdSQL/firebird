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
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/Attachment.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// -------------------------------------------
// Data access: sequential complete table scan
// -------------------------------------------

FullTableScan::FullTableScan(CompilerScratch* csb, const string& alias,
							 StreamType stream, jrd_rel* relation,
							 const Array<DbKeyRangeNode*>& dbkeyRanges)
	: RecordStream(csb, stream),
	  m_alias(csb->csb_pool, alias),
	  m_relation(relation),
	  m_dbkeyRanges(csb->csb_pool, dbkeyRanges)
{
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = csb->csb_rpt[stream].csb_cardinality;
}

void FullTableScan::internalOpen(thread_db* tdbb) const
{
	Database* const dbb = tdbb->getDatabase();
	Attachment* const attachment = tdbb->getAttachment();
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	RLCK_reserve_relation(tdbb, request->req_transaction, m_relation, false);

	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;

	// Unless this is the only attachment, limit the cache flushing
	// effect of large sequential scans on the page working sets of
	// other attachments

	if (attachment && (attachment != dbb->dbb_attachments || attachment->att_next))
	{
		// If the relation has more data pages than the number of
		// pages in the buffer cache then mark the input window
		// block as a large scan so that a data page is released
		// to the LRU tail after its last record is fetched.
		//
		// A database backup treats everything as a large scan
		// because the cumulative effect of scanning all relations
		// is equal to that of a single large relation.

		BufferControl* const bcb = dbb->dbb_bcb;

		if (attachment->isGbak() || DPM_data_pages(tdbb, m_relation) > bcb->bcb_count)
		{
			rpb->getWindow(tdbb).win_flags = WIN_large_scan;
			rpb->rpb_org_scans = m_relation->rel_scan_count++;
		}
	}

	rpb->rpb_number.setValue(BOF_NUMBER);

	if (m_dbkeyRanges.hasData())
	{
		impure->irsb_lower.setValid(false);
		impure->irsb_upper.setValid(false);

		EVL_dbkey_bounds(tdbb, m_dbkeyRanges, rpb->rpb_relation,
			impure->irsb_lower, impure->irsb_upper);

		if (impure->irsb_lower.isValid())
		{
			auto number = impure->irsb_lower.getValue();

			const auto ppages = rpb->rpb_relation->getPages(tdbb)->rel_pages;
			const auto maxRecno = (SINT64) ppages->count() *
				dbb->dbb_dp_per_pp * dbb->dbb_max_records - 1;
			if (number > maxRecno)
				number = maxRecno;

			rpb->rpb_number.setValue(number - 1); // position prior to the starting one
		}
	}
}

void FullTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		record_param* const rpb = &request->req_rpb[m_stream];
		if ((rpb->getWindow(tdbb).win_flags & WIN_large_scan) &&
			m_relation->rel_scan_count)
		{
			m_relation->rel_scan_count--;
		}
	}
}

bool FullTableScan::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	const RecordNumber* upper = impure->irsb_upper.isValid() ? &impure->irsb_upper : nullptr;

	if (VIO_next_record(tdbb, rpb, request->req_transaction, request->req_pool, DPM_next_all, upper))
	{
		rpb->rpb_number.setValid(true);
		return true;
	}

	rpb->rpb_number.setValid(false);
	return false;
}

void FullTableScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias, false) + " NATURAL";

	if (!level)
		plan += ")";
}

void FullTableScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "FullTableScan";

	auto lowerBounds = 0, upperBounds = 0;
	for (const auto range : m_dbkeyRanges)
	{
		if (range->lower)
			lowerBounds++;

		if (range->upper)
			upperBounds++;
	}

	string bounds;
	if (lowerBounds && upperBounds)
		bounds += " (lower bound, upper bound)";
	else if (lowerBounds)
		bounds += " (lower bound)";
	else if (upperBounds)
		bounds += " (upper bound)";

	planEntry.lines.add().text = "Table " + printName(tdbb, m_relation->rel_name, m_alias) + " Full Scan" + bounds;
	printOptInfo(planEntry.lines);

	planEntry.objectType = m_relation->getObjectType();
	planEntry.objectName = m_relation->rel_name;

	if (m_alias.hasData() && m_relation->rel_name.object != m_alias)
		planEntry.alias = m_alias;
}
