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
		const auto node = rse->rse_relations[pos];
		auto& joinStream = joinStreams[pos];

		if (nodeIs<RelationSourceNode>(node) || nodeIs<LocalTableSourceNode>(node))
		{
			const auto stream = node->getStream();
			fb_assert(!(csb->csb_rpt[stream].csb_flags & csb_active));
			joinStream.number = stream;
		}
		else
		{
			const auto river = rivers.pop();
			joinStream.rsb = river->getRecordSource();
		}
	};

	fb_assert(rivers.isEmpty());

	// Determine which stream should be outer and which is inner.
	// In the case of a left join, the syntactically left stream is the outer,
	// and the right stream is the inner. For a right join, just swap the sides.
	// For a full join, order does not matter, but historically it has been reversed,
	// so let's preserve this for the time being.

	if (rse->rse_jointype != blr_left)
	{
		// RIGHT JOIN is converted into LEFT JOIN by the BLR parser,
		// so it should never appear here
		fb_assert(rse->rse_jointype == blr_full);
		std::swap(joinStreams[0], joinStreams[1]);
	}
}


// Generate a top level outer join. The "outer" and "inner" sub-streams must be
// handled differently from each other. The inner is like other streams.
// The outer one isn't because conjuncts may not eliminate records from the stream.
// They only determine if a join with an inner stream record is to be attempted.

RecordSource* OuterJoin::generate()
{
	if (!optimizer->isFullJoin())
	{
		fb_assert(optimizer->isLeftJoin());
		return process();
	}

	StreamList outerStreams;
	const auto outerJoinRsb = process(&outerStreams);

	// A FULL JOIN B is currently implemented similar to:
	//
	// (A LEFT JOIN B)
	// UNION ALL
	// (B LEFT JOIN A WHERE A.* IS NULL)
	//
	// See also FullOuterJoin class implementation.
	//
	// At this point we already have the first part -- (A LEFT JOIN B) -- ready,
	// so just swap the sides and make the second (inverted) join.

	auto& outerStream = joinStreams[0];
	auto& innerStream = joinStreams[1];

	std::swap(outerStream, innerStream);

	// Reset both streams to their original states

	if (outerStream.number != INVALID_STREAM)
	{
		outerStream.rsb = nullptr;
		csb->csb_rpt[outerStream.number].deactivate();
	}

	if (innerStream.number != INVALID_STREAM)
	{
		innerStream.rsb = nullptr;
		csb->csb_rpt[innerStream.number].deactivate();
	}

	// Clone the booleans to make them re-usable for an anti-join

	for (auto iter = optimizer->getConjuncts(); iter.hasData(); ++iter)
	{
		if (iter & Optimizer::CONJUNCT_USED)
			iter.reset(CMP_clone_node_opt(tdbb, csb, iter));
	}

	const auto antiJoinRsb = process();

	// Allocate and return the final join record source

	return FB_NEW_POOL(getPool()) FullOuterJoin(csb, outerJoinRsb, antiJoinRsb, outerStreams);
}


RecordSource* OuterJoin::process(StreamList* outerStreams)
{
	BoolExprNode* boolean = nullptr;

	auto& outerStream = joinStreams[0];
	auto& innerStream = joinStreams[1];

	// Generate record sources for the sub-streams.
	// For the outer sub-stream we also will get a boolean back.

	if (outerStream.number != INVALID_STREAM)
	{
		fb_assert(!outerStream.rsb);
		outerStream.rsb = optimizer->generateRetrieval(outerStream.number,
			optimizer->isFullJoin() ? nullptr : sortPtr, true, false, &boolean);
	}
	else
	{
		// Ensure the inner streams are inactive

		StreamList streams;

		if (innerStream.rsb)
			innerStream.rsb->findUsedStreams(streams);

		StreamStateHolder stateHolder(csb, streams);
		stateHolder.deactivate();

		// Collect booleans computable for the outer sub-stream, it must be active now

		boolean = optimizer->composeBoolean();
	}

	if (outerStreams)
		outerStream.rsb->findUsedStreams(*outerStreams);

	if (innerStream.number != INVALID_STREAM)
	{
		fb_assert(!innerStream.rsb);
		// AB: the sort clause for the inner stream of an OUTER JOIN
		//	   should never be used for the index retrieval
		innerStream.rsb = optimizer->generateRetrieval(innerStream.number, nullptr, false, true);
	}

	// Generate a parent filter record source for any remaining booleans that
	// were not satisfied via an index lookup

	const auto innerRsb = optimizer->applyResidualBoolean(innerStream.rsb);

	// Allocate and return the join record source

	return FB_NEW_POOL(getPool()) NestedLoopJoin(csb, outerStream.rsb, innerRsb, boolean);
};


