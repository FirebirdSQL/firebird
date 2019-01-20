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
#include "../common/classes/Hash.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/intl.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/intl_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ----------------------
// Data access: hash join
// ----------------------

// NS: FIXME - Why use static hash table here??? Hash table shall support dynamic resizing
static const ULONG HASH_SIZE = 1009;
static const ULONG BUCKET_PREALLOCATE_SIZE = 32;	// 256 bytes per slot

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
			CollisionList* const collisions = m_collisions[i];

			if (collisions)
				collisions->sort();
		}
	}

private:
	const ULONG m_streamCount;
	const ULONG m_tableSize;
	CollisionList** m_collisions;
	ULONG m_slot;
};


HashJoin::HashJoin(thread_db* tdbb, CompilerScratch* csb, FB_SIZE_T count,
				   RecordSource* const* args, NestValueArray* const* keys)
	: m_args(csb->csb_pool, count - 1)
{
	fb_assert(count >= 2);

	m_impure = CMP_impure(csb, sizeof(Impure));

	m_leader.source = args[0];
	m_leader.keys = keys[0];
	const FB_SIZE_T leaderKeyCount = m_leader.keys->getCount();
	m_leader.keyLengths = FB_NEW_POOL(csb->csb_pool) ULONG[leaderKeyCount];
	m_leader.totalKeyLength = 0;

	for (FB_SIZE_T j = 0; j < leaderKeyCount; j++)
	{
		dsc desc;
		(*m_leader.keys)[j]->getDesc(tdbb, csb, &desc);

		USHORT keyLength = desc.isText() ? desc.getStringLength() : desc.dsc_length;

		if (IS_INTL_DATA(&desc))
			keyLength = INTL_key_length(tdbb, INTL_INDEX_TYPE(&desc), keyLength);

		m_leader.keyLengths[j] = keyLength;
		m_leader.totalKeyLength += keyLength;
	}

	for (FB_SIZE_T i = 1; i < count; i++)
	{
		RecordSource* const sub_rsb = args[i];
		fb_assert(sub_rsb);

		SubStream sub;
		sub.buffer = FB_NEW_POOL(csb->csb_pool) BufferedStream(csb, sub_rsb);
		sub.keys = keys[i];
		const FB_SIZE_T subKeyCount = sub.keys->getCount();
		sub.keyLengths = FB_NEW_POOL(csb->csb_pool) ULONG[subKeyCount];
		sub.totalKeyLength = 0;

		for (FB_SIZE_T j = 0; j < subKeyCount; j++)
		{
			dsc desc;
			(*sub.keys)[j]->getDesc(tdbb, csb, &desc);

			USHORT keyLength = desc.isText() ? desc.getStringLength() : desc.dsc_length;

			if (IS_INTL_DATA(&desc))
				keyLength = INTL_key_length(tdbb, INTL_INDEX_TYPE(&desc), keyLength);

			sub.keyLengths[j] = keyLength;
			sub.totalKeyLength += keyLength;
		}

		m_args.add(sub);
	}
}

void HashJoin::open(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_mustread;

	delete impure->irsb_hash_table;
	delete[] impure->irsb_leader_buffer;

	MemoryPool& pool = *tdbb->getDefaultPool();

	const FB_SIZE_T argCount = m_args.getCount();

	impure->irsb_hash_table = FB_NEW_POOL(pool) HashTable(pool, argCount);
	impure->irsb_leader_buffer = FB_NEW_POOL(pool) UCHAR[m_leader.totalKeyLength];

	UCharBuffer buffer(pool);

	for (FB_SIZE_T i = 0; i < argCount; i++)
	{
		// Read and cache the inner streams. While doing that,
		// hash the join condition values and populate hash tables.

		m_args[i].buffer->open(tdbb);

		ULONG counter = 0;
		UCHAR* const keyBuffer = buffer.getBuffer(m_args[i].totalKeyLength, false);

		while (m_args[i].buffer->getRecord(tdbb))
		{
			const ULONG hash = computeHash(tdbb, request, m_args[i], keyBuffer);
			impure->irsb_hash_table->put(i, hash, counter++);
		}
	}

	impure->irsb_hash_table->sort();

	m_leader.source->open(tdbb);
}

