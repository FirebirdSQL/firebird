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

#ifndef JRD_RUNTIME_STATISTICS_H
#define JRD_RUNTIME_STATISTICS_H

#include "../common/classes/alloc.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/init.h"
#include "../common/classes/tree.h"

namespace Firebird
{

// declared in firebird/Interface.h
struct TraceCounts;
struct PerformanceInfo;

} // namespace Firebird


namespace Jrd {

class Attachment;
class Database;
class thread_db;
class jrd_rel;

typedef Firebird::HalfStaticArray<Firebird::TraceCounts, 5> TraceCountsArray;

// Runtime statistics class

class RuntimeStatistics : protected Firebird::AutoStorage
{
public:
	enum StatType {
		PAGE_FETCHES = 0,
		PAGE_READS,
		PAGE_MARKS,
		PAGE_WRITES,
		RECORD_FIRST_ITEM,
		RECORD_SEQ_READS = RECORD_FIRST_ITEM,
		RECORD_IDX_READS,
		RECORD_UPDATES,
		RECORD_INSERTS,
		RECORD_DELETES,
		RECORD_BACKOUTS,
		RECORD_PURGES,
		RECORD_EXPUNGES,
		RECORD_LOCKS,
		RECORD_WAITS,
		RECORD_CONFLICTS,
		RECORD_BACKVERSION_READS,
		RECORD_FRAGMENT_READS,
		RECORD_RPT_READS,
		RECORD_IMGC,
		RECORD_LAST_ITEM = RECORD_IMGC,
		TOTAL_ITEMS		// last
	};

private:
	static const size_t REL_BASE_OFFSET = RECORD_FIRST_ITEM;
	static const size_t REL_TOTAL_ITEMS = RECORD_LAST_ITEM - REL_BASE_OFFSET + 1;

	// Performance counters for individual table

	class RelationCounts
	{
	public:
		explicit RelationCounts(SLONG relation_id)
			: rlc_relation_id(relation_id)
		{
			memset(rlc_counter, 0, sizeof(rlc_counter));
		}

		SLONG getRelationId() const
		{
			return rlc_relation_id;
		}

		const SINT64* getCounterVector() const
		{
			return rlc_counter;
		}

		SINT64 getCounter(size_t index) const
		{
			fb_assert(index >= REL_BASE_OFFSET && index < REL_BASE_OFFSET + REL_TOTAL_ITEMS);
			return rlc_counter[index - REL_BASE_OFFSET];
		}

		void bumpCounter(size_t index, SINT64 delta = 1)
		{
			fb_assert(index >= REL_BASE_OFFSET && index < REL_BASE_OFFSET + REL_TOTAL_ITEMS);
			rlc_counter[index - REL_BASE_OFFSET] += delta;
		}

		bool setToDiff(const RelationCounts& other)
		{
			fb_assert(rlc_relation_id == other.rlc_relation_id);

			bool ret = false;

			for (size_t i = 0; i < REL_TOTAL_ITEMS; i++)
			{
				if ( (rlc_counter[i] = other.rlc_counter[i] - rlc_counter[i]) )
					ret = true;
			}

			return ret;
		}

		RelationCounts& operator+=(const RelationCounts& other)
		{
			fb_assert(rlc_relation_id == other.rlc_relation_id);

			for (size_t i = 0; i < REL_TOTAL_ITEMS; i++)
				rlc_counter[i] += other.rlc_counter[i];

			return *this;
		}

		RelationCounts& operator-=(const RelationCounts& other)
		{
			fb_assert(rlc_relation_id == other.rlc_relation_id);

			for (size_t i = 0; i < REL_TOTAL_ITEMS; i++)
				rlc_counter[i] -= other.rlc_counter[i];

			return *this;
		}

		inline static const SLONG& generate(const RelationCounts& item)
		{
			return item.rlc_relation_id;
		}

	private:
		SLONG rlc_relation_id;
		SINT64 rlc_counter[REL_TOTAL_ITEMS];
	};

	typedef Firebird::SortedArray<RelationCounts, Firebird::EmptyStorage<RelationCounts>,
		SLONG, RelationCounts> RelCounters;

public:
	RuntimeStatistics()
		: Firebird::AutoStorage(), rel_counts(getPool())
	{
		reset();
	}

	explicit RuntimeStatistics(MemoryPool& pool)
		: Firebird::AutoStorage(pool), rel_counts(getPool())
	{
		reset();
	}

	RuntimeStatistics(const RuntimeStatistics& other)
		: Firebird::AutoStorage(), rel_counts(getPool())
	{
		memcpy(values, other.values, sizeof(values));
		rel_counts = other.rel_counts;

		allChgNumber = other.allChgNumber;
		relChgNumber = other.relChgNumber;
	}

	RuntimeStatistics(MemoryPool& pool, const RuntimeStatistics& other)
		: Firebird::AutoStorage(pool), rel_counts(getPool())
	{
		memcpy(values, other.values, sizeof(values));
		rel_counts = other.rel_counts;

		allChgNumber = other.allChgNumber;
		relChgNumber = other.relChgNumber;
	}

