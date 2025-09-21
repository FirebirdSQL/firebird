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
#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/init.h"
#include "../common/classes/tree.h"
#include "../common/classes/File.h"

#include <algorithm>

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

// Runtime statistics

enum class PageStatType
{
	FETCHES = 0,
	READS,
	MARKS,
	WRITES,
	TOTAL_ITEMS
};

enum class RecordStatType
{
	SEQ_READS = 0,
	IDX_READS,
	UPDATES,
	INSERTS,
	DELETES,
	BACKOUTS,
	PURGES,
	EXPUNGES,
	LOCKS,
	WAITS,
	CONFLICTS,
	BACK_READS,
	FRAGMENT_READS,
	RPT_READS,
	IMGC,
	TOTAL_ITEMS
};

class RuntimeStatistics : protected Firebird::AutoStorage
{
	static constexpr size_t PAGE_TOTAL_ITEMS = static_cast<size_t>(PageStatType::TOTAL_ITEMS);
	static constexpr size_t RECORD_TOTAL_ITEMS = static_cast<size_t>(RecordStatType::TOTAL_ITEMS);

public:
	// Number of globally counted items.
	//
	// dimitr:	Currently, they include page-level and record-level counters.
	// 			However, this is not strictly required to maintain global record-level counters,
	//			as they may be aggregated from the rel_counts array on demand. This would slow down
	//			the retrieval of counters but save some CPU cycles inside tdbb->bumpStats().
	//			As long as public struct PerformanceInfo don't include record-level counters,
	//			this is not going to affect any existing applications/plugins.
	//			So far I leave everything as is but it can be reconsidered in the future.
	//			sumValue() method is already in place for that purpose.
	//
	static constexpr size_t GLOBAL_ITEMS = PAGE_TOTAL_ITEMS + RECORD_TOTAL_ITEMS;

private:
	template <typename T> class CountsVector
	{
		static constexpr size_t SIZE = static_cast<size_t>(T::TOTAL_ITEMS);

	public:
		CountsVector() = default;

		SINT64& operator[](const T index)
		{
			return m_counters[static_cast<size_t>(index)];
		}

		const SINT64& operator[](const T index) const
		{
			return m_counters[static_cast<size_t>(index)];
		}

		CountsVector& operator+=(const CountsVector& other)
		{
			for (size_t i = 0; i < m_counters.size(); i++)
				m_counters[i] += other.m_counters[i];

			return *this;
		}

		CountsVector& operator-=(const CountsVector& other)
		{
			for (size_t i = 0; i < m_counters.size(); i++)
				m_counters[i] -= other.m_counters[i];

			return *this;
		}

		const SINT64* getCounterVector() const
		{
			return m_counters.data();
		}

		bool isEmpty() const
		{
			return std::all_of(m_counters.begin(), m_counters.end(),
							   [](SINT64 value) { return value == 0; });
		}

		bool hasData() const
		{
			return std::any_of(m_counters.begin(), m_counters.end(),
							   [](SINT64 value) { return value != 0; });
		}

	protected:
		std::array<SINT64, SIZE> m_counters = {};
	};

	// Performance counters for individual table

	class RelationCounts : public CountsVector<RecordStatType>
	{
	public:
		explicit RelationCounts(SLONG relation_id)
			: m_relation_id(relation_id)
		{
		}

		SLONG getRelationId() const
		{
			return m_relation_id;
		}

		bool setToDiff(const RelationCounts& other)
		{
			fb_assert(m_relation_id == other.m_relation_id);

			bool ret = false;

			for (size_t i = 0; i < m_counters.size(); i++)
			{
				if ( (m_counters[i] = other.m_counters[i] - m_counters[i]) )
					ret = true;
			}

			return ret;
		}

		RelationCounts& operator+=(const RelationCounts& other)
		{
			fb_assert(m_relation_id == other.m_relation_id);
			CountsVector::operator+=(other);
			return *this;
		}

		RelationCounts& operator-=(const RelationCounts& other)
		{
			fb_assert(m_relation_id == other.m_relation_id);
			CountsVector::operator-=(other);
			return *this;
		}

		inline static const SLONG& generate(const RelationCounts& item)
		{
			return item.m_relation_id;
		}

	private:
		SLONG m_relation_id;
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

	~RuntimeStatistics() = default;

