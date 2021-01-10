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
 *  The Original Code was created by Vladyslav Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Vladyslav Khorsun <hvlad@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "../jrd/ConfigTable.h"
#include "../jrd/ini.h"
#include "../jrd/ids.h"

using namespace Jrd;
using namespace Firebird;


///  class ConfigTable

ConfigTable::ConfigTable(MemoryPool& pool, const Config* conf) :
	SnapshotData(pool),
	m_conf(conf)
{
}

RecordBuffer* ConfigTable::getRecords(thread_db* tdbb, jrd_rel* relation)
{
	fb_assert(relation);
	fb_assert(relation->rel_id == rel_config);

	RecordBuffer* recordBuffer = getData(relation);
	if (recordBuffer)
		return recordBuffer;

	recordBuffer = allocBuffer(tdbb, *tdbb->getDefaultPool(), relation->rel_id);

	// Check privileges to see RDB$CONFIG
	const Attachment* att = tdbb->getAttachment();
	if (!att->att_user->locksmith(tdbb, SELECT_ANY_OBJECT_IN_DATABASE))
		return recordBuffer;

	for (unsigned int key = 0; key < Config::getKeyCount(); key++)
	{
		Record* rec = recordBuffer->getTempRecord();
		rec->nullify();

		SINT64 id = key;
		putField(tdbb, rec, DumpField(f_cfg_id, VALUE_INTEGER, sizeof(id), &id));

		const char* name = Config::getKeyName(key);
		putField(tdbb, rec, DumpField(f_cfg_name, VALUE_STRING, strlen(name), name));

		string str;
		if (m_conf->getValue(key, str))
			putField(tdbb, rec, DumpField(f_cfg_value, VALUE_STRING, str.length(), str.c_str()));

		if (m_conf->getDefaultValue(key, str))
			putField(tdbb, rec, DumpField(f_cfg_default, VALUE_STRING, str.length(), str.c_str()));

		char set = m_conf->getIsSet(key) ? 1 : 0;
		putField(tdbb, rec, DumpField(f_cfg_is_set, VALUE_BOOLEAN, 1, &set));

		const char* valSrc = m_conf->getValueSource(key);
		if (valSrc)
			putField(tdbb, rec, DumpField(f_cfg_source, VALUE_STRING, strlen(valSrc), valSrc));

		recordBuffer->store(rec);
	}

	return recordBuffer;
}


///  class ConfigTableScan

void ConfigTableScan::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	delete impure->table;
	impure->table = nullptr;

	VirtualTableScan::close(tdbb);
}

const Format* ConfigTableScan::getFormat(thread_db* tdbb, jrd_rel* relation) const
{
	RecordBuffer* records = getRecords(tdbb, relation);
	return records->getFormat();
}

bool ConfigTableScan::retrieveRecord(thread_db* tdbb, jrd_rel* relation,
	FB_UINT64 position, Record* record) const
{
	RecordBuffer* records = getRecords(tdbb, relation);
	return records->fetch(position, record);
}

RecordBuffer* ConfigTableScan::getRecords(thread_db* tdbb, jrd_rel* relation) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->table)
	{
		MemoryPool* pool = tdbb->getDefaultPool();
		impure->table = FB_NEW_POOL(*pool) ConfigTable(*pool, tdbb->getDatabase()->dbb_config);
	}

	return impure->table->getRecords(tdbb, relation);
}
