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

	StreamList outerStreams;
	outer.getStreams(outerStreams);

	RecordSource* joinRsb = nullptr;

	if (inner.number != INVALID_STREAM)
	{
		// Search for for possible equi-join conditions
		BooleanList equiMatches;

		auto iter = optimizer->getConjuncts(false, true);
		for (iter.rewind(); iter.hasData(); ++iter)
		{
			if (!(iter & Optimizer::CONJUNCT_USED))
			{
				// Check whether we have an equivalence operation
				if (!optimizer->checkEquiJoin(iter))
					continue;

				// Ensure the boolean references both inner and outer streams
				const auto cmpNode = nodeAs<ComparativeBoolNode>(*iter);
				fb_assert(cmpNode && (cmpNode->blrOp == blr_eql || cmpNode->blrOp == blr_equiv));
				auto outerArg = cmpNode->arg1;
				auto innerArg = cmpNode->arg2;

				if (!innerArg->containsStream(inner.number, true))
				{
					if (!outerArg->containsStream(inner.number, true))
						continue;

					std::swap(outerArg, innerArg);
				}

				if (outerArg->containsStream(inner.number))
					continue;

				if (!outerArg->computable(csb, inner.number, false))
					continue;

				equiMatches.add(*iter);
			}
		}

		// Estimate costs for both LOOP and HASH joins and choose the cheapest option
		if (equiMatches.hasData())
		{
			Retrieval loopRetrieval(tdbb, optimizer, inner.number, false, true, nullptr, true);
			const auto loopCandidate = loopRetrieval.getInversion();

			// Beware conditional retrievals, hash joining is impossible for them
			if (!loopCandidate->condition)
			{
				const auto streamCardinality = csb->csb_rpt[inner.number].csb_cardinality;

				// Calculate the match cardinality
				const auto matchSelectivity = loopCandidate->selectivity;
				auto matchCardinality = streamCardinality * matchSelectivity;

				const bool firstRows = optimizer->favorFirstRows();

				if ((loopCandidate->unique || firstRows) && matchCardinality > MINIMUM_CARDINALITY)
					matchCardinality = MINIMUM_CARDINALITY;

				// Get the outer stream cardinality, taking first-rows optimization into account
				const auto outerCardinality = (firstRows && matchSelectivity) ?
					MIN(outerRsb->getCardinality() * matchSelectivity, MINIMUM_CARDINALITY) :
					outerRsb->getCardinality();

				// Calculate the nested loop join cost (excluding the outer stream)
				const auto loopCost = Optimizer::getLoopJoinCost(loopCandidate->cost, outerCardinality);

				// Calculate the hash join cost (excluding the outer stream)
				StreamStateHolder outerHolder(csb, outerStreams);
				outerHolder.deactivate();

				Retrieval hashRetrieval(tdbb, optimizer, inner.number, false, true, nullptr, true);
				const auto hashCandidate = hashRetrieval.getInversion();

				const auto hashCardinality = hashCandidate->selectivity * streamCardinality;
				const bool allowHashJoin = Optimizer::allowHashJoin(streamCardinality,
																	hashCardinality,
																	hashCandidate->indexes);

				// Consider whether the current stream can be hash-joined to the prior one
				if (allowHashJoin)
				{
					const auto hashCost = Optimizer::getHashJoinCost(hashCandidate->cost, outerCardinality,
																	 hashCardinality, matchCardinality);
					if (hashCost <= loopCost)
					{
						// Create an independent retrieval
						innerRsb = optimizer->generateRetrieval(inner.number, nullptr, false, true);

						// Prepare record sources and corresponding equivalence keys for hash-joining
						RecordSource* hashJoinRsbs[] = {outerRsb, innerRsb};

						HalfStaticArray<NestValueArray*, OPT_STATIC_ITEMS> keys;

						keys.add(FB_NEW_POOL(getPool()) NestValueArray(getPool()));
						keys.add(FB_NEW_POOL(getPool()) NestValueArray(getPool()));

						for (const auto match : equiMatches)
						{
							NestConst<ValueExprNode> node1;
							NestConst<ValueExprNode> node2;

							if (!optimizer->getEquiJoinKeys(match, &node1, &node2))
								fb_assert(false);

							if (!node2->containsStream(inner.number))
							{
								fb_assert(node1->containsStream(inner.number));

								// Swap the sides
								std::swap(node1, node2);
							}

							keys[0]->add(node1);
							keys[1]->add(node2);
						}

						// Create a hash join
						joinRsb = FB_NEW_POOL(getPool())
							HashJoin(tdbb, csb, boolean, hashJoinRsbs, keys.begin(), matchSelectivity);

						for (iter.rewind(); iter.hasData(); ++iter)
						{
							if (equiMatches.exist(*iter))
								iter |= Optimizer::CONJUNCT_JOINED;
						}
					}
				}
			}
		}

		if (!joinRsb)
		{
			// AB: the sort clause for the inner stream of an OUTER JOIN
			//         should never be used for the index retrieval
			innerRsb = optimizer->generateRetrieval(inner.number, nullptr, false, true);
		}
	}
	else
	{
		if (inner.river)
			innerRsb = inner.river->getRecordSource();
		else
		{
			fb_assert(optimizer->isFullJoin());

			optimizer->setOuterStreams(outerStreams);

			innerRsb = inner.node->compile(tdbb, optimizer, true);
		}
	}

	fb_assert(innerRsb);

	// Generate a parent filter record source for any remaining booleans that
	// were not satisfied via an index lookup

	innerRsb = optimizer->applyResidualBoolean(innerRsb);

	// Allocate and return the join record source

	return joinRsb ? joinRsb : FB_NEW_POOL(getPool()) NestedLoopJoin(csb, outerRsb, innerRsb, boolean);
};


