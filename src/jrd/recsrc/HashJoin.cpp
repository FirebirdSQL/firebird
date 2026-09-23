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
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/Aligner.h"
#include "../common/classes/Hash.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/intl.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/optimizer/Optimizer.h"

#include <algorithm>
#include <iterator>

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

//#define PRINT_HASH_TABLE

// ----------------------
// Data access: hash join
// ----------------------

namespace
{
	// Below this threshold the temp buffer is small enough to fit in L3,
	// so the cache-friendly sequential read path is faster and the extra
	// memory is negligible. Above the threshold the extra memory starts
	// to matter for concurrent connections, and we accept the cache cost
	// of in-place redistribution.
	constexpr ULONG IN_PLACE_THRESHOLD = 1 << 20;   // 1M entries = 8 MB
	
	// Predefined table sizes (all prime). Table sizing is done once, in build(),
	// based on the actual number of collected entries.	
	constexpr ULONG HASH_SIZES[] =
	{
		1009, 2003, 4001, 8009, 16001, 32003,
		64007, 128021, 256019, 512009, 1000003
	};

	constexpr ULONG MIN_HASH_SIZE = HASH_SIZES[0];
	constexpr ULONG MAX_HASH_SIZE = HASH_SIZES[std::size(HASH_SIZES) - 1];

	// Desired average number of entries per bucket. We sort each bucket
	// by hash and use binary search on lookup, so even a moderately long
	// bucket is cheap. A small factor here means more buckets and a larger
	// bucketStart array; a large factor means longer buckets.
	constexpr ULONG HASH_LOAD_FACTOR = 4;

	constexpr ULONG nextHashSize(ULONG current)
	{
		for (const ULONG size : HASH_SIZES)
		{
			if (size > current)
				return size;
		}
		return MAX_HASH_SIZE;
	}

}

unsigned HashJoin::maxCapacity() noexcept
{
	// Upper bound on the number of records the optimizer may reasonably
	// feed into a single hash join. Above this the bucket scan starts
	// to dominate and the optimizer should pick a different join method.
	return MAX_HASH_SIZE * HASH_LOAD_FACTOR;
}


class HashJoin::HashTable final : public PermanentStorage
{
	// A single {hash, position} pair. The position refers to a record
	// inside the corresponding BufferedStream.
	struct Entry
	{
		ULONG hash;
		ULONG position;
	};

	// Per-stream state and the operations on it. Entries are collected
	// unsorted in put(), then rearranged in build() so that records
	// sharing the same bucket occupy a contiguous range and are sorted
	// by hash within it. After build() every lookup is a binary search
	// inside the bucket's range.
	struct Stream : public PermanentStorage
	{
		Array<Entry> entries;        // all records, grouped by bucket after build()
		Array<ULONG> bucketStart;    // size = tableSize + 1; see build() for layout
		ULONG iterator;              // current cursor for iterate()

		explicit Stream(MemoryPool& pool) 
			: PermanentStorage(pool)
			, entries(pool)
			, bucketStart(pool)
			, iterator(0) 
		{}

		// Collect one raw entry. No bucket logic here, so this can be
		// called freely while reading the inner stream.
		void put(ULONG hash, ULONG position)
		{
			entries.add({ hash, position });
		}

		// Group entries by bucket and sort each bucket by hash.
		//
		// The table size is chosen by HashTable::build() from the total
		// number of entries across all streams, not from any cardinality
		// estimate, so this runs exactly once and never re-runs.
		//
		// Each bucket is expressed as a half-open range
		// [bucketStart[b], bucketStart[b + 1]). The extra (tableSize + 1)-th
		// element is needed so that the range of the last bucket can be
		// written uniformly, without a special case.
		void build(ULONG tableSize);

		bool bucketEmpty(ULONG slot) const noexcept
		{
			return bucketStart[slot] == bucketStart[slot + 1];
		}


		// Position the cursor at the first entry with hash >= the target.
		// If the target is absent, the cursor ends up at the first entry
		// greater than it, and iterate() immediately returns false.
		void reset(ULONG slot, ULONG hash)
		{
			ULONG lo = bucketStart[slot];
			ULONG hi = bucketStart[slot + 1];

			while (lo < hi)
			{
				const ULONG mid = lo + (hi - lo) / 2;
				if (entries[mid].hash < hash)
					lo = mid + 1;
				else
					hi = mid;
			}

			iterator = lo;
		}

