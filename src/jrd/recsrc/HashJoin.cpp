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

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

//#define PRINT_HASH_TABLE

// ----------------------
// Data access: hash join
// ----------------------

// NS: FIXME - Why use static hash table here??? Hash table shall support dynamic resizing
static const ULONG HASH_SIZE = 1009;
static const ULONG BUCKET_PREALLOCATE_SIZE = 32;	// 256 bytes per bucket

unsigned HashJoin::maxCapacity()
{
	// Binary search across 1000 collisions is computationally similar to
	// linear search across 10 collisions. We use this number as a rough
	// estimation of whether the lookup performance is likely to be acceptable.
	return HASH_SIZE * 1000;
}


class HashJoin::HashTable : public PermanentStorage
{
	class CollisionList
	{
		static const FB_SIZE_T INVALID_ITERATOR = FB_SIZE_T(~0);

		struct Entry
		{
			Entry()
				: hash(0), position(0)
			{}

			Entry(ULONG h, ULONG pos)
				: hash(h), position(pos)
			{}

			static const ULONG generate(const Entry& item)
			{
				return item.hash;
			}

			ULONG hash;
			ULONG position;
		};

	public:
		CollisionList(MemoryPool& pool)
			: m_collisions(pool, BUCKET_PREALLOCATE_SIZE),
			  m_iterator(INVALID_ITERATOR)
		{
			m_collisions.setSortMode(FB_ARRAY_SORT_MANUAL);
		}

		void sort()
		{
			m_collisions.sort();
		}

		ULONG getCount() const
		{
			return (ULONG) m_collisions.getCount();
		}

		void add(ULONG hash, ULONG position)
		{
			m_collisions.add(Entry(hash, position));
		}

		bool locate(ULONG hash)
		{
			if (m_collisions.find(hash, m_iterator))
				return true;

			m_iterator = INVALID_ITERATOR;
			return false;
		}

		bool iterate(ULONG hash, ULONG& position)
		{
			if (m_iterator >= m_collisions.getCount())
				return false;

			const Entry& collision = m_collisions[m_iterator++];

			if (hash != collision.hash)
			{
				m_iterator = INVALID_ITERATOR;
				return false;
			}

			position = collision.position;
			return true;
		}

	private:
		SortedArray<Entry, EmptyStorage<Entry>, ULONG, Entry> m_collisions;
		FB_SIZE_T m_iterator;
	};

public:
	HashTable(MemoryPool& pool, ULONG streamCount, ULONG tableSize = HASH_SIZE)
		: PermanentStorage(pool), m_streamCount(streamCount),
		  m_tableSize(tableSize), m_slot(0)
	{
		m_collisions = FB_NEW_POOL(pool) CollisionList*[streamCount * tableSize];
		memset(m_collisions, 0, streamCount * tableSize * sizeof(CollisionList*));
	}

	~HashTable()
	{
		for (ULONG i = 0; i < m_streamCount * m_tableSize; i++)
			delete m_collisions[i];

		delete[] m_collisions;
	}

	void put(ULONG stream, ULONG hash, ULONG position)
	{
		const ULONG slot = hash % m_tableSize;

		fb_assert(stream < m_streamCount);
		fb_assert(slot < m_tableSize);

		CollisionList* collisions = m_collisions[stream * m_tableSize + slot];

		if (!collisions)
		{
			collisions = FB_NEW_POOL(getPool()) CollisionList(getPool());
			m_collisions[stream * m_tableSize + slot] = collisions;
		}

		collisions->add(hash, position);
	}

	bool setup(ULONG hash)
	{
		const ULONG slot = hash % m_tableSize;

		for (ULONG i = 0; i < m_streamCount; i++)
		{
			CollisionList* const collisions = m_collisions[i * m_tableSize + slot];

			if (!collisions)
				return false;

			if (!collisions->locate(hash))
				return false;
		}

		m_slot = slot;
		return true;
	}

	void reset(ULONG stream, ULONG hash)
	{
		fb_assert(stream < m_streamCount);

		CollisionList* const collisions = m_collisions[stream * m_tableSize + m_slot];
		collisions->locate(hash);
	}

	bool iterate(ULONG stream, ULONG hash, ULONG& position)
	{
		fb_assert(stream < m_streamCount);

		CollisionList* const collisions = m_collisions[stream * m_tableSize + m_slot];
		return collisions->iterate(hash, position);
	}

	void sort()
	{
		for (ULONG i = 0; i < m_streamCount * m_tableSize; i++)
		{
			if (const auto collisions = m_collisions[i])
				collisions->sort();
		}

#ifdef PRINT_HASH_TABLE
		FB_UINT64 total = 0;
		ULONG min = MAX_ULONG, max = 0, count = 0;

		for (ULONG i = 0; i < m_streamCount * m_tableSize; i++)
		{
			CollisionList* const collisions = m_collisions[i];
			if (!collisions)
				continue;

			const auto cnt = collisions->getCount();

			if (cnt < min)
				min = cnt;
			if (cnt > max)
				max = cnt;
			total += cnt;
			count++;
		}

		if (count)
		{
			printf("Hash table size %u, count %u, buckets %u, min %u, max %u, avg %u\n",
				   m_tableSize, (ULONG) total, count, min, max, (ULONG) (total / count));
		}
#endif
	}

private:
	const ULONG m_streamCount;
	const ULONG m_tableSize;
	CollisionList** m_collisions;
	ULONG m_slot;
};


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
		selectivity = pow(REDUCE_SELECTIVITY_FACTOR_EQUALITY, keyCount);

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

	const auto inner = m_subs.front().source;

	while (true)
	{
		if (impure->irsb_flags & irsb_mustread)
		{
			// Fetch the record from the leading stream

			if (!m_leader.source->getRecord(tdbb))
				return false;

			if (m_boolean && !m_boolean->execute(tdbb, request))
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

				impure->irsb_hash_table->sort();
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

	Join::getPlan(tdbb, planEntry, level, recurse);
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

		if (desc && !(request->req_flags & req_null))
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
					MOV_move(tdbb, desc, &to);
				}
			}
			else
			{
				const auto data = desc->dsc_address;

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