void HashJoin::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	invalidateRecords(request);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		delete impure->irsb_hash_table;
		impure->irsb_hash_table = NULL;

		delete[] impure->irsb_leader_buffer;
		impure->irsb_leader_buffer = NULL;

		for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
			m_args[i].buffer->close(tdbb);

		m_leader.source->close(tdbb);
	}
}

bool HashJoin::getRecord(thread_db* tdbb) const
{
	if (--tdbb->tdbb_quantum < 0)
		JRD_reschedule(tdbb, 0, true);

	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	while (true)
	{
		if (impure->irsb_flags & irsb_mustread)
		{
			// Fetch the record from the leading stream

			if (!m_leader.source->getRecord(tdbb))
				return false;

			// Compute and hash the comparison keys

			impure->irsb_leader_hash =
				computeHash(tdbb, request, m_leader, impure->irsb_leader_buffer);

			// Ensure the every inner stream having matches for this hash slot.
			// Setup the hash table for the iteration through collisions.

			if (!impure->irsb_hash_table->setup(impure->irsb_leader_hash))
				continue;

			impure->irsb_flags &= ~irsb_mustread;
			impure->irsb_flags |= irsb_first;
		}

		// Fetch collisions from the inner streams

		if (impure->irsb_flags & irsb_first)
		{
			bool found = true;

			for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
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
				continue;
			}

			impure->irsb_flags &= ~irsb_first;
		}
		else if (!fetchRecord(tdbb, impure, m_args.getCount() - 1))
		{
			impure->irsb_flags |= irsb_mustread;
			continue;
		}

		break;
	}

	return true;
}

bool HashJoin::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

bool HashJoin::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
	return false; // compiler silencer
}

void HashJoin::print(thread_db* tdbb, string& plan, bool detailed, unsigned level) const
{
	if (detailed)
	{
		plan += printIndent(++level) + "Hash Join (inner)";

		m_leader.source->print(tdbb, plan, true, level);

		for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
			m_args[i].source->print(tdbb, plan, true, level);
	}
	else
	{
		level++;
		plan += "HASH (";
		m_leader.source->print(tdbb, plan, false, level);
		plan += ", ";
		for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
		{
			if (i)
				plan += ", ";

			m_args[i].source->print(tdbb, plan, false, level);
		}
		plan += ")";
	}
}

void HashJoin::markRecursive()
{
	m_leader.source->markRecursive();

	for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
		m_args[i].source->markRecursive();
}

void HashJoin::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_leader.source->findUsedStreams(streams, expandAll);

	for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
		m_args[i].source->findUsedStreams(streams, expandAll);
}

void HashJoin::invalidateRecords(jrd_req* request) const
{
	m_leader.source->invalidateRecords(request);

	for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
		m_args[i].source->invalidateRecords(request);
}

void HashJoin::nullRecords(thread_db* tdbb) const
{
	m_leader.source->nullRecords(tdbb);

	for (FB_SIZE_T i = 0; i < m_args.getCount(); i++)
		m_args[i].source->nullRecords(tdbb);
}

ULONG HashJoin::computeHash(thread_db* tdbb,
							jrd_req* request,
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
				// We don't enforce proper alignments inside the key buffer,
				// so use plain byte copying instead of MOV_move() to avoid bus errors
				fb_assert(keyLength == desc->dsc_length);
				memcpy(keyPtr, desc->dsc_address, keyLength);
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

	const BufferedStream* const arg = m_args[stream].buffer;

	ULONG position;
	if (hashTable->iterate(stream, impure->irsb_leader_hash, position))
	{
		arg->locate(tdbb, position);

		if (arg->getRecord(tdbb))
			return true;
	}

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
