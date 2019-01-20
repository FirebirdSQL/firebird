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
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/TimeZone.h"
#include "../jrd/Record.h"
#include "../jrd/ini.h"
#include "../jrd/tra.h"
#include "gen/ids.h"

using namespace Jrd;
using namespace Firebird;


TimeZoneSnapshot::TimeZoneSnapshot(thread_db* tdbb, MemoryPool& pool)
	: SnapshotData(pool)
{
	RecordBuffer* tzBuffer = allocBuffer(tdbb, pool, rel_time_zones);
	Record* tzRecord = tzBuffer->getTempRecord();
	tzRecord->nullify();

	TimeZoneUtil::iterateRegions(
		[=]
		(USHORT id, const char* name)
		{
			SINT64 idValue = id;

			putField(tdbb, tzRecord, DumpField(f_tz_id, VALUE_INTEGER, sizeof(idValue), &idValue));
			putField(tdbb, tzRecord, DumpField(f_tz_name, VALUE_STRING, static_cast<USHORT>(strlen(name)), name));
			tzBuffer->store(tzRecord);
		}
	);
}


//--------------------------------------


TimeZonesTableScan::TimeZonesTableScan(CompilerScratch* csb, const string& alias,
		StreamType stream, jrd_rel* relation)
	: VirtualTableScan(csb, alias, stream, relation)
{
}

const Format* TimeZonesTableScan::getFormat(thread_db* tdbb, jrd_rel* relation) const
{
	return tdbb->getTransaction()->getTimeZoneSnapshot(tdbb)->getData(relation)->getFormat();
}


//--------------------------------------


bool TimeZonesTableScan::retrieveRecord(thread_db* tdbb, jrd_rel* relation,
	FB_UINT64 position, Record* record) const
{
	return tdbb->getTransaction()->getTimeZoneSnapshot(tdbb)->getData(relation)->fetch(position, record);
}


//--------------------------------------


TimeZoneTransitionsResultSet::TimeZoneTransitionsResultSet(ThrowStatusExceptionWrapper* status,
		IExternalContext* context, void* inMsg, void* outMsg)
	: out(static_cast<TimeZoneTransitionsOutput::Type*>(outMsg))
{
	TimeZoneTransitionsInput::Type* in = static_cast<TimeZoneTransitionsInput::Type*>(inMsg);

	out->startTimestampNull = out->endTimestampNull = out->zoneOffsetNull =
		out->dstOffsetNull = out->effectiveOffsetNull = FB_FALSE;

	USHORT tzId = TimeZoneUtil::parseRegion(in->timeZoneName.str, in->timeZoneName.length);

	iterator = FB_NEW TimeZoneRuleIterator(tzId, in->fromTimestamp, in->toTimestamp);
}

FB_BOOLEAN TimeZoneTransitionsResultSet::fetch(ThrowStatusExceptionWrapper* status)
{
	if (!iterator->next())
		return false;

	out->startTimestamp = iterator->startTimestamp;
	out->endTimestamp = iterator->endTimestamp;
	out->zoneOffset = iterator->zoneOffset;
	out->dstOffset = iterator->dstOffset;
	out->effectiveOffset = iterator->zoneOffset + iterator->dstOffset;

	return true;
}


//--------------------------------------


void TimeZoneDatabaseVersionFunction::execute(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, void* inMsg, void* outMsg)
{
	TimeZoneDatabaseVersionOutput::Type* out = static_cast<TimeZoneDatabaseVersionOutput::Type*>(outMsg);

	string str;
	TimeZoneUtil::getDatabaseVersion(str);

	out->versionNull = FB_FALSE;
	out->version.set(str.c_str());
}
