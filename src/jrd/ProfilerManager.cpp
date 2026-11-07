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

#include "firebird.h"
#include "../jrd/ProfilerManager.h"
#include "../common/ipc/IpcChat.h"
#include "../common/ipc/IpcMessage.h"
#include "../jrd/Record.h"
#include "../jrd/ini.h"
#include "../jrd/tra.h"
#include "../jrd/ids.h"
#include "../jrd/recsrc/Cursor.h"
#include "../dsql/BoolNodes.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/lck.h"
#include "../jrd/met_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/Statement.h"
#include <variant>

using namespace Jrd;
using namespace Firebird;


//--------------------------------------


namespace
{
	struct CheckUserRequest
	{
		char userName[USERNAME_LENGTH + 1];
	};

	struct Nothing {};

	struct ExceptionResponse
	{
		char text[4096];
	};

	using IpcRequestMessage = std::variant<
		CheckUserRequest,
		ProfilerPackage::DiscardInput::Type,
		ProfilerPackage::FlushInput::Type,
		ProfilerPackage::CancelSessionInput::Type,
		ProfilerPackage::PauseSessionInput::Type,
		ProfilerPackage::ResumeSessionInput::Type,
		ProfilerPackage::FinishSessionInput::Type,
		ProfilerPackage::SetFlushIntervalInput::Type,
		ProfilerPackage::StartSessionInput::Type
	>;

	using IpcResponseMessage = std::variant<
		Nothing,
		ExceptionResponse,
		ProfilerPackage::StartSessionOutput::Type
	>;

	IpcMessageParameters buildParameters(thread_db* tdbb, AttNumber attachmentId)
	{
		static_assert(std::is_same<decltype(attachmentId), FB_UINT64>::value);

		static constexpr USHORT VERSION = 4;

		const auto database = tdbb->getDatabase();

		PathName fileName;
		fileName.printf(PROFILER_FILE, database->getUniqueFileId().c_str(), attachmentId);

		return {
			.physicalName = fileName.c_str(),
			.logicalName = "ProfilerManager",
			.type = static_cast<USHORT>(SharedMemoryBase::SRAM_PROFILER),
			.version = VERSION,
		};
	}

	void startRemoteProfiler(thread_db* tdbb, AttNumber attachmentId)
	{
		ThreadStatusGuard tempStatus(tdbb);

		Lock tempLock(tdbb, sizeof(SINT64), LCK_attachment);
		tempLock.setKey(attachmentId);

		// Check if attachment is alive.
		if (LCK_lock(tdbb, &tempLock, LCK_EX, LCK_NO_WAIT))
		{
			LCK_release(tdbb, &tempLock);
			(Arg::Gds(isc_random) << "Cannot start remote profile session - attachment is not active").raise();
		}

		// Ask remote attachment to initialize the profile listener.

		tempLock.lck_type = LCK_profiler_listener;

		if (LCK_lock(tdbb, &tempLock, LCK_SR, LCK_WAIT))
			LCK_release(tdbb, &tempLock);
	}

	// Generations identify the ProfilerRequestProfile pointers stored in requests. They are unique in the
	// process, so a pointer left in a request by a previous session or manager is never taken as valid.
	ULONG newProfileGeneration()
	{
		// Process-wide: a manager can be recreated while requests still hold profile pointers of the old one.
		static std::atomic<ULONG> lastGeneration = 0;
		ULONG generation;

		while ((generation = ++lastGeneration) == 0)
			;

		return generation;
	}

	const auto& checkResponseIsNotException(const std::optional<IpcResponseMessage>& responseMessageOpt)
	{
		if (responseMessageOpt.has_value())
		{
			if (const auto exceptionResponse = std::get_if<ExceptionResponse>(&responseMessageOpt.value()))
				(Arg::Gds(isc_random) << exceptionResponse->text).raise();
		}

		return responseMessageOpt;
	}

	const auto& checkResponseIsPresent(const std::optional<IpcResponseMessage>& responseMessageOpt)
	{
		if (!responseMessageOpt.has_value())
			(Arg::Gds(isc_random) << "Profiler client disconnected from server").raise();;

		return responseMessageOpt;
	}

	void checkResponseIsNothing(const std::optional<IpcResponseMessage>& responseMessageOpt)
	{
		checkResponseIsPresent(responseMessageOpt);

		if (!std::holds_alternative<Nothing>(responseMessageOpt.value()))
			(Arg::Gds(isc_random) << "Invalid profiler's remote response").raise();
	}

