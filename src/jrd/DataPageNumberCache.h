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

#ifndef JRD_DATAPAGENUMBERCACHE_H
#define JRD_DATAPAGENUMBERCACHE_H

#include "firebird.h"
#include "../common/classes/Hash.h"

namespace Jrd
{
	// Per-attachment cache of relation data page numbers. It is shared by all relations
	// used by the attachment, therefore the map key is {pageSetId, sequence}.
	// Given an attachment serializes access to its own data, no synchronization is needed.
	// Cached numbers are validated before use, so stale entries are harmless.
	//
	// Storage and replacement policy:
	//
	//   - the capacity is fixed and all slots are preallocated, so neither a hit nor a
	//     miss allocates memory;
	//   - unused slots are kept on a singly linked free list and taken from it before
	//     anything is evicted, so filling the cache does not disturb the clock hand;
	//   - once the free list is empty, CLOCK (second chance) replacement is used as a
	//     cheap approximation of LRU. Every slot has one reference bit, set on a hit
	//     and on an insert/update and cleared only by the sweep. The hand walks forward
	//     and evicts the first slot whose reference bit is clear, so a slot touched
	//     since the previous visit survives one more revolution. Hits only set a bit
	//     instead of relinking entries, which keeps the hit path to one hash lookup and
	//     one store.

	class DataPageNumberCache
	{
	public:
		// Prime number of hash buckets just below the capacity, keeping the load
		// factor at ~1.0 when every slot is occupied
		static constexpr FB_SIZE_T CAPACITY = 1024;
		static constexpr FB_SIZE_T HASH_SIZE = 1021; // prime just below CAPACITY

		// Monotonic identifier of a specific incarnation of a relation page set.
		// Assigned when a RelationPages object is created, reset or reused, so
		// dropped, moved or recycled page sets never alias cached entries of an
		// earlier incarnation.

		typedef FB_UINT64 PageSetId;

		struct Key
		{
			PageSetId pageSetId = 0;
			ULONG sequence = 0;

			bool operator==(const Key& other) const noexcept
			{
				return pageSetId == other.pageSetId && sequence == other.sequence;
			}
		};

	private:
		struct Item;
		typedef Firebird::HashTable<Item, HASH_SIZE, Key, Item, Item> ItemMap;

		// One cache slot. Slots are stored in the preallocated array below and are
		// either linked into the hash table (linked == true) or waiting on the free
		// list (linked == false, chained through nextFree). The reference bit is the
		// only per-slot state the replacement policy maintains.

		struct Item : public ItemMap::Entry
		{
			Key key{};
			ULONG pageNumber = 0;		// 0 is not a valid data page number
			bool referenced = false;	// clock reference bit, set on any access
			bool linked = false;		// present in the hash table
			Item* nextFree = nullptr;	// free list link, used when not linked

			static Key generate(Item& item) noexcept
			{
				return item.key;
			}

			// Combines pageSetId and sequence using the 64-bit golden-ratio constant
			// (2^64 / phi, as in boost::hash_combine) so that swapped or sequential
			// pairs do not collide, then applies the first mixing stage of SplitMix64
			// (Stafford's mix13 / MurmurHash3 fmix64) before folding modulo the prime
			// bucket count.

			static FB_SIZE_T hash(const Key& key, FB_SIZE_T hashSize) noexcept
			{
				FB_UINT64 value = key.pageSetId;
				value += static_cast<FB_UINT64>(key.sequence) * 0x9E3779B97F4A7C15ULL;
				value ^= value >> 30;
				value *= 0xBF58476D1CE4E5B9ULL;
				value ^= value >> 27;
				return static_cast<FB_SIZE_T>(value % hashSize);
			}

			bool isEqual(const Key& k) const override
			{
				return key == k;
			}

			Item* get() override
			{
				return this;
			}
		};

	public:
		DataPageNumberCache() noexcept
		{
			// Chain all slots into the free list, in array order, so the cache fills
			// up without invoking the replacement policy

			for (FB_SIZE_T i = 1; i < CAPACITY; ++i)
				m_items[i - 1].nextFree = &m_items[i];

			m_free = m_items;
		}

