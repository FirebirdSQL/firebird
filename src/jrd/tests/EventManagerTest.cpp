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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "boost/test/unit_test.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../common/classes/fb_string.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/config/config.h"
#include "../common/status.h"
#include "../common/ThreadStart.h"
#include "../jrd/event_proto.h"
#include "../yvalve/gds_proto.h"

using namespace Firebird;
using namespace Jrd;


namespace
{

// Wait for a condition with a deadline, polling periodically.
bool waitCond(const std::function<bool()>& cond, unsigned timeoutMs)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	while (!cond())
	{
		if (std::chrono::steady_clock::now() >= deadline)
			return false;

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	return true;
}

std::string getUniqueId()
{
	static std::atomic<unsigned> counter{0};

	const auto now = std::chrono::system_clock::now();
	const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
		now.time_since_epoch()).count();

	return "evt_" + std::to_string(nowNs) + "_" + std::to_string(counter.fetch_add(1));
}

// Build an EPB (event parameter block) v1 for a single event.
std::vector<UCHAR> makeEpb(const std::string& name, SLONG priorCount)
{
	if (name.length() > 255)
		BOOST_FAIL("event name is too long");

	std::vector<UCHAR> epb;
	epb.push_back(EPB_version1);
	epb.push_back(static_cast<UCHAR>(name.length()));
	epb.insert(epb.end(), name.begin(), name.end());

	for (int i = 0; i < 4; ++i)
		epb.push_back(static_cast<UCHAR>((priorCount >> (8 * i)) & 0xFF));

	return epb;
}


class TestEventCallback final :
	public RefCntIface<IEventCallbackImpl<TestEventCallback, CheckStatusWrapper> >
{
public:
	// Number of times the AST was invoked.
	std::atomic<unsigned> calls{0};

	void eventCallbackFunction(unsigned length, const unsigned char* events) override
	{
		std::lock_guard<std::mutex> lock(mutex);
		++calls;

		if (length >= 1 && events[0] == EPB_version1)
		{
			const UCHAR* p = events + 1;
			const UCHAR* const end = events + length;

			while (p + 1 <= end)
			{
				const USHORT count = *p++;
				if (USHORT(end - p) < count + 4u)
					break;

				std::string name(reinterpret_cast<const char*>(p), count);
				p += count;
				received.emplace_back(name, gds__vax_integer(p, 4));
				p += 4;
			}
		}
	}

	// Returns the count reported for the given event name, or -1 if not found.
	SLONG getCount(const std::string& name) const
	{
		std::lock_guard<std::mutex> lock(mutex);

		for (const auto& item : received)
		{
			if (item.first == name)
				return item.second;
		}

		return -1;
	}

private:
	mutable std::mutex mutex;
	std::vector<std::pair<std::string, SLONG>> received;
};


// Holds the configuration alive for the duration of a test.
struct TestEnv
{
	ConfigFile configFile;
	Config config;

	TestEnv()
		: configFile(ConfigFile::USE_TEXT, "\n"),
		  config(configFile)
	{
	}
};


struct ManagerHolder
{
	TestEnv env;
	Firebird::string id;
	std::unique_ptr<EventManager> manager;
	SLONG session = 0;

	ManagerHolder()
		: id(getUniqueId().c_str())
	{
		try
		{
			manager = std::make_unique<EventManager>(id, &env.config);
			session = manager->create_session();
		}
		catch (const Exception& ex)
		{
			FbLocalStatus status;
			ex.stuffException(&status);
			const ISC_STATUS* s = status->getErrors();
			TEXT buffer[1024];
			while (fb_interpret(buffer, sizeof(buffer), &s))
				fprintf(stderr, "EVENTTEST: init failed: %s\n", buffer);
			throw;
		}
	}

	~ManagerHolder()
	{
		manager->deleteSession(session);
	}
};

} // anonymous namespace


BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(EventManagerSuite)


BOOST_AUTO_TEST_CASE(BasicRoundTripTest)
{
	ManagerHolder h;
	TestEventCallback cb;

	// A request with prior count equal to the current count is satisfied
	// immediately and delivered asynchronously without any explicit posting.
	const auto epbAuto = makeEpb("EV_AUTO", 0);
	h.manager->queEvents(h.session, epbAuto.size(), epbAuto.data(), &cb);

	BOOST_TEST(waitCond([&]() { return cb.calls >= 1; }, 5000));
	BOOST_TEST(cb.getCount("EV_AUTO") == 1);

	// A cancelled request must never be delivered.
	const auto epbCancel = makeEpb("EV_CANCEL", 50);
	const SLONG cancelId = h.manager->queEvents(h.session, epbCancel.size(), epbCancel.data(), &cb);
	BOOST_TEST(cancelId != 0);
	h.manager->cancelEvents(cancelId);

	h.manager->postEvent(sizeof("EV_CANCEL") - 1, "EV_CANCEL", 100);
	h.manager->deliverEvents();
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	BOOST_TEST(cb.calls.load() == 1);
	BOOST_TEST(cb.getCount("EV_CANCEL") == -1);

	// A request satisfied by postEvent is delivered by deliverEvents().
	const auto epbPost = makeEpb("EV_POST", 10);
	h.manager->queEvents(h.session, epbPost.size(), epbPost.data(), &cb);

	h.manager->postEvent(sizeof("EV_POST") - 1, "EV_POST", 20);
	h.manager->deliverEvents();

	BOOST_TEST(waitCond([&]() { return cb.getCount("EV_POST") >= 0; }, 5000));
	BOOST_TEST(cb.getCount("EV_POST") == 21);
}