	template <typename T>
	std::optional<IpcResponseMessage> clientSendAndReceiveMessage(thread_db* tdbb, AttNumber attachmentId, const T& in)
	{
		const auto attachment = tdbb->getAttachment();
		std::optional<MetaString> userName;

		if (!attachment->locksmith(tdbb, PROFILE_ANY_ATTACHMENT))
			userName = attachment->getUserName();

		startRemoteProfiler(tdbb, attachmentId);

		EngineCheckout cout(tdbb, FB_FUNCTION);

		IpcChatClient<IpcRequestMessage, IpcResponseMessage> chatClient(buildParameters(tdbb, attachmentId));

		const auto udleFunc = [&] {
			Attachment::SyncGuard attGuard(attachment, FB_FUNCTION);
			JRD_reschedule(tdbb, true);
		};

		if (userName.has_value())
		{
			CheckUserRequest checkUserRequest;
			strcpy(checkUserRequest.userName, userName->c_str());
			checkResponseIsNotException(chatClient.sendAndReceive(checkUserRequest, udleFunc));
		}

		return checkResponseIsNotException(chatClient.sendAndReceive(in, udleFunc));
	}
}


class Jrd::ProfilerListener final
{
public:
	explicit ProfilerListener(thread_db* tdbb);
	~ProfilerListener();

	ProfilerListener(const ProfilerListener&) = delete;
	ProfilerListener& operator=(const ProfilerListener&) = delete;

public:
	void exceptionHandler(const Firebird::Exception& ex, ThreadFinishSync<ProfilerListener*>::ThreadRoutine* routine);

private:
	void watcherThread();

	static void watcherThread(ProfilerListener* listener)
	{
		listener->watcherThread();
	}

	IpcResponseMessage processCommand(thread_db* tdbb, const IpcRequestMessage& requestMessage);

private:
	Attachment* const attachment;
	IpcChatServer<IpcRequestMessage, IpcResponseMessage> chatServer;
	ThreadFinishSync<ProfilerListener*> cleanupSync;
};


//--------------------------------------


IExternalResultSet* ProfilerPackage::discardProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const DiscardInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->discard();

	return nullptr;
}

IExternalResultSet* ProfilerPackage::flushProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const FlushInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->flush();

	return nullptr;
}

IExternalResultSet* ProfilerPackage::cancelSessionProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const CancelSessionInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->cancelSession();

	return nullptr;
}

IExternalResultSet* ProfilerPackage::finishSessionProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const FinishSessionInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->finishSession(tdbb, in->flush);

	return nullptr;
}

IExternalResultSet* ProfilerPackage::pauseSessionProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const PauseSessionInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->pauseSession(in->flush);

	return nullptr;
}

IExternalResultSet* ProfilerPackage::resumeSessionProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const ResumeSessionInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->resumeSession();

	return nullptr;
}

IExternalResultSet* ProfilerPackage::setFlushIntervalProcedure(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const SetFlushIntervalInput::Type* in, void* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		checkResponseIsNothing(clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));
		return nullptr;
	}

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	profilerManager->setFlushInterval(in->flushInterval);

	return nullptr;
}

void ProfilerPackage::startSessionFunction(ThrowStatusExceptionWrapper* /*status*/,
	IExternalContext* context, const StartSessionInput::Type* in, StartSessionOutput::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (!in->attachmentIdNull && AttNumber(in->attachmentId) != attachment->att_attachment_id)
	{
		const auto responseMessageOpt = checkResponseIsPresent(
			clientSendAndReceiveMessage(tdbb, AttNumber(in->attachmentId), *in));

		if (std::holds_alternative<StartSessionOutput::Type>(responseMessageOpt.value()))
			*out = std::get<StartSessionOutput::Type>(responseMessageOpt.value());
		else
		{
			fb_assert(false);
			out->sessionIdNull = FB_TRUE;
		}

		return;
	}

	const string description(in->description.str, in->descriptionNull ? 0 : in->description.length);
	const std::optional<SLONG> flushInterval(in->flushIntervalNull ?
		std::nullopt : std::optional{in->flushInterval});
	const PathName pluginName(in->pluginName.str, in->pluginNameNull ? 0 : in->pluginName.length);
	const string pluginOptions(in->pluginOptions.str, in->pluginOptionsNull ? 0 : in->pluginOptions.length);
	const std::optional<SLONG> samplingInterval(in->samplingIntervalNull ?
		std::nullopt : std::optional{in->samplingInterval});

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	out->sessionIdNull = FB_FALSE;
	out->sessionId = profilerManager->startSession(tdbb, flushInterval, pluginName, description, pluginOptions,
		samplingInterval);
}


//--------------------------------------


