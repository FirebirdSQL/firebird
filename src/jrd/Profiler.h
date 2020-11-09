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
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_PROFILER_H
#define JRD_PROFILER_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/Nullable.h"
#include "../jrd/Monitoring.h"
#include "../jrd/SystemPackages.h"

namespace Jrd {

class Attachment;
class jrd_req;
class thread_db;
class Profiler;


class ProfileSnapshotData : public SnapshotData
{
public:
	ProfileSnapshotData(thread_db* tdbb);

public:
	void update(thread_db* tdbb, Profiler* profiler);
	void reset(thread_db* tdbb);

private:
	void allocBuffers(thread_db* tdbb);

private:
	MemoryPool& pool;
};


class ProfileTableScan : public VirtualTableScan
{
public:
	using VirtualTableScan::VirtualTableScan;

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position, Record* record) const override;
};


class Profiler
{
	friend class ProfileSnapshotData;
	friend class ProfileTableScan;
	friend class ProfilerPackage;

public:
	class Stats
	{
	public:
		void hit(SINT64 runTime)
		{
			if (counter == 0 || runTime < minTime)
				minTime = runTime;

			if (counter == 0 || runTime > maxTime)
				maxTime = runTime;

			totalTime += runTime;
			++counter;
		}

	public:
		FB_UINT64 counter = 0;
		FB_UINT64 minTime = 0;
		FB_UINT64 maxTime = 0;
		FB_UINT64 totalTime = 0;
	};

	class ProfileRecordSource
	{
	public:
		ProfileRecordSource(MemoryPool& pool)
			: accessPath(pool)
		{
		}

		unsigned sequence = 0;
		Nullable<ULONG> parentSequence;
		Firebird::string accessPath;
		Stats openStats;
		Stats fetchStats;
	};

	class ProfileCursor
	{
	public:
		ProfileCursor(MemoryPool& pool)
			: sources(pool)
		{
		}

		Firebird::RightPooledMap<ULONG, ProfileRecordSource> sources;
	};

	class Request
	{
	public:
		Request(MemoryPool& pool)
			: requestType(pool),
			  packageName(pool),
			  routineName(pool),
			  cursors(pool),
			  sourcesCursor(pool),
			  stats(pool),
			  sqlText(pool)
		{
		}

		void init(jrd_req* request);

		ISC_TIMESTAMP_TZ timeStamp = Firebird::TimeZoneUtil::getCurrentSystemTimeStamp();
		Firebird::string requestType;
		Firebird::MetaString packageName;
		Firebird::MetaString routineName;
		Firebird::RightPooledMap<ULONG, ProfileCursor> cursors;
		Firebird::NonPooledMap<ULONG, ULONG> sourcesCursor;
		Firebird::NonPooledMap<FB_UINT64, Stats> stats;
		Firebird::string sqlText;
	};

	class Session
	{
	public:
		Session(MemoryPool& pool)
			: requests(pool),
			  description(pool)
		{
		}

		void init(Attachment* attachment, const Firebird::string& aDescription);

		Firebird::RightPooledMap<StmtNumber, Request> requests;
		ISC_TIMESTAMP_TZ startTimeStamp = Firebird::TimeZoneUtil::getCurrentSystemTimeStamp();
		Nullable<ISC_TIMESTAMP_TZ> finishTimeStamp;
		Firebird::string description;
	};

private:
	struct SnapshotCounters
	{
		size_t sessions = 0;
		size_t requests = 0;
		size_t recSourceStats = 0;
		size_t stats = 0;
	};

private:
	Profiler(thread_db* tdbb);

public:
	static Profiler* create(thread_db* tdbb);

public:
	void setupCursor(thread_db* tdbb, jrd_req* request, const Cursor* cursor);
	void hitLineColumn(jrd_req* request, ULONG line, ULONG column, SINT64 runTime);
	void hitRecSourceOpen(jrd_req* request, ULONG recSourceId, SINT64 runTime);
	void hitRecSourceGetRecord(jrd_req* request, ULONG recSourceId, SINT64 runTime);

	bool isActive() const
	{
		return activeSession && !paused;
	}

private:
	static FB_UINT64 encodeLineColumn(ULONG line, ULONG column)
	{
		return (FB_UINT64(line) << 32) | column;
	}

	static Firebird::Pair<Firebird::NonPooled<ULONG, ULONG>> decodeLineColumn(FB_UINT64 lineColumn)
	{
		return Firebird::Pair<Firebird::NonPooled<ULONG, ULONG>>(
			ULONG((lineColumn >> 32) & 0xFFFFFFFF), ULONG(lineColumn & 0xFFFFFFFF));
	}

	Request* getRequest(jrd_req* request);

private:
	ULONG currentSessionId = 0;
	bool activeSession = false;
	bool paused = false;
	Firebird::RightPooledMap<ULONG, Session> sessions;
	ProfileSnapshotData snapshotData;
	SnapshotCounters previousSnapshotCounters;
};


class ProfilerPackage : public SystemPackage
{
public:
	ProfilerPackage(Firebird::MemoryPool& pool);

private:
	static Firebird::IExternalResultSet* updateSnapshotProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const void* in, void* out);

	//----------

	FB_MESSAGE(FinishSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, updateSnapshot)
	);

	static Firebird::IExternalResultSet* finishSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const FinishSessionInput::Type* in, void* out);

	//----------

	FB_MESSAGE(PauseSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, updateSnapshot)
	);

	static Firebird::IExternalResultSet* pauseSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const PauseSessionInput::Type* in, void* out);

	//----------

	static Firebird::IExternalResultSet* purgeSnapshotsProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const void* in, void* out);

	//----------

	static Firebird::IExternalResultSet* resumeSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const void* in, void* out);

	//----------

	FB_MESSAGE(StartSessionInput, Firebird::ThrowStatusExceptionWrapper,
		////  TODO: Options: PSQL, SQL.
		(FB_INTL_VARCHAR(255, CS_METADATA), description)
	);

	FB_MESSAGE(StartSessionOutput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, sessionId)
	);

	static void startSessionFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const StartSessionInput::Type* in, StartSessionOutput::Type* out);
};


}	// namespace

#endif	// JRD_PROFILER_H
