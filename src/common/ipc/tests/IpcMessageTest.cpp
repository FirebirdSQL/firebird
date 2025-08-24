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
#include "../common/ipc/IpcMessage.h"
#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>

using namespace Firebird;
using namespace std::chrono_literals;


static std::string getTempPath()
{
	static std::atomic<int> counter{0};

	const auto now = std::chrono::system_clock::now();
	const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

	return "message_test_" +
		std::to_string(nowNs) + "_" +
		std::to_string(counter.fetch_add(1));
}


BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(IpcMessageSuite)


BOOST_AUTO_TEST_CASE(ProducerConsumerMessageTest)
{
	struct Small
	{
		unsigned n;
	};

	struct Big
	{
		Big(unsigned aN)
			: n(aN)
		{
			memset(s, n % 256, sizeof(s));
		}

		Big()
		{}

		unsigned n;
		char s[32000]{};
	};

	struct Stop {};

	using TestMessage = std::variant<Small, Big, Stop>;

	constexpr auto ENV_NAME = "FB_PRODUCER_CONSUMER_MESSAGE_TEST_NAME";
	constexpr auto ENV_RECEIVER = "FB_PRODUCER_CONSUMER_MESSAGE_TEST_RECEIVER";
	constexpr auto ENV_PRODUCER_PROCESSES = "FB_PRODUCER_CONSUMER_MESSAGE_TEST_PRODUCER_PROCESSES";

	const char* const envName = std::getenv(ENV_NAME);
	const char* const envReceiver = std::getenv(ENV_RECEIVER);
	const auto envProducerProcesses = std::getenv(ENV_PRODUCER_PROCESSES);

	const bool multiProcess = envName != nullptr;
	const bool multiProcessIsReceiver = multiProcess && envReceiver != nullptr;
	const unsigned processCount = multiProcess ? (unsigned) std::stoi(envProducerProcesses) : 1u;
	const auto testPath = envName ? std::string(envName) : getTempPath();

	std::optional<IpcMessageReceiver<TestMessage>> receiver;

	if (multiProcessIsReceiver || !multiProcess)
	{
		receiver.emplace(IpcMessageParameters{
			.physicalName = testPath,
			.logicalName = "IpcMessageTest",
			.type = 1,
			.version = 1
		});
	}

	std::vector<std::unique_ptr<IpcMessageSender<TestMessage>>> senders;

	for (unsigned i = 0u; i < (multiProcessIsReceiver ? 0u : 2u); ++i)
	{
		senders.emplace_back(std::make_unique<IpcMessageSender<TestMessage>>(IpcMessageParameters{
			.physicalName = testPath,
			.logicalName = "IpcMessageTest",
			.type = 1,
			.version = 1
		}));
	}

	constexpr unsigned numMessages = 8'000;
	constexpr unsigned start[2] = {0, numMessages + 10};
	unsigned writeNum[2] = {0, 0};
	unsigned readCount = 0;
	unsigned stopReads = 0;
	unsigned smallReads = 0;
	unsigned bigReads = 0;
	std::atomic_uint problems = 0;
	std::vector<std::thread> threads;

	if (!multiProcess || !multiProcessIsReceiver)
	{
		const auto senderFunc = [&](unsigned i) {
			for (writeNum[i] = start[i]; writeNum[i] - start[i] < numMessages; ++writeNum[i])
			{
				if (writeNum[i] % 2 == 0)
				{
					if (!senders[i]->send(Small{ writeNum[i] }))
						++problems;
				}
				else
				{
					if (!senders[i]->send(Big{ writeNum[i] }))
						++problems;
				}
			}

			if (!senders[i]->send(Stop{}))
				++problems;
		};

		for (unsigned i = 0u; i < 2; ++i)
			threads.emplace_back(senderFunc, i);
	}

	if (!multiProcess || multiProcessIsReceiver)
	{
		threads.emplace_back([&]() {
			for (readCount = 0; readCount < (numMessages + 1u) * processCount * 2u;)
			{
				const auto message = receiver->receive();

				if (!message.has_value())
					continue;

				if (std::holds_alternative<Stop>(message.value()))
					++stopReads;
				else if (std::holds_alternative<Small>(message.value()))
					++smallReads;
				else
				{
					if (std::holds_alternative<Big>(message.value()))
					{
						const auto& big = std::get<Big>(message.value());

						char s[sizeof(big.s)];
						memset(s, big.n % 256, sizeof(s));
						if (memcmp(s, big.s, sizeof(s)) != 0)
							++problems;

						++bigReads;
					}
					else
						++problems;
				}

				++readCount;
			}
		});
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_CHECK_EQUAL(problems, 0);

	if (!multiProcess || !multiProcessIsReceiver)
	{
		BOOST_CHECK_EQUAL(writeNum[0], start[0] + numMessages);
		BOOST_CHECK_EQUAL(writeNum[1], start[1] + numMessages);
	}

	if (!multiProcess || multiProcessIsReceiver)
	{
		BOOST_CHECK_EQUAL(readCount, (numMessages + 1u) * processCount * 2u);
		BOOST_CHECK_EQUAL(stopReads, processCount * 2u);
		BOOST_CHECK_EQUAL(smallReads, processCount * numMessages);
		BOOST_CHECK_EQUAL(bigReads, processCount * numMessages);
	}
}


BOOST_AUTO_TEST_CASE(ServerDisconnectMessageTest)
{
	struct Message
	{
		unsigned n;
	};

	using TestMessage = std::variant<Message>;

	const auto testPath = getTempPath();

	IpcMessageReceiver<TestMessage> server({
		.physicalName = testPath,
		.logicalName = "IpcMessageTest",
		.type = 1,
		.version = 1
	});
	IpcMessageSender<TestMessage> client({
		.physicalName = testPath,
		.logicalName = "IpcMessageTest",
		.type = 1,
		.version = 1
	});

	unsigned produced = 0;
	unsigned consumed = 0;

	std::thread producerThread([&]() {
		try
		{
			while (!server.isDisconnected())
			{
				if (client.send(Message{0}))
					++produced;
			}
		}
		catch (...)
		{
		}
	});

	std::thread consumerThread([&]() {
		try
		{
			while (!server.isDisconnected())
			{
				const auto message = server.receive();

				if (message.has_value())
					++consumed;
			}
		}
		catch (...)
		{
		}
	});

	std::this_thread::sleep_for(1s);
	server.disconnect();

	producerThread.join();
	consumerThread.join();

	BOOST_CHECK_GT(produced, 0u);
	BOOST_CHECK_GT(consumed, 0u);
	BOOST_CHECK(produced == consumed || produced - 1u == consumed);
}


BOOST_AUTO_TEST_SUITE_END()	// IpcMessageSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
