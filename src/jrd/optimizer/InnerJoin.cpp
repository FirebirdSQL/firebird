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
 *  The Original Code was created by Arno Brinkman
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Arno Brinkman <firebird@abvisie.nl>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *  Adriano dos Santos Fernandes
 *  Dmitry Yemanov
 *
 */

#include "firebird.h"

#include "../jrd/jrd.h"
#include "../jrd/exe.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/Collation.h"
#include "../jrd/ods.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"

#include "../jrd/optimizer/Optimizer.h"

using namespace Firebird;
using namespace Jrd;


//
// Constructor
//

InnerJoin::InnerJoin(thread_db* aTdbb, Optimizer* opt,
					 const StreamList& streams,
					 SortNode** sortClause, bool hasPlan)
	: PermanentStorage(*aTdbb->getDefaultPool()),
	  tdbb(aTdbb),
	  optimizer(opt),
	  csb(opt->getCompilerScratch()),
	  sortPtr(sortClause),
	  plan(hasPlan),
	  innerStreams(getPool(), streams.getCount()),
	  joinedStreams(getPool()),
	  bestStreams(getPool())
{
	joinedStreams.grow(streams.getCount());

	for (const auto stream : streams)
		innerStreams.add(FB_NEW_POOL(getPool()) StreamInfo(getPool(), stream));

	calculateStreamInfo();
}


//
// Calculate the needed information for all streams
//

void InnerJoin::calculateStreamInfo()
{
	StreamList streams;

	// First get the base cost without any relation to any other inner join stream

#ifdef OPT_DEBUG_RETRIEVAL
	optimizer->printf("Base stream info:\n");
#endif

	const auto sort = sortPtr ? *sortPtr : nullptr;

	for (auto innerStream : innerStreams)
	{
		streams.add(innerStream->stream);
		csb->csb_rpt[innerStream->stream].activate();

		Retrieval retrieval(tdbb, optimizer, innerStream->stream, false, false, sort, true);
		const auto candidate = retrieval.getInversion();

		innerStream->baseCost = candidate->cost;
		innerStream->baseSelectivity = candidate->selectivity;
		innerStream->baseIndexes = candidate->indexes;
		innerStream->baseUnique = candidate->unique;
		innerStream->baseNavigated = candidate->navigated;

		csb->csb_rpt[innerStream->stream].deactivate();
	}

	// Collect dependencies between every pair of streams

	for (const auto baseStream : streams)
	{
		csb->csb_rpt[baseStream].activate();

		for (const auto innerStream : innerStreams)
		{
			const StreamType testStream = innerStream->stream;

			if (baseStream != testStream)
			{
				csb->csb_rpt[testStream].activate();
				getIndexedRelationships(innerStream);
				csb->csb_rpt[testStream].deactivate();
			}
		}

		csb->csb_rpt[baseStream].deactivate();
	}

	// Collect more complex inter-dependencies (involving three and more streams), if any

	if (streams.getCount() > 2)
	{
		StreamStateHolder stateHolder(csb, streams);
		stateHolder.activate();

		for (const auto innerStream : innerStreams)
			getIndexedRelationships(innerStream);
	}

	// Unless PLAN is enforced, sort the streams based on independency and cost
	if (!plan && (innerStreams.getCount() > 1))
	{
		StreamInfoList tempStreams;

		for (const auto innerStream : innerStreams)
		{
			FB_SIZE_T index = 0;
			for (; index < tempStreams.getCount(); index++)
			{
				if (StreamInfo::cheaperThan(innerStream, tempStreams[index]))
					break;
			}
			tempStreams.insert(index, innerStream);
		}

		// Finally update the innerStreams with the sorted streams
		innerStreams = tempStreams;
	}
}


//
// Estimate the cost for the stream
//

