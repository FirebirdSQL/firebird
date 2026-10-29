#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/alloc.h"

#ifdef LINUX
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(AllocSuite)


BOOST_AUTO_TEST_SUITE(AllocTests)

#ifdef LINUX

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

	// Makes any new mapping fail until restored
	void limitAddressSpace(bool limit)
	{
		struct rlimit lim;
		getrlimit(RLIMIT_AS, &lim);
		lim.rlim_cur = limit ? getVmSize() + 16 * 1024 : lim.rlim_max;
		setrlimit(RLIMIT_AS, &lim);
	}
}

// When a new medium extent can't be allocated, the current extent must not lose
// its usage reference more than once. Otherwise it's reclaimed while blocks
// allocated from it are still in use and unlinkBlocks() treats them as free.
BOOST_AUTO_TEST_CASE(MediumExtentAllocationFailure)
{
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
	limitAddressSpace(true);
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
	limitAddressSpace(false);
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

#endif // LINUX

BOOST_AUTO_TEST_SUITE_END()	// AllocTests


BOOST_AUTO_TEST_SUITE_END()	// AllocSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
