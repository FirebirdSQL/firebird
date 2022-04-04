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
 *  Copyright (c) 2021 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_KEYWORDS_TABLE_H
#define JRD_KEYWORDS_TABLE_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../jrd/Monitoring.h"
#include "../jrd/recsrc/RecordSource.h"


namespace Jrd
{

class KeywordsTable : public SnapshotData
{
public:
	KeywordsTable(MemoryPool& pool)
		: SnapshotData(pool)
	{
	}

public:
	RecordBuffer* getRecords(thread_db* tdbb, jrd_rel* relation);
};


class KeywordsTableScan final : public VirtualTableScan
{
public:
	KeywordsTableScan(CompilerScratch* csb, const Firebird::string& alias,
					  StreamType stream, jrd_rel* relation)
		: VirtualTableScan(csb, alias, stream, relation)
	{
		impureOffset = csb->allocImpure<Impure>();
	}

	void close(thread_db* tdbb) const override;

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;

	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position,
		Record* record) const override;

private:
	struct Impure
	{
		KeywordsTable* table;
	};

	RecordBuffer* getRecords(thread_db* tdbb, jrd_rel* relation) const;

	ULONG impureOffset;
};

} // namespace Jrd

#endif // JRD_KEYWORDS_TABLE_H