	~RuntimeStatistics() {}

	void reset()
	{
		memset(values, 0, sizeof values);
		rel_counts.clear();
		rel_last_pos = (FB_SIZE_T) ~0;
		allChgNumber = 0;
		relChgNumber = 0;
	}

	SINT64 getValue(const StatType index) const
	{
		return values[index];
	}

	void bumpValue(const StatType index, SINT64 delta = 1)
	{
		values[index] += delta;
		++allChgNumber;
	}

	SINT64 getRelValue(const StatType index, SLONG relation_id) const
	{
		FB_SIZE_T pos;
		return rel_counts.find(relation_id, pos) ? rel_counts[pos].getCounter(index) : 0;
	}

	void bumpRelValue(const StatType index, SLONG relation_id, SINT64 delta = 1)
	{
		fb_assert(index >= 0);
		++relChgNumber;

		if (rel_last_pos != (FB_SIZE_T)~0 && rel_counts[rel_last_pos].getRelationId() == relation_id)
			rel_counts[rel_last_pos].bumpCounter(index, delta);
		else
			findAndBumpRelValue(index, relation_id, delta);
	}

	void findAndBumpRelValue(const StatType index, SLONG relation_id, SINT64 delta);

	// Calculate difference between counts stored in this object and current
	// counts of given request. Counts stored in object are destroyed.
	Firebird::PerformanceInfo* computeDifference(Attachment* att, const RuntimeStatistics& new_stat,
		Firebird::PerformanceInfo& dest, TraceCountsArray& temp);

	// add difference between newStats and baseStats to our counters
	// newStats and baseStats must be "in-sync"
	void adjust(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats, bool relStatsOnly = false)
	{
		if (baseStats.allChgNumber != newStats.allChgNumber)
		{
			const size_t FIRST_ITEM = relStatsOnly ? REL_BASE_OFFSET : 0;

			allChgNumber++;
			for (size_t i = FIRST_ITEM; i < TOTAL_ITEMS; ++i)
				values[i] += newStats.values[i] - baseStats.values[i];

			if (baseStats.relChgNumber != newStats.relChgNumber)
			{
				relChgNumber++;
				addRelCounts(newStats.rel_counts, true);
				addRelCounts(baseStats.rel_counts, false);
			}
		}

	}

	// copy counters values from other instance
	// after copying both instances are "in-sync" i.e. have the same
	// allChgNumber and relChgNumber values
	RuntimeStatistics& assign(const RuntimeStatistics& other)
	{
		if (allChgNumber != other.allChgNumber)
		{
			memcpy(values, other.values, sizeof(values));
			allChgNumber = other.allChgNumber;
		}

		if (relChgNumber != other.relChgNumber)
		{
			rel_counts = other.rel_counts;
			relChgNumber = other.relChgNumber;
		}

		return *this;
	}

	static RuntimeStatistics* getDummy()
	{
		return &dummy;
	}

	class Iterator
	{
		friend class RuntimeStatistics;

		explicit Iterator(const RelationCounts* counts)
			: m_counts(counts)
		{}

	public:
		bool operator==(const Iterator& other) const
		{
			return (m_counts == other.m_counts);
		}

		bool operator!=(const Iterator& other) const
		{
			return (m_counts != other.m_counts);
		}

		Iterator& operator++()
		{
			m_counts++;
			return *this;
		}

		const RelationCounts& operator*() const
		{
			return *m_counts;
		}

	private:
		const RelationCounts* m_counts;
	};

	Iterator begin() const
	{
		return Iterator(rel_counts.begin());
	}

	Iterator end() const
	{
		return Iterator(rel_counts.end());
	}

	class Accumulator
	{
	public:
		Accumulator(thread_db* tdbb, const jrd_rel* relation, StatType type);
		~Accumulator();

		void operator++()
		{
			m_counter++;
		}

	private:
		thread_db* m_tdbb;
		StatType m_type;
		SLONG m_id;
		SINT64 m_counter;
	};

private:
	void addRelCounts(const RelCounters& other, bool add);

	SINT64 values[TOTAL_ITEMS];
	RelCounters rel_counts;
	FB_SIZE_T rel_last_pos;

	// These two numbers are used in adjust() and assign() methods as "generation"
	// values in order to avoid costly operations when two instances of RuntimeStatistics
	// contain equal counters values. This is intended to use *only* with the
	// same pair of class instances, as in Request.
	ULONG allChgNumber;		// incremented when any counter changes
	ULONG relChgNumber;		// incremented when relation counter changes

	// This dummy RuntimeStatistics is used instead of missing elements in tdbb,
	// helping us to avoid conditional checks in time-critical places of code.
	// Values of it contain actually garbage - don't be surprised when debugging.
	static Firebird::GlobalPtr<RuntimeStatistics> dummy;
};

} // namespace

#endif // JRD_RUNTIME_STATISTICS_H
