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

#ifndef JRD_RECORD_BUFFER_H
#define JRD_RECORD_BUFFER_H

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/auto.h"
#include "../common/classes/File.h"
#include "../jrd/TempSpace.h"

namespace Jrd {

class Format;
class Record;

class RecordBuffer : public Firebird::PermanentStorage
{
public:
	RecordBuffer(MemoryPool&, const Format*);
	~RecordBuffer();

	size_t getCount() const
	{
		return count;
	}

	Record* getTempRecord()
	{
		return record.get();
	}

	const Format* getFormat() const;

	void reset();
	offset_t store(const Record*);
	bool fetch(offset_t, Record*);

private:
	void reserveCache();
	void releaseCache();
	void flush();

private:
	// Size of the chunk of records read at once when they are not in memory.
	static constexpr ULONG CACHE_SIZE = 64 * 1024;
	// Maximum number of cached chunks. Windows access the frame start and end, besides the current record.
	static constexpr unsigned CACHE_CHUNKS = 4;

	struct CacheChunk
	{
		offset_t start = 0;
		offset_t count = 0;
		FB_UINT64 lastUse = 0;
	};

	offset_t count = 0;
	// Cached chunks followed by the records not yet written (when batching writes), allocated after
	// the space spilled to disk and counted in the temp cache usage.
	Firebird::Array<UCHAR> cache;
	CacheChunk cacheChunks[CACHE_CHUNKS];
	FB_UINT64 cacheUse = 0;
	FB_SIZE_T cacheReserved = 0;	// bytes counted in the temp cache usage
	offset_t chunkRecords = 0;		// records per chunk
	unsigned cacheChunkCount = 0;	// number of usable chunks
	bool batchWrites = false;		// records are written in chunks after the space spilled
	offset_t pendingCount = 0;		// records not yet written
	offset_t written = 0;
	bool spilled = false;
	Firebird::AutoPtr<Record> record;
	Firebird::AutoPtr<TempSpace> space;
};

} // namespace

#endif // JRD_RECORD_BUFFER_H
