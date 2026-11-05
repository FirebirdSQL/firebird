#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/alloc.h"

// The test needs RLIMIT_AS. On Android ARM builds the tests run through the
// emulator's binary translator, which aborts when its own mmap() hits the limit.
#if defined(LINUX) && !defined(ANDROID)
#define ALLOC_FAILURE_TEST
#endif

#ifdef ALLOC_FAILURE_TEST
#include <sys/mman.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(AllocSuite)


BOOST_AUTO_TEST_SUITE(AllocTests)

#ifdef ALLOC_FAILURE_TEST

namespace
{
	// Medium block that fits 6 times into a 64K extent
	constexpr size_t MEDIUM_SIZE = 10000;
	constexpr unsigned BLOCKS_PER_EXTENT = 6;

	// Medium block that needs a whole 64K extent
	constexpr size_t EXTENT_SIZED = 60000;

	size_t getVmSize()
	{
		size_t kb = 0;

		if (FILE* f = fopen("/proc/self/status", "r"))
		{
			char line[256];
			while (fgets(line, sizeof(line), f))
			{
				if (strncmp(line, "VmSize:", 7) == 0)
					kb = strtoul(line + 7, NULL, 10);
			}
			fclose(f);
		}

		return kb * 1024;
	}

	// Makes any new mapping fail until restored. Returns false if the limit can't be changed.
	bool limitAddressSpace(bool limit)
	{
		struct rlimit lim;
		if (getrlimit(RLIMIT_AS, &lim) != 0)
			return false;

		if (limit)
		{
			const size_t vmSize = getVmSize();
			if (!vmSize)
				return false;

			lim.rlim_cur = vmSize + 16 * 1024;
		}
		else
			lim.rlim_cur = lim.rlim_max;

		return setrlimit(RLIMIT_AS, &lim) == 0;
	}

	// RLIMIT_AS is not enforced everywhere, e.g. QEMU user-mode emulation ignores it
	bool addressSpaceLimitWorks()
	{
		if (!limitAddressSpace(true))
			return false;

		const size_t probeSize = 64 * 1024;
		void* probe = mmap(NULL, probeSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		const bool works = (probe == MAP_FAILED);
		if (!works)
			munmap(probe, probeSize);

		return limitAddressSpace(false) && works;
	}
}

// When a new medium extent can't be allocated, the current extent must not lose
// its usage reference more than once. Otherwise it's reclaimed while blocks
// allocated from it are still in use and unlinkBlocks() treats them as free.
BOOST_AUTO_TEST_CASE(MediumExtentAllocationFailure)
{
	if (!addressSpaceLimitWorks())
	{
		BOOST_TEST_MESSAGE("RLIMIT_AS is not enforced, test skipped");
		return;
	}

	// Empty the cache of released extents, so that next extent comes from the OS
	MemoryPool* drainPool = MemoryPool::createPool();
	for (unsigned i = 0; i < 32; ++i)
		drainPool->allocate(EXTENT_SIZED);

	MemoryPool* pool = MemoryPool::createPool();

	// Exhaust redirection of small medium blocks to parent pool
	for (unsigned i = 0; i < 16; ++i)
		pool->allocate(2000);

	void* first[BLOCKS_PER_EXTENT];
	for (unsigned i = 0; i < BLOCKS_PER_EXTENT; ++i)
		first[i] = pool->allocate(MEDIUM_SIZE);

	// Allocation of next extent fails twice
	unsigned failures = 0;
	BOOST_REQUIRE(limitAddressSpace(true));
	for (unsigned i = 0; i < 2; ++i)
	{
		try
		{
			pool->allocate(MEDIUM_SIZE);
		}
		catch (const std::bad_alloc&)
		{
			++failures;
		}
	}
	BOOST_REQUIRE(limitAddressSpace(false));
	BOOST_REQUIRE_EQUAL(failures, 2u);

	// Fill second extent and move to the third one
	void* second[BLOCKS_PER_EXTENT];
	for (unsigned i = 0; i < BLOCKS_PER_EXTENT; ++i)
		second[i] = pool->allocate(MEDIUM_SIZE);
	pool->allocate(MEDIUM_SIZE);

	// Keep 2 blocks of the first extent in use
	for (unsigned i = 0; i < BLOCKS_PER_EXTENT - 2; ++i)
		pool->deallocate(first[i]);
	memset(first[4], 0, MEDIUM_SIZE);
	memset(first[5], 0, MEDIUM_SIZE);

	// Second extent becomes free: the first one must not be reclaimed
	for (unsigned i = 0; i < BLOCKS_PER_EXTENT; ++i)
		pool->deallocate(second[i]);

	pool->deallocate(first[4]);
	pool->deallocate(first[5]);

	MemoryPool::deletePool(pool);
	MemoryPool::deletePool(drainPool);
}

#endif // ALLOC_FAILURE_TEST

BOOST_AUTO_TEST_SUITE_END()	// AllocTests


BOOST_AUTO_TEST_SUITE_END()	// AllocSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
