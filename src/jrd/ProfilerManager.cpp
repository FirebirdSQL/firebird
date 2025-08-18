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
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/tra_proto.h"
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

		static constexpr USHORT VERSION = 3;

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

	const auto transaction = tdbb->getTransaction();
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

	const auto profilerManager = attachment->getProfilerManager(tdbb);

	out->sessionIdNull = FB_FALSE;
	out->sessionId = profilerManager->startSession(tdbb, flushInterval, pluginName, description, pluginOptions);
}


//--------------------------------------


ProfilerManager::ProfilerManager(thread_db* tdbb)
	: activePlugins(*tdbb->getAttachment()->att_pool)
{
	const auto attachment = tdbb->getAttachment();

	flushTimer = FB_NEW TimerImpl();

	flushTimer->setOnTimer([this, attachment](auto) {
		FbLocalStatus statusVector;
		EngineContextHolder innerTdbb(&statusVector, attachment->getInterface(), FB_FUNCTION);

		flush(false);
		updateFlushTimer(false);
	});
}

ProfilerManager::~ProfilerManager()
{
	flushTimer->stop();
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
	const PathName& pluginName, const string& description, const string& options)
{
	if (flushInterval.has_value())
		checkFlushInterval(flushInterval.value());

	AutoSetRestore<bool> pauseProfiler(&paused, true);

	const auto attachment = tdbb->getAttachment();
	ThrowLocalStatus status;

	const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(attachment->att_current_timezone);

	if (currentSession)
	{
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

	paused = false;

	if (flushInterval.has_value())
		setFlushInterval(flushInterval.value());

	return currentSession->pluginSession->getId();
}

void ProfilerManager::prepareCursor(thread_db* tdbb, Request* request, const Select* select)
{
	auto profileStatement = getStatement(request);

	if (!profileStatement)
		return;

	auto cursorId = select->getCursorId();

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

	if (profileStatement->recSourceSequence.exist(recordSource->getRecSourceId()))
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

		profileStatement->recSourceSequence.put(recSourceId, *sequencePtr);
	}
}

void ProfilerManager::onRequestFinish(Request* request, Stats& stats)
{
	if (const auto profileRequestId = getRequest(request, 0))
	{
		const auto profileStatement = getStatement(request);
		const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(request->req_attachment->att_current_timezone);

		LogLocalStatus status("Profiler onRequestFinish");
		currentSession->pluginSession->onRequestFinish(&status, profileStatement->id, profileRequestId,
			timestamp, &stats);

		currentSession->requests.findAndRemove(profileRequestId);
	}
}

void ProfilerManager::cancelSession()
{
	if (currentSession)
	{
		LogLocalStatus status("Profiler cancelSession");

		currentSession->pluginSession->cancel(&status);
		currentSession = nullptr;
	}
}

void ProfilerManager::finishSession(thread_db* tdbb, bool flushData)
{
	if (currentSession)
	{
		const auto attachment = tdbb->getAttachment();
		const auto timestamp = TimeZoneUtil::getCurrentTimeStamp(attachment->att_current_timezone);
		LogLocalStatus status("Profiler finish");

		currentSession->pluginSession->finish(&status, timestamp);
		currentSession = nullptr;
	}

	if (flushData)
		flush();
}

void ProfilerManager::pauseSession(bool flushData)
{
	if (currentSession)
		paused = true;

	if (flushData)
		flush();
}

void ProfilerManager::resumeSession()
{
	if (currentSession)
	{
		paused = false;
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
	currentSession = nullptr;
	activePlugins.clear();
}

void ProfilerManager::flush(bool updateTimer)
{
	{	// scope
		AutoSetRestore<bool> pauseProfiler(&paused, true);

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

void ProfilerManager::updateFlushTimer(bool canStopTimer)
{
	if (currentSession && !paused && currentFlushInterval)
		flushTimer->reset(currentFlushInterval);
	else if (canStopTimer)
		flushTimer->stop();
}

ProfilerManager::Statement* ProfilerManager::getStatement(Request* request)
{
	if (!isActive())
		return nullptr;

	auto mainProfileStatement = currentSession->statements.get(request->getStatement()->getStatementId());

	if (mainProfileStatement)
		return mainProfileStatement;

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

			return ProfilerPackage::StartSessionOutput::Type{
				.sessionId = profilerManager->startSession(tdbb, flushInterval,
					pluginName, description, pluginOptions),
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
				},
				{fld_prof_ses_id, false}
			)
		}
	)
{
}
