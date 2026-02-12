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

NestedLoopJoin::NestedLoopJoin(CompilerScratch* csb,
							   FB_SIZE_T count,
							   RecordSource* const* args,
							   JoinType joinType)
	: RecordSource(csb),
	  m_joinType(joinType),
	  m_boolean(nullptr),
	  m_args(csb->csb_pool, count)
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
							   RecordSource* outer,
							   RecordSource* inner,
							   BoolExprNode* boolean)
	: RecordSource(csb),
	  m_joinType(OUTER_JOIN),
	  m_boolean(boolean),
	  m_args(csb->csb_pool, 2)
{
	fb_assert(outer && inner);

	m_impure = csb->allocImpure<Impure>();

	m_args.add(outer);
	m_args.add(inner);

	m_cardinality = outer->getCardinality() * inner->getCardinality();
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

		for (const auto arg : m_args)
			arg->close(tdbb);
	}
}

bool NestedLoopJoin::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	if (m_joinType == INNER_JOIN)
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
	else if (m_joinType == SEMI_JOIN || m_joinType == ANTI_JOIN)
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
					if (m_joinType == ANTI_JOIN)
					{
						stopArg = i;
						break;
					}
				}
				else
				{
					if (m_joinType == SEMI_JOIN)
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
		fb_assert(m_joinType == OUTER_JOIN);
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

bool NestedLoopJoin::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult NestedLoopJoin::lockRecord(thread_db* tdbb) const
{
	if (m_joinType == SEMI_JOIN || m_joinType == ANTI_JOIN)
		return m_args.front()->lockRecord(tdbb);

	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void NestedLoopJoin::getChildren(Array<const RecordSource*>& children) const
{
	for (const auto arg : m_args)
		children.add(arg);
}

void NestedLoopJoin::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
{
	if (m_args.hasData())
	{
		if (detailed)
		{
			plan += printIndent(++level) + "Nested Loop Join ";

			switch (m_joinType)
			{
				case INNER_JOIN:
					plan += "(inner)";
					break;

				case OUTER_JOIN:
					plan += "(outer)";
					break;

				case SEMI_JOIN:
					plan += "(semi)";
					break;

				case ANTI_JOIN:
					plan += "(anti)";
					break;

				default:
					fb_assert(false);
			}

			printOptInfo(plan);

			if (recurse)
			{
				for (const auto arg : m_args)
					arg->print(tdbb, plan, true, level, recurse);
			}
		}
		else
		{
			level++;
			plan += "JOIN (";
			for (const auto arg : m_args)
			{
				if (arg != m_args.front())
					plan += ", ";

				arg->print(tdbb, plan, false, level, recurse);
			}
			plan += ")";
		}
	}
}

void NestedLoopJoin::markRecursive()
{
	for (auto arg : m_args)
		arg->markRecursive();
}

void NestedLoopJoin::findUsedStreams(StreamList& streams, bool expandAll) const
{
	for (const auto arg : m_args)
		arg->findUsedStreams(streams, expandAll);
}

void NestedLoopJoin::invalidateRecords(Request* request) const
{
	for (const auto arg : m_args)
		arg->invalidateRecords(request);
}

void NestedLoopJoin::nullRecords(thread_db* tdbb) const
{
	for (const auto arg : m_args)
		arg->nullRecords(tdbb);
}

bool NestedLoopJoin::fetchRecord(thread_db* tdbb, FB_SIZE_T n) const
{
	fb_assert(m_joinType == INNER_JOIN);

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
