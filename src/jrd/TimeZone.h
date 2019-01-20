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

#ifndef JRD_TIME_ZONE_H
#define JRD_TIME_ZONE_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/fb_string.h"
#include "../jrd/Monitoring.h"
#include "../jrd/recsrc/RecordSource.h"

namespace Jrd {

class thread_db;
class jrd_tra;
class RecordBuffer;


class TimeZoneSnapshot : public SnapshotData
{
public:
	TimeZoneSnapshot(thread_db* tdbb, MemoryPool& pool);
};

class TimeZonesTableScan: public VirtualTableScan
{
public:
	TimeZonesTableScan(CompilerScratch* csb, const Firebird::string& alias, StreamType stream, jrd_rel* relation);

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position, Record* record) const override;
};


FB_MESSAGE(TimeZoneTransitionsInput, Firebird::ThrowStatusExceptionWrapper,
	(FB_INTL_VARCHAR(MAX_SQL_IDENTIFIER_LEN, CS_METADATA), timeZoneName)
	(FB_TIMESTAMP_TZ, fromTimestamp)
	(FB_TIMESTAMP_TZ, toTimestamp)
);

FB_MESSAGE(TimeZoneTransitionsOutput, Firebird::ThrowStatusExceptionWrapper,
	(FB_TIMESTAMP_TZ, startTimestamp)
	(FB_TIMESTAMP_TZ, endTimestamp)
	(FB_SMALLINT, zoneOffset)
	(FB_SMALLINT, dstOffset)
	(FB_SMALLINT, effectiveOffset)
);

class TimeZoneTransitionsResultSet :
	public Firebird::DisposeIface<
			Firebird::IExternalResultSetImpl<TimeZoneTransitionsResultSet, Firebird::ThrowStatusExceptionWrapper> >
{
public:
	TimeZoneTransitionsResultSet(Firebird::ThrowStatusExceptionWrapper* status, Firebird::IExternalContext* context,
		void* inMsg, void* outMsg);

public:
	void dispose() override
	{
		delete this;
	}

public:
	FB_BOOLEAN fetch(Firebird::ThrowStatusExceptionWrapper* status) override;

private:
	TimeZoneTransitionsOutput::Type* out;
	Firebird::AutoPtr<Firebird::TimeZoneRuleIterator> iterator;
};

class TimeZoneTransitionsProcedure :
	public Firebird::DisposeIface<
			Firebird::IExternalProcedureImpl<TimeZoneTransitionsProcedure, Firebird::ThrowStatusExceptionWrapper> >
{
public:
	TimeZoneTransitionsProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IMetadataBuilder* inBuilder, Firebird::IMetadataBuilder* outBuilder)
	{
		TimeZoneTransitionsInput::setup(status, inBuilder);
		TimeZoneTransitionsOutput::setup(status, outBuilder);
	}

public:
	void dispose() override
	{
		delete this;
	}

public:
	void getCharSet(Firebird::ThrowStatusExceptionWrapper* status, Firebird::IExternalContext* context,
		char* name, unsigned nameSize) override
	{
		strncpy(name, "UTF8", nameSize);
	}

	Firebird::IExternalResultSet* open(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, void* inMsg, void* outMsg) override
	{
		return FB_NEW TimeZoneTransitionsResultSet(status, context, inMsg, outMsg);
	}
};


FB_MESSAGE(TimeZoneDatabaseVersionOutput, Firebird::ThrowStatusExceptionWrapper,
	(FB_INTL_VARCHAR(10, CS_ASCII), version)
);

class TimeZoneDatabaseVersionFunction :
	public Firebird::DisposeIface<
			Firebird::IExternalFunctionImpl<TimeZoneDatabaseVersionFunction, Firebird::ThrowStatusExceptionWrapper> >
{
public:
	TimeZoneDatabaseVersionFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IMetadataBuilder* inBuilder, Firebird::IMetadataBuilder* outBuilder)
	{
		TimeZoneDatabaseVersionOutput::setup(status, outBuilder);
	}

public:
	void dispose() override
	{
		delete this;
	}

public:
	void getCharSet(Firebird::ThrowStatusExceptionWrapper* status, Firebird::IExternalContext* context,
		char* name, unsigned nameSize) override
	{
		strncpy(name, "UTF8", nameSize);
	}

	void execute(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, void* inMsg, void* outMsg) override;
};


}	// namespace

#endif	// JRD_TIME_ZONE_H
