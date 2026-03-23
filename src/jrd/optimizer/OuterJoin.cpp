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
 *  Copyright (c) 2023 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"

#include "../jrd/jrd.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/RecordSourceNodes.h"

#include "../jrd/optimizer/Optimizer.h"

using namespace Firebird;
using namespace Jrd;


//
// Constructor
//

OuterJoin::OuterJoin(thread_db* aTdbb, Optimizer* opt,
					 const RseNode* rse, RiverList& rivers,
					 SortNode** sortClause)
	: PermanentStorage(*aTdbb->getDefaultPool()),
	  tdbb(aTdbb),
	  optimizer(opt),
	  csb(opt->getCompilerScratch()),
	  sortPtr(sortClause)
{
	// Loop through the join sub-streams. Do it backwards, as rivers are passed as a stack.

	fb_assert(rse->rse_relations.getCount() == 2);
	fb_assert(rivers.getCount() <= 2);

	for (int pos = 1; pos >= 0; pos--)
	{
		auto node = rse->rse_relations[pos];
		auto& joinStream = joinStreams[pos];
		joinStream.node = node;

		if (nodeIs<RelationSourceNode>(node) || nodeIs<LocalTableSourceNode>(node))
		{
			const auto stream = node->getStream();
			fb_assert(!(csb->csb_rpt[stream].csb_flags & csb_active));
			joinStream.number = stream;
		}
		else
		{
			joinStream.river = rivers.pop();
		}
	};

	fb_assert(rivers.isEmpty());

	// Determine which stream should be outer and which is inner.
	// In the case of a left join, the syntactically left stream is the outer,
	// and the right stream is the inner. For a right join, just swap the sides.
	// For a full join, the order does not matter, but given the first outer join
	// is already compiled, let's better preserve the original order.

	if (rse->rse_jointype == blr_right)
	{
		// RIGHT JOIN is converted into LEFT JOIN by the BLR parser,
		// so it should never appear here
		fb_assert(false);
		std::swap(joinStreams[0], joinStreams[1]);
	}
}


// Generate a top level outer join. The "outer" and "inner" sub-streams must be
// handled differently from each other. The inner is like other streams.
// The outer one isn't because conjuncts may not eliminate records from the stream.
// They only determine if a join with an inner stream record is to be attempted.

RecordSource* OuterJoin::generate()
{
	const auto outerJoinRsb = process();

	if (!optimizer->isFullJoin())
		return outerJoinRsb;

	auto& outer = joinStreams[0];
	auto& inner = joinStreams[1];

	StreamList outerStreams, innerStreams;
	outer.getStreams(outerStreams);
	inner.getStreams(innerStreams);

	// A FULL JOIN B is currently implemented similar to:
	//
	// (A LEFT JOIN B)
	// UNION ALL
	// (B LEFT JOIN A WHERE A.* IS NULL)
	//
	// See also FullOuterJoin class implementation.
	//
	// At this point we already have the first part -- (A LEFT JOIN B) -- ready,
	// so just swap the sides and make the second (reversed) join.

	std::swap(outer, inner);

	// Reset both streams to their original states

	for (const auto stream : outerStreams)
		csb->csb_rpt[stream].deactivate();

	outer.river = nullptr;

	for (const auto stream : innerStreams)
		csb->csb_rpt[stream].deactivate();

	inner.river = nullptr;

	// Clone the booleans to make them re-usable for a reversed join

	for (auto iter = optimizer->getConjuncts(); iter.hasData(); ++iter)
	{
		if (iter & Optimizer::CONJUNCT_USED)
			iter.reset(CMP_clone_node_opt(tdbb, csb, iter));
	}

	const auto reversedJoinRsb = process();

	// Allocate and return the final join record source

	return FB_NEW_POOL(getPool()) FullOuterJoin(csb, outerJoinRsb, reversedJoinRsb, outerStreams);
}


RecordSource* OuterJoin::process()
{
	BoolExprNode* boolean = nullptr;

	auto& outer = joinStreams[0];
	auto& inner = joinStreams[1];

	// Generate record sources for the sub-streams.
	// For the outer sub-stream we also will get a boolean back.

	RecordSource* outerRsb = nullptr;
	RecordSource* innerRsb = nullptr;

	if (outer.number != INVALID_STREAM)
	{
		outerRsb = optimizer->generateRetrieval(outer.number,
			optimizer->isFullJoin() ? nullptr : sortPtr, true, false, &boolean);
	}
	else
	{
		if (outer.river)
			outerRsb = outer.river->getRecordSource();
		else
		{
			fb_assert(optimizer->isFullJoin());

			outerRsb = outer.node->compile(tdbb, optimizer, false);
		}

		// Collect booleans computable for the outer sub-stream, it must be active now
		boolean = optimizer->composeBoolean();
	}

	fb_assert(outerRsb);

	if (inner.number != INVALID_STREAM)
	{
		// AB: the sort clause for the inner stream of an OUTER JOIN
		//	   should never be used for the index retrieval
		innerRsb = optimizer->generateRetrieval(inner.number, nullptr, false, true);
	}
	else
	{
		if (inner.river)
			innerRsb = inner.river->getRecordSource();
		else
		{
			fb_assert(optimizer->isFullJoin());

			StreamList outerStreams;
			outerRsb->findUsedStreams(outerStreams);
			optimizer->setOuterStreams(outerStreams);

			innerRsb = inner.node->compile(tdbb, optimizer, true);
		}
	}

	fb_assert(innerRsb);

	// Generate a parent filter record source for any remaining booleans that
	// were not satisfied via an index lookup

	innerRsb = optimizer->applyResidualBoolean(innerRsb);

	// Allocate and return the join record source

	return FB_NEW_POOL(getPool()) NestedLoopJoin(csb, outerRsb, innerRsb, boolean);
};


