/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ProtectRelations.h
 *	DESCRIPTION:	Relation lock holder
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
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"

#include "../jrd/Resources.h"
#include "../common/classes/array.h"


#if (!defined(FB_JRD_PROTECT_RELATIONS))
#define FB_JRD_PROTECT_RELATIONS

namespace Jrd {

class thread_db;
class jrd_tra;

// Lock relation with protected_read level or raise existing relation lock
// to this level to ensure nobody can write to this relation.
// Used when new index is built.
// releaseLock set to true if there was no existing lock before
class ProtectRelations
{
public:
	ProtectRelations(thread_db* tdbb, jrd_tra* transaction) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
	}

	ProtectRelations(thread_db* tdbb, jrd_tra* transaction, Cached::Relation* relation) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
		addRelation(relation);
		lock();
	}

	~ProtectRelations()
	{
		unlock();
	}

	void addRelation(Cached::Relation* relation)
	{
		if (!(relation->rel_flags & REL_temp_ltt))
		{
			FB_SIZE_T pos;
			if (!m_locks.find(relation->rel_id, pos))
				m_locks.insert(pos, relLock(relation));
		}
	}

	bool exists(USHORT rel_id) const
	{
		FB_SIZE_T pos;
		return m_locks.find(rel_id, pos);
	}

	void lock()
	{
		relLock* item = m_locks.begin();
		const relLock* const end = m_locks.end();
		for (; item < end; item++)
			item->takeLock(m_tdbb, m_transaction);
	}

	void unlock()
	{
		relLock* item = m_locks.begin();
		const relLock* const end = m_locks.end();
		for (; item < end; item++)
			item->releaseLock(m_tdbb, m_transaction);
	}

private:
	struct relLock
	{
		relLock(Cached::Relation* relation = NULL) :
			m_relation(relation),
			m_lock(NULL),
			m_release(false)
		{
		}

		void takeLock(thread_db* tdbb, jrd_tra* transaction);
		void releaseLock(thread_db* tdbb, jrd_tra* transaction);

		static const USHORT generate(const relLock& item)
		{
			return item.m_relation->rel_id;
		}

		Cached::Relation* m_relation;
		Lock* m_lock;
		bool m_release;
	};

	thread_db* m_tdbb;
	jrd_tra* m_transaction;
	Firebird::SortedArray<relLock, Firebird::InlineStorage<relLock, 2>, USHORT, relLock> m_locks;
};

/*
// Lock relation with protected_read level or raise existing relation lock
// to this level to ensure nobody can write to this relation.
// Used when new index is built.
// releaseLock set to true if there was no existing lock before
class ProtectRelations
{
public:
	ProtectRelations(thread_db* tdbb, jrd_tra* transaction) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
	}

	ProtectRelations(thread_db* tdbb, jrd_tra* transaction, Cached::Relation* relation) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
		addRelation(relation);
		lock();
	}

	~ProtectRelations()
	{
		unlock();
	}

	void addRelation(Cached::Relation* relation)
	{
		FB_SIZE_T pos;
		if (!m_locks.find(relation->getId(), pos))
			m_locks.insert(pos, relLock(relation));
	}

	bool exists(USHORT rel_id) const
	{
		FB_SIZE_T pos;
		return m_locks.find(rel_id, pos);
	}

	void lock()
	{
		for (auto& item : m_locks)
			item.takeLock(m_tdbb, m_transaction);
	}

	void unlock()
	{
		for (auto& item : m_locks)
			item.releaseLock(m_tdbb, m_transaction);
	}

private:
	struct relLock
	{
		relLock(Cached::Relation* relation = nullptr) :
			m_relation(relation),
			m_lock(NULL),
			m_release(false)
		{
		}

		relLock(MemoryPool&, const Jrd::ProtectRelations::relLock& l) :
			m_relation(l.m_relation),
			m_lock(l.m_lock),
			m_release(l.m_release)
		{
			fb_assert(!m_lock);
		}

		void takeLock(thread_db* tdbb, jrd_tra* transaction);
		void releaseLock(thread_db* tdbb, jrd_tra* transaction);

		static const USHORT generate(const relLock& item)
		{
			return item.m_relation->getId();
		}

		Cached::Relation* m_relation;
		Lock* m_lock;
		bool m_release;
	};

	thread_db* m_tdbb;
	jrd_tra* m_transaction;
	SortedArray<relLock, InlineStorage<relLock, 2>, USHORT, relLock> m_locks;
};
*/

} // namespace Jrd

#endif // FB_JRD_PROTECT_RELATIONS