void InnerJoin::estimateCost(unsigned position,
							 const StreamInfo* stream,
							 double* cost,
							 double* resultingCardinality) const
{
	const auto sort = (position == 0 && sortPtr) ? *sortPtr : nullptr;

	// Create the optimizer retrieval generation class and calculate
	// which indexes will be used and the total estimated selectivity will be returned
	Retrieval retrieval(tdbb, optimizer, stream->stream, false, false, sort, true);
	const auto candidate = retrieval.getInversion();

	*cost = candidate->cost;

	// Calculate cardinality
	const auto tail = &csb->csb_rpt[stream->stream];
	const double cardinality = tail->csb_cardinality * candidate->selectivity;

	*resultingCardinality = MAX(cardinality, MINIMUM_CARDINALITY);
}


//
// Find the best order out of the streams. First return a stream if it can't use
// an index based on a previous stream and it can't be used by another stream.
// Next loop through the remaining streams and find the best order.
//

bool InnerJoin::findJoinOrder()
{
	bestStreams.clear();
	bestCount = 0;
	remainingStreams = 0;

#ifdef OPT_DEBUG
	// Debug
	printStartOrder();
#endif

	int filters = 0, navigations = 0;

	for (const auto innerStream : innerStreams)
	{
		if (!innerStream->used)
		{
			remainingStreams++;

			const int currentFilter = innerStream->isFiltered() ? 1 : 0;

			if (navigations && currentFilter)
				navigations = 0;

			filters += currentFilter;

			if (innerStream->baseNavigated && currentFilter == filters)
				navigations++;

			if (innerStream->isIndependent())
			{
				if (!bestCount || innerStream->baseCost < bestCost)
				{
					joinedStreams[0].bestStream = innerStream->stream;
					bestCount = 1;
					bestCost = innerStream->baseCost;
				}
			}
		}
	}

	if (bestCount == 0)
	{
		IndexedRelationships indexedRelationships;

		for (const auto innerStream : innerStreams)
		{
			if (!innerStream->used)
			{
				// If optimization for first rows has been requested and index navigations are
				// possible, then consider only join orders starting with a navigational stream.
				// Except cases when other streams have local predicates applied.

				const int currentFilter = innerStream->isFiltered() ? 1 : 0;

				if (!optimizer->favorFirstRows() || !navigations ||
					(innerStream->baseNavigated && currentFilter == filters))
				{
					indexedRelationships.clear();
					findBestOrder(0, innerStream, indexedRelationships, 0.0, 1.0);

					if (plan)
					{
						// If a explicit PLAN was specified we should be ready;
						break;
					}
				}
			}
		}
	}

	// Mark streams as used
	for (unsigned i = 0; i < bestCount; i++)
	{
		auto streamInfo = getStreamInfo(joinedStreams[i].bestStream);
		streamInfo->used = true;
		bestStreams.add(joinedStreams[i].bestStream);
	}

#ifdef OPT_DEBUG
	// Debug
	printBestOrder();
#endif

	return bestStreams.hasData();
}


//
// Make different combinations to find out the join order.
// For every position we start with the stream that has the best selectivity
// for that position. If we've have used up all our streams after that
// we assume we're done.
//

