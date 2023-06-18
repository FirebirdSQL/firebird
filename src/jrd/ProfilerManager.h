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

#ifndef JRD_PROFILER_MANAGER_H
#define JRD_PROFILER_MANAGER_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/Nullable.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/TimerImpl.h"
#include "../jrd/SystemPackages.h"

namespace Jrd {

class Attachment;
class Request;
class RecordSource;
class Select;
class thread_db;

class ProfilerListener;


class ProfilerManager final
{
	friend class ProfilerListener;
	friend class ProfilerPackage;

public:
	class Stats final : public Firebird::IProfilerStatsImpl<Stats, Firebird::ThrowStatusExceptionWrapper>
	{
	public:
		explicit Stats(FB_UINT64 aElapsedTicks)
			: elapsedTicks(aElapsedTicks)
		{}

	public:
		FB_UINT64 getElapsedTicks() override
		{
			return elapsedTicks;
		}

	private:
		FB_UINT64 elapsedTicks;
	};

private:
	class Statement final
	{
	public:
		Statement(MemoryPool& pool)
			: cursorNextSequence(pool),
			  definedCursors(pool),
			  recSourceSequence(pool)
		{
		}

		Statement(const Statement&) = delete;
		void operator=(const Statement&) = delete;

		SINT64 id = 0;
		Firebird::NonPooledMap<ULONG, ULONG> cursorNextSequence;
		Firebird::SortedArray<ULONG> definedCursors;
		Firebird::NonPooledMap<ULONG, ULONG> recSourceSequence;
	};

	class Session final
	{
	public:
		Session(MemoryPool& pool)
			: statements(pool),
			  requests(pool)
		{
		}

		Session(const Session&) = delete;
		void operator=(const Session&) = delete;

		Firebird::AutoPlugin<Firebird::IProfilerPlugin> plugin;
		Firebird::AutoDispose<Firebird::IProfilerSession> pluginSession;
		Firebird::RightPooledMap<StmtNumber, Statement> statements;
		Firebird::SortedArray<StmtNumber> requests;
		unsigned flags = 0;
	};

private:
	ProfilerManager(thread_db* tdbb);

public:
	~ProfilerManager();

public:
	static ProfilerManager* create(thread_db* tdbb);

	static int blockingAst(void* astObject);

	ProfilerManager(const ProfilerManager&) = delete;
	void operator=(const ProfilerManager&) = delete;

public:
	SINT64 startSession(thread_db* tdbb, Nullable<SLONG> flushInterval,
		const Firebird::PathName& pluginName, const Firebird::string& description, const Firebird::string& options);

	void prepareCursor(thread_db* tdbb, Request* request, const Select* select);
	void prepareRecSource(thread_db* tdbb, Request* request, const RecordSource* rsb);
	void onRequestFinish(Request* request, Stats& stats);
	void beforePsqlLineColumn(Request* request, ULONG line, ULONG column);
	void afterPsqlLineColumn(Request* request, ULONG line, ULONG column, Stats& stats);
	void beforeRecordSourceOpen(Request* request, const RecordSource* rsb);
	void afterRecordSourceOpen(Request* request, const RecordSource* rsb, Stats& stats);
	void beforeRecordSourceGetRecord(Request* request, const RecordSource* rsb);
	void afterRecordSourceGetRecord(Request* request, const RecordSource* rsb, Stats& stats);

	bool isActive() const
	{
		return currentSession && !paused;
	}

	static void checkFlushInterval(SLONG interval)
	{
		if (interval < 0)
		{
			Firebird::status_exception::raise(
				Firebird::Arg::Gds(isc_not_valid_for_var) <<
				"FLUSH_INTERVAL" <<
				Firebird::Arg::Num(interval));
		}
	}

private:
	void cancelSession();
	void finishSession(thread_db* tdbb, bool flushData);
	void pauseSession(bool flushData);
	void resumeSession();
	void setFlushInterval(SLONG interval);
	void discard();
	void flush(bool updateTimer = true);

	void updateFlushTimer(bool canStopTimer = true);

	Statement* getStatement(Request* request);
	SINT64 getRequest(Request* request, unsigned flags);

private:
	Firebird::AutoPtr<ProfilerListener> listener;
	Firebird::LeftPooledMap<Firebird::PathName, Firebird::AutoPlugin<Firebird::IProfilerPlugin>> activePlugins;
	Firebird::AutoPtr<Session> currentSession;
	Firebird::RefPtr<Firebird::TimerImpl> flushTimer;
	unsigned currentFlushInterval = 0;
	bool paused = false;
};


class ProfilerPackage final : public SystemPackage
{
	friend class ProfilerListener;
	friend class ProfilerManager;

public:
	ProfilerPackage(Firebird::MemoryPool& pool);

	ProfilerPackage(const ProfilerPackage&) = delete;
	ProfilerPackage& operator=(const ProfilerPackage&) = delete;

private:
	FB_MESSAGE(AttachmentIdMessage, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, attachmentId)
	);

	//----------

	using DiscardInput = AttachmentIdMessage;

	static Firebird::IExternalResultSet* discardProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const DiscardInput::Type* in, void* out);

	//----------

	using FlushInput = AttachmentIdMessage;

	static Firebird::IExternalResultSet* flushProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const FlushInput::Type* in, void* out);

	//----------

	using CancelSessionInput = AttachmentIdMessage;

	static Firebird::IExternalResultSet* cancelSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const CancelSessionInput::Type* in, void* out);

	//----------

	FB_MESSAGE(FinishSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, flush)
		(FB_BIGINT, attachmentId)
	);

	static Firebird::IExternalResultSet* finishSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const FinishSessionInput::Type* in, void* out);

	//----------

	FB_MESSAGE(PauseSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, flush)
		(FB_BIGINT, attachmentId)
	);

	static Firebird::IExternalResultSet* pauseSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const PauseSessionInput::Type* in, void* out);

	//----------

	using ResumeSessionInput = AttachmentIdMessage;

	static Firebird::IExternalResultSet* resumeSessionProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const ResumeSessionInput::Type* in, void* out);

	//----------

	FB_MESSAGE(SetFlushIntervalInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, flushInterval)
		(FB_BIGINT, attachmentId)
	);

	static Firebird::IExternalResultSet* setFlushIntervalProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const SetFlushIntervalInput::Type* in, void* out);

	//----------

	FB_MESSAGE(StartSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(255, CS_METADATA), description)
		(FB_INTEGER, flushInterval)
		(FB_BIGINT, attachmentId)
		(FB_INTL_VARCHAR(255, CS_METADATA), pluginName)
		(FB_INTL_VARCHAR(255, CS_METADATA), pluginOptions)
	);

	FB_MESSAGE(StartSessionOutput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, sessionId)
	);

	static void startSessionFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const StartSessionInput::Type* in, StartSessionOutput::Type* out);
};


}	// namespace

#endif	// JRD_PROFILER_MANAGER_H
