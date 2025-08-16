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
 *  Copyright (c) 2025 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_IPC_MESSAGE_H
#define COMMON_IPC_MESSAGE_H

#include "firebird.h"
#include "../StdHelper.h"
#include "../classes/fb_string.h"
#include "../StatusArg.h"
#include "../isc_s_proto.h"
#include "../isc_proto.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <cstdint>

namespace Firebird {


inline constexpr SLONG IPC_MESSAGE_TIMEOUT_MICROSECONDS = 500'000;	// 0.5s


struct IpcMessageParameters final
{
	const std::string physicalName;
	const std::string logicalName;
	const USHORT type = 0;
	const USHORT version = 0;
};


/*
 * MessageConcept can be a std::variant or a std::pair<std::variant, FixedMessage>.
 * std::variant is a variant message of PODs that can be exchanged between client and server.
 * The optional FixedMessage is a fixed POD type that also goes together with the variant message.
 */
template <typename T>
concept MessageConcept =
	Variant<T> ||
	(requires {
		typename T::first_type;
		typename T::second_type;
	} && Variant<typename T::first_type>);


template <MessageConcept Message>
class IpcMessageObjectImpl final : public IpcObject
{
private:
	template <typename T>
	struct IsPair : std::false_type {};

	template <typename T, typename U>
	struct IsPair<std::pair<T, U>> : std::true_type {};

public:
	static constexpr bool isMessagePair = IsPair<Message>::value;

private:
	static constexpr size_t getMaxSize()
	{
		if constexpr (isMessagePair)
			return maxVariantSize<typename Message::first_type>() + sizeof(typename Message::second_type);
		else
			return maxVariantSize<Message>();
	}

	static_assert(getMaxSize() <= std::numeric_limits<uint16_t>::max());

public:
	struct Header : public MemoryHeader
	{
		event_t receiverEvent;
		event_t senderEvent;
		uint16_t messageLen;
		uint8_t messageIndex;
		uint8_t messageBuffer[getMaxSize()];
	};

public:
	explicit IpcMessageObjectImpl(const IpcMessageParameters& aParameters)
		: parameters(aParameters),
		  sharedMemory(parameters.physicalName.c_str(), sizeof(Header), this)
	{
		checkHeader(sharedMemory.getHeader());
	}

	~IpcMessageObjectImpl()
	{
		const auto header = sharedMemory.getHeader();

		if (header->receiverEvent.event_pid == 0 && header->senderEvent.event_pid == 0)
			sharedMemory.removeMapFile();
	}

	IpcMessageObjectImpl(const IpcMessageObjectImpl&) = delete;
	IpcMessageObjectImpl& operator=(const IpcMessageObjectImpl&) = delete;

public:
	bool initialize(SharedMemoryBase* sm, bool init) override
	{
		if (init)
		{
			const auto header = reinterpret_cast<Header*>(sm->sh_mem_header);

			// Initialize the shared data header.
			initHeader(header);
		}

		return true;
	}

	void mutexBug(int osErrorCode, const char* text) override
	{
		iscLogStatus(("Error when working with " + parameters.logicalName).c_str(),
			(Arg::Gds(isc_sys_request) << text << Arg::OsError(osErrorCode)).value());
	}

	USHORT getType() const override
	{
		return parameters.type;
	}

	USHORT getVersion() const override
	{
		return parameters.version;
	}

	const char* getName() const override
	{
		return parameters.logicalName.c_str();
	}

public:
	IpcMessageParameters parameters;
	SharedMemory<Header> sharedMemory;
};


template <MessageConcept Message>
class IpcMessageReceiver final
{
public:
	explicit IpcMessageReceiver(const IpcMessageParameters& parameters);

	IpcMessageReceiver(const IpcMessageReceiver&) = delete;
	IpcMessageReceiver& operator=(const IpcMessageReceiver&) = delete;

	~IpcMessageReceiver();

public:
	// May be called while receive is being called in another thread
	void disconnect();

	std::optional<Message> receive(std::function<void ()> idleFunc = nullptr);

	const auto& getParameters() const
	{
		return ipc.parameters;
	}