		bool iterate(ULONG slot, ULONG hash, ULONG& position) noexcept
		{
			const ULONG end = bucketStart[slot + 1];

			if (iterator >= end || entries[iterator].hash != hash)
				return false;

			position = entries[iterator].position;
			iterator++;
			return true;
		}
	};

public:
	HashTable(MemoryPool& pool, ULONG streamCount)
		: PermanentStorage(pool), m_streams(pool, streamCount)
	{
		for (ULONG i = 0; i < streamCount; i++)
			m_streams.add(FB_NEW_POOL(pool) Stream(pool));
	}

	~HashTable()
	{
		for (auto* s : m_streams)
			delete s;
	}

	// Phase 1: collect raw (hash, position) pairs into the appropriate
	// stream. Called by HashJoin::internalGetRecord() while the inner
	// stream is being read.
	void put(ULONG stream, ULONG hash, ULONG position)
	{
		fb_assert(stream < m_streams.getCount());

		m_streams[stream]->put(hash, position);
	}

	// Phase 2: once all inner streams are fully read, group their entries
	// by bucket. Called exactly once, never re-run.
	void build();

	// Lookup API. The typical usage pattern is:
	//
	//     if (table->setup(hash))
	//         while (table->iterate(stream, hash, position)) { ... }
	//
	// setup() primes all streams to the first matching record in the
	// current bucket, iterate() advances the cursor of a single stream.
	// If iterate() advances past the end of the bucket, the caller is
	// expected to advance the previous stream (see HashJoin::fetchRecord)
	// and call reset() for the current stream to restart the scan of
	// matching records.

	bool setup(ULONG hash);

	void reset(ULONG stream, ULONG hash)
	{
		fb_assert(stream < m_streams.getCount());

		m_streams[stream]->reset(m_slot, hash);
	}

	bool iterate(ULONG stream, ULONG hash, ULONG& position) noexcept
	{
		fb_assert(stream < m_streams.getCount());

		return m_streams[stream]->iterate(m_slot, hash, position);
	}

private:
	Array<Stream*> m_streams;
	ULONG m_tableSize = 0;
	ULONG m_slot = 0;
};