void InnerJoin::findBestOrder(unsigned position,
							  StreamInfo* stream,
							  IndexedRelationships& processList,
							  double cost,
							  double cardinality)
{
	const auto tail = &csb->csb_rpt[stream->stream];

	// Do some initializations
	tail->activate();
	joinedStreams[position].number = stream->stream;

	// Save the various flag bits from the optimizer block to reset its
	// state after each test
	HalfStaticArray<bool, OPT_STATIC_ITEMS> streamFlags(innerStreams.getCount());
	for (const auto innerStream : innerStreams)
		streamFlags.add(innerStream->used);

	// Compute delta and total estimate cost to fetch this stream
	double positionCost = 0, positionCardinality = 0, newCost = 0, newCardinality = 0;

	if (!plan)
	{
		estimateCost(position, stream, &positionCost, &positionCardinality);
		newCost = cost + cardinality * positionCost;
		newCardinality = positionCardinality * cardinality;
	}

	position++;

	// If the partial order is either longer than any previous partial order,
	// or the same length and cheap, save order as "best"
	if (position > bestCount || (position == bestCount && newCost < bestCost))
	{
		bestCount = position;
		bestCost = newCost;

		const auto end = joinedStreams.begin() + position;
		for (auto iter = joinedStreams.begin(); iter != end; ++iter)
		{
			auto& joinedStream = *iter;
			joinedStream.bestStream = joinedStream.number;
		}
	}

#ifdef OPT_DEBUG
	// Debug information
	printFoundOrder(position, positionCost, positionCardinality, newCost, newCardinality);
#endif

	// Mark this stream as "used" in the sense that it is already included
	// in this particular proposed stream ordering
	stream->used = true;
	bool done = false;

	// If we've used up all the streams there's no reason to go any further
	if (position == remainingStreams)
		done = true;

	// If we know a combination with all streams used and the
	// current cost is higher as the one from the best we're done
	if (bestCount == remainingStreams && bestCost < newCost)
		done = true;

	if (plan)
	{
		// If a explicit PLAN was specific pick the next relation.
		// The order in innerStreams is expected to be exactly the order as
		// specified in the explicit PLAN.
		for (auto nextStream : innerStreams)
		{
			if (!nextStream->used)
			{
				findBestOrder(position, nextStream, processList, newCost, newCardinality);
				break;
			}
		}
	}
	else if (!done)
	{
		// Add these relations to the processing list
		for (auto& relationship : stream->indexedRelationships)
		{
			const auto relationStreamInfo = getStreamInfo(relationship.stream);

			if (relationStreamInfo->used)
				continue;

			bool usable = true;
			for (const auto depStream : relationship.depStreams)
			{
				if (!(csb->csb_rpt[depStream].csb_flags & csb_active))
				{
					usable = false;
					break;
				}
			}

			if (!usable)
				continue;

			bool found = false;
			IndexRelationship* processRelationship = processList.begin();
			for (FB_SIZE_T index = 0; index < processList.getCount(); index++)
			{
				if (relationStreamInfo->stream == processRelationship[index].stream)
				{
					// If the cost of this relationship is cheaper then remove the
					// old relationship and add this one
					if (IndexRelationship::cheaperThan(relationship, processRelationship[index]))
					{
						processList.remove(index);
						break;
					}

					found = true;
					break;
				}
			}

			if (found)
				continue;

			// Add relationship sorted on cost (cheapest as first)
			processList.add(relationship);
		}

		for (const auto& nextRelationship : processList)
		{
			auto relationStreamInfo = getStreamInfo(nextRelationship.stream);
			if (!relationStreamInfo->used)
			{
				findBestOrder(position, relationStreamInfo, processList, newCost, newCardinality);
				break;
			}
		}
	}

	// Clean up from any changes made for compute the cost for this stream
	tail->deactivate();
	for (FB_SIZE_T i = 0; i < streamFlags.getCount(); i++)
		innerStreams[i]->used = streamFlags[i];
}


//
// Form streams into rivers (combinations of streams)
//

River* InnerJoin::formRiver()
{
	fb_assert(bestCount);
	fb_assert(bestStreams.hasData());

	if (bestStreams.getCount() != innerStreams.getCount())
		sortPtr = nullptr;

	HalfStaticArray<RecordSource*, OPT_STATIC_ITEMS> rsbs;

	for (const auto stream : bestStreams)
	{
		const auto rsb = optimizer->generateRetrieval(stream, sortPtr, false, false);
		rsbs.add(rsb);
		sortPtr = nullptr;
	}

	const auto rsb = (rsbs.getCount() == 1) ? rsbs[0] :
		FB_NEW_POOL(getPool()) NestedLoopJoin(csb, rsbs.getCount(), rsbs.begin());

	// Allocate a river block and move the best order into it
	const auto river = FB_NEW_POOL(getPool()) River(csb, rsb, nullptr, bestStreams);
	river->deactivate(csb);
	return river;
}


//
// Check if the testStream can use a index when the baseStream is active. If so
// then we create a indexRelationship and fill it with the needed information.
// The reference is added to the baseStream and the baseStream is added as previous
// expected stream to the testStream.
//