	bool isDisconnected() const
	{
		return disconnected;
	}

private:
	IpcMessageObjectImpl<Message> ipc;
	SLONG eventCounter = 1;
	std::atomic_bool disconnected = false;
	std::mutex mutex;
};

template <MessageConcept Message>
class IpcMessageSender final
{
public:
	explicit IpcMessageSender(const IpcMessageParameters& parameters);

	IpcMessageSender(const IpcMessageSender&) = delete;
	IpcMessageSender& operator=(const IpcMessageSender&) = delete;

	~IpcMessageSender();

public:
	static bool sendTo(const IpcMessageParameters& parameters, const Message& message,
		std::function<void ()> idleFunc = nullptr);

public:
	// May be called while send is being called in another thread
	void disconnect();

	bool send(const Message& message, std::function<void ()> idleFunc = nullptr);

	const auto& getParameters() const
	{
		return ipc.parameters;
	}

	bool isDisconnected() const
	{
		return disconnected;
	}

private:
	IpcMessageObjectImpl<Message> ipc;
	std::atomic_bool disconnected = false;
	std::mutex mutex;
};


template <MessageConcept Message>
inline IpcMessageReceiver<Message>::IpcMessageReceiver(const IpcMessageParameters& parameters)
	: ipc(parameters)
{
	const auto sharedMemory = &ipc.sharedMemory;
	const auto header = sharedMemory->getHeader();

	SharedMutexGuard guard(sharedMemory);

	if (sharedMemory->eventInit(&header->receiverEvent) != FB_SUCCESS)
		(Arg::Gds(isc_random) << (ipc.parameters.logicalName + " eventInit(receiverEvent) failed").c_str()).raise();
}

template <MessageConcept Message>
inline IpcMessageReceiver<Message>::~IpcMessageReceiver()
{
	disconnect();

	const auto sharedMemory = &ipc.sharedMemory;
	const auto header = sharedMemory->getHeader();

	if (header->receiverEvent.event_pid)
	{
		sharedMemory->eventFini(&header->receiverEvent);
		header->receiverEvent.event_pid = 0;
	}
}

template <MessageConcept Message>
inline void IpcMessageReceiver<Message>::disconnect()
{
	if (!disconnected)
	{
		disconnected = true;
		std::lock_guard mutexLock(mutex);
	}
}

template <MessageConcept Message>
inline std::optional<Message> IpcMessageReceiver<Message>::receive(std::function<void ()> idleFunc)
{
	std::lock_guard mutexLock(mutex);

	if (disconnected)
		return std::nullopt;

	const auto sharedMemory = &ipc.sharedMemory;
	const auto header = sharedMemory->getHeader();

	while (sharedMemory->eventWait(&header->receiverEvent, eventCounter, IPC_MESSAGE_TIMEOUT_MICROSECONDS) !=
				FB_SUCCESS)
	{
		if (disconnected)
			return std::nullopt;

		if (idleFunc)
			idleFunc();
	}

	eventCounter = sharedMemory->eventClear(&header->receiverEvent);

	if constexpr (IpcMessageObjectImpl<Message>::isMessagePair)
	{
		std::optional<Message> messageOpt(std::make_pair(
			createVariantByIndex<typename Message::first_type>(header->messageIndex),
			typename Message::second_type{}));
		auto& varMessage = messageOpt->first;
		auto& fixedMessage = messageOpt->second;

		const auto span = getVariantIndexAndSpan(varMessage).second;
		fb_assert(span.size() == header->messageLen);

		memcpy(&fixedMessage, header->messageBuffer, sizeof(fixedMessage));
		memcpy(span.data(), header->messageBuffer + sizeof(fixedMessage), span.size());

		if (sharedMemory->eventPost(&header->senderEvent) != FB_SUCCESS)
			(Arg::Gds(isc_random) << (ipc.parameters.logicalName + " eventPost(senderEvent) failed").c_str()).raise();

		return messageOpt;
	}
	else
	{
		std::optional<Message> messageOpt(createVariantByIndex<Message>(header->messageIndex));
		auto& varMessage = messageOpt.value();

		const auto span = getVariantIndexAndSpan(varMessage).second;
		fb_assert(span.size() == header->messageLen);

		memcpy(span.data(), header->messageBuffer, span.size());

		if (sharedMemory->eventPost(&header->senderEvent) != FB_SUCCESS)
			(Arg::Gds(isc_random) << (ipc.parameters.logicalName + " eventPost(senderEvent) failed").c_str()).raise();

		return messageOpt;
	}
}


template <MessageConcept Message>
inline IpcMessageSender<Message>::IpcMessageSender(const IpcMessageParameters& parameters)
	: ipc(parameters)
{
}

template <MessageConcept Message>
inline IpcMessageSender<Message>::~IpcMessageSender()
{
	disconnect();

	const auto sharedMemory = &ipc.sharedMemory;
	const auto header = sharedMemory->getHeader();

	if (header->senderEvent.event_pid)
	{
		sharedMemory->eventFini(&header->senderEvent);
		header->senderEvent.event_pid = 0;
	}
}

template <MessageConcept Message>
inline bool IpcMessageSender<Message>::sendTo(const IpcMessageParameters& parameters, const Message& message,
	std::function<void ()> idleFunc)
{
	IpcMessageSender<Message> sender(parameters);
	return sender.send(message, idleFunc);
}

template <MessageConcept Message>
inline void IpcMessageSender<Message>::disconnect()
{
	if (!disconnected)
	{
		disconnected = true;
		std::lock_guard mutexLock(mutex);
	}
}

template <MessageConcept Message>
inline bool IpcMessageSender<Message>::send(const Message& message, std::function<void ()> idleFunc)
{
	std::lock_guard mutexLock(mutex);

	if (disconnected)
		return false;

	const auto sharedMemory = &ipc.sharedMemory;
	const auto header = sharedMemory->getHeader();

	SharedMutexGuard guard(sharedMemory, false);

	while (!guard.tryLock(std::chrono::milliseconds(IPC_MESSAGE_TIMEOUT_MICROSECONDS / 1000)))
	{
		if (disconnected)
			return false;

		if (idleFunc)
			idleFunc();
	}

	if constexpr (IpcMessageObjectImpl<Message>::isMessagePair)
	{
		const auto& varMessage = message.first;
		const auto& fixedMessage = message.second;

		const auto [index, span] = getVariantIndexAndSpan(varMessage);

		header->messageIndex = index;
		header->messageLen = static_cast<uint16_t>(span.size());
		memcpy(header->messageBuffer, &fixedMessage, sizeof(fixedMessage));
		memcpy(header->messageBuffer + sizeof(fixedMessage), span.data(), span.size());
	}
	else
	{
		const auto [index, span] = getVariantIndexAndSpan(message);

		header->messageIndex = index;
		header->messageLen = static_cast<uint16_t>(span.size());
		memcpy(header->messageBuffer, span.data(), span.size());
	}

	if (sharedMemory->eventInit(&header->senderEvent) != FB_SUCCESS)
		(Arg::Gds(isc_random) << (ipc.parameters.logicalName + " eventInit(senderEvent) failed").c_str()).raise();

	Cleanup senderEventCleanup([&] {
		if (header->senderEvent.event_pid)
		{
			sharedMemory->eventFini(&header->senderEvent);
			header->senderEvent.event_pid = 0;
		}
	});

	const SLONG eventCounter = sharedMemory->eventClear(&header->senderEvent);

	if (sharedMemory->eventPost(&header->receiverEvent) != FB_SUCCESS)
		(Arg::Gds(isc_random) << (ipc.parameters.logicalName + " eventPost(receiverEvent) failed").c_str()).raise();

	while (sharedMemory->eventWait(&header->senderEvent, eventCounter, IPC_MESSAGE_TIMEOUT_MICROSECONDS) != FB_SUCCESS)
	{
		if (disconnected)
			return false;

		if (idleFunc)
			idleFunc();
	}

	return true;
}


} // namespace Firebird

#endif // COMMON_IPC_MESSAGE_H
