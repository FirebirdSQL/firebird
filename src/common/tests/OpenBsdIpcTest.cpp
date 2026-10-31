#include "firebird.h"

#ifdef OPENBSD

#include "boost/test/unit_test.hpp"
#include "../common/isc_s_proto.h"
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

using namespace Firebird;

namespace {

struct TestHeader : MemoryHeader
{
	event_t event;
	event_t ready;
	unsigned counter;
};

class TestMemory : public IpcObject
{
public:
	explicit TestMemory(const char* existingName = nullptr)
		: owner(!existingName)
	{
		static unsigned sequence = 0;
		if (existingName)
			name = existingName;
		else
			name.printf("fb_openbsd_ipc_test_%ld_%u", long(getpid()), ++sequence);
		memory.reset(new SharedMemory<TestHeader>(name.c_str(), sizeof(TestHeader), this));
	}

	~TestMemory()
	{
		if (owner)
		{
			memory->eventFini(&memory->getHeader()->event);
			memory->eventFini(&memory->getHeader()->ready);
			memory->removeMapFile();
		}
	}

	bool initialize(SharedMemoryBase* shared, bool init) override
	{
		auto* header = static_cast<TestHeader*>(shared->sh_mem_header);
		if (init)
		{
			initHeader(header);
			header->counter = 0;
			if (shared->eventInit(&header->event) != FB_SUCCESS ||
				shared->eventInit(&header->ready) != FB_SUCCESS)
				system_call_failed::raise("eventInit");
		}
		else
			checkHeader(header);
		return true;
	}

	void mutexBug(int code, const char* text) override
	{
		system_call_failed::raise(text, code);
	}

	USHORT getType() const override { return 0x7fff; }
	USHORT getVersion() const override { return 1; }
	const char* getName() const override { return name.c_str(); }

	PathName name;
	std::unique_ptr<SharedMemory<TestHeader>> memory;

private:
	bool owner;
};

class Child
{
public:
	Child(const char* mode, const char* name)
	{
		const auto& suite = boost::unit_test::framework::master_test_suite();
		const char* args[] = {suite.argv[0], "--run_test=OpenBsdIpcSuite/ChildProcess",
			"--", mode, name, nullptr};
		const int rc = posix_spawnp(&pid, args[0], nullptr, nullptr,
			const_cast<char* const*>(args), environ);
		if (rc)
			system_call_failed::raise("posix_spawnp", rc);
	}

	~Child()
	{
		if (pid > 0)
		{
			kill(pid, SIGKILL);
			int status;
			while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
			{ }
		}
	}

	bool succeeded()
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (std::chrono::steady_clock::now() < deadline)
		{
			int status;
			const pid_t result = waitpid(pid, &status, WNOHANG);
			if (result == pid)
			{
				pid = -1;
				return WIFEXITED(status) && WEXITSTATUS(status) == 0;
			}
			if (result == -1 && errno != EINTR)
			{
				if (errno == ECHILD)
					pid = -1;
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	}

private:
	pid_t pid = -1;
};

void increment(SharedMemory<TestHeader>* memory)
{
	for (unsigned i = 0; i < 1000; ++i)
	{
		SharedMutexGuard guard(memory);
		const unsigned value = memory->getHeader()->counter;
		if (i % 16 == 0)
			std::this_thread::yield();
		memory->getHeader()->counter = value + 1;
	}
}

} // namespace

BOOST_AUTO_TEST_SUITE(OpenBsdIpcSuite)

// A fresh executable avoids inheriting the parent's process-local lock registry.
BOOST_AUTO_TEST_CASE(ChildProcess)
{
	const auto& suite = boost::unit_test::framework::master_test_suite();
	if (suite.argc != 3)
		return;

	alarm(10);
	TestMemory shared(suite.argv[2]);
	if (strcmp(suite.argv[1], "increment") == 0)
		increment(shared.memory.get());
	else if (strcmp(suite.argv[1], "contend") == 0)
	{
		const bool acquired = shared.memory->mutexLockCond();
		if (acquired)
			shared.memory->mutexUnlock();
		BOOST_TEST(!acquired);
		BOOST_REQUIRE(shared.memory->eventPost(&shared.memory->getHeader()->ready) == FB_SUCCESS);
		SharedMutexGuard guard(shared.memory.get());
		++shared.memory->getHeader()->counter;
	}
	else if (strcmp(suite.argv[1], "ping-pong") == 0)
	{
		auto* header = shared.memory->getHeader();
		for (SLONG i = 1; i <= 500; ++i)
		{
			BOOST_REQUIRE(shared.memory->eventWait(&header->ready, i, 3000000) == FB_SUCCESS);
			header->counter = i;
			BOOST_REQUIRE(shared.memory->eventPost(&header->event) == FB_SUCCESS);
		}
	}
	else if (strcmp(suite.argv[1], "owner-exit") == 0)
	{
		shared.memory->mutexLock();
		shared.memory->getHeader()->counter = 41;
		_exit(0);
	}
	else if (strcmp(suite.argv[1], "post") == 0)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		auto* event = shared.memory->mapObject<event_t>(&status, offsetof(TestHeader, event));
		BOOST_REQUIRE(event);
		BOOST_TEST(event != &shared.memory->getHeader()->event);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		BOOST_TEST(shared.memory->eventPost(event) == FB_SUCCESS);
		shared.memory->unmapObject(&status, &event);
		BOOST_TEST(!status.hasData());
	}
	else
		BOOST_FAIL("Unknown IPC child operation");
	alarm(0);
}