void HashJoin::HashTable::Stream::build(ULONG tableSize)
{
	const ULONG n = entries.getCount();

	bucketStart.grow(tableSize + 1);
	memset(bucketStart.begin(), 0, (tableSize + 1) * sizeof(ULONG));

	// Pass 1: count how many entries fall into each bucket.
	// bucketStart[b + 1] is used as a per-bucket counter, so after
	// this pass bucketStart[b + 1] holds the size of bucket b.
	// The +1 offset lets us compute prefix sums below without
	// touching bucketStart[0], which stays zero.
	for (ULONG k = 0; k < n; k++)
		bucketStart[entries[k].hash % tableSize + 1]++;

	// Pass 2: turn per-bucket counts into start offsets.
	// After this pass bucketStart[b] is the index of the first
	// entry of bucket b, and bucketStart[b + 1] is one past the last.
	for (ULONG b = 0; b < tableSize; b++)
		bucketStart[b + 1] += bucketStart[b];

	// Pass 3: physically move entries into their buckets. We cannot
	// write into entries while still reading from it without
	// corrupting data, so we allocate a temporary buffer and use a
	// separate cursor array 'next' that starts as a copy of bucketStart
	// and is incremented as entries are placed.
	//
	// This is the only place where O(n) temporary memory is needed.
	// Peak footprint during build is roughly twice the size of the
	// entries array; the temporary buffer is released right after.

	if (n == 0)
		return;

	Array<ULONG> next(getPool(), tableSize);
	next.grow(tableSize);
	memcpy(next.begin(), bucketStart.begin(), tableSize * sizeof(ULONG));

	if (n < IN_PLACE_THRESHOLD)
	{
		Array<Entry> temp(getPool(), n);
		temp.grow(n);

		for (ULONG k = 0; k < n; k++)
		{
			const Entry& e = entries[k];
			temp[next[e.hash % tableSize]++] = e;
		}

		entries.assign(temp);
    }
	else
	{
		// Try to rearrange array by placing entry in correct bucket without making copy.
		// `tableSize - 1` is because after iterating over all buckets except the last one, the lasting elements will be
		// already placed at the correct (last) bucket.
		for (ULONG bucket = 0; bucket < tableSize - 1; bucket++)
		{
			// Iterate bucket by bucket and place entry in the correct spot.
			// After placing entry, the bucket start point will be incremented, so when we are starting to process next bucket,
			// we will skip entries, that already at the right bucket.
			for (ULONG k = next[bucket]; k < bucketStart[bucket + 1]; k++)
			{
				Entry& e = entries[k];
				const auto targetBucket = e.hash % tableSize;

				// Already placed at correct bucket, just increment the pointer.
				if (targetBucket == bucket)
				{
					++next[bucket];
					continue;
				}

				// Place entry in correct bucket by swapping current entry with entry that is taking our spot.
				std::swap(e, entries[next[targetBucket]++]);

				// Due to the swap operation, we need to process newly arrived entry at the current position once more.
				--k;
			}
		}
	}

	// Pass 4: sort each bucket by hash. Singletons are skipped.
	// Records with equal hashes may end up in any order relative to
	// each other, which is fine for the join: they all compare equal
	// on the join key anyway.
	auto compareHash = [](const Entry& a, const Entry& b) { return a.hash < b.hash; };

	for (ULONG bucket = 0; bucket < tableSize; bucket++)
	{
		const ULONG start = bucketStart[bucket];
		const ULONG end = bucketStart[bucket + 1];
		if (end > start + 1)
			std::sort(entries.begin() + start, entries.begin() + end, compareHash);
	}

#ifdef PRINT_HASH_TABLE
	// Per-stream bucket statistics. Useful when tuning HASH_LOAD_FACTOR
	// or debugging hash distribution for a particular key type.
	{
		FB_UINT64 total = 0;
		ULONG min = MAX_ULONG, max = 0, occupied = 0;

		for (ULONG b = 0; b < tableSize; b++)
		{
			const ULONG cnt = bucketStart[b + 1] - bucketStart[b];
			if (cnt == 0)
				continue;

			if (cnt < min)
				min = cnt;
			if (cnt > max)
				max = cnt;
			total += cnt;
			occupied++;
		}

		if (occupied)
		{
			printf("Hash table size %u, entries %u, buckets %u, min %u, max %u, avg %u\n",
				tableSize, (ULONG)total, occupied, min, max,
				(ULONG)(total / occupied));
		}
	}
#endif
}

void HashJoin::HashTable::build()
{
	// The table size is chosen from the *actual* number of collected
	// entries, not from any cardinality estimate. Estimates for table
	// functions and stored procedures use a fixed default and may be
	// off by orders of magnitude; getting the size wrong is exactly
	// what a dynamic resizing scheme would be paying for later.
	// Since all inner streams are fully materialized by the time
	// build() runs, we can afford to decide once and for all.
	ULONG total = 0;
	for (auto* s : m_streams)
		total += s->entries.getCount();

	const ULONG desired = total / (m_streams.getCount() * HASH_LOAD_FACTOR);
	m_tableSize = nextHashSize(desired);

	for (auto* s : m_streams)
		s->build(m_tableSize);
}

bool HashJoin::HashTable::setup(ULONG hash)
{
	const ULONG slot = hash % m_tableSize;

	// Every stream must have at least one entry in this bucket, otherwise
	// the join cannot produce output for this leader row. This is a cheap
	// O(1) check on bucketStart and lets us skip binary searches for the
	// common case of a leader row with no counterpart.
	for (auto* s : m_streams)
	{
		if (s->bucketEmpty(slot))
			return false;
	}

	m_slot = slot;

	// Prime the cursor of each stream to the first entry with the target
	// hash (or to its insertion point if the target is absent).
	for (ULONG i = 0; i < m_streams.getCount(); i++)
		reset(i, hash);

	return true;
}


