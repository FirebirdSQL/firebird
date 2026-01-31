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

#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/ipc/IpcChat.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

using namespace Firebird;
using namespace std::chrono_literals;


static std::string getTempPath()
{
	static std::atomic<int> counter{0};

	const auto now = std::chrono::system_clock::now();
	const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

	return "chat_test_" +
		std::to_string(nowNs) + "_" +
		std::to_string(counter.fetch_add(1));
}


BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(IpcChatSuite)


BOOST_AUTO_TEST_CASE(IpcChatTest)
{
	struct Request
	{
		IpcChatClientAddress clientAddress;
		unsigned n;
	};

	struct Response
	{
		unsigned n;
	};

	using RequestMessage = std::variant<Request>;
	using ResponseMessage = std::variant<Response>;

	const auto testPath = getTempPath();

	const IpcMessageParameters parameters{
		.physicalName = testPath,
		.logicalName = "IpcChatClientTest",
		.type = 1,
		.version = 1
	};

	IpcChatServer<RequestMessage, ResponseMessage> server(parameters);
	IpcChatClient<RequestMessage, ResponseMessage> client(parameters);

	constexpr unsigned NUM_MESSAGES = 4'000;
	unsigned readCount = 0;

	std::thread consumerThread([&]() {
		for (readCount = 0; readCount < NUM_MESSAGES; ++readCount)
		{
			const auto requestMessageOpt = server.receive();

			if (!requestMessageOpt.has_value())
				continue;

			const auto& [requestMessage, clientAddress] = requestMessageOpt.value();

			if (clientAddress.pid != client.getAddress().pid || clientAddress.uid != client.getAddress().uid)
				throw std::domain_error("Invalid address");	// cannot use boost.test here in another thread

			if (std::holds_alternative<Request>(requestMessage))
			{
				const auto& request = std::get<Request>(requestMessage);
				server.sendTo(request.clientAddress, Response{ .n = request.n * 2 });
			}
		}
	});

	for (unsigned writeCount = 0; writeCount < NUM_MESSAGES; ++writeCount)
	{
		BOOST_CHECK(client.send(Request{ .clientAddress = client.getAddress(), .n = writeCount }));
		const auto responseMessageOpt = client.receive();
		const auto& responseMessage = responseMessageOpt.value();

		BOOST_CHECK(std::holds_alternative<Response>(responseMessage));
		BOOST_CHECK_EQUAL(std::get<Response>(responseMessage).n, writeCount * 2);
	}

	consumerThread.join();

	BOOST_CHECK_EQUAL(readCount, NUM_MESSAGES);
}


BOOST_AUTO_TEST_SUITE_END()	// IpcChatSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
