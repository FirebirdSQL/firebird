/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		PageToBufferMap.h
 *	DESCRIPTION:	Support of disk cache manager
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 *  The Original Code was created by Vladyslav Khorsun for the
 *  Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2023 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_PAGE_TO_BUFFER_MAP
#define JRD_PAGE_TO_BUFFER_MAP

#include "../common/classes/Hash.h"
#include "../jrd/cch.h"

namespace Jrd {

// PageToBufferMap used to cache pointers to the often used page buffers.
// Its purpose is to avoid more costly usage of shared hash table.

class PageToBufferMap
{
	static const size_t MAP_SIZE = 64;

public:
	explicit PageToBufferMap(MemoryPool& pool) :
		m_map(pool)
	{
		Item* items = FB_NEW_POOL(pool) Item[MAP_SIZE];
		for (FB_SIZE_T i = 1; i < MAP_SIZE; i++)
			items[i - 1].m_next = &items[i];

		m_free = items;
	}

	BufferDesc* get(PageNumber page)
	{
		Item* item = m_map.lookup(page);

		if (item)
		{
			if (item->m_bdb->bdb_page == page)
			{
				// Move item into MRU position
				if (m_list != item)
				{
					listRemove(item);
					listInsert(item);
				}
				return item->m_bdb;
			}

			// bdb was reassigned
			remove(page);
		}

		return nullptr;
	}

	void put(BufferDesc* bdb)
	{
		Item* item = m_map.lookup(bdb->bdb_page);
		if (item)
		{
			fb_assert(item->m_bdb != bdb);
			if (m_list != item)
				listRemove(item);
		}
		else
		{
			item = getFreeItem();
			item->m_page = bdb->bdb_page;
			m_map.add(item);
		}

		item->m_bdb = bdb;
		if (m_list != item)
			listInsert(item);
	}

	void remove(PageNumber page)
	{
		Item* item = m_map.remove(page);

		fb_assert(item);
		if (!item)
			return;

		listRemove(item);

		item->m_bdb = nullptr;
		item->m_next = m_free;
		m_free = item;
	}

private:
	struct Item;
	using HashTableType = Firebird::HashTable<Item, MAP_SIZE, PageNumber, Item, Item>;

	struct Item : public HashTableType::Entry
	{
		PageNumber m_page;
		BufferDesc* m_bdb = nullptr;
		Item* m_next = nullptr;			// LRU list	support
		Item* m_prev = nullptr;

		// KeyOfValue
		static PageNumber generate(Item& item)
		{
			return item.m_page;
		}

		// Hash function
		static FB_SIZE_T hash(const PageNumber& value, FB_SIZE_T hashSize)
		{
			return value.getPageNum() % hashSize;
		}

		// HashTable::Entry
		bool isEqual(const PageNumber& page) const override
		{
			return this->m_page == page;
		}

		Item* get()	override
		{
			return this;
		}
	};

	Item* getFreeItem()
	{
		Item* item = m_free;
		if (item)
		{
			m_free = m_free->m_next;
			item->m_next = nullptr;
			return item;
		}

		// get least recently used item from list
		fb_assert(m_list != nullptr);

		item = m_list->m_prev;
		listRemove(item);

		if (m_map.remove(item->m_page) != item)
			fb_assert(false);

		item->m_bdb = nullptr;
		return item;
	}


	void listRemove(Item* item)
	{
		// remove item from list
		fb_assert(m_list != nullptr);
		fb_assert(item->m_next);
		fb_assert(item->m_prev);

		if (item->m_next == item)
		{
			fb_assert(item->m_prev == item);
			fb_assert(m_list == item);
			m_list = nullptr;
		}
		else
		{
			if (m_list == item)
				m_list = item->m_next;

			item->m_next->m_prev = item->m_prev;
			item->m_prev->m_next = item->m_next;
		}

		item->m_next = item->m_prev = nullptr;
	}

	void listInsert(Item* item)
	{
		// put item at the list head
		if (m_list != nullptr)
		{
			fb_assert(m_list->m_next);
			fb_assert(m_list->m_prev);

			item->m_next = m_list;
			item->m_prev = m_list->m_prev;

			item->m_next->m_prev = item;
			item->m_prev->m_next = item;
		}
		else
		{
			item->m_next = item;
			item->m_prev = item;
		}

		m_list = item;
	}

	HashTableType m_map;
	Item* m_list = nullptr;		// head of LRU list
	Item* m_free = nullptr;		// unused items
};

} // namespace Jrd

#endif // JRD_PAGE_TO_BUFFER_MAP
