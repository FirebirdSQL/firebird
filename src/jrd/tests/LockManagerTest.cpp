#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include <atomic>
#include <latch>
#include <memory>
#include <thread>
#include <vector>
#include "../common/status.h"
#include "../common/classes/fb_string.h"
#include "../common/config/config.h"
#include "../lock/lock_proto.h"
#include "../jrd/lck.h"

using namespace Firebird;
using namespace Jrd;


namespace
{
	class LockManagerTestCallbacks : public LockManager::Callbacks
	{
	public:
		ISC_STATUS getCancelState() const
		{
			return 0;
		}

		ULONG adjustWait(ULONG wait) const
		{
			return wait;
		}

		void checkoutRun(std::function<void()> func) const
		{
			func();
		}
	};
}


static std::string getUniqueId()
{
	static std::atomic<int> lockSuccess{0};

	const auto now = std::chrono::system_clock::now();
	const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

	return "lm_" +
		std::to_string(nowNs) + "_" +
		std::to_string(lockSuccess.fetch_add(1));
}


BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(LockManagerSuite)
BOOST_AUTO_TEST_SUITE(LockManagerTests)


BOOST_AUTO_TEST_CASE(LockUnlockTest)
{
	constexpr unsigned THREAD_COUNT = 8u;
	constexpr unsigned ITERATION_COUNT = 10'000u;

	ConfigFile configFile(ConfigFile::USE_TEXT, "\n");
	Config config(configFile);

	LockManagerTestCallbacks callbacks;
	const string lockManagerId(getUniqueId().c_str());
	auto lockManager = std::make_unique<LockManager>(lockManagerId, &config);

	unsigned lockSuccess = 0u;
	std::atomic_uint lockFail = 0;

	std::vector<std::thread> threads;
	std::latch latch(THREAD_COUNT);

	for (unsigned threadNum = 0u; threadNum < THREAD_COUNT; ++threadNum)
	{
		threads.emplace_back([&, threadNum]() {
			const UCHAR LOCK_KEY[] = {'1'};
			FbLocalStatus statusVector;
			LOCK_OWNER_T ownerId = threadNum + 1;
			SLONG ownerHandle = 0;

			lockManager->initializeOwner(&statusVector, ownerId, LCK_OWNER_attachment, &ownerHandle);

			latch.arrive_and_wait();

			for (unsigned i = 0; i < ITERATION_COUNT; ++i)
			{
				const auto lockId = lockManager->enqueue(callbacks, &statusVector, 0,
					LCK_expression, LOCK_KEY, sizeof(LOCK_KEY), LCK_EX, nullptr, nullptr, 0, LCK_WAIT, ownerHandle);

				if (lockId)
				{
					++lockSuccess;
					lockManager->dequeue(lockId);
				}
				else
					++lockFail;
			}

			lockManager->shutdownOwner(callbacks, &ownerHandle);
		});
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_CHECK_EQUAL(lockFail.load(), 0u);
	BOOST_CHECK_EQUAL(lockSuccess, THREAD_COUNT * ITERATION_COUNT);

	lockManager.reset();
}


BOOST_AUTO_TEST_SUITE_END()	// LockManagerTests
BOOST_AUTO_TEST_SUITE_END()	// LockManagerSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
