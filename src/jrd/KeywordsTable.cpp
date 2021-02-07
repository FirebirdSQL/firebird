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

#include "../jrd/KeywordsTable.h"
#include "../jrd/ini.h"
#include "../jrd/ids.h"
#include "../common/keywords.h"

using namespace Jrd;
using namespace Firebird;


RecordBuffer* KeywordsTable::getRecords(thread_db* tdbb, jrd_rel* relation)
{
	fb_assert(relation);
	fb_assert(relation->rel_id == rel_keywords);

	auto recordBuffer = getData(relation);
	if (recordBuffer)
		return recordBuffer;

	recordBuffer = allocBuffer(tdbb, *tdbb->getDefaultPool(), relation->rel_id);

	const auto record = recordBuffer->getTempRecord();

	for (const auto* token = keywordGetTokens(); token->tok_string; ++token)
	{
		if (isalpha(token->tok_string[0]))
		{
			record->nullify();

			putField(tdbb, record,
				DumpField(f_keyword_name, VALUE_STRING, strlen(token->tok_string), token->tok_string));

			const bool reserved = !token->nonReserved;
			putField(tdbb, record, DumpField(f_keyword_reserved, VALUE_BOOLEAN, 1, &reserved));

			recordBuffer->store(record);
		}
	}

	return recordBuffer;
}


//--------------------------------------


void KeywordsTableScan::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(impureOffset);

	delete impure->table;
	impure->table = nullptr;

	VirtualTableScan::close(tdbb);
}

const Format* KeywordsTableScan::getFormat(thread_db* tdbb, jrd_rel* relation) const
{
	const auto records = getRecords(tdbb, relation);
	return records->getFormat();
}

bool KeywordsTableScan::retrieveRecord(thread_db* tdbb, jrd_rel* relation,
	FB_UINT64 position, Record* record) const
{
	const auto records = getRecords(tdbb, relation);
	return records->fetch(position, record);
}

RecordBuffer* KeywordsTableScan::getRecords(thread_db* tdbb, jrd_rel* relation) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(impureOffset);

	if (!impure->table)
		impure->table = FB_NEW_POOL(*tdbb->getDefaultPool()) KeywordsTable(*tdbb->getDefaultPool());

	return impure->table->getRecords(tdbb, relation);
}
