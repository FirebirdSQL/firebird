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

#ifndef COMMON_IPC_CHAT_H
#define COMMON_IPC_CHAT_H

#include "firebird.h"
#include "../ipc/IpcMessage.h"
#include "../classes/auto.h"
#include "../classes/fb_string.h"
#include "../file_params.h"
#include <atomic>
#include <functional>
#include <optional>
#include <utility>
#include <cstdint>

#ifdef WIN_NT
#include <process.h>
#endif

namespace Firebird {


struct IpcChatClientAddress
{
	uint64_t pid;
	uint64_t uid;
};


inline std::atomic_uint64_t nextIpcChatClientAddressUid = 0;


template <Variant VarRequestMessage, Variant VarResponseMessage>
class IpcChatServer final
{
public:
	explicit IpcChatServer(const IpcMessageParameters& parameters);

	IpcChatServer(const IpcChatServer&) = delete;
	IpcChatServer& operator=(const IpcChatServer&) = delete;

	~IpcChatServer()
	{
		disconnect();
	}

public:
	// May be called while receive or sendTo is being called in another thread
	void disconnect();

	std::optional<std::pair<VarRequestMessage, IpcChatClientAddress>> receive(
		std::function<void ()> idleFunc = nullptr);

	bool sendTo(const IpcChatClientAddress& clientAddress, const VarResponseMessage& message,
		std::function<void ()> idleFunc = nullptr);

	const auto& getParameters() const
	{
		return receiver.getParameters();
	}

	bool isDisconnected() const
	{
		return receiver.isDisconnected();
	}

private:
	IpcMessageReceiver<std::pair<VarRequestMessage, IpcChatClientAddress>> receiver;
	const USHORT version;
};


template <Variant VarRequestMessage, Variant VarResponseMessage>
class IpcChatClient final
{
public:
	explicit IpcChatClient(const IpcMessageParameters& parameters);

	IpcChatClient(const IpcChatClient&) = delete;
	IpcChatClient& operator=(const IpcChatClient&) = delete;

	~IpcChatClient()
	{
		disconnect();
	}

public:
	// May be called while receive or send is being called in another thread
	void disconnect();

	std::optional<VarResponseMessage> receive(std::function<void ()> idleFunc = nullptr);

	bool send(const VarRequestMessage& message, std::function<void ()> idleFunc = nullptr);

	std::optional<VarResponseMessage> sendAndReceive(const VarRequestMessage& message,
		std::function<void ()> idleFunc = nullptr);

	const auto& getAddress() const
	{
		return address;
	}

	const auto& getParameters() const
	{
		return receiver.getParameters();
	}

	bool isDisconnected() const
	{
		return receiver.isDisconnected() || sender.isDisconnected();
	}

private:
	static IpcMessageParameters buildReceiverParameters(const IpcChatClientAddress& address, USHORT version)
	{
		PathName fileName;
		fileName.printf(IPC_CHAT_CLIENT_FILE, address.pid, address.uid);

		return {
			.physicalName = fileName.c_str(),
			.logicalName = "IpcChatClient",
			.type = static_cast<USHORT>(SharedMemoryBase::SRAM_CHAT_CLIENT),
			.version = version,
		};
	}

private:
	const IpcChatClientAddress address;
	IpcMessageSender<std::pair<VarRequestMessage, IpcChatClientAddress>> sender;
	IpcMessageReceiver<std::pair<VarResponseMessage, IpcChatClientAddress>> receiver;
};


template <Variant VarRequestMessage, Variant VarResponseMessage>
inline IpcChatServer<VarRequestMessage, VarResponseMessage>::IpcChatServer(const IpcMessageParameters& parameters)
	: receiver(parameters),
	  version(parameters.version)
{
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline void IpcChatServer<VarRequestMessage, VarResponseMessage>::disconnect()
{
	receiver.disconnect();
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline std::optional<std::pair<VarRequestMessage, IpcChatClientAddress>>
IpcChatServer<VarRequestMessage, VarResponseMessage>::receive(std::function<void ()> idleFunc)
{
	return receiver.receive(idleFunc);
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline bool IpcChatServer<VarRequestMessage, VarResponseMessage>::sendTo(
	const IpcChatClientAddress& clientAddress, const VarResponseMessage& message, std::function<void ()> idleFunc)
{
	PathName fileName;
	fileName.printf(IPC_CHAT_CLIENT_FILE, clientAddress.pid, clientAddress.uid);

	IpcMessageSender<std::pair<VarResponseMessage, IpcChatClientAddress>> sender({
		.physicalName = fileName.c_str(),
		.logicalName = "IpcChatClient",
		.type = static_cast<USHORT>(SharedMemoryBase::SRAM_CHAT_CLIENT),
		.version = version,
	});

	return sender.send(std::make_pair(message, clientAddress), idleFunc);
}


template <Variant VarRequestMessage, Variant VarResponseMessage>
inline IpcChatClient<VarRequestMessage, VarResponseMessage>::IpcChatClient(const IpcMessageParameters& parameters)
	: address{
		.pid = static_cast<uint64_t>(getpid()),
		.uid = nextIpcChatClientAddressUid++
	  },
	  sender(parameters),
	  receiver(buildReceiverParameters(address, parameters.version))
{
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline void IpcChatClient<VarRequestMessage, VarResponseMessage>::disconnect()
{
	sender.disconnect();
	receiver.disconnect();
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline std::optional<VarResponseMessage> IpcChatClient<VarRequestMessage, VarResponseMessage>::receive(
	std::function<void ()> idleFunc)
{
	const auto received = receiver.receive(idleFunc);
	return received.has_value() ? std::make_optional(received->first) : std::nullopt;
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline bool IpcChatClient<VarRequestMessage, VarResponseMessage>::send(const VarRequestMessage& message,
	std::function<void ()> idleFunc)
{
	return sender.send(std::make_pair(message, address), idleFunc);
}

template <Variant VarRequestMessage, Variant VarResponseMessage>
inline std::optional<VarResponseMessage> IpcChatClient<VarRequestMessage, VarResponseMessage>::sendAndReceive(
	const VarRequestMessage& message, std::function<void ()> idleFunc)
{
	if (!send(message, idleFunc))
		return std::nullopt;

	return receive(idleFunc);
}


} // namespace Firebird

#endif // COMMON_IPC_CHAT_H
