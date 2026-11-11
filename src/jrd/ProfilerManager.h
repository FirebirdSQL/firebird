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
#include <optional>
#include "../common/PerformanceStopWatch.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/TimerImpl.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/req.h"
#include "../jrd/ProfilerSampler.h"
#include "../jrd/SystemPackages.h"
#include "../jrd/Statement.h"

namespace Jrd {

class Attachment;
class Request;
class RecordSource;
class Select;
class thread_db;

class ProfilerListener;


// Exact hit counters of a request, used when timings are sampled.
// They are delivered to the plugin in batches instead of per event.
class ProfilerRequestProfile final
{
public:
	struct LineCounter
	{
		ULONG line = 0;
		ULONG column = 0;
		FB_UINT64 counter = 0;
	};

	struct RecSourceCounter
	{
		ULONG cursorId = 0;
		FB_UINT64 openCounter = 0;
		FB_UINT64 fetchCounter = 0;
	};

public:
	explicit ProfilerRequestProfile(MemoryPool& pool)
		: lines(pool),
		  recSources(pool)
	{
	}

	ProfilerRequestProfile(const ProfilerRequestProfile&) = delete;
	ProfilerRequestProfile& operator=(const ProfilerRequestProfile&) = delete;

public:
	// Prepares a new or recycled profile for a request, keeping the memory of the arrays.
	void reset(SINT64 aStatementId, SINT64 aRequestId)
	{
		statementId = aStatementId;
		requestId = aRequestId;
		lines.clear();
		recSources.clear();
	}

	void hitLine(ULONG slot, ULONG line, ULONG column)
	{
		if (slot >= lines.getCount())
			lines.grow(slot + 1);

		auto& lineCounter = lines[slot];

		// The position of a slot never changes, so it's stored by its first hit. A new entry is zeroed.
		if (!lineCounter.counter)
		{
			lineCounter.line = line;
			lineCounter.column = column;
		}

		++lineCounter.counter;
	}

	RecSourceCounter& getRecSource(ULONG recSourceId, ULONG cursorId)
	{
		if (recSourceId >= recSources.getCount())
			recSources.grow(recSourceId + 1);

		auto& recSourceCounter = recSources[recSourceId];
		recSourceCounter.cursorId = cursorId;
		return recSourceCounter;
	}

public:
	SINT64 statementId = 0;
	SINT64 requestId = 0;
	Firebird::Array<LineCounter> lines;				// indexed by StmtNode::profileSlot
	Firebird::Array<RecSourceCounter> recSources;	// indexed by record source id
	FB_SIZE_T activeIndex = 0;						// position in ProfilerManager::requestProfiles
};


class ProfilerManager final : public Firebird::PerformanceStopWatch
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

	// Publishes the frame of an executing request for the sampler, popping it when destroyed.
	class RequestFrame final
	{
	public:
		RequestFrame() = default;

		~RequestFrame()
		{
			pop();
		}

		RequestFrame(const RequestFrame&) = delete;
		RequestFrame& operator=(const RequestFrame&) = delete;

	public:
		// Does nothing when timings are not sampled.
		void push(ProfilerManager* profilerManager, Request* request)
		{
			fb_assert(!state);
			state = profilerManager->pushRequestFrame(request, depth);
		}

		void pop() noexcept
		{
			if (state)
			{
				state->popTo(depth);
				state = nullptr;
			}
		}

		bool isPushed() const noexcept
		{
			return state != nullptr;
		}

		ProfilerSampleState* getState() const noexcept
		{
			return state;
		}

		unsigned getIndex() const noexcept
		{
			return depth;
		}

	private:
		ProfilerSampleState* state = nullptr;
		unsigned depth = 0;
	};

	class RecordSourceStopWatcher final
	{
	public:
		enum class Event
		{
			OPEN,
			GET_RECORD
		};

	public:
		RecordSourceStopWatcher(thread_db* tdbb, const RecordSource* recordSource, Event aEvent)
			: RecordSourceStopWatcher(tdbb, getActiveForNonInternalStatement(tdbb), recordSource, aEvent)
		{
		}