	void reset()
	{
		memset(values, 0, sizeof values);
		rel_counts.clear();
		rel_last_pos = (FB_SIZE_T) ~0;
		allChgNumber = 0;
		relChgNumber = 0;
	}

	const SINT64& operator[](const PageStatType type) const
	{
		const auto index = static_cast<size_t>(type);
		return values[index];
	}

	void bumpValue(const PageStatType type, SINT64 delta = 1)
	{
		const auto index = static_cast<size_t>(type);
		values[index] += delta;
		++allChgNumber;
	}

	const SINT64& operator[](const RecordStatType type) const
	{
		const auto index = static_cast<size_t>(type);
		return values[PAGE_TOTAL_ITEMS + index];
	}

	SINT64 sumValue(const RecordStatType type) const
	{
		SINT64 value = 0;

		for (const auto& counts : rel_counts)
			value += counts[type];

		return value;
	}

	void bumpValue(const RecordStatType type, SINT64 delta = 1)
	{
		const auto index = static_cast<size_t>(type);
		values[PAGE_TOTAL_ITEMS + index] += delta;
		++allChgNumber;
	}

	void bumpValue(const RecordStatType type, SLONG relation_id, SINT64 delta = 1)
	{
		++allChgNumber;
		++relChgNumber;

		if ((rel_last_pos != (FB_SIZE_T)~0 && rel_counts[rel_last_pos].getRelationId() == relation_id) ||
			// if rel_last_pos is mispositioned
			rel_counts.find(relation_id, rel_last_pos))
		{
			rel_counts[rel_last_pos][type] += delta;
		}
		else
		{
			RelationCounts counts(relation_id);
			counts[type] += delta;
			rel_counts.insert(rel_last_pos, counts);
		}
	}

	// Calculate difference between counts stored in this object and current
	// counts of given request. Counts stored in object are destroyed.
	Firebird::PerformanceInfo* computeDifference(Attachment* att, const RuntimeStatistics& new_stat,
		Firebird::PerformanceInfo& dest, TraceCountsArray& temp, Firebird::ObjectsArray<Firebird::string>& tempNames);

	// Add difference between newStats and baseStats to our counters
	// (newStats and baseStats must be "in-sync")
	void adjust(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats);

	void adjustPageStats(RuntimeStatistics& baseStats, const RuntimeStatistics& newStats);

	// Copy counters values from other instance.
	// After copying both instances are "in-sync" i.e. have the same change numbers.
	RuntimeStatistics& assign(const RuntimeStatistics& other)
	{
		if (allChgNumber != other.allChgNumber)
		{
			memcpy(values, other.values, sizeof(values));
			allChgNumber = other.allChgNumber;

			if (relChgNumber != other.relChgNumber)
			{
				rel_counts = other.rel_counts;
				relChgNumber = other.relChgNumber;
			}
		}

		return *this;
	}

	static RuntimeStatistics* getDummy()
	{
		return &dummy;
	}

	class Accumulator
	{
	public:
		Accumulator(thread_db* tdbb, const jrd_rel* relation, const RecordStatType type);
		~Accumulator();

		void operator++()
		{
			m_counter++;
		}

	private:
		thread_db* const m_tdbb;
		const RecordStatType m_type;
		const SLONG m_id;
		SINT64 m_counter = 0;
	};

	template <class T> class Iterator
	{
	public:
		explicit Iterator(const T& counts)
			: m_iter(counts.begin()), m_end(counts.end())
		{
			advance();
		}

		void operator++()
		{
			m_iter++;
			advance();
		}

		T::const_reference operator*() const
		{
			return *m_iter;
		}

		operator bool() const
		{
			return (m_iter != m_end);
		}

	private:
		T::const_iterator m_iter;
		const T::const_iterator m_end;

		void advance()
		{
			while (m_iter != m_end && m_iter->isEmpty())
				m_iter++;
		}
	};

	typedef Iterator<RelCounters> RelationIterator;

	RelationIterator getRelCounters() const
	{
		return RelationIterator(rel_counts);
	}

private:
	void adjustRelStats(const RuntimeStatistics& baseStats, const RuntimeStatistics& newStats);

	SINT64 values[GLOBAL_ITEMS];
	RelCounters rel_counts;
	FB_SIZE_T rel_last_pos;

	// These three numbers are used in adjust() and assign() methods as "generation"
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