HashJoin::HashJoin(thread_db* tdbb, CompilerScratch* csb, JoinType joinType,
				   FB_SIZE_T count, RecordSource* const* args, NestValueArray* const* keys,
				   double selectivity)
	: Join(csb, count, joinType),
	  m_subs(csb->csb_pool, count - 1)
{
	fb_assert(count >= 2);

	init(tdbb, csb, count, args, keys, selectivity);
}

HashJoin::HashJoin(thread_db* tdbb, CompilerScratch* csb,
				   BoolExprNode* boolean,
				   RecordSource* const* args, NestValueArray* const* keys,
				   double selectivity)
	: Join(csb, 2, JoinType::OUTER, boolean),
	  m_subs(csb->csb_pool, 1)
{
	init(tdbb, csb, 2, args, keys, selectivity);
}

void HashJoin::init(thread_db* tdbb, CompilerScratch* csb, FB_SIZE_T count,
					RecordSource* const* args, NestValueArray* const* keys,
					double selectivity)
{
	m_impure = csb->allocImpure<Impure>();

	m_leader.source = args[0];
	m_leader.keys = keys[0];
	const FB_SIZE_T leaderKeyCount = m_leader.keys->getCount();
	m_leader.keyLengths = FB_NEW_POOL(csb->csb_pool) ULONG[leaderKeyCount];
	m_leader.totalKeyLength = 0;

	m_cardinality = m_leader.source->getCardinality();
	m_args.add(m_leader.source);

	for (FB_SIZE_T j = 0; j < leaderKeyCount; j++)
	{
		dsc desc;
		(*m_leader.keys)[j]->getDesc(tdbb, csb, &desc);

		USHORT keyLength = desc.isText() ? desc.getStringLength() : desc.dsc_length;

		if (IS_INTL_DATA(&desc))
			keyLength = INTL_key_length(tdbb, INTL_INDEX_TYPE(&desc), keyLength);
		else if (desc.isTime())
			keyLength = sizeof(ISC_TIME);
		else if (desc.isTimeStamp())
			keyLength = sizeof(ISC_TIMESTAMP);
		else if (desc.dsc_dtype == dtype_dec64)
			keyLength = Decimal64::getKeyLength();
		else if (desc.dsc_dtype == dtype_dec128)
			keyLength = Decimal128::getKeyLength();

		m_leader.keyLengths[j] = keyLength;
		m_leader.totalKeyLength += keyLength;
	}

	auto keyCount = 0;

	for (FB_SIZE_T i = 1; i < count; i++)
	{
		const auto subRsb = args[i];
		fb_assert(subRsb);

		if (m_joinType == JoinType::INNER || m_joinType == JoinType::OUTER)
			m_cardinality *= subRsb->getCardinality();

		SubStream sub;
		sub.buffer = FB_NEW_POOL(csb->csb_pool) BufferedStream(csb, subRsb);
		sub.keys = keys[i];
		const FB_SIZE_T subKeyCount = sub.keys->getCount();
		sub.keyLengths = FB_NEW_POOL(csb->csb_pool) ULONG[subKeyCount];
		sub.totalKeyLength = 0;

		keyCount += subKeyCount;

		for (FB_SIZE_T j = 0; j < subKeyCount; j++)
		{
			dsc desc;
			(*sub.keys)[j]->getDesc(tdbb, csb, &desc);

			USHORT keyLength = desc.isText() ? desc.getStringLength() : desc.dsc_length;

			if (IS_INTL_DATA(&desc))
				keyLength = INTL_key_length(tdbb, INTL_INDEX_TYPE(&desc), keyLength);
			else if (desc.isTime())
				keyLength = sizeof(ISC_TIME);
			else if (desc.isTimeStamp())
				keyLength = sizeof(ISC_TIMESTAMP);
			else if (desc.dsc_dtype == dtype_dec64)
				keyLength = Decimal64::getKeyLength();
			else if (desc.dsc_dtype == dtype_dec128)
				keyLength = Decimal128::getKeyLength();

			sub.keyLengths[j] = keyLength;
			sub.totalKeyLength += keyLength;
		}

		m_subs.add(sub);
		m_args.add(sub.buffer);
	}

	if (!selectivity)
	{
		selectivity = (m_joinType == JoinType::INNER || m_joinType == JoinType::OUTER) ?
			pow(REDUCE_SELECTIVITY_FACTOR_EQUALITY, keyCount) : REDUCE_SELECTIVITY_FACTOR_ANY;
	}

	m_cardinality *= selectivity;
}