		RecordSourceStopWatcher(thread_db* tdbb, ProfilerManager* aProfilerManager,
					const AccessPath* recordSource, Event aEvent)
			: request(tdbb->getRequest()),
			  profilerManager(aProfilerManager),
			  recordSource(recordSource),
			  event(aEvent)
		{
			if (profilerManager && profilerManager->sampled)
			{
				sampledProfilerManager = profilerManager;
				sampleState = profilerManager->sampleState;

				frameDepth = sampleState->push(profilerManager->makeFrame(request,
					recordSource->getRecSourceId(), recordSource->getCursorId(),
					(event == Event::OPEN ?
						ProfilerSampleState::FrameKind::OPEN :
						ProfilerSampleState::FrameKind::FETCH)));

				profilerManager = nullptr;
			}

			if (profilerManager)
			{
				if (profilerManager->currentSession->flags & Firebird::IProfilerSession::FLAG_BEFORE_EVENTS)
				{
					const SINT64 overheadStartTicks = profilerManager->queryTicks();

					if (event == Event::OPEN)
						profilerManager->beforeRecordSourceOpen(request, recordSource);
					else
						profilerManager->beforeRecordSourceGetRecord(request, recordSource);

					lastTicks = profilerManager->endOverhead(overheadStartTicks);
				}
				else
					lastTicks = profilerManager->beginInterval();

				lastAccumulatedOverhead = profilerManager->getAccumulatedOverhead();
			}
		}

		~RecordSourceStopWatcher()
		{
			if (sampleState)
			{
				sampleState->popTo(frameDepth);

				// Counted after the event, like the measured events, so a session paused or finished
				// during the event doesn't count it.
				if (const auto profile = sampledProfilerManager->isActive() ?
						sampledProfilerManager->getRequestProfile(request) : nullptr)
				{
					auto& counter = profile->getRecSource(recordSource->getRecSourceId(), recordSource->getCursorId());

					if (event == Event::OPEN)
						++counter.openCounter;
					else
						++counter.fetchCounter;
				}
			}
			else if (profilerManager && !profilerManager->sampled)	// a session started during the event may sample
			{
				const SINT64 currentTicks = profilerManager->queryTicks();
				const SINT64 elapsedTicks = profilerManager->getElapsedTicksAndAdjustOverhead(
					currentTicks, lastTicks, lastAccumulatedOverhead);
				Stats stats(elapsedTicks);

				if (event == Event::OPEN)
					profilerManager->afterRecordSourceOpen(request, recordSource, stats);
				else
					profilerManager->afterRecordSourceGetRecord(request, recordSource, stats);

				profilerManager->endOverhead(currentTicks);
			}
		}

	private:
		Request* request;
		ProfilerManager* profilerManager;
		const AccessPath* recordSource;
		ProfilerManager* sampledProfilerManager = nullptr;
		ProfilerSampleState* sampleState = nullptr;
		SINT64 lastTicks = 0;
		SINT64 lastAccumulatedOverhead = 0;
		unsigned frameDepth = 0;
		Event event;
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

		ULONG getRecSourceSequence(ULONG recSourceId) const
		{
			return recSourceId < recSourceSequence.getCount() ? recSourceSequence[recSourceId] : 0;
		}

		void setRecSourceSequence(ULONG recSourceId, ULONG sequence)
		{
			if (recSourceId >= recSourceSequence.getCount())
				recSourceSequence.grow(recSourceId + 1);

			recSourceSequence[recSourceId] = sequence;
		}

		Statement(const Statement&) = delete;
		void operator=(const Statement&) = delete;

		SINT64 id = 0;
		Firebird::NonPooledMap<ULONG, ULONG> cursorNextSequence;
		Firebird::SortedArray<ULONG> definedCursors;
		Firebird::Array<ULONG> recSourceSequence;	// indexed by record source id, 0 = not defined
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

	// Status of the plugin calls done for every request, which share ProfilerManager::requestCallStatus,
	// as constructing a LocalStatus for each of them is not cheap. Errors are logged, like LogLocalStatus.
	class RequestCallStatus final : public Firebird::LogWrapper
	{
	public:
		RequestCallStatus(ProfilerManager* manager, const char* text)
			: LogWrapper(&manager->requestCallStatus, text)
		{
		}

