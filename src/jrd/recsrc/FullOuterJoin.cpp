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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ----------------------------
// Data access: full outer join
// ----------------------------

FullOuterJoin::FullOuterJoin(CompilerScratch* csb,
							 RecordSource* arg1, RecordSource* arg2,
							 const StreamList& checkStreams)
	: Join(csb, 2, JoinType::OUTER),
	  m_checkStreams(csb->csb_pool, checkStreams)
{
	fb_assert(arg1 && arg2);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = arg1->getCardinality() + arg2->getCardinality();

	m_args.add(arg1);
	m_args.add(arg2);
}

void FullOuterJoin::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_first;

	m_args[0]->open(tdbb);
}

void FullOuterJoin::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		Join::close(tdbb);
	}
}

bool FullOuterJoin::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	const auto arg1 = m_args[0];
	const auto arg2 = m_args[1];

	if (impure->irsb_flags & irsb_first)
	{
		if (arg1->getRecord(tdbb))
			return true;

		impure->irsb_flags &= ~irsb_first;
		arg1->close(tdbb);
		arg2->open(tdbb);
	}

	// We should exclude matching records from the right-joined (second) record source,
	// as they're already returned from the left-joined (first) record source

	while (arg2->getRecord(tdbb))
	{
		bool matched = false;

		for (const auto stream : m_checkStreams)
		{
			if (request->req_rpb[stream].rpb_number.isValid())
			{
				matched = true;
				break;
			}
		}

		if (!matched)
			return true;
	}

	return false;
}

void FullOuterJoin::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	level++;
	plan += "JOIN (";
	Join::getLegacyPlan(tdbb, plan, level);
	plan += ")";
}

void FullOuterJoin::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "FullOuterJoin";

	planEntry.lines.add().text = "Full Outer Join";
	printOptInfo(planEntry.lines);

	Join::getPlan(tdbb, planEntry, level, recurse);
}
