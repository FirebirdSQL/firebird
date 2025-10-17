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
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/optimizer/Optimizer.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------------
// Data access: nested loops join
// ------------------------------

NestedLoopJoin::NestedLoopJoin(CompilerScratch* csb, JoinType joinType,
							   FB_SIZE_T count, RecordSource* const* args)
	: Join(csb, count, joinType)
{
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = MINIMUM_CARDINALITY;

	for (FB_SIZE_T i = 0; i < count; i++)
	{
		m_args.add(args[i]);
		m_cardinality *= args[i]->getCardinality();
	}
}

NestedLoopJoin::NestedLoopJoin(CompilerScratch* csb,
							   RecordSource* outer, RecordSource* inner,
							   BoolExprNode* boolean)
	: Join(csb, 2, JoinType::OUTER, boolean)
{
	fb_assert(outer && inner);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = outer->getCardinality() * inner->getCardinality();

	m_args.add(outer);
	m_args.add(inner);
}

void NestedLoopJoin::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_first | irsb_mustread;
}

void NestedLoopJoin::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		Join::close(tdbb);
	}
}

bool NestedLoopJoin::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	if (m_joinType == JoinType::INNER)
	{
		if (impure->irsb_flags & irsb_first)
		{
			for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
			{
				m_args[i]->open(tdbb);

				if (!fetchRecord(tdbb, i))
					return false;
			}

			impure->irsb_flags &= ~irsb_first;
		}
		// hvlad: self referenced members are removed from recursive SELECT's
		// in recursive CTE (it is done in dsql\pass1.cpp). If there are no other
		// members in such SELECT then rsb_count will be zero. Handle it.
		else if (m_args.isEmpty())
			return false;
		else if (!fetchRecord(tdbb, m_args.getCount() - 1))
			return false;
	}
	else if (m_joinType == JoinType::SEMI || m_joinType == JoinType::ANTI)
	{
		const auto outer = m_args[0];

		if (impure->irsb_flags & irsb_first)
		{
			outer->open(tdbb);

			impure->irsb_flags &= ~irsb_first;
		}

		while (true)
		{
			if (impure->irsb_flags & irsb_joined)
			{
				for (FB_SIZE_T i = 1; i < m_args.getCount(); i++)
					m_args[i]->close(tdbb);

				impure->irsb_flags &= ~irsb_joined;
			}

			if (!outer->getRecord(tdbb))
				return false;

			FB_SIZE_T stopArg = 0;

			for (FB_SIZE_T i = 1; i < m_args.getCount(); i++)
			{
				m_args[i]->open(tdbb);

				if (m_args[i]->getRecord(tdbb))
				{
					if (m_joinType == JoinType::ANTI)
					{
						stopArg = i;
						break;
					}
				}
				else
				{
					if (m_joinType == JoinType::SEMI)
					{
						stopArg = i;
						break;
					}
				}
			}

			if (!stopArg)
				break;

			for (FB_SIZE_T i = 1; i <= stopArg; i++)
				m_args[i]->close(tdbb);
		}

		impure->irsb_flags |= irsb_joined;
	}
	else
	{
		fb_assert(m_joinType == JoinType::OUTER);
		fb_assert(m_args.getCount() == 2);

		const auto outer = m_args[0];
		const auto inner = m_args[1];

		if (impure->irsb_flags & irsb_first)
		{
			outer->open(tdbb);
			impure->irsb_flags &= ~irsb_first;
		}

		while (true)
		{
			if (impure->irsb_flags & irsb_mustread)
			{
				if (!outer->getRecord(tdbb))
					return false;

				if (m_boolean && !m_boolean->execute(tdbb, request))
				{
					// The boolean pertaining to the left sub-stream is false
					// so just join sub-stream to a null valued right sub-stream
					inner->nullRecords(tdbb);
					return true;
				}

				impure->irsb_flags &= ~(irsb_mustread | irsb_joined);
				inner->open(tdbb);
			}

			if (inner->getRecord(tdbb))
			{
				impure->irsb_flags |= irsb_joined;
				return true;
			}

			inner->close(tdbb);
			impure->irsb_flags |= irsb_mustread;

			if (!(impure->irsb_flags & irsb_joined))
			{
				// The current left sub-stream record has not been joined to anything.
				// Join it to a null valued right sub-stream.
				inner->nullRecords(tdbb);
				return true;
			}
		}
	}

	return true;
}

void NestedLoopJoin::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (m_args.hasData())
	{
		level++;
		plan += "JOIN (";
		Join::getLegacyPlan(tdbb, plan, level);
		plan += ")";
	}
}

void NestedLoopJoin::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "NestedLoopJoin";

	planEntry.lines.add().text = "Nested Loop Join " + printType();
	printOptInfo(planEntry.lines);

	Join::internalGetPlan(tdbb, planEntry, level, recurse);
}

bool NestedLoopJoin::fetchRecord(thread_db* tdbb, FB_SIZE_T n) const
{
	fb_assert(m_joinType == JoinType::INNER);

	const RecordSource* const arg = m_args[n];

	if (arg->getRecord(tdbb))
		return true;

	// We have exhausted this stream, so close it; if there is
	// another candidate record from the n-1 streams to the left,
	// then reopen the stream and start again from the beginning.

	while (true)
	{
		arg->close(tdbb);

		if (n == 0 || !fetchRecord(tdbb, n - 1))
			return false;

		arg->open(tdbb);

		if (arg->getRecord(tdbb))
			return true;
	}
}
