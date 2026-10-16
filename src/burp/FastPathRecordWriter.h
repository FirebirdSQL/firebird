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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef BURP_FAST_PATH_RECORD_WRITER_H
#define BURP_FAST_PATH_RECORD_WRITER_H

#include "../common/common.h"
#include "../common/classes/auto.h"

struct burp_rel;

namespace Firebird {
	class IAttachment;
	class ITransaction;
}

namespace Burp {

class RestoreMessageLayout;

class FastPathRecordWriter
{
public:
	FastPathRecordWriter();
	~FastPathRecordWriter();

	FastPathRecordWriter(const FastPathRecordWriter&) = delete;
	FastPathRecordWriter& operator=(const FastPathRecordWriter&) = delete;

public:
	// The caller must route only persistent user tables to direct VIO. Other
	// problems raise an error.
	void init(Firebird::IAttachment* att, Firebird::ITransaction* tra,
		const burp_rel* relation, const RestoreMessageLayout& layout);

	// Convert and buffer a batch of messages using a single engine attachment
	// sync. Any failure aborts the direct restore; there is no per-record
	// recovery because BulkInsert has no rollback for blob/array side effects.
	void writeRecords(const UCHAR* messageBuffer, RCRD_LENGTH msgLength, unsigned recordCount);

	// Flush buffered pages and unregister the bulk insert from the
	// transaction. The bulk object auto-flushes full extents internally and
	// stays alive across message batches, so this runs at table end and at
	// commit; the next row transparently registers a fresh one when needed.
	// Safe to call when no bulk insert is registered.
	void finishBulk();

	// Release ArrayFields left behind by records in the last batch. Must only be
	// called after a whole VIO batch has been processed: future batches have not
	// created their arrays yet.
	void releaseRequestlessArrays();

	void clear();

private:
	struct Imp;
	Firebird::AutoPtr<Imp> imp;
};

} // namespace Burp

#endif // BURP_FAST_PATH_RECORD_WRITER_H
