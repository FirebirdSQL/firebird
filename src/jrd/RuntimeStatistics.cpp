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

void RuntimeStatistics::adjust(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats)
{
	if (baseStats.allChgNumber == newStats.allChgNumber)
		return;

	allChgNumber++;
	for (size_t i = 0; i < GLOBAL_ITEMS; ++i)
		values[i] += newStats.values[i] - baseStats.values[i];

	if (baseStats.pageChgNumber != newStats.pageChgNumber)
	{
		pageChgNumber++;
		page_counts.adjust(baseStats.page_counts, newStats.page_counts);
	}

	if (baseStats.relChgNumber != newStats.relChgNumber)
	{
		relChgNumber++;
		rel_counts.adjust(baseStats.rel_counts, newStats.rel_counts);
	}
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

template <class Counts, typename Key>
void RuntimeStatistics::GroupedCountsArray<Counts, Key>::adjust(const GroupedCountsArray& baseStats, const GroupedCountsArray& newStats)
{
	auto baseIter = baseStats.m_counts.begin(), newIter = newStats.m_counts.begin();
	const auto baseEnd = baseStats.m_counts.end(), newEnd = newStats.m_counts.end();

	// The loop below assumes that newStats cannot miss objects existing in baseStats,
	// this must be always the case as long as newStats is an incremented version of baseStats

	while (newIter != newEnd || baseIter != baseEnd)
	{
		if (baseIter == baseEnd)
		{
			// Object exists in newStats but missing in baseStats
			const auto newKey = newIter->getGroupKey();
			(*this)[newKey] += *newIter++;
		}
		else if (newIter != newEnd)
		{
			const auto baseKey = baseIter->getGroupKey();
			const auto newKey = newIter->getGroupKey();

			if (newKey == baseKey)
			{
				// Object exists in both newStats and baseStats
				(*this)[newKey] += *newIter++;
				(*this)[newKey] -= *baseIter++;
			}
			else if (newKey < baseKey)
			{
				// Object exists in newStats but missing in baseStats
				(*this)[newKey] += *newIter++;
			}
			else
				fb_assert(false); // should never happen
		}
		else
			fb_assert(false); // should never happen
	}
}

PerformanceInfo* RuntimeStatistics::computeDifference(Attachment* att,
													  const RuntimeStatistics& newStats,
													  PerformanceInfo& dest,
													  TraceCountsArray& relStatsTemp,
													  ObjectsArray<string>& tempNames)
{
	// NOTE: we do not initialize dest.pin_time. This must be done by the caller

	// Calculate database-level statistics

	for (size_t i = 0; i < GLOBAL_ITEMS; i++)
		values[i] = newStats.values[i] - values[i];

	// Calculate relation-level statistics

	relStatsTemp.clear();

	for (const auto& newCounts : newStats.rel_counts)
	{
		const auto relationId = newCounts.getGroupKey();
		auto& counts = rel_counts[relationId];

		if (counts.setToDiff(newCounts))
		{
			const char* relationName = nullptr;

			if (relationId < static_cast<SLONG>(att->att_relations->count()))
			{
				if (const auto relation = (*att->att_relations)[relationId])
				{
					auto& tempName = tempNames.add();
					tempName = relation->rel_name.toQuotedString();
					relationName = tempName.c_str();
				}
			}

			TraceCounts traceCounts;
			traceCounts.trc_relation_id = relationId;
			traceCounts.trc_relation_name = relationName;
			traceCounts.trc_counters = counts.getCounterVector();
			relStatsTemp.add(traceCounts);
		}
	}

	dest.pin_count = relStatsTemp.getCount();
	dest.pin_tables = relStatsTemp.begin();

	return &dest;
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