void InnerJoin::getIndexedRelationships(StreamInfo* testStream)
{
#ifdef OPT_DEBUG_RETRIEVAL
	const auto name = optimizer->getStreamName(testStream->stream);
	optimizer->printf("Dependencies for stream %u (%s):\n",
					  testStream->stream, name.c_str());
#endif

	const auto tail = &csb->csb_rpt[testStream->stream];

	Retrieval retrieval(tdbb, optimizer, testStream->stream, false, false, nullptr, true);
	const auto candidate = retrieval.getInversion();

	for (const auto baseStream : innerStreams)
	{
		if (baseStream->stream != testStream->stream &&
			candidate->dependentFromStreams.exist(baseStream->stream))
		{
			// If the base stream already depends on the testing stream, don't store it again
			bool found = false;
			for (const auto& relationship : baseStream->indexedRelationships)
			{
				if (relationship.stream == testStream->stream)
				{
					found = true;
					break;
				}
			}

			if (found)
				continue;

			if (candidate->dependentFromStreams.getCount() > IndexRelationship::MAX_DEP_STREAMS)
				continue;

			// If we could use more conjunctions on the testing stream
			// with the base stream active as without the base stream
			// then the test stream has a indexed relationship with the base stream.
			IndexRelationship indexRelationship;
			indexRelationship.stream = testStream->stream;
			indexRelationship.unique = candidate->unique;
			indexRelationship.cost = candidate->cost;
			indexRelationship.cardinality = candidate->unique ?
				tail->csb_cardinality : tail->csb_cardinality * candidate->selectivity;

			for (const auto depStream : candidate->dependentFromStreams)
				indexRelationship.depStreams.add(depStream);

			// Relationships are kept sorted by cost and uniqueness in the array
			baseStream->indexedRelationships.add(indexRelationship);
			testStream->previousExpectedStreams++;
		}
	}
}


//
// Return stream information based on the stream number
//

InnerJoin::StreamInfo* InnerJoin::getStreamInfo(StreamType stream)
{
	for (FB_SIZE_T i = 0; i < innerStreams.getCount(); i++)
	{
		if (innerStreams[i]->stream == stream)
			return innerStreams[i];
	}

	// We should never come here
	fb_assert(false);
	return nullptr;
}

#ifdef OPT_DEBUG
// Dump finally selected stream order
void InnerJoin::printBestOrder() const
{
	if (bestStreams.isEmpty())
		return;

	optimizer->printf("  best order, streams:");

	const auto end = bestStreams.end();
	for (auto iter = bestStreams.begin(); iter != end; iter++)
	{
		const auto name = optimizer->getStreamName(*iter);
		optimizer->printf(" %u (%s)", *iter, name.c_str());

		if (iter != end - 1)
			optimizer->printf(",");
	}

	optimizer->printf("\n");
}

// Dump currently passed streams to a debug file
void InnerJoin::printFoundOrder(StreamType position,
								double positionCost,
								double positionCardinality,
								double cost,
								double cardinality) const
{
	for (auto i = position - 1; i > 0; i--)
		optimizer->printf("  ");

	optimizer->printf("  #%2.2u, streams:", position);

	auto iter = joinedStreams.begin();
	const auto end = iter + position;
	for (; iter < end; iter++)
	{
		const auto name = optimizer->getStreamName(iter->number);
		optimizer->printf(" %u (%s)", iter->number, name.c_str());

		if (iter != end - 1)
			optimizer->printf(",");
	}

	optimizer->printf("\n");

	for (auto i = position - 1; i > 0; i--)
		optimizer->printf("  ");

	optimizer->printf("       position cardinality (%10.2f), position cost (%10.2f),", positionCardinality, positionCost);
	optimizer->printf(" cardinality (%10.2f), cost (%10.2f)", cardinality, cost);

	optimizer->printf("\n");
}

// Dump finally selected stream order
void InnerJoin::printStartOrder() const
{
	optimizer->printf("Start join order, streams:");

	const auto end = innerStreams.end();
	for (auto iter = innerStreams.begin(); iter != end; iter++)
	{
		const auto innerStream = *iter;
		if (!innerStream->used)
		{
			const auto name = optimizer->getStreamName(innerStream->stream);
			optimizer->printf(" %u (%s) base cost (%1.2f)",
							  innerStream->stream, name.c_str(), innerStream->baseCost);

			if (iter != end - 1)
				optimizer->printf(",");
		}
	}

	optimizer->printf("\n");
}
#endif
