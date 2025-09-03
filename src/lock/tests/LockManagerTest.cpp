#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include <atomic>
#include <latch>
#include <memory>
#include <mutex>
#include <unordered_map>
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
		LockManagerTestCallbacks(std::mutex* aMutex = nullptr)
			: mutex(aMutex)
		{
		}

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
			if (mutex)
			{
				Cleanup lockCleanup([&]() { mutex->lock(); });
				mutex->unlock();
				func();
			}
			else
				func();
		}

	private:
		std::mutex* mutex;
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


BOOST_AUTO_TEST_CASE(LockUnlockWaitTest)
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


BOOST_AUTO_TEST_CASE(LockUnlockNoWaitTest)
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
					LCK_expression, LOCK_KEY, sizeof(LOCK_KEY), LCK_EX, nullptr, nullptr, 0, LCK_NO_WAIT, ownerHandle);

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

	BOOST_CHECK_GT(lockFail.load(), 0u);
	BOOST_CHECK_GT(lockSuccess, 0u);
	BOOST_CHECK_EQUAL(lockSuccess + lockFail, THREAD_COUNT * ITERATION_COUNT);

	lockManager.reset();
}


BOOST_AUTO_TEST_CASE(LockUnlockAstTest)
{
	struct Lock;

	struct ThreadData
	{
		std::thread::id threadId;
		LockManager* lockManager = nullptr;
		std::mutex localMutex;
		std::unordered_map<SLONG, Lock*> locks;
		SLONG ownerHandle;
		bool shutdown = false;
	};

	struct Lock
	{
		ThreadData* threadData = nullptr;
		unsigned key = 0;
		SLONG lockId = 0;
		bool locked = false;
		bool blocking = false;
	};

	constexpr unsigned THREAD_COUNT = 8u;
	constexpr unsigned ITERATION_COUNT = 100'000u;

	ConfigFile configFile(ConfigFile::USE_TEXT, "\n");
	Config config(configFile);

	const string lockManagerId(getUniqueId().c_str());
	auto lockManager = std::make_unique<LockManager>(lockManagerId, &config);

	std::atomic_uint lockSuccess = 0u;
	std::atomic_uint lockFail = 0u;

	std::vector<std::thread> threads;
	std::latch latch(THREAD_COUNT);

	static const auto ast = [](void* astArg) -> int {
		const auto lock = static_cast<Lock*>(astArg);
		const auto threadData = lock->threadData;

		std::lock_guard localMutexGuard(threadData->localMutex);

		if (threadData->shutdown)
			return 0;

		fb_assert(!lock->locked || lock->lockId);

		if (lock->locked)
		{
			if (!threadData->lockManager->dequeue(lock->lockId))
				fb_assert(false);

			const auto num = threadData->locks.erase(lock->lockId);
			fb_assert(num == 1);
			delete lock;
		}
		else
			lock->blocking = true;

		return 0;
	};

	//std::mutex globalMutex;

	for (unsigned threadNum = 0u; threadNum < THREAD_COUNT; ++threadNum)
	{
		threads.emplace_back([&, threadNum]() {
			ThreadData threadData;
			threadData.threadId = std::this_thread::get_id();
			threadData.lockManager = lockManager.get();

			LockManagerTestCallbacks callbacks(&threadData.localMutex);
			FbLocalStatus statusVector;
			LOCK_OWNER_T ownerId = threadNum + 1;
			SLONG ownerHandle = 0;

			lockManager->initializeOwner(&statusVector, ownerId, LCK_OWNER_attachment, &ownerHandle);
			threadData.ownerHandle = ownerHandle;

			latch.arrive_and_wait();

			for (unsigned i = 0; i < ITERATION_COUNT; ++i)
			{
				//std::lock_guard globalMutexGuard(globalMutex);
				std::lock_guard localMutexGuard(threadData.localMutex);

				const auto lock = new Lock();
				lock->threadData = &threadData;
				lock->key = i;

				lock->lockId = lockManager->enqueue(callbacks, &statusVector, 0,
					LCK_expression, (const UCHAR*) &lock->key, sizeof(lock->key), LCK_EX,
					ast, lock, 0, LCK_WAIT, ownerHandle);

				if (lock->lockId)
				{
					++lockSuccess;

					if (lock->blocking)
					{
						lockManager->dequeue(lock->lockId);
						delete lock;
					}
					else
					{
						lock->locked = true;
						threadData.locks.insert({ lock->lockId, lock });
					}
				}
				else
				{
					fb_assert(false);
					delete lock;
					++lockFail;
				}
			}

			{	// scope
				//std::lock_guard globalMutexGuard(globalMutex);
				std::lock_guard localMutexGuard(threadData.localMutex);

				threadData.shutdown = true;

				for (const auto [lockId, lock] : threadData.locks)
				{
					fb_assert(!lock->blocking);
					fb_assert(lock->locked);
					fb_assert(lock->lockId);
					lockManager->dequeue(lockId);
				}

				lockManager->shutdownOwner(callbacks, &ownerHandle);

				for (const auto [lockId, lock] : threadData.locks)
					delete lock;

				threadData.locks.clear();
			}

		});
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_CHECK_EQUAL(lockFail.load(), 0u);
	BOOST_CHECK_EQUAL(lockSuccess.load(), THREAD_COUNT * ITERATION_COUNT);

	lockManager.reset();
}


BOOST_AUTO_TEST_SUITE_END()	// LockManagerTests
BOOST_AUTO_TEST_SUITE_END()	// LockManagerSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
