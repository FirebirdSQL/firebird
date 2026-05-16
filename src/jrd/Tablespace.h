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
 *  The Original Code was created by Roman Simakov
 *  for the RedDatabase project.
 *
 *  Copyright (c) 2018 <roman.simakov@red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_TABLESPACE_H
#define JRD_TABLESPACE_H

#include "../common/classes/alloc.h"
#include "../jrd/MetaName.h"
#include "../jrd/pag.h"
#include "../jrd/SharedReadVector.h"

namespace Jrd
{
	class thread_db;
	class jrd_tra;
	class Lock;

	class Tablespace
	{
		static constexpr unsigned CREATED = 1;
		static constexpr unsigned MODIFIED = 2;
		static constexpr unsigned DELETED = 4;
		static constexpr unsigned BLOCKING = 8;
		static constexpr unsigned OBSOLETE = 16;
		static constexpr unsigned NOALLOC = 32;

	public:
		enum class Operation {	CREATE, ALTER, DROP	};

		static void lock(thread_db* tdbb, ULONG pageSpaceId);

		class UsageList
		{
		public:
			explicit UsageList(MemoryPool& pool)
				: m_tablespaces(pool, 1) // preallocate slot for DB_PAGE_SPACE
			{}

			~UsageList()
			{
				fb_assert(m_tablespaces.isEmpty());
			}

			void add(thread_db* tdbb, ULONG pageSpaceId)
			{
				fb_assert(PageSpace::isTablespace(pageSpaceId));

				if (!test(pageSpaceId))
				{
					const auto tableSpace = Tablespace::lookup(tdbb, pageSpaceId);
					tableSpace->addRef(tdbb);
					m_tablespaces.add(tableSpace);
					set(pageSpaceId);
				}
			}

			void release(thread_db* tdbb)
			{
				for (const auto tableSpace : m_tablespaces)
					tableSpace->release(tdbb);

				m_tablespaces.clear();
				memset(m_bits, 0, sizeof(m_bits));
			}

		private:
			static_assert((TRANS_PAGE_SPACE + 1) % BITS_PER_LONG == 0);
			ULONG m_bits[(TRANS_PAGE_SPACE + 1) / BITS_PER_LONG];
			Firebird::Array<Tablespace*> m_tablespaces;

			inline void set(ULONG id)
			{
				m_bits[id / BITS_PER_LONG] |= (1UL << (id % BITS_PER_LONG));
			}

			inline bool test(ULONG id)
			{
				return (m_bits[id / BITS_PER_LONG] & (1UL << (id % BITS_PER_LONG))) != 0;
			}
		};

		class Cache
		{
		public:
			explicit Cache(MemoryPool& pool)
			{
				// predefined DB_PAGE_SPACE
				if (!m_tablespaces.writeAccessor()->add(FB_NEW_POOL(pool) Tablespace(pool)))
					fb_assert(false);
			}

			~Cache() = default;

			Cache(const Cache&) = delete;
			Cache& operator=(const Cache&) = delete;

			Tablespace* get(ULONG id);
			Tablespace* get(const MetaName& name);

			void store(Tablespace* tabpeSpace);
			void remove(ULONG id);
			void release(thread_db* tdbb);
			void rollback(thread_db* tdbb, jrd_tra* transaction);

		private:
			SharedReadVector<Tablespace*, 8> m_tablespaces;
			Firebird::Mutex m_mutex;
		};

		explicit Tablespace(MemoryPool& pool, ULONG id = DB_PAGE_SPACE)
			: m_id(id), m_name(pool), m_fileName(pool)
		{}

		~Tablespace();

		Tablespace(const Tablespace&) = delete;
		Tablespace& operator=(const Tablespace&) = delete;

		ULONG getId() const
		{
			return m_id;
		}

		const MetaName& getName() const
		{
			static const MetaName primaryName = PRIMARY_TABLESPACE_NAME;
			return m_name.hasData() ? m_name : primaryName;
		}

		static Tablespace* create(thread_db* tdbb, ULONG id);

		static Tablespace* lookup(thread_db* tdbb, ULONG id, bool open = true);
		static Tablespace* lookup(thread_db* tdbb, const MetaName& name);

		static ULONG lookupId(thread_db* tdbb, const MetaName& name)
		{
			if (const auto tableSpace = lookup(tdbb, name))
				return tableSpace->getId();

			fb_assert(false);
			return DB_PAGE_SPACE;
		}

		void mark(thread_db* tdbb, Operation operation, jrd_tra* transaction, const Firebird::PathName& fileName);
		void commit(thread_db* tdbb, jrd_tra* transaction);
		void rollback(thread_db* tdbb, jrd_tra* transaction);

		void addRef(thread_db* tdbb);
		void release(thread_db* tdbb, bool force = false);

		bool isUsed() const
		{
			return (m_useCount > 0);
		}

	private:
		const ULONG m_id;						// tablespace ID (aka pagespace ID)
		MetaName m_name;						// tablespace name
		Firebird::PathName m_fileName;			// file name
		Firebird::AutoPtr<Lock> m_lock;			// existence lock
		int m_useCount = 0;
		unsigned m_flags = 0;
		jrd_tra* m_transaction = nullptr;

		void init(const MetaName& name, const Firebird::PathName& fileName, Lock* lock)
		{
			fb_assert(m_name.isEmpty() && m_fileName.isEmpty() && !m_lock);

			m_name = name;
			m_fileName = fileName;
			m_lock = lock;

			m_flags |= NOALLOC;
		}

		void allocate(thread_db* tdbb, bool create);

		inline bool isVisible(thread_db* tdbb) const;

		inline bool isReady() const
		{
			return (m_flags & (OBSOLETE | NOALLOC)) == 0;
		}

		void setLock(Lock* lock)
		{
			fb_assert(!m_lock);
			m_lock = lock;
		}

		void astHandler();
		void rescan(thread_db* tdbb, bool open);

		static Tablespace* init(thread_db* tdbb, ULONG id, const MetaName& name,
								std::optional<bool> alloc = std::nullopt);

		static int blockingAst(void* arg)
		{
			const auto tableSpace = static_cast<Tablespace*>(arg);

			try
			{
				tableSpace->astHandler();
			}
			catch (const Firebird::Exception&)
			{} // no-op

			return 0;
		}
	};
}

#endif // JRD_TABLESPACE_H