ProfilerManager::ProfilerManager(thread_db* tdbb)
	: attachment(tdbb->getAttachment()),
	  activePlugins(*attachment->att_pool),
	  requestCallStatus(*attachment->att_pool),
	  requestProfiles(*attachment->att_pool),
	  freeRequestProfiles(*attachment->att_pool),
	  requestSampledTicks(*attachment->att_pool),
	  profileGeneration(newProfileGeneration())
{
	flushTimer = FB_NEW TimerImpl();

	flushTimer->setOnTimer([this](auto) {
		FbLocalStatus statusVector;
		EngineContextHolder innerTdbb(&statusVector, attachment->getInterface(), FB_FUNCTION);

		flush(false);
		updateFlushTimer(false);
	});
}

ProfilerManager::~ProfilerManager()
{
	attachment->att_profiler_active = false;
	flushTimer->stop();
	stopSampling();

	for (const auto profile : freeRequestProfiles)
		delete profile;
}

ProfilerManager* ProfilerManager::create(thread_db* tdbb)
{
	return FB_NEW_POOL(*tdbb->getAttachment()->att_pool) ProfilerManager(tdbb);
}

int ProfilerManager::blockingAst(void* astObject)
{
	const auto attachment = static_cast<Attachment*>(astObject);

	try
	{
		const auto dbb = attachment->att_database;
		AsyncContextHolder tdbb(dbb, FB_FUNCTION, attachment->att_profiler_listener_lock);

		if (!(attachment->att_flags & ATT_shutdown))
		{
			const auto profilerManager = attachment->getProfilerManager(tdbb);

			if (!profilerManager->listener)
				profilerManager->listener = FB_NEW_POOL(*attachment->att_pool) ProfilerListener(tdbb);
		}

		LCK_release(tdbb, attachment->att_profiler_listener_lock);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

SINT64 ProfilerManager::startSession(thread_db* tdbb, std::optional<SLONG> flushInterval,
	const PathName& pluginName, const string& description, const string& options,
	std::optional<SLONG> samplingInterval)
{
	if (flushInterval.has_value())
		checkFlushInterval(flushInterval.value());

	if (samplingInterval.has_value())
		checkSamplingInterval(samplingInterval.value());

	const bool oldPaused = paused;
	setPaused(true);
	Cleanup restorePaused([&] { setPaused(oldPaused); });

	resetCache();

	ThrowLocalStatus status;

	const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(attachment->att_current_timezone);

	if (currentSession)
	{
		if (sampled)
			deliverProfiles(nullptr);

		stopSampling();

		currentSession->pluginSession->finish(&status, timestamp);
		currentSession = nullptr;
	}

	auto pluginPtr = activePlugins.get(pluginName);

	AutoPlugin<IProfilerPlugin> plugin;

	if (pluginPtr)
	{
		(*pluginPtr)->addRef();
		plugin.reset(pluginPtr->get());
	}
	else
	{
		GetPlugins<IProfilerPlugin> plugins(IPluginManager::TYPE_PROFILER, pluginName.nullStr());

		if (!plugins.hasData())
		{
			string msg;
			msg.printf("Profiler plugin %s is not found", pluginName.c_str());
			(Arg::Gds(isc_random) << msg).raise();
		}

		plugin.reset(plugins.plugin());
		plugin->addRef();

		plugin->init(&status, attachment->getInterface(), (FB_UINT64) fb_utils::query_performance_frequency());

		plugin->addRef();
		activePlugins.put(pluginName)->reset(plugin.get());
	}

	AutoDispose<IProfilerSession> pluginSession = plugin->startSession(&status,
		description.c_str(),
		options.c_str(),
		timestamp);

	auto& pool = *tdbb->getAttachment()->att_pool;

	currentSession.reset(FB_NEW_POOL(pool) ProfilerManager::Session(pool));
	currentSession->pluginSession = std::move(pluginSession);
	currentSession->plugin = std::move(plugin);
	currentSession->flags = currentSession->pluginSession->getFlags();

	// Plugins that don't know the aggregated stats methods get every event measured.
	if (currentSession->pluginSession->cloopVTable->version >= IProfilerSession::VERSION)
	{
		const ULONG interval = (ULONG) samplingInterval.value_or(DEFAULT_SAMPLING_INTERVAL);

		currentSession->pluginSession->setSamplingInterval(interval);

		if (interval)
			startSampling(interval);
	}

	setPaused(false);

	if (flushInterval.has_value())
		setFlushInterval(flushInterval.value());

	return currentSession->pluginSession->getId();
}

void ProfilerManager::prepareCursor(thread_db* tdbb, Request* request, const Select* select)
{
	auto profileStatement = getStatement(request);

	if (!profileStatement)
		return;

	// Fast path for every fetch after the first: a defined record source implies a defined cursor.
	if (profileStatement->getRecSourceSequence(select->getRecSourceId()))
		return;

	const SINT64 overheadStartTicks = queryTicks();
	Cleanup endOverheadCleanup([&] { endOverhead(overheadStartTicks); });

	const auto cursorId = select->getCursorId();

	if (!profileStatement->definedCursors.exist(cursorId))
	{
		currentSession->pluginSession->defineCursor(profileStatement->id, cursorId,
			select->getName().nullStr(), select->getLine(), select->getColumn());

		profileStatement->definedCursors.add(cursorId);
	}

	prepareRecSource(tdbb, request, select);
}

void ProfilerManager::prepareRecSource(thread_db* tdbb, Request* request, const AccessPath* recordSource)
{
	auto profileStatement = getStatement(request);

	if (!profileStatement)
		return;

	if (profileStatement->getRecSourceSequence(recordSource->getRecSourceId()))
		return;

	fb_assert(profileStatement->definedCursors.exist(recordSource->getCursorId()));

	PlanEntry rootEntry;
	recordSource->getPlan(tdbb, rootEntry, 0, true);

	Array<NonPooledPair<const PlanEntry*, const PlanEntry*>> flatPlan;
	rootEntry.asFlatList(flatPlan);

	NonPooledMap<ULONG, ULONG> idSequenceMap;
	auto sequencePtr = profileStatement->cursorNextSequence.getOrPut(recordSource->getCursorId());

	for (const auto& [planEntry, parentPlanEntry] : flatPlan)
	{
		const auto cursorId = planEntry->accessPath->getCursorId();
		const auto recSourceId = planEntry->accessPath->getRecSourceId();
		idSequenceMap.put(recSourceId, ++*sequencePtr);

		ULONG parentSequence = 0;

		if (parentPlanEntry)
			parentSequence = *idSequenceMap.get(parentPlanEntry->accessPath->getRecSourceId());

		string accessPath;
		planEntry->getDescriptionAsString(accessPath);

		currentSession->pluginSession->defineRecordSource(profileStatement->id, cursorId,
			*sequencePtr, planEntry->level, accessPath.c_str(), parentSequence);

		profileStatement->setRecSourceSequence(recSourceId, *sequencePtr);
	}
}

void ProfilerManager::onRequestFinish(Request* request, Stats& stats)
{
	// The overhead is subtracted only from measured timings, so it's not accounted when sampling.
	const bool accountOverhead = !sampled;
	const SINT64 overheadStartTicks = accountOverhead ? queryTicks() : 0;

	Cleanup endOverheadCleanup([&] {
		if (accountOverhead)
			endOverhead(overheadStartTicks);
	});

	if (const auto profileRequestId = getRequest(request, 0))
	{
		const auto* profileStatement = getStatement(request);
		const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(request->req_attachment->att_current_timezone);

		RequestCallStatus status(this, "Profiler onRequestFinish");

		if (const auto profile = sampled ? getRequestProfile(request) : nullptr)
		{
			deliverProfiles(profile);

			FB_UINT64 sampledTicks = 0;

			if (const auto ticksPtr = requestSampledTicks.get(profileRequestId))
			{
				sampledTicks = *ticksPtr;
				requestSampledTicks.remove(profileRequestId);
			}

			Stats sampledStats(sampledTicks);
			currentSession->pluginSession->onRequestFinish(&status, profileStatement->id, profileRequestId,
				timestamp, &sampledStats);

			request->req_profiler_profile = nullptr;
			request->req_profiler_sampled = false;
			releaseRequestProfile(profile);
		}
		else
		{
			currentSession->pluginSession->onRequestFinish(&status, profileStatement->id, profileRequestId,
				timestamp, &stats);
		}

		currentSession->requests.findAndRemove(profileRequestId);
		resetCache();
	}
}

// Releases the profile of a request that finished without onRequestFinish, as when its session was paused,
// before the request gets a new id. Its pending counters are delivered with the previous id.
void ProfilerManager::releaseStaleRequestProfile(Request* request)
{
	const auto profile = request->req_profiler_profile;

	request->req_profiler_profile = nullptr;
	request->req_profiler_sampled = false;

	if (request->req_profiler_generation != profileGeneration)
		return;

	if (sampled && currentSession)
	{
		deliverProfiles(profile);
		requestSampledTicks.remove(profile->requestId);
	}

	releaseRequestProfile(profile);
}

ProfilerRequestProfile* ProfilerManager::createRequestProfile(Request* request)
{
	if (!sampled)
		return nullptr;

	const auto profileRequestId = getRequest(request, 0);

	if (!profileRequestId)
		return nullptr;

	const auto profileStatement = getStatement(request);

	if (!profileStatement)
		return nullptr;

	// A request with a profile of the current generation doesn't get here, and profiles of finished requests
	// and of other generations are not active, so the request has no active profile to look up.
	const auto profile = freeRequestProfiles.hasData() ?
		freeRequestProfiles.pop() :
		FB_NEW_POOL(*attachment->att_pool) ProfilerRequestProfile(*attachment->att_pool);

	profile->reset(profileStatement->id, profileRequestId);
	profile->activeIndex = requestProfiles.add(profile);

	request->req_profiler_profile = profile;
	request->req_profiler_generation = profileGeneration;

	return profile;
}

void ProfilerManager::releaseRequestProfile(ProfilerRequestProfile* profile)
{
	fb_assert(requestProfiles[profile->activeIndex] == profile);

	const auto last = requestProfiles.pop();

	if (last != profile)
	{
		requestProfiles[profile->activeIndex] = last;
		last->activeIndex = profile->activeIndex;
	}

	freeRequestProfiles.add(profile);
}

// Delivers the counters of one or all request profiles to the plugin, together with the times sampled
// since the last call. Sampled request totals are accumulated in requestSampledTicks until the request finishes.
void ProfilerManager::deliverProfiles(ProfilerRequestProfile* onlyProfile)
{
	using Key = ProfilerSampleState::Key;
	using KeyKind = ProfilerSampleState::KeyKind;

	ProfilerSampleState::SampleMap samples(*getDefaultMemoryPool());

	// A finished request has its frames already popped, and requests usually finish much more often than
	// samples are taken, so the samples are taken only when the sampler may have added something.
	if (!onlyProfile || sampleState->mayHaveSamples())
		sampleState->takeSamples(samples);

	const auto takeTicks = [&](const Key& key) -> FB_UINT64
	{
		if (const auto ticksPtr = samples.get(key))
		{
			const FB_UINT64 ticks = *ticksPtr;
			samples.remove(key);
			return ticks;
		}

		return 0;
	};

	const auto deliverCounters = [&](ProfilerRequestProfile& profile)
	{
		const auto statementId = profile.statementId;
		const auto requestId = profile.requestId;

		for (auto& lineCounter : profile.lines)
		{
			if (!lineCounter.counter)
				continue;

			const auto ticks = takeTicks({statementId, requestId, lineCounter.line, lineCounter.column, KeyKind::LINE});

			currentSession->pluginSession->addPsqlLineColumnStats(statementId, requestId,
				lineCounter.line, lineCounter.column, lineCounter.counter, ticks);

			lineCounter.counter = 0;
		}

		for (ULONG recSourceId = 0; recSourceId < profile.recSources.getCount(); ++recSourceId)
		{
			auto& recSourceCounter = profile.recSources[recSourceId];

			if (recSourceCounter.openCounter)
			{
				const auto ticks = takeTicks(
					{statementId, requestId, recSourceId, recSourceCounter.cursorId, KeyKind::OPEN});

				deliverRecSourceStats(statementId, requestId, recSourceCounter.cursorId, recSourceId,
					true, recSourceCounter.openCounter, ticks);

				recSourceCounter.openCounter = 0;
			}

			if (recSourceCounter.fetchCounter)
			{
				const auto ticks = takeTicks(
					{statementId, requestId, recSourceId, recSourceCounter.cursorId, KeyKind::FETCH});

				deliverRecSourceStats(statementId, requestId, recSourceCounter.cursorId, recSourceId,
					false, recSourceCounter.fetchCounter, ticks);

				recSourceCounter.fetchCounter = 0;
			}
		}
	};

	if (onlyProfile)
		deliverCounters(*onlyProfile);
	else
	{
		for (const auto profile : requestProfiles)
			deliverCounters(*profile);
	}

	// Times of keys without new hits.
	for (const auto& [key, ticks] : samples)
	{
		switch (key.kind)
		{
			case KeyKind::REQUEST:
				*requestSampledTicks.getOrPut(key.requestId) += ticks;
				break;

			case KeyKind::LINE:
				currentSession->pluginSession->addPsqlLineColumnStats(key.statementId, key.requestId,
					key.a, key.b, 0, ticks);
				break;

			case KeyKind::OPEN:
			case KeyKind::FETCH:
				deliverRecSourceStats(key.statementId, key.requestId, key.b, key.a,
					key.kind == KeyKind::OPEN, 0, ticks);
				break;
		}
	}
}

void ProfilerManager::deliverRecSourceStats(SINT64 statementId, SINT64 requestId, ULONG cursorId, ULONG recSourceId,
	bool open, FB_UINT64 counter, FB_UINT64 elapsedTicks)
{
	const auto profileStatement = currentSession->statements.get(StmtNumber(statementId));

	if (!profileStatement)
		return;

	// Record sources not yet defined to the plugin are not reported, like in the measured mode.
	if (const auto sequence = profileStatement->getRecSourceSequence(recSourceId))
	{
		if (open)
		{
			currentSession->pluginSession->addRecordSourceOpenStats(statementId, requestId,
				cursorId, sequence, counter, elapsedTicks);
		}
		else
		{
			currentSession->pluginSession->addRecordSourceGetRecordStats(statementId, requestId,
				cursorId, sequence, counter, elapsedTicks);
		}
	}
}

void ProfilerManager::startSampling(ULONG interval)
{
	if (!sampleState)
		sampleState = FB_NEW ProfilerSampleState();

	sampleState->setInterval(interval);
	sampled = true;
	ProfilerSampler::add(sampleState);
	updateActive();
}

void ProfilerManager::stopSampling()
{
	if (sampled)
	{
		ProfilerSampler::remove(sampleState);
		sampled = false;
	}

	if (sampleState)
	{
		sampleState->setActive(false);
		sampleState->clearSamples();
	}

	// Their counters may be pending, but they are discarded with the generation and reset when reused.
	for (const auto profile : requestProfiles)
		freeRequestProfiles.add(profile);

	requestProfiles.clear();
	requestSampledTicks.clear();
	profileGeneration = newProfileGeneration();
}

void ProfilerManager::cancelSession()
{
	resetCache();

	if (currentSession)
	{
		stopSampling();

		LogLocalStatus status("Profiler cancelSession");

		currentSession->pluginSession->cancel(&status);
		currentSession = nullptr;
		updateActive();
	}
}

void ProfilerManager::finishSession(thread_db* tdbb, bool flushData)
{
	resetCache();

	if (currentSession)
	{
		if (sampled)
			deliverProfiles(nullptr);

		stopSampling();

		const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(tdbb->getAttachment()->att_current_timezone);
		LogLocalStatus status("Profiler finish");

		currentSession->pluginSession->finish(&status, timestamp);
		currentSession = nullptr;
		updateActive();
	}

	if (flushData)
		flush();
}

void ProfilerManager::pauseSession(bool flushData)
{
	if (currentSession)
		setPaused(true);

	if (flushData)
		flush();
}

void ProfilerManager::resumeSession()
{
	if (currentSession)
	{
		setPaused(false);
		updateFlushTimer();
	}
}

void ProfilerManager::setFlushInterval(SLONG interval)
{
	checkFlushInterval(interval);

	currentFlushInterval = (unsigned) interval;

	updateFlushTimer();
}

void ProfilerManager::discard()
{
	resetCache();
	stopSampling();

	currentSession = nullptr;
	updateActive();
	activePlugins.clear();
}

void ProfilerManager::flush(bool updateTimer)
{
	if (sampled && currentSession)
		deliverProfiles(nullptr);

	{	// scope
		const bool oldPaused = paused;
		setPaused(true);
		Cleanup restorePaused([&] { setPaused(oldPaused); });

		auto pluginAccessor = activePlugins.accessor();

		for (bool hasNext = pluginAccessor.getFirst(); hasNext;)
		{
			auto& [pluginName, plugin] = *pluginAccessor.current();

			LogLocalStatus status("Profiler flush");
			plugin->flush(&status);

			hasNext = pluginAccessor.getNext();

			if (!currentSession || plugin.get() != currentSession->plugin.get())
				activePlugins.remove(pluginName);
		}
	}

	if (updateTimer)
		updateFlushTimer();
}

void ProfilerManager::updateActive()
{
	attachment->att_profiler_active = isActive();

	if (sampleState)
		sampleState->setActive(sampled && isActive());
}

void ProfilerManager::updateFlushTimer(bool canStopTimer)
{
	if (currentSession && !paused && currentFlushInterval)
		flushTimer->reset(currentFlushInterval);
	else if (canStopTimer)
		flushTimer->stop();
}

ProfilerManager::Statement* ProfilerManager::lookupStatement(Request* request)
{
	const StmtNumber mainStatementId = request->getStatement()->getStatementId();
	auto mainProfileStatement = currentSession->statements.get(mainStatementId);

	if (mainProfileStatement)
	{
		lastStatement = mainProfileStatement;
		lastStatementId = mainStatementId;

		return mainProfileStatement;
	}

	lastStatement = nullptr;	// the map is about to change

	for (const auto* statement = request->getStatement();
		 statement && !currentSession->statements.exist(statement->getStatementId());
		 statement = statement->parentStatement)
	{
		QualifiedName name;
		const char* type;

		if (const auto routine = statement->getRoutine())
		{
			if (statement->procedure)
				type = "PROCEDURE";
			else if (statement->function)
				type = "FUNCTION";
			else
				fb_assert(false);

			name = routine->getName();
		}
		else if (statement->triggerName.object.hasData())
		{
			type = "TRIGGER";
			name = statement->triggerName;
		}
		else
			type = "BLOCK";

		const StmtNumber parentStatementId = statement->parentStatement ?
			statement->parentStatement->getStatementId() : 0;

		LogLocalStatus status("Profiler defineStatement2");
		currentSession->pluginSession->defineStatement2(&status,
			(SINT64) statement->getStatementId(), (SINT64) parentStatementId,
			type, name.schema.nullStr(), name.package.nullStr(), name.object.nullStr(),
			(statement->sqlText.hasData() ? statement->sqlText->c_str() : ""));

		auto profileStatement = currentSession->statements.put(statement->getStatementId());
		profileStatement->id = statement->getStatementId();

		if (!mainProfileStatement)
			mainProfileStatement = profileStatement;
	}

	return mainProfileStatement;
}


//--------------------------------------


ProfilerListener::ProfilerListener(thread_db* tdbb)
	: attachment(tdbb->getAttachment()),
	  chatServer(buildParameters(tdbb, attachment->att_attachment_id)),
	  cleanupSync(*attachment->att_pool, watcherThread, THREAD_medium)
{
	cleanupSync.run(this);
}

ProfilerListener::~ProfilerListener()
{
	chatServer.disconnect();

	// Terminate the watcher thread.
	cleanupSync.waitForCompletion();
}

void ProfilerListener::exceptionHandler(const Exception& ex, ThreadFinishSync<ProfilerListener*>::ThreadRoutine*)
{
	iscLogException("Error closing profiler watcher thread\n", ex);
}

void ProfilerListener::watcherThread()
{
	try
	{
		while (!chatServer.isDisconnected())
		{
			const auto requestMessageOpt = chatServer.receive();
			if (!requestMessageOpt.has_value())
				continue;

			const auto& [requestMessage, clientAddress] = requestMessageOpt.value();
			IpcResponseMessage responseMessage;

			try
			{
				{	// scope
					FbLocalStatus statusVector;
					EngineContextHolder tdbb(&statusVector, attachment->getInterface(), FB_FUNCTION);

					responseMessage = processCommand(tdbb, requestMessage);
				}

				chatServer.sendTo(clientAddress, responseMessage);
			}
			catch (const status_exception& e)
			{
				//// TODO: Serialize status vector instead of formated message.

				const ISC_STATUS* status = e.value();
				string errorMsg;
				TEXT temp[BUFFER_LARGE];

				while (fb_interpret(temp, sizeof(temp), &status))
				{
					if (errorMsg.hasData())
						errorMsg += "\n\t";

					errorMsg += temp;
				}

				ExceptionResponse exceptionResponse;
				const auto errorLen = MIN(errorMsg.length(), sizeof(exceptionResponse.text) - 1);

				memcpy(exceptionResponse.text, errorMsg.c_str(), errorLen);
				exceptionResponse.text[errorLen] = '\0';

				chatServer.sendTo(clientAddress, exceptionResponse);
			}
		}
	}
	catch (const Exception& ex)
	{
		iscLogException("Error in profiler watcher thread\n", ex);
	}
}

IpcResponseMessage ProfilerListener::processCommand(thread_db* tdbb, const IpcRequestMessage& requestMessage)
{
	const auto profilerManager = attachment->getProfilerManager(tdbb);

	return std::visit(StdVisitOverloads{
		[&](const CheckUserRequest& checkUser) -> IpcResponseMessage
		{
			if (attachment->getUserName() != checkUser.userName)
				status_exception::raise(Arg::Gds(isc_miss_prvlg) << "PROFILE_ANY_ATTACHMENT");
			return Nothing{};
		},

		[&](const ProfilerPackage::CancelSessionInput::Type&) -> IpcResponseMessage
		{
			profilerManager->cancelSession();
			return Nothing{};
		},

		[&](const ProfilerPackage::DiscardInput::Type&) -> IpcResponseMessage
		{
			profilerManager->discard();
			return Nothing{};
		},

		[&](const ProfilerPackage::FinishSessionInput::Type& message) -> IpcResponseMessage
		{
			profilerManager->finishSession(tdbb, message.flush);
			return Nothing{};
		},

		[&](const ProfilerPackage::FlushInput::Type&) -> IpcResponseMessage
		{
			profilerManager->flush();
			return Nothing{};
		},

		[&](const ProfilerPackage::PauseSessionInput::Type& message) -> IpcResponseMessage
		{
			profilerManager->pauseSession(message.flush);
			return Nothing{};
		},

		[&](const ProfilerPackage::ResumeSessionInput::Type&) -> IpcResponseMessage
		{
			profilerManager->resumeSession();
			return Nothing{};
		},

		[&](const ProfilerPackage::SetFlushIntervalInput::Type& message) -> IpcResponseMessage
		{
			profilerManager->setFlushInterval(message.flushInterval);
			return Nothing{};
		},

		[&](const ProfilerPackage::StartSessionInput::Type& message) -> IpcResponseMessage
		{
			const string description(message.description.str,
				message.descriptionNull ? 0 : message.description.length);
			const std::optional<SLONG> flushInterval(message.flushIntervalNull ?
				std::nullopt : std::optional{message.flushInterval});
			const PathName pluginName(message.pluginName.str,
				message.pluginNameNull ? 0 : message.pluginName.length);
			const string pluginOptions(message.pluginOptions.str,
				message.pluginOptionsNull ? 0 : message.pluginOptions.length);
			const std::optional<SLONG> samplingInterval(message.samplingIntervalNull ?
				std::nullopt : std::optional{message.samplingInterval});

			return ProfilerPackage::StartSessionOutput::Type{
				.sessionId = profilerManager->startSession(tdbb, flushInterval,
					pluginName, description, pluginOptions, samplingInterval),
				.sessionIdNull = FB_FALSE,
			};
		},
	}, requestMessage);
}


//--------------------------------------


ProfilerPackage::ProfilerPackage(MemoryPool& pool)
	: SystemPackage(
		pool,
		"RDB$PROFILER",
		ODS_13_1,
		// procedures
		{
			SystemProcedure(
				pool,
				"CANCEL_SESSION",
				SystemProcedureFactory<CancelSessionInput, VoidMessage, cancelSessionProcedure>(),
				prc_executable,
				// input parameters
				{
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"DISCARD",
				SystemProcedureFactory<DiscardInput, VoidMessage, discardProcedure>(),
				prc_executable,
				// input parameters
				{
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"FINISH_SESSION",
				SystemProcedureFactory<FinishSessionInput, VoidMessage, finishSessionProcedure>(),
				prc_executable,
				// input parameters
				{
					{"FLUSH", fld_bool, false, "true", {blr_literal, blr_bool, 1}},
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"FLUSH",
				SystemProcedureFactory<FlushInput, VoidMessage, flushProcedure>(),
				prc_executable,
				// input parameters
				{
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"PAUSE_SESSION",
				SystemProcedureFactory<PauseSessionInput, VoidMessage, pauseSessionProcedure>(),
				prc_executable,
				// input parameters
				{
					{"FLUSH", fld_bool, false, "false", {blr_literal, blr_bool, 0}},
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"RESUME_SESSION",
				SystemProcedureFactory<ResumeSessionInput, VoidMessage, resumeSessionProcedure>(),
				prc_executable,
				// input parameters
				{
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			),
			SystemProcedure(
				pool,
				"SET_FLUSH_INTERVAL",
				SystemProcedureFactory<SetFlushIntervalInput, VoidMessage, setFlushIntervalProcedure>(),
				prc_executable,
				// input parameters
				{
					{"FLUSH_INTERVAL", fld_seconds_interval, false},
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}}
				},
				// output parameters
				{
				}
			)
		},
		// functions
		{
			SystemFunction(
				pool,
				"START_SESSION",
				SystemFunctionFactory<StartSessionInput, StartSessionOutput, startSessionFunction>(),
				// parameters
				{
					{"DESCRIPTION", fld_short_description, true, "null", {blr_null}},
					{"FLUSH_INTERVAL", fld_seconds_interval, true, "null", {blr_null}},
					{"ATTACHMENT_ID", fld_att_id, true, "null", {blr_null}},
					{"PLUGIN_NAME", fld_file_name2, true, "null", {blr_null}},
					{"PLUGIN_OPTIONS", fld_short_description, true, "null", {blr_null}},
					{"SAMPLING_INTERVAL", fld_integer, true, "100", {blr_literal, blr_long, 0, 0x64, 0, 0, 0}},
				},
				{fld_prof_ses_id, false}
			)
		}
	)
{
}