		~RequestCallStatus()
		{
			// Don't leave errors or warnings in the shared status.
			if (isDirty())
				status->init();
		}
	};

private:
	ProfilerManager(thread_db* tdbb);

public:
	~ProfilerManager();

public:
	static ProfilerManager* create(thread_db* tdbb);

	static ProfilerManager* getActiveForNonInternalStatement(thread_db* tdbb)
	{
		const auto attachment = tdbb->getAttachment();

		return attachment->isProfilerActive() && !tdbb->getRequest()->hasInternalStatement() ?
			attachment->att_profiler_manager.get() :
			nullptr;
	}

	static int blockingAst(void* astObject);

	ProfilerManager(const ProfilerManager&) = delete;
	void operator=(const ProfilerManager&) = delete;

public:
	SINT64 startSession(thread_db* tdbb, std::optional<SLONG> flushInterval,
		const Firebird::PathName& pluginName, const Firebird::string& description, const Firebird::string& options,
		std::optional<SLONG> samplingInterval);

	void prepareCursor(thread_db* tdbb, Request* request, const Select* select);
	void onRequestFinish(Request* request, Stats& stats);
	void releaseStaleRequestProfile(Request* request);

	// Timings are sampled by ProfilerSampler instead of measured for every event.
	bool isSampled() const
	{
		return sampled;
	}

	ProfilerRequestProfile* getRequestProfile(Request* request)
	{
		if (request->req_profiler_profile && request->req_profiler_generation == profileGeneration)
			return request->req_profiler_profile;

		return createRequestProfile(request);
	}

	// Publishes that the request is executing. Returns the state where the frame was pushed, or nullptr.
	// Used by RequestFrame, which restores the state with popTo(depth).
	// The request is not defined to the plugin here, but only with its first statement or record source,
	// as it's done when every event is measured.
	ProfilerSampleState* pushRequestFrame(Request* request, unsigned& depth)
	{
		if (!sampled)
			return nullptr;

		request->req_profiler_sampled = true;
		depth = sampleState->push(makeFrame(request, 0, 0, ProfilerSampleState::FrameKind::REQUEST));

		return sampleState;
	}

	ProfilerSampleState::Frame makeFrame(Request* request, ULONG a, ULONG b, ProfilerSampleState::FrameKind kind)
	{
		if (request->req_profiler_profile && request->req_profiler_generation == profileGeneration)
		{
			const auto profile = request->req_profiler_profile;
			return {profile->statementId, profile->requestId, a, b, kind};
		}

		return {(SINT64) request->getStatement()->getStatementId(), (SINT64) request->getRequestId(), a, b, kind};
	}

	void hitPsqlLine(Request* request, ProfilerSampleState* state, unsigned frameIndex,
		ULONG slot, ULONG line, ULONG column)
	{
		if (const auto profile = getRequestProfile(request))
			profile->hitLine(slot, line, column);

		state->setLine(frameIndex, line, column);
	}

