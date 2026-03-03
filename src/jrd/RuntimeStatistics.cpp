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
#include "../jrd/met.h"

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
		pageCounters.adjust(baseStats.pageCounters, newStats.pageCounters);
	}

	if (baseStats.tabChgNumber != newStats.tabChgNumber)
	{
		tabChgNumber++;
		tableCounters.adjust(baseStats.tableCounters, newStats.tableCounters);
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

template <class Counts>
void RuntimeStatistics::GroupedCountsArray<Counts>::adjust(const GroupedCountsArray& baseStats, const GroupedCountsArray& newStats)
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
			const auto newId = newIter->getGroupId();
			(*this)[newId] += *newIter++;
		}
		else if (newIter != newEnd)
		{
			const auto baseId = baseIter->getGroupId();
			const auto newId = newIter->getGroupId();

			if (newId == baseId)
			{
				// Object exists in both newStats and baseStats
				(*this)[newId] += *newIter++;
				(*this)[newId] -= *baseIter++;
			}
			else if (newId < baseId)
			{
				// Object exists in newStats but missing in baseStats
				(*this)[newId] += *newIter++;
			}
			else
				fb_assert(false); // should never happen
		}
		else
			fb_assert(false); // should never happen
	}
}

void RuntimeStatistics::setToDiff(const RuntimeStatistics& newStats)
{
	for (size_t i = 0; i < GLOBAL_ITEMS; i++)
		values[i] = newStats.values[i] - values[i];

	for (const auto& newCounts : newStats.pageCounters)
	{
		const auto pageSpaceId = newCounts.getGroupId();
		if (!pageCounters[pageSpaceId].setToDiff(newCounts))
			pageCounters.remove(pageSpaceId);
	}

	for (const auto& newCounts : newStats.tableCounters)
	{
		const auto relationId = newCounts.getGroupId();
		if (!tableCounters[relationId].setToDiff(newCounts))
			tableCounters.remove(relationId);
	}
}

RuntimeStatistics::Accumulator::Accumulator(thread_db* tdbb, const jrd_rel* relation,
											const RecordStatType type)
	: m_tdbb(tdbb), m_type(type), m_id(relation->getId())
{}

RuntimeStatistics::Accumulator::~Accumulator()
{
	if (m_counter)
		m_tdbb->bumpStats(m_type, m_id, m_counter);
}

} // namespace