BOOST_AUTO_TEST_CASE(ThreadExclusion, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	std::atomic<unsigned> ready{0};
	std::atomic<bool> start{false};
	std::array<std::thread, 4> threads;
	for (auto& thread : threads)
	{
		thread = std::thread([&] {
			ready.fetch_add(1);
			while (!start.load())
				std::this_thread::yield();
			increment(shared.memory.get());
		});
	}
	while (ready.load() != threads.size())
		std::this_thread::yield();
	start.store(true);
	for (auto& thread : threads)
		thread.join();
	BOOST_TEST(shared.memory->getHeader()->counter == 4000);
}

BOOST_AUTO_TEST_CASE(ProcessExclusion, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	Child child("increment", shared.name.c_str());
	increment(shared.memory.get());
	BOOST_REQUIRE(child.succeeded());
	BOOST_TEST(shared.memory->getHeader()->counter == 2000);
}

BOOST_AUTO_TEST_CASE(OwnerExit, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	Child child("owner-exit", shared.name.c_str());
	BOOST_REQUIRE(child.succeeded());
	BOOST_REQUIRE(shared.memory->mutexLockCond());
	++shared.memory->getHeader()->counter;
	shared.memory->mutexUnlock();
	BOOST_TEST(shared.memory->getHeader()->counter == 42);
}

BOOST_AUTO_TEST_CASE(ProcessContention, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	std::unique_ptr<Child> child;
	{
		SharedMutexGuard guard(shared.memory.get());
		auto* ready = &shared.memory->getHeader()->ready;
		const SLONG value = shared.memory->eventClear(ready);
		child.reset(new Child("contend", shared.name.c_str()));
		BOOST_TEST(shared.memory->eventWait(ready, value, 3000000) == FB_SUCCESS);
	}
	BOOST_REQUIRE(child->succeeded());
	BOOST_TEST(shared.memory->getHeader()->counter == 1);
}

BOOST_AUTO_TEST_CASE(EventPingPong, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	auto* header = shared.memory->getHeader();
	Child child("ping-pong", shared.name.c_str());
	for (SLONG i = 1; i <= 500; ++i)
	{
		BOOST_REQUIRE(shared.memory->eventPost(&header->ready) == FB_SUCCESS);
		BOOST_REQUIRE(shared.memory->eventWait(&header->event, i, 3000000) == FB_SUCCESS);
		BOOST_TEST(header->counter == unsigned(i));
	}
	BOOST_REQUIRE(child.succeeded());
}

BOOST_AUTO_TEST_CASE(EventAcrossMappings, * boost::unit_test::timeout(20))
{
	TestMemory shared;
	auto* event = &shared.memory->getHeader()->event;
	const SLONG value = shared.memory->eventClear(event);
	Child child("post", shared.name.c_str());
	BOOST_TEST(shared.memory->eventWait(event, value, 3000000) == FB_SUCCESS);
	BOOST_TEST(child.succeeded());
}

BOOST_AUTO_TEST_CASE(EventTimeoutAndWrap)
{
	TestMemory shared;
	auto* event = &shared.memory->getHeader()->event;
	__atomic_store_n(&event->event_count, UINT32_MAX, __ATOMIC_RELAXED);
	const SLONG value = shared.memory->eventClear(event);
	BOOST_TEST(value == 0);
	BOOST_TEST(shared.memory->eventWait(event, value, 20000) == FB_FAILURE);
	BOOST_TEST(shared.memory->eventPost(event) == FB_SUCCESS);
	BOOST_TEST(shared.memory->eventWait(event, value, 20000) == FB_SUCCESS);
}

BOOST_AUTO_TEST_SUITE_END()	// OpenBsdIpcSuite

#endif // OPENBSD