	void beforePsqlLineColumn(Request* request, ULONG line, ULONG column)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_BEFORE_EVENTS))
		{
			const auto* profileStatement = getStatement(request);
			currentSession->pluginSession->beforePsqlLineColumn(profileStatement->id, profileRequestId, line, column);
		}
	}

	void afterPsqlLineColumn(Request* request, ULONG line, ULONG column, Stats& stats)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_AFTER_EVENTS))
		{
			const auto* profileStatement = getStatement(request);
			currentSession->pluginSession->afterPsqlLineColumn(profileStatement->id, profileRequestId,
				line, column, &stats);
		}
	}

	void beforeRecordSourceOpen(Request* request, const AccessPath* recordSource)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_BEFORE_EVENTS))
		{
			const auto* profileStatement = getStatement(request);

			if (const auto sequence = profileStatement->getRecSourceSequence(recordSource->getRecSourceId()))
			{
				currentSession->pluginSession->beforeRecordSourceOpen(
					profileStatement->id, profileRequestId, recordSource->getCursorId(), sequence);
			}
		}
	}

	void afterRecordSourceOpen(Request* request, const AccessPath* recordSource, Stats& stats)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_AFTER_EVENTS))
		{
			const auto* profileStatement = getStatement(request);

			if (const auto sequence = profileStatement->getRecSourceSequence(recordSource->getRecSourceId()))
			{
				currentSession->pluginSession->afterRecordSourceOpen(
					profileStatement->id, profileRequestId, recordSource->getCursorId(), sequence, &stats);
			}
		}
	}

	void beforeRecordSourceGetRecord(Request* request, const AccessPath* recordSource)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_BEFORE_EVENTS))
		{
			const auto* profileStatement = getStatement(request);

			if (const auto sequence = profileStatement->getRecSourceSequence(recordSource->getRecSourceId()))
			{
				currentSession->pluginSession->beforeRecordSourceGetRecord(
					profileStatement->id, profileRequestId, recordSource->getCursorId(), sequence);
			}
		}
	}

	void afterRecordSourceGetRecord(Request* request, const AccessPath* recordSource, Stats& stats)
	{
		if (const auto profileRequestId = getRequest(request, Firebird::IProfilerSession::FLAG_AFTER_EVENTS))
		{
			const auto* profileStatement = getStatement(request);

			if (const auto sequence = profileStatement->getRecSourceSequence(recordSource->getRecSourceId()))
			{
				currentSession->pluginSession->afterRecordSourceGetRecord(
					profileStatement->id, profileRequestId, recordSource->getCursorId(), sequence, &stats);
			}
		}
	}

	bool isActive() const
	{
		return currentSession && !paused;
	}

	bool haveListener() const
	{
		return listener.hasData();
	}

	static constexpr SLONG DEFAULT_SAMPLING_INTERVAL = 100;		// microseconds
	static constexpr SLONG MIN_SAMPLING_INTERVAL = 50;			// microseconds
	static constexpr SLONG MAX_SAMPLING_INTERVAL = 1'000'000;	// microseconds

	// 0 means every event is measured.
	static void checkSamplingInterval(SLONG interval)
	{
		if (interval != 0 && (interval < MIN_SAMPLING_INTERVAL || interval > MAX_SAMPLING_INTERVAL))
		{
			Firebird::status_exception::raise(
				Firebird::Arg::Gds(isc_not_valid_for_var) <<
				"\"SAMPLING_INTERVAL\"" <<
				Firebird::Arg::Num(interval));
		}
	}

	static void checkFlushInterval(SLONG interval)
	{
		if (interval < 0)
		{
			Firebird::status_exception::raise(
				Firebird::Arg::Gds(isc_not_valid_for_var) <<
				"\"FLUSH_INTERVAL\"" <<
				Firebird::Arg::Num(interval));
		}
	}

private:
	void prepareRecSource(thread_db* tdbb, Request* request, const AccessPath* recordSource);

	ProfilerRequestProfile* createRequestProfile(Request* request);
	void releaseRequestProfile(ProfilerRequestProfile* profile);
	void deliverProfiles(ProfilerRequestProfile* onlyProfile);
	void deliverRecSourceStats(SINT64 statementId, SINT64 requestId, ULONG cursorId, ULONG recSourceId,
		bool open, FB_UINT64 counter, FB_UINT64 elapsedTicks);
	void startSampling(ULONG interval);
	void stopSampling();

	void resetCache()
	{
		lastRequest = nullptr;
		lastRequestId = 0;
		lastStatement = nullptr;
		lastStatementId = 0;
	}

	void setPaused(bool value)
	{
		paused = value;
		updateActive();
	}

	void updateActive();

	void cancelSession();
	void finishSession(thread_db* tdbb, bool flushData);
	void pauseSession(bool flushData);
	void resumeSession();
	void setFlushInterval(SLONG interval);
	void discard();
	void flush(bool updateTimer = true);

	void updateFlushTimer(bool canStopTimer = true);

	Statement* getStatement(Request* request)
	{
		if (!isActive())
			return nullptr;

		if (lastStatement && request->getStatement()->getStatementId() == lastStatementId)
			return lastStatement;

		return lookupStatement(request);
	}

	Statement* lookupStatement(Request* request);

	SINT64 getRequest(Request* request, unsigned flags)
	{
		using namespace Firebird;

		if (!isActive() || (flags && !(currentSession->flags & flags)))
			return 0;

		const auto mainRequest = request;
		const auto mainRequestId = request->getRequestId();

		if (request == lastRequest && mainRequestId == lastRequestId)
			return mainRequestId;

		if (!currentSession->requests.exist(mainRequestId))
		{
			const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(request->req_attachment->att_current_timezone);

			do
			{
				getStatement(request);  // define the statement and ignore the result

				const StmtNumber callerStatementId = request->req_caller ?
					request->req_caller->getStatement()->getStatementId() : 0;
				const StmtNumber callerRequestId = request->req_caller ? request->req_caller->getRequestId() : 0;

				RequestCallStatus status(this, "Profiler onRequestStart");
				currentSession->pluginSession->onRequestStart(&status,
					(SINT64) request->getStatement()->getStatementId(), (SINT64) request->getRequestId(),
					(SINT64) callerStatementId, (SINT64) callerRequestId, timestamp);

				currentSession->requests.add(request->getRequestId());

				request = request->req_caller;
			} while (request && !currentSession->requests.exist(request->getRequestId()));
		}

		lastRequest = mainRequest;
		lastRequestId = mainRequestId;

		return mainRequestId;
	}

