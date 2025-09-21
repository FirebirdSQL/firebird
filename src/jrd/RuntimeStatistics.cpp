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
 *  Copyright (c) 2006 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../jrd/req.h"

#include "../jrd/RuntimeStatistics.h"
#include "../jrd/ntrace.h"

using namespace Firebird;

namespace Jrd {

GlobalPtr<RuntimeStatistics> RuntimeStatistics::dummy;

void RuntimeStatistics::adjustRelStats(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats)
{
	if (baseStats.relChgNumber == newStats.relChgNumber)
		return;

	relChgNumber++;

	auto locate = [this](SLONG relId) -> FB_SIZE_T
	{
		FB_SIZE_T pos;
		if (!rel_counts.find(relId, pos))
			rel_counts.insert(pos, RelationCounts(relId));
		return pos;
	};

	auto baseIter = baseStats.rel_counts.begin(), newIter = newStats.rel_counts.begin();
	const auto baseEnd = baseStats.rel_counts.end(), newEnd = newStats.rel_counts.end();

	// The loop below assumes that newStats cannot miss relations existing in baseStats,
	// this must be always the case as long as newStats is an incremented version of baseStats

	while (newIter != newEnd || baseIter != baseEnd)
	{
		if (baseIter == baseEnd)
		{
			// Relation exists in newStats but missing in baseStats
			const auto newRelId = newIter->getRelationId();
			rel_counts[locate(newRelId)] += *newIter++;
		}
		else if (newIter != newEnd)
		{
			const auto baseRelId = baseIter->getRelationId();
			const auto newRelId = newIter->getRelationId();

			if (newRelId == baseRelId)
			{
				// Relation exists in both newStats and baseStats
				fb_assert(baseRelId == newRelId);
				const auto pos = locate(baseRelId);
				rel_counts[pos] -= *baseIter++;
				rel_counts[pos] += *newIter++;
			}
			else if (newRelId < baseRelId)
			{
				// Relation exists in newStats but missing in baseStats
				rel_counts[locate(newRelId)] += *newIter++;
			}
			else
				fb_assert(false); // should never happen
		}
		else
			fb_assert(false); // should never happen
	}
}

PerformanceInfo* RuntimeStatistics::computeDifference(Attachment* att,
													  const RuntimeStatistics& new_stat,
													  PerformanceInfo& dest,
													  TraceCountsArray& temp,
													  ObjectsArray<string>& tempNames)
{
	// NOTE: we do not initialize dest.pin_time. This must be done by the caller

	// Calculate database-level statistics
	for (size_t i = 0; i < GLOBAL_ITEMS; i++)
		values[i] = new_stat.values[i] - values[i];

	dest.pin_counters = values;

	// Calculate relation-level statistics
	temp.clear();
	tempNames.clear();

	// This loop assumes that base array is smaller than new one
	RelCounters::iterator base_cnts = rel_counts.begin();
	bool base_found = (base_cnts != rel_counts.end());

	for (const auto& new_cnts : new_stat.rel_counts)
	{
		const SLONG rel_id = new_cnts.getRelationId();

		if (base_found && base_cnts->getRelationId() == rel_id)
		{
			// Point TraceCounts to counts array from baseline object
			if (base_cnts->setToDiff(new_cnts))
			{
				jrd_rel* const relation =
					rel_id < static_cast<SLONG>(att->att_relations->count()) ?
					(*att->att_relations)[rel_id] : NULL;

				TraceCounts traceCounts;
				traceCounts.trc_relation_id = rel_id;
				traceCounts.trc_counters = base_cnts->getCounterVector();

				if (relation)
				{
					auto& tempName = tempNames.add();
					tempName = relation->rel_name.toQuotedString();
					traceCounts.trc_relation_name = tempName.c_str();
				}
				else
					traceCounts.trc_relation_name = nullptr;

				temp.add(traceCounts);
			}

			++base_cnts;
			base_found = (base_cnts != rel_counts.end());
		}
		else
		{
			jrd_rel* const relation =
				rel_id < static_cast<SLONG>(att->att_relations->count()) ?
				(*att->att_relations)[rel_id] : NULL;

			// Point TraceCounts to counts array from object with updated counters
			TraceCounts traceCounts;
			traceCounts.trc_relation_id = rel_id;
			traceCounts.trc_counters = new_cnts.getCounterVector();

			if (relation)
			{
				auto& tempName = tempNames.add();
				tempName = relation->rel_name.toQuotedString();
				traceCounts.trc_relation_name = tempName.c_str();
			}
			else
				traceCounts.trc_relation_name = nullptr;

			temp.add(traceCounts);
		}
	};

	dest.pin_count = temp.getCount();
	dest.pin_tables = temp.begin();

	return &dest;
}

void RuntimeStatistics::adjust(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats)
{
	if (baseStats.allChgNumber == newStats.allChgNumber)
		return;

	allChgNumber++;
	for (size_t i = 0; i < GLOBAL_ITEMS; ++i)
		values[i] += newStats.values[i] - baseStats.values[i];

	adjustRelStats(baseStats, newStats);
}

void RuntimeStatistics::adjustPageStats(RuntimeStatistics& baseStats, const RuntimeStatistics& newStats)
{
	if (baseStats.allChgNumber == newStats.allChgNumber)
		return;

	allChgNumber++;
	for (size_t i = 0; i < PAGE_TOTAL_ITEMS; ++i)
	{
		const SINT64 delta = newStats.values[i] - baseStats.values[i];

		values[i] += delta;
		baseStats.values[i] += delta;
	}
}

RuntimeStatistics::Accumulator::Accumulator(thread_db* tdbb, const jrd_rel* relation,
											const RecordStatType type)
	: m_tdbb(tdbb), m_type(type), m_id(relation->rel_id)
{}

RuntimeStatistics::Accumulator::~Accumulator()
{
	if (m_counter)
		m_tdbb->bumpStats(m_type, m_id, m_counter);
}

} // namespace
