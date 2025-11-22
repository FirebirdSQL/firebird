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
#include "../jrd/ini.h"
#include "../jrd/pag.h"

#include <algorithm>

namespace Jrd {

class Attachment;
class Database;
class thread_db;
class jrd_rel;


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

		bool setToDiff(const CountsVector& other)
		{
			bool ret = false;

			for (size_t i = 0; i < m_counters.size(); i++)
			{
				if ( (m_counters[i] = other.m_counters[i] - m_counters[i]) )
					ret = true;
			}

			return ret;
		}

		static unsigned getVectorCapacity()
		{
			return (unsigned) SIZE;
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

	template <class T, typename Key>
	class CountsGroup : public CountsVector<T>
	{
	public:
		explicit CountsGroup(Key key)
			: m_key(key)
		{}

		Key getGroupKey() const
		{
			return m_key;
		}

		CountsGroup& operator+=(const CountsGroup& other)
		{
			fb_assert(m_key == other.m_key);
			CountsVector<T>::operator+=(other);
			return *this;
		}

		CountsGroup& operator-=(const CountsGroup& other)
		{
			fb_assert(m_key == other.m_key);
			CountsVector<T>::operator-=(other);
			return *this;
		}

		bool setToDiff(const CountsGroup& other)
		{
			fb_assert(m_key == other.m_key);
			return CountsVector<T>::setToDiff(other);
		}

		inline static const Key& generate(const CountsGroup& item)
		{
			return item.m_key;
		}

	private:
		Key m_key;
	};

	template <class Counts, typename Key>
	class GroupedCountsArray
	{
		typedef Firebird::SortedArray<
			Counts, Firebird::EmptyStorage<Counts>, Key, Counts> SortedCountsArray;
		typedef typename SortedCountsArray::const_iterator ConstIterator;

	public:
		GroupedCountsArray(MemoryPool& pool, FB_SIZE_T capacity)
			: m_counts(pool, capacity)
		{}

		GroupedCountsArray(MemoryPool& pool, const GroupedCountsArray& other)
			: m_counts(pool, other.m_counts.getCapacity())
		{}

		Counts& operator[](Key key)
		{
			if ((m_lastPos != (FB_SIZE_T) ~0 && m_counts[m_lastPos].getGroupKey() == key) ||
				// if m_lastPos is mispositioned
				m_counts.find(key, m_lastPos))
			{
				return m_counts[m_lastPos];
			}

			Counts counts(key);
			m_counts.insert(m_lastPos, counts);
			return m_counts[m_lastPos];
		}

		unsigned getCount() const
		{
			return m_counts.getCount();
		}

		static unsigned getVectorCapacity()
		{
			return Counts::getVectorCapacity();
		}

		void remove(Key key)
		{
			if ((m_lastPos != (FB_SIZE_T) ~0 && m_counts[m_lastPos].getGroupKey() == key) ||
				// if m_lastPos is mispositioned
				m_counts.find(key, m_lastPos))
			{
				m_counts.remove(m_lastPos);
				m_lastPos = (FB_SIZE_T) ~0;
			}
		}

		void reset()
		{
			m_counts.clear();
			m_lastPos = (FB_SIZE_T) ~0;
		}

		ConstIterator begin() const
		{
			return m_counts.begin();
		}

		ConstIterator end() const
		{
			return m_counts.end();
		}

		void adjust(const GroupedCountsArray& baseStats, const GroupedCountsArray& newStats);

	private:
		SortedCountsArray m_counts;
		FB_SIZE_T m_lastPos = (FB_SIZE_T) ~0;
	};

	typedef CountsGroup<PageStatType, ULONG> PageCounts;
	typedef CountsGroup<RecordStatType, SLONG> RelationCounts;

public:
	typedef GroupedCountsArray<PageCounts, ULONG> PageCounters;
	typedef GroupedCountsArray<RelationCounts, SLONG> RelationCounters;

	RuntimeStatistics()
		: Firebird::AutoStorage(),
		  page_counts(getPool(), DB_PAGE_SPACE + 1),
		  rel_counts(getPool(), rel_MAX)
	{
		reset();
	}

	explicit RuntimeStatistics(MemoryPool& pool)
		: Firebird::AutoStorage(pool),
		  page_counts(getPool(), DB_PAGE_SPACE + 1),
		  rel_counts(getPool(), rel_MAX)
	{
		reset();
	}

	RuntimeStatistics(const RuntimeStatistics& other)
		: Firebird::AutoStorage(),
		  page_counts(getPool(), other.page_counts),
		  rel_counts(getPool(), other.rel_counts)
	{
		memcpy(values, other.values, sizeof(values));

		page_counts = other.page_counts;
		rel_counts = other.rel_counts;

		allChgNumber = other.allChgNumber;
		pageChgNumber = other.pageChgNumber;
		relChgNumber = other.relChgNumber;
	}

	RuntimeStatistics(MemoryPool& pool, const RuntimeStatistics& other)
		: Firebird::AutoStorage(pool),
		  page_counts(getPool(), other.page_counts),
		  rel_counts(getPool(), other.rel_counts)
	{
		memcpy(values, other.values, sizeof(values));

		page_counts = other.page_counts;
		rel_counts = other.rel_counts;

		allChgNumber = other.allChgNumber;
		pageChgNumber = other.pageChgNumber;
		relChgNumber = other.relChgNumber;
	}

	~RuntimeStatistics() = default;

	void reset()
	{
		memset(values, 0, sizeof values);

		page_counts.reset();
		rel_counts.reset();

		allChgNumber = 0;
		pageChgNumber = 0;
		relChgNumber = 0;
	}

	const SINT64& operator[](const PageStatType type) const
	{
		const auto index = static_cast<size_t>(type);
		return values[index];
	}

	void bumpValue(const PageStatType type, ULONG pageSpaceId, SINT64 delta = 1)
	{
		++allChgNumber;
		const auto index = static_cast<size_t>(type);
		values[index] += delta;

		if (isValid()) // optimization for non-trivial data access
		{
			++pageChgNumber;
			page_counts[pageSpaceId][type] += delta;
		}
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

	void bumpValue(const RecordStatType type, SLONG relationId, SINT64 delta = 1)
	{
		++allChgNumber;
		const auto index = static_cast<size_t>(type);
		values[PAGE_TOTAL_ITEMS + index] += delta;

		if (isValid()) // optimization for non-trivial data access
		{
			++relChgNumber;
			rel_counts[relationId][type] += delta;
		}
	}

	// Calculate difference between counts stored in this object and current
	// counts of given request. Counts stored in object are destroyed.
	void setToDiff(const RuntimeStatistics& newStats);

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

			if (pageChgNumber != other.pageChgNumber)
			{
				page_counts = other.page_counts;
				pageChgNumber = other.pageChgNumber;
			}

			if (relChgNumber != other.relChgNumber)
			{
				rel_counts = other.rel_counts;
				relChgNumber = other.relChgNumber;
			}
		}

		return *this;
	}

	bool isValid() const
	{
		return (this != &dummy);
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

	const SINT64* getGlobalCounterVector() const
	{
		return values;
	}

	const PageCounters& getPageCounters() const
	{
		return page_counts;
	}

	const RelationCounters& getRelationCounters() const
	{
		return rel_counts;
	}

private:
	SINT64 values[GLOBAL_ITEMS];
	PageCounters page_counts;
	RelationCounters rel_counts;

	// These numbers are used in adjust() and assign() methods as "generation"
	// values in order to avoid costly operations when two instances of RuntimeStatistics
	// contain equal counters values. This is intended to use *only* with the
	// same pair of class instances, as in Request.
	ULONG allChgNumber;		// incremented when any counter changes
	ULONG pageChgNumber;	// incremented when page counter changes
	ULONG relChgNumber;		// incremented when relation counter changes

	// This dummy RuntimeStatistics is used instead of missing elements in tdbb,
	// helping us to avoid conditional checks in time-critical places of code.
	// Values of it contain actually garbage - don't be surprised when debugging.
	static Firebird::GlobalPtr<RuntimeStatistics> dummy;
};

} // namespace

#endif // JRD_RUNTIME_STATISTICS_H