private:
	Attachment* const attachment;
	Firebird::AutoPtr<ProfilerListener> listener;
	Firebird::LeftPooledMap<Firebird::PathName, Firebird::AutoPlugin<Firebird::IProfilerPlugin>> activePlugins;
	Firebird::AutoPtr<Session> currentSession;
	Firebird::LocalStatus requestCallStatus;	// see RequestCallStatus
	Firebird::RefPtr<Firebird::TimerImpl> flushTimer;
	unsigned currentFlushInterval = 0;
	bool paused = false;
	bool sampled = false;

	// Sampling state. sampleState lives as long as the manager, so frames pushed in one session
	// are popped correctly even when the session changes in the middle of a request.
	Firebird::RefPtr<ProfilerSampleState> sampleState;
	// Profiles of unfinished requests and finished ones kept for reuse, so requests don't allocate them.
	// Both own their profiles.
	Firebird::Array<ProfilerRequestProfile*> requestProfiles;
	Firebird::Array<ProfilerRequestProfile*> freeRequestProfiles;
	Firebird::NonPooledMap<SINT64, FB_UINT64> requestSampledTicks;	// sampled totals of unfinished requests
	ULONG profileGeneration;	// changed when requestProfiles is cleared, unique in the process

	// Last request known to the current session, to avoid repeated lookups on every event.
	Request* lastRequest = nullptr;
	StmtNumber lastRequestId = 0;

	// Last statement looked up in the current session. It's keyed by the statement id instead of by
	// lastRequest, as sampled timings don't call getRequest for every event, so lastRequest may be
	// another request, like the caller of a procedure that fetches from a cursor.
	Statement* lastStatement = nullptr;
	StmtNumber lastStatementId = 0;
};


class ProfilerPackage final : public SystemPackage
{
	friend class ProfilerListener;
	friend class ProfilerManager;

public:
	ProfilerPackage(Firebird::MemoryPool& pool);

	ProfilerPackage(const ProfilerPackage&) = delete;
	ProfilerPackage& operator=(const ProfilerPackage&) = delete;

public:
	FB_MESSAGE(DiscardInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, attachmentId)
	);

	static Firebird::IExternalResultSet* discardProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const DiscardInput::Type* in, void* out);

	//----------

	FB_MESSAGE(FlushInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, attachmentId)
	);

	static Firebird::IExternalResultSet* flushProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const FlushInput::Type* in, void* out);

	//----------

	FB_MESSAGE(CancelSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, attachmentId)
	);

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

	FB_MESSAGE(ResumeSessionInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, attachmentId)
	);

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
		(FB_INTL_VARCHAR(255 * METADATA_BYTES_PER_CHAR, CS_METADATA), description)
		(FB_INTEGER, flushInterval)
		(FB_BIGINT, attachmentId)
		(FB_INTL_VARCHAR(255 * METADATA_BYTES_PER_CHAR, CS_METADATA), pluginName)
		(FB_INTL_VARCHAR(255 * METADATA_BYTES_PER_CHAR, CS_METADATA), pluginOptions)
		(FB_INTEGER, samplingInterval)
	);

	FB_MESSAGE(StartSessionOutput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BIGINT, sessionId)
	);

	static void startSessionFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const StartSessionInput::Type* in, StartSessionOutput::Type* out);
};


}	// namespace

#endif	// JRD_PROFILER_MANAGER_H