		// Returns the cached page number, or 0 when the sequence is not cached
		// (0 is not a valid data page number). A hit refreshes the reference bit,
		// giving the entry a second chance when the clock hand next passes it.

		ULONG getDPNumber(PageSetId pageSetId, ULONG sequence)
		{
			if (const auto item = m_map.lookup(Key{pageSetId, sequence}))
			{
				item->referenced = true; // give the entry a second chance
				fb_assert(item->pageNumber);
				return item->pageNumber;
			}

			return 0;
		}

		// Stores a non-zero page number. An update only refreshes the reference bit,
		// so it does not disturb the clock hand; a new entry is taken from the free
		// list or evicted by the hand, then linked into the hash table.

		void setDPNumber(PageSetId pageSetId, ULONG sequence, ULONG pageNumber)
		{
			fb_assert(pageNumber);

			const Key key{pageSetId, sequence};

			if (const auto item = m_map.lookup(key))
			{
				item->pageNumber = pageNumber;
				item->referenced = true;
				return;
			}

			Item* const item = acquire(key);
			item->pageNumber = pageNumber;
			item->referenced = true;
			item->linked = true;
			const bool added = m_map.add(item);
			fb_assert(added);
		}

		// Drops a cached page number when the data page was freed or failed validation,
		// returning the slot to the free list

		void invalidateDPNumber(PageSetId pageSetId, ULONG sequence) noexcept
		{
			if (const auto item = m_map.lookup(Key{pageSetId, sequence}))
				release(item);
		}

		// Drops all cached page numbers belonging to the given page set incarnation,
		// returning their slots to the free list so they do not occupy capacity or
		// require clock eviction

		void invalidateDPNumbers(PageSetId pageSetId) noexcept
		{
			for (FB_SIZE_T i = 0; i < CAPACITY; ++i)
			{
				auto& item = m_items[i];
				if (item.linked && item.key.pageSetId == pageSetId)
					release(&item);
			}
		}

	private:
		// Removes the entry from the hash table and returns the slot to the free list,
		// so the next insertion can reuse it without waiting for the clock hand to reach it

		void release(Item* item) noexcept
		{
			item->unLink();
			item->referenced = false;
			item->linked = false;
			item->pageNumber = 0;
			item->nextFree = m_free;
			m_free = item;
		}

		// Obtains a slot for a new entry: from the free list while the cache is not full,
		// otherwise by evicting a slot with the clock hand

		Item* acquire(const Key& key) noexcept
		{
			// Fast path: reuse a free slot, the clock hand is not involved

			if (m_free)
			{
				Item* const item = m_free;
				m_free = item->nextFree;
				item->nextFree = nullptr;
				item->key = key;
				return item;
			}

			// CLOCK (second chance) replacement, used once the free list is empty.
			// The hand keeps its position between insertions and examines slots in
			// array order:
			//
			//   - an unlinked slot is on the free list and is not a candidate
			//     (defensive test: all slots are linked while the free list is empty);
			//   - a linked slot with the reference bit set was used since the previous
			//     visit: clear the bit and skip it, granting one more revolution;
			//   - a linked slot with the reference bit clear is the victim: unlink it
			//     from the hash table and let the caller refill it in place.
			//
			// The sweep cannot loop forever: each reference bit it meets is cleared,
			// so after at most one full revolution only clear bits remain and the next
			// examined slot is evicted. Since a hit costs a single bit store, the
			// replacement policy stays amortized O(1) and no LRU list or timestamp
			// needs to be maintained on the hit path.

			while (true)
			{
				auto& item = m_items[m_hand];
				m_hand = (m_hand + 1) % CAPACITY;

				if (!item.linked)
					continue;

				if (item.referenced)
				{
					item.referenced = false;
					continue;
				}

				item.unLink();
				item.linked = false;
				item.key = key;
				return &item;
			}
		}

		ItemMap m_map;				// hash table
		Item m_items[CAPACITY];		// preallocated slots
		Item* m_free = nullptr;		// unused slots, linked via nextFree
		FB_SIZE_T m_hand = 0;		// clock hand, position preserved between evictions
	};

} // namespace Jrd

#endif // JRD_DATAPAGENUMBERCACHE_H
