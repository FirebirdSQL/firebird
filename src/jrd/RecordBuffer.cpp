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
#include "../jrd/jrd.h"
#include "../jrd/req.h"

#include "../jrd/RecordBuffer.h"

const char* const SCRATCH = "fb_recbuf_";

using namespace Jrd;

RecordBuffer::RecordBuffer(MemoryPool& pool, const Format* format)
	: PermanentStorage(pool),
	  cache(pool)
{
	record = FB_NEW_POOL(pool) Record(pool, format);
}

RecordBuffer::~RecordBuffer()
{
	releaseCache();
}

void RecordBuffer::reset()
{
	count = 0;
	releaseCache();
	written = 0;
	spilled = false;
	space.reset();
}

offset_t RecordBuffer::store(const Record* new_record)
{
	const ULONG length = record->getLength();
	fb_assert(new_record->getLength() == length);

	if (!space)
		space = FB_NEW_POOL(getPool()) TempSpace(getPool(), SCRATCH);

	if (batchWrites)
	{
		memcpy(cache.begin() + (cacheChunkCount * chunkRecords + pendingCount) * length,
			new_record->getData(), length);

		if (++pendingCount == chunkRecords)
			flush();
	}
	else
	{
		space->write(count * length, new_record->getData(), length);
		written = count + 1;

		// A record crossing two memory blocks is not in memory as a whole, so check only its end.
		if (!spilled && !space->inMemory(count * length + length - 1, 1))
		{
			spilled = true;
			reserveCache();
		}
	}

	return count++;
}

bool RecordBuffer::fetch(offset_t position, Record* to_record)
{
	const ULONG length = record->getLength();

	if (position >= count)
		return false;

	fb_assert(space.hasData());

	if (position >= written)
	{
		memcpy(to_record->getData(),
			cache.begin() + (cacheChunkCount * chunkRecords + position - written) * length, length);
		return true;
	}

	if (!cacheChunkCount)
	{
		space->read(position * length, to_record->getData(), length);
		return true;
	}

	const offset_t chunkStart = position / chunkRecords * chunkRecords;
	CacheChunk* victim = cacheChunks;

	for (auto chunk = cacheChunks; chunk < cacheChunks + cacheChunkCount; ++chunk)
	{
		if (chunk->count && position >= chunk->start && position < chunk->start + chunk->count)
		{
			chunk->lastUse = ++cacheUse;
			memcpy(to_record->getData(),
				cache.begin() + ((chunk - cacheChunks) * chunkRecords + position - chunk->start) * length,
				length);
			return true;
		}

		// Reuse a partially read chunk with the same records, otherwise the least recently used one.
		if (victim->start != chunkStart || !victim->count)
		{
			if ((chunk->count && chunk->start == chunkStart) || chunk->lastUse < victim->lastUse)
				victim = chunk;
		}
	}

	if (const auto ptr = space->inMemory(position * length, length))
	{
		memcpy(to_record->getData(), ptr, length);
		return true;
	}

	// The record is not in memory, so read a chunk of records around it to avoid
	// a file read for each record.

	UCHAR* const data = cache.begin() + (victim - cacheChunks) * chunkRecords * length;

	victim->start = chunkStart;
	victim->count = MIN(chunkRecords, written - chunkStart);
	victim->lastUse = ++cacheUse;

	space->read(victim->start * length, data, victim->count * length);

	memcpy(to_record->getData(), data + (position - victim->start) * length, length);

	return true;
}

// Allocate the cache after the space spilled to disk. It is counted in the temp cache usage, so
// other temp spaces spill earlier, but it is allocated even when that exceeds the limit, which
// is usually the reason of the spill.
void RecordBuffer::reserveCache()
{
	fb_assert(!cacheReserved);

	const ULONG length = record->getLength();
	const offset_t records = MAX(CACHE_SIZE / length, 1);
	// One more chunk for the batched writes.
	const FB_SIZE_T size = (CACHE_CHUNKS + 1) * records * length;
	const auto dbb = GET_DBB();

	dbb->incTempCacheUsage(size, true);

	try
	{
		cache.getBuffer(size);
	}
	catch (const Firebird::BadAlloc&)
	{
		dbb->decTempCacheUsage(size);
		return;
	}

	cacheReserved = size;
	chunkRecords = records;
	cacheChunkCount = CACHE_CHUNKS;
	batchWrites = true;
}

void RecordBuffer::releaseCache()
{
	for (auto& chunk : cacheChunks)
		chunk = CacheChunk();

	cacheUse = 0;
	chunkRecords = 0;
	cacheChunkCount = 0;
	batchWrites = false;
	pendingCount = 0;
	cache.free();

	if (cacheReserved)
	{
		GET_DBB()->decTempCacheUsage(cacheReserved);
		cacheReserved = 0;
	}
}

void RecordBuffer::flush()
{
	if (pendingCount)
	{
		const ULONG length = record->getLength();

		space->write(written * length, cache.begin() + cacheChunkCount * chunkRecords * length,
			pendingCount * length);
		written += pendingCount;
		pendingCount = 0;
	}
}

const Format* RecordBuffer::getFormat() const
{
	return record->getFormat();
}
