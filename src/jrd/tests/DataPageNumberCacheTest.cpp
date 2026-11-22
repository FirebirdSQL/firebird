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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2026 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/Relation.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(DataPageNumberCacheSuite)

namespace
{
	MemoryPool& testPool()
	{
		// RelationPages retires its pointer page generation through the CDS
		// hazard pointer library, so the current thread must be attached to it.
		// In the engine this happens on the first hazard pointer use; a test
		// suite may run in isolation, so attach it explicitly here.

#ifndef CDS_UNAVAILABLE
		cds::gc::dhp::smr::attach_thread();
#endif

		return *getDefaultMemoryPool();
	}
}

// The cache is keyed by {pageSetId, sequence}. Different RelationPages
// objects (even with the same page space / instance id) must not collide.
BOOST_AUTO_TEST_CASE(KeyIncludesPageSet)
{
	MemoryPool& pool = testPool();
	RelationPages first(pool, DB_PAGE_SPACE);
	RelationPages second(pool, DB_PAGE_SPACE + 1);
	RelationPages third(pool, DB_PAGE_SPACE);
	RelationPages fourth(pool, DB_PAGE_SPACE, 10);
	DataPageNumberCache cache;

	cache.setDPNumber(first.getPageSetId(), 7, 1001);
	cache.setDPNumber(second.getPageSetId(), 7, 2002);
	cache.setDPNumber(third.getPageSetId(), 7, 3003);
	cache.setDPNumber(fourth.getPageSetId(), 7, 4004);

	BOOST_TEST(cache.getDPNumber(first.getPageSetId(), 7) == 1001U);
	BOOST_TEST(cache.getDPNumber(second.getPageSetId(), 7) == 2002U);
	BOOST_TEST(cache.getDPNumber(third.getPageSetId(), 7) == 3003U);
	BOOST_TEST(cache.getDPNumber(fourth.getPageSetId(), 7) == 4004U);
	BOOST_TEST(cache.getDPNumber(first.getPageSetId(), 8) == 0U);
	BOOST_TEST(cache.getDPNumber(second.getPageSetId(), 8) == 0U);
	BOOST_TEST(cache.getDPNumber(third.getPageSetId(), 8) == 0U);
	BOOST_TEST(cache.getDPNumber(fourth.getPageSetId(), 8) == 0U);
}

// Recycling a RelationPages object for another instance (or resetting its
// page space, even to the same value) must make all entries cached for its
// previous incarnation unreachable.
BOOST_AUTO_TEST_CASE(ReuseChangesIdentity)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, 99);
	DataPageNumberCache cache;

	cache.setDPNumber(pages.getPageSetId(), 1, 42);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 42U);

	pages.reset(nullptr);					// page space becomes INVALID
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 0U);

	cache.setDPNumber(pages.getPageSetId(), 1, 43);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 43U);

	pages.reset(nullptr, 99);				// reset back to the same page space
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 0U);

	pages.reuse(nullptr, 99, 7);			// reuse for an instance
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 0U);

	cache.setDPNumber(pages.getPageSetId(), 1, 44);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 44U);

	pages.reset(nullptr);					// reset while instanceId != 0
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 0U);

	cache.setDPNumber(pages.getPageSetId(), 1, 45);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 45U);

	pages.reuse(nullptr, 99, 7);			// reuse with the exact same arguments
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 0U);
}

// Retaining a GTT instance across a retaining commit (setInstanceId) keeps its
// physical pages, so cached data page numbers remain valid.
BOOST_AUTO_TEST_CASE(SetInstanceIdPreservesIdentity)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, 99, 5);
	DataPageNumberCache cache;

	cache.setDPNumber(pages.getPageSetId(), 1, 42);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 42U);

	pages.setInstanceId(6);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 42U);
}

// The map is limited to CAPACITY entries and uses clock replacement. Feeding
// more distinct keys than the capacity must keep the hot (recent) ones.
BOOST_AUTO_TEST_CASE(CapacityAndClockEviction)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, DB_PAGE_SPACE);
	DataPageNumberCache cache;

	const ULONG total = DataPageNumberCache::CAPACITY * 2;
	for (ULONG i = 0; i < total; ++i)
		cache.setDPNumber(pages.getPageSetId(), i, i + 1);

	// Everything inserted after the table filled up is still hot
	for (ULONG i = total - 100; i < total; ++i)
		BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), i) == i + 1);

	// One of the very first entries must have been evicted
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 0) == 0U);
}

// Updating an existing key overwrites its page number in place; invalidating
// an entry (or an entire page set) returns its slot(s) to the free list and
// must not resurrect invalidated keys on subsequent inserts.
BOOST_AUTO_TEST_CASE(RemoveAndReuse)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, DB_PAGE_SPACE);
	RelationPages other(pool, DB_PAGE_SPACE);
	DataPageNumberCache cache;

	cache.setDPNumber(pages.getPageSetId(), 10, 100);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 10) == 100U);

	cache.setDPNumber(pages.getPageSetId(), 10, 105);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 10) == 105U);

	cache.invalidateDPNumber(pages.getPageSetId(), 10);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 10) == 0U);

	cache.setDPNumber(pages.getPageSetId(), 11, 110);
	cache.setDPNumber(pages.getPageSetId(), 12, 120);
	cache.setDPNumber(other.getPageSetId(), 11, 210);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 11) == 110U);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 10) == 0U);

	cache.invalidateDPNumbers(pages.getPageSetId());
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 11) == 0U);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 12) == 0U);
	BOOST_TEST(cache.getDPNumber(other.getPageSetId(), 11) == 210U);
}

// Clock replacement must replace one entry per insert, not a batch. This
// distinguishes it from the previous "drop the oldest half" policy, which
// would remove many still-hot entries at once.
BOOST_AUTO_TEST_CASE(ClockEvictsOneAtATime)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, DB_PAGE_SPACE);
	DataPageNumberCache cache;

	const ULONG cap = DataPageNumberCache::CAPACITY;
	for (ULONG i = 0; i < cap; ++i)
		cache.setDPNumber(pages.getPageSetId(), i, i + 1);

	// One more entry can only replace exactly one existing one
	cache.setDPNumber(pages.getPageSetId(), cap, cap + 1);

	ULONG evicted = 0;
	for (ULONG i = 0; i <= cap; ++i)
	{
		if (!cache.getDPNumber(pages.getPageSetId(), i))
			++evicted;
	}
	BOOST_TEST(evicted == 1U);
}

// A recently used entry must get a second chance and survive the current
// clock round, while a cold entry ahead of the hand is replaced instead.
BOOST_AUTO_TEST_CASE(ClockSecondChance)
{
	MemoryPool& pool = testPool();
	RelationPages pages(pool, DB_PAGE_SPACE);
	DataPageNumberCache cache;

	const ULONG cap = DataPageNumberCache::CAPACITY;
	for (ULONG i = 0; i < cap; ++i)
		cache.setDPNumber(pages.getPageSetId(), i, i + 1);

	// The first extra insert sweeps the hand over all entries, clears their
	// reference bits and evicts sequence 0; the hand now points at sequence 1
	cache.setDPNumber(pages.getPageSetId(), cap, cap + 1);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 0) == 0U);

	// Mark sequence 1 as recently used
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 2U);

	// The next insert must skip sequence 1 (second chance) and evict 2
	cache.setDPNumber(pages.getPageSetId(), cap + 1, cap + 2);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 1) == 2U);
	BOOST_TEST(cache.getDPNumber(pages.getPageSetId(), 2) == 0U);
}

BOOST_AUTO_TEST_SUITE_END()	// DataPageNumberCacheSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