void HashJoin::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_mustread;

	delete impure->irsb_hash_table;
	impure->irsb_hash_table = nullptr;

	delete[] impure->irsb_leader_buffer;
	impure->irsb_leader_buffer = nullptr;

	m_leader.source->open(tdbb);
}

void HashJoin::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	invalidateRecords(request);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		Join::close(tdbb);

		delete impure->irsb_hash_table;
		impure->irsb_hash_table = nullptr;

		delete[] impure->irsb_leader_buffer;
		impure->irsb_leader_buffer = nullptr;
	}
}

bool HashJoin::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	const auto* const inner = m_subs.front().source;

	while (true)
	{
		if (impure->irsb_flags & irsb_mustread)
		{
			// Fetch the record from the leading stream

			if (!m_leader.source->getRecord(tdbb))
				return false;

			if (m_boolean && m_boolean->execute(tdbb, request) != TriState(true))
			{
				// The boolean pertaining to the left sub-stream is false
				// so just join sub-stream to a null valued right sub-stream
				inner->nullRecords(tdbb);
				return true;
			}

			// We have something to join with, so ensure the hash table is initialized

			if (!impure->irsb_hash_table && !impure->irsb_leader_buffer)
			{
				auto& pool = *tdbb->getDefaultPool();
				const auto argCount = m_subs.getCount();

				impure->irsb_hash_table = FB_NEW_POOL(pool) HashTable(pool, argCount);
				impure->irsb_leader_buffer = FB_NEW_POOL(pool) UCHAR[m_leader.totalKeyLength];

				UCharBuffer buffer(pool);

				for (FB_SIZE_T i = 0; i < argCount; i++)
				{
					// Read and cache the inner streams. While doing that,
					// hash the join condition values and populate hash tables.

					m_subs[i].buffer->open(tdbb);

					ULONG counter = 0;
					const auto keyBuffer = buffer.getBuffer(m_subs[i].totalKeyLength, false);

					while (m_subs[i].buffer->getRecord(tdbb))
					{
						const auto hash = computeHash(tdbb, request, m_subs[i], keyBuffer);
						impure->irsb_hash_table->put(i, hash, counter++);
					}
				}

				impure->irsb_hash_table->build();
			}

			// Compute and hash the comparison keys

			impure->irsb_leader_hash =
				computeHash(tdbb, request, m_leader, impure->irsb_leader_buffer);

			// Ensure the every inner stream having matches for this hash slot.
			// Setup the hash table for the iteration through collisions.

			if (!impure->irsb_hash_table->setup(impure->irsb_leader_hash))
			{
				if (m_joinType == JoinType::INNER || m_joinType == JoinType::SEMI)
					continue;

				if (m_joinType == JoinType::OUTER)
					inner->nullRecords(tdbb);

				return true;
			}

			impure->irsb_flags &= ~irsb_mustread;
			impure->irsb_flags |= irsb_first;
		}

		// Fetch collisions from the inner streams

		if (impure->irsb_flags & irsb_first)
		{
			bool found = true;

			for (FB_SIZE_T i = 0; i < m_subs.getCount(); i++)
			{
				if (!fetchRecord(tdbb, impure, i))
				{
					found = false;
					break;
				}
			}

			if (!found)
			{
				impure->irsb_flags |= irsb_mustread;

				if (m_joinType == JoinType::INNER || m_joinType == JoinType::SEMI)
					continue;

				if (m_joinType == JoinType::OUTER)
					inner->nullRecords(tdbb);

				break;
			}

			if (m_joinType == JoinType::SEMI || m_joinType == JoinType::ANTI)
			{
				impure->irsb_flags |= irsb_mustread;

				if (m_joinType == JoinType::ANTI)
					continue;
			}

			impure->irsb_flags &= ~irsb_first;
		}
		else if (!fetchRecord(tdbb, impure, m_subs.getCount() - 1))
		{
			fb_assert(m_joinType == JoinType::INNER);
			impure->irsb_flags |= irsb_mustread;
			continue;
		}

		break;
	}

	return true;
}

