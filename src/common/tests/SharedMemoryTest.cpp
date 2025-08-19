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
#include "../common/StatusArg.h"
#include "../common/isc_s_proto.h"
#include "../common/isc_proto.h"
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace Firebird;
using namespace std::chrono_literals;


namespace
{
	class IpcObjectImpl final : public IpcObject
	{
	public:
		struct Header : public MemoryHeader
		{
			event_t receiverEvent;
			event_t senderEvent;
			unsigned data;
		};

	public:
		explicit IpcObjectImpl(const std::string aPhysicalName)
			: physicalName(aPhysicalName),
			  sharedMemory(physicalName.c_str(), sizeof(Header), this)
		{
			checkHeader(sharedMemory.getHeader());
		}

		~IpcObjectImpl()
		{
			const auto header = sharedMemory.getHeader();

			if (header->receiverEvent.event_pid == 0 && header->senderEvent.event_pid == 0)
				sharedMemory.removeMapFile();
		}

		IpcObjectImpl(const IpcObjectImpl&) = delete;
		IpcObjectImpl& operator=(const IpcObjectImpl&) = delete;

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
			iscLogStatus(("Error when working with " + physicalName).c_str(),
				(Arg::Gds(isc_sys_request) << text << Arg::OsError(osErrorCode)).value());
		}

		USHORT getType() const override
		{
			return 1;
		}

		USHORT getVersion() const override
		{
			return 1;
		}

		const char* getName() const override
		{
			return "IpcObjectImpl test";
		}

	public:
		const std::string physicalName;
		SharedMemory<Header> sharedMemory;
	};
}


static std::string getTempPath()
{
	static std::atomic<int> counter{0};

	const auto now = std::chrono::system_clock::now();
	const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

	return "shared_memory_test_" +
		std::to_string(nowNs) + "_" +
		std::to_string(counter.fetch_add(1));
}


BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(SharedMemorySuite)


BOOST_AUTO_TEST_CASE(SharedMemoryTest)
{
	constexpr unsigned PRODUCER_COUNT = 2;

	const auto testPath = getTempPath();
	std::vector<std::unique_ptr<IpcObjectImpl>> producerObjects;

	for (unsigned i = 0; i < PRODUCER_COUNT; ++i)
		producerObjects.emplace_back(std::make_unique<IpcObjectImpl>(testPath));

	IpcObjectImpl consumerObject(testPath);

	constexpr unsigned messageCount = 500000;
	std::atomic_uint produced = 0;
	std::vector<unsigned> consumedData(messageCount);

	consumerObject.sharedMemory.eventInit(&consumerObject.sharedMemory.getHeader()->receiverEvent);
	consumerObject.sharedMemory.eventInit(&consumerObject.sharedMemory.getHeader()->senderEvent);

	std::vector<std::thread> threads;

	const auto producer = [&](unsigned producerIdx) {
		auto& sharedMemory = producerObjects[producerIdx]->sharedMemory;
		auto& header = *sharedMemory.getHeader();

		for (unsigned i = 0; i < messageCount; ++i)
		{
			SharedMutexGuard guard(&sharedMemory);

			header.data = i;

			// Problem - begin
			sharedMemory.eventFini(&header.senderEvent);
			if (sharedMemory.eventInit(&header.senderEvent) != FB_SUCCESS)
				(Arg::Gds(isc_random) << " eventInit(senderEvent) failed").raise();
			// Problem - end

			const SLONG eventCounter = sharedMemory.eventClear(&header.senderEvent);

			if (sharedMemory.eventPost(&header.receiverEvent) != FB_SUCCESS)
				(Arg::Gds(isc_random) << " eventPost failed").raise();

			if (sharedMemory.eventWait(&header.senderEvent, eventCounter, 0) != FB_SUCCESS)
				(Arg::Gds(isc_random) << " eventWait failed").raise();

			++produced;
		}
	};

	const auto consumer = [&]() {
		auto& sharedMemory = consumerObject.sharedMemory;
		auto& header = *sharedMemory.getHeader();
		SLONG eventCounter = 0;

		for (unsigned i = 0; i < messageCount * PRODUCER_COUNT; ++i)
		{
			if (sharedMemory.eventWait(&header.receiverEvent, eventCounter, 0) != FB_SUCCESS)
				(Arg::Gds(isc_random) << " eventWait failed").raise();

			const auto data = header.data;
			++consumedData[data];

			eventCounter = sharedMemory.eventClear(&header.receiverEvent);

			if (sharedMemory.eventPost(&header.senderEvent) != FB_SUCCESS)
				(Arg::Gds(isc_random) << " eventPost failed").raise();
		}
	};

	for (unsigned i = 0; i < PRODUCER_COUNT; ++i)
		threads.emplace_back(producer, i);

	threads.emplace_back(consumer);

	for (auto& thread : threads)
		thread.join();

	BOOST_CHECK_EQUAL(produced.load(), messageCount * PRODUCER_COUNT);
	BOOST_CHECK_EQUAL(consumedData.size(), messageCount);

	for (const auto& data : consumedData)
		BOOST_CHECK_EQUAL(data, 2);
}


BOOST_AUTO_TEST_SUITE_END()	// SharedMemorySuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
