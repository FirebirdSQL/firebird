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

void RuntimeStatistics::findAndBumpRelValue(const StatType index, SLONG relation_id, SINT64 delta)
{
	if (rel_counts.find(relation_id, rel_last_pos))
		rel_counts[rel_last_pos].bumpCounter(index, delta);
	else
	{
		RelationCounts counts(relation_id);
		counts.bumpCounter(index, delta);
		rel_counts.insert(rel_last_pos, counts);
	}
}

void RuntimeStatistics::addRelCounts(const RelCounters& other, bool add)
{
	if (other.isEmpty())
		return;

	RelCounters::const_iterator src(other.begin());
	const RelCounters::const_iterator end(other.end());

	FB_SIZE_T pos;
	rel_counts.find(src->getRelationId(), pos);
	for (; src != end; ++src)
	{
		const FB_SIZE_T cnt = rel_counts.getCount();

		while (pos < cnt && rel_counts[pos].getRelationId() < src->getRelationId())
			pos++;

		if (pos >= cnt || rel_counts[pos].getRelationId() > src->getRelationId())
		{
			RelationCounts counts(src->getRelationId());
			rel_counts.insert(pos, counts);
		}

		fb_assert(pos >= 0 && pos < rel_counts.getCount());

		if (add)
			rel_counts[pos] += *src;
		else
			rel_counts[pos] -= *src;
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
	for (int i = 0; i < TOTAL_ITEMS; i++)
		values[i] = new_stat.values[i] - values[i];

	dest.pin_counters = values;

	// Calculate relation-level statistics
	temp.clear();
	tempNames.clear();

	// This loop assumes that base array is smaller than new one
	RelCounters::iterator base_cnts = rel_counts.begin();
	bool base_found = (base_cnts != rel_counts.end());

	RelCounters::const_iterator new_cnts = new_stat.rel_counts.begin();
	const RelCounters::const_iterator end = new_stat.rel_counts.end();
	for (; new_cnts != end; ++new_cnts)
	{
		const SLONG rel_id = new_cnts->getRelationId();

		if (base_found && base_cnts->getRelationId() == rel_id)
		{
			// Point TraceCounts to counts array from baseline object
			if (base_cnts->setToDiff(*new_cnts))
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
			traceCounts.trc_counters = new_cnts->getCounterVector();

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

RuntimeStatistics::Accumulator::Accumulator(thread_db* tdbb, const jrd_rel* relation, StatType type)
	: m_tdbb(tdbb), m_type(type), m_id(relation->rel_id), m_counter(0)
{}

RuntimeStatistics::Accumulator::~Accumulator()
{
	if (m_counter)
		m_tdbb->bumpRelStats(m_type, m_id, m_counter);
}

} // namespace