void HashJoin::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	level++;
	plan += "HASH (";
	Join::getLegacyPlan(tdbb, plan, level);
	plan += ")";
}

void HashJoin::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "HashJoin";

	planEntry.lines.add().text = "Hash Join " + printType();

	string extras;
	extras.printf(" (keys: %" ULONGFORMAT", total key length: %" ULONGFORMAT")",
				  m_leader.keys->getCount(), m_leader.totalKeyLength);

	planEntry.lines.back().text += extras;

	printOptInfo(planEntry.lines);

	Join::internalGetPlan(tdbb, planEntry, level, recurse);
}

ULONG HashJoin::computeHash(thread_db* tdbb,
							Request* request,
						    const SubStream& sub,
							UCHAR* keyBuffer) const
{
	memset(keyBuffer, 0, sub.totalKeyLength);

	UCHAR* keyPtr = keyBuffer;

	for (FB_SIZE_T i = 0; i < sub.keys->getCount(); i++)
	{
		dsc* const desc = EVL_expr(tdbb, request, (*sub.keys)[i]);
		const USHORT keyLength = sub.keyLengths[i];

		if (desc)
		{
			if (desc->isText())
			{
				dsc to;
				to.makeText(keyLength, desc->getTextType(), keyPtr);

				if (IS_INTL_DATA(desc))
				{
					// Convert the INTL string into the binary comparable form
					INTL_string_to_key(tdbb, INTL_INDEX_TYPE(desc),
									   desc, &to, INTL_KEY_UNIQUE);
				}
				else
				{
					// This call ensures that the padding bytes are appended
					MOV_move(tdbb, desc, &to, true);
				}
			}
			else
			{
				const auto* const data = desc->dsc_address;

				if (desc->isDecFloat())
				{
					// Values inside our key buffer are not aligned,
					// so ensure we satisfy our platform's alignment rules
					OutAligner<ULONG, MAX_DEC_KEY_LONGS> key(keyPtr, keyLength);

					if (desc->dsc_dtype == dtype_dec64)
						((Decimal64*) data)->makeKey(key);
					else if (desc->dsc_dtype == dtype_dec128)
						((Decimal128*) data)->makeKey(key);
					else
						fb_assert(false);
				}
				else if (desc->dsc_dtype == dtype_real && *(float*) data == 0)
				{
					fb_assert(keyLength == sizeof(float));
					memset(keyPtr, 0, keyLength); // positive zero in binary
				}
				else if (desc->dsc_dtype == dtype_double && *(double*) data == 0)
				{
					fb_assert(keyLength == sizeof(double));
					memset(keyPtr, 0, keyLength); // positive zero in binary
				}
				else
				{
					// We don't enforce proper alignments inside the key buffer,
					// so use plain byte copying instead of MOV_move() to avoid bus errors.
					// Note: for date/time with time zone, we copy only the UTC part.
					fb_assert(keyLength <= desc->dsc_length);
					memcpy(keyPtr, data, keyLength);
				}
			}
		}

		keyPtr += keyLength;
	}

	fb_assert(keyPtr - keyBuffer == sub.totalKeyLength);

	return InternalHash::hash(sub.totalKeyLength, keyBuffer);
}

bool HashJoin::fetchRecord(thread_db* tdbb, Impure* impure, FB_SIZE_T stream) const
{
	HashTable* const hashTable = impure->irsb_hash_table;

	const BufferedStream* const arg = m_subs[stream].buffer;

	ULONG position;
	if (hashTable->iterate(stream, impure->irsb_leader_hash, position))
	{
		arg->locate(tdbb, position);

		if (arg->getRecord(tdbb))
			return true;
	}

	if (m_joinType == JoinType::SEMI || m_joinType == JoinType::ANTI)
		return false;

	while (true)
	{
		if (stream == 0 || !fetchRecord(tdbb, impure, stream - 1))
			return false;

		hashTable->reset(stream, impure->irsb_leader_hash);

		if (hashTable->iterate(stream, impure->irsb_leader_hash, position))
		{
			arg->locate(tdbb, position);

			if (arg->getRecord(tdbb))
				return true;
		}
	}
}