BOOST_AUTO_TEST_CASE(MalformedEpbTruncatedTailTest)
{
	// An EPB whose last item declares a name that does not fit into the
	// buffer together with the mandatory 4-byte prior count must be rejected.
	ManagerHolder h;
	TestEventCallback cb;

	auto epb = makeEpb("AAA", 0);
	epb.push_back(10);		// name length of the last item
	epb.push_back(1);		// a couple of trailing bytes, far too few
	epb.push_back(2);

	bool raised = false;

	try
	{
		h.manager->queEvents(h.session, epb.size(), epb.data(), &cb);
	}
	catch (const Exception&)
	{
		raised = true;
	}

	BOOST_TEST(raised);

	// Nothing must be delivered for the malformed request and no residue
	// must be left behind.
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	BOOST_TEST(cb.calls.load() == 0);

	// The manager must stay fully functional.
	const auto epbGood = makeEpb("EV_AFTER_TRUNCATED", 0);
	h.manager->queEvents(h.session, epbGood.size(), epbGood.data(), &cb);
	BOOST_TEST(waitCond([&]() { return cb.getCount("EV_AFTER_TRUNCATED") >= 0; }, 5000));
	BOOST_TEST(cb.getCount("EV_AFTER_TRUNCATED") == 1);
	BOOST_TEST(cb.calls.load() == 1);
}


BOOST_AUTO_TEST_CASE(MalformedEpbOverlongNameTest)
{
	// An EPB declaring an event name longer than the whole buffer must be
	// rejected and must leave no partially built request behind. Such a
	// leftover request would be considered completed and asynchronously
	// deliver a garbage event block to the callback.
	ManagerHolder h;
	TestEventCallback cb;

	auto epb = makeEpb("BBB", 0);
	epb.push_back(250);		// bogus name length, exceeds the buffer

	bool raised = false;

	try
	{
		h.manager->queEvents(h.session, epb.size(), epb.data(), &cb);
	}
	catch (const Exception&)
	{
		raised = true;
	}

	BOOST_TEST(raised);

	// No garbage delivery may arrive for the malformed request.
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	BOOST_TEST(cb.calls.load() == 0);

	// The manager must stay fully functional.
	const auto epbGood = makeEpb("EV_AFTER_OVERLONG", 0);
	h.manager->queEvents(h.session, epbGood.size(), epbGood.data(), &cb);
	BOOST_TEST(waitCond([&]() { return cb.getCount("EV_AFTER_OVERLONG") >= 0; }, 5000));
	BOOST_TEST(cb.getCount("EV_AFTER_OVERLONG") == 1);
	BOOST_TEST(cb.calls.load() == 1);
}


BOOST_AUTO_TEST_CASE(ConcurrentStressTest)
{
	constexpr unsigned THREAD_COUNT = 4u;
	constexpr unsigned ITERATION_COUNT = 300u;

	ManagerHolder h;
	TestEventCallback cb;
	std::atomic<ULONG> expectedCalls{0};

	std::vector<std::thread> threads;
	std::atomic<unsigned> startBarrier{0};

	for (unsigned threadNum = 0; threadNum < THREAD_COUNT; ++threadNum)
	{
		threads.emplace_back([&, threadNum]() {
			const std::string prefix = "ST_" + std::to_string(threadNum) + "_";
			ULONG localExpected = 0;

			startBarrier.fetch_add(1, std::memory_order_acq_rel);
			while (startBarrier.load(std::memory_order_acquire) < THREAD_COUNT)
				std::this_thread::yield();

			for (unsigned i = 0; i < ITERATION_COUNT; ++i)
			{
				const std::string name = prefix + std::to_string(i);
				const auto epb = makeEpb(name, 0);

				switch (i % 3)
				{
					// Immediately satisfied request, delivered on its own.
					case 0:
						h.manager->queEvents(h.session, epb.size(), epb.data(), &cb);
						++localExpected;
						break;

					// Request satisfied by postEvent and delivered by deliverEvents().
					case 1:
					{
						const auto epbLate = makeEpb(name, 5);
						h.manager->queEvents(h.session, epbLate.size(), epbLate.data(), &cb);
						h.manager->postEvent(name.length(), name.c_str(), 10);
						h.manager->deliverEvents();
						++localExpected;
						break;
					}

					// Cancelled request, never delivered.
					case 2:
					{
						const auto epbCancelled = makeEpb(name, 100);
						const SLONG id = h.manager->queEvents(h.session,
							epbCancelled.size(), epbCancelled.data(), &cb);
						h.manager->cancelEvents(id);
						break;
					}
				}
			}

			expectedCalls += localExpected;
		});
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_TEST(waitCond([&]() { return cb.calls >= expectedCalls; }, 30000));

	// Every satisfied request must be delivered exactly once.
	h.manager->deliverEvents();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	BOOST_TEST(cb.calls.load() == expectedCalls.load());
}


BOOST_AUTO_TEST_SUITE_END()	// EventManagerSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
