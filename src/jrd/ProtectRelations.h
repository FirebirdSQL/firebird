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

// Prevent concurrent transactions from modification of relation data.
// To do it, upgrade existing relation lock to PR level at least.
// Note, existing lock level should not be lowered.
// Used when new index is built.
class ProtectRelations
{
public:
	ProtectRelations(thread_db* tdbb, jrd_tra* transaction, bool autoRelease = true) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_release(autoRelease),
		m_locks()
	{
	}

	ProtectRelations(thread_db* tdbb, jrd_tra* transaction, jrd_rel* relation, bool autoRelease = true) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_release(autoRelease),
		m_locks()
	{
		addRelation(relation);
		lock();
	}

	~ProtectRelations()
	{
		if (m_release)
			unlock();
	}

	void addRelation(jrd_rel* relation)
	{
		if (!(relation->getPermanent()->rel_flags & REL_temp_ltt))
		{
			FB_SIZE_T pos;
			if (!m_locks.find(relation->getId(), pos))
				m_locks.insert(pos, RelationLock(relation));
		}
	}

	bool exists(USHORT rel_id) const
	{
		return m_locks.exist(rel_id);
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
	struct RelationLock
	{
		RelationLock(jrd_rel* relation = nullptr) :
			m_relation(relation),
			m_lock(nullptr),
			m_level(LCK_none)
		{
		}

		void takeLock(thread_db* tdbb, jrd_tra* transaction);
		void releaseLock(thread_db* tdbb, jrd_tra* transaction);

		static const USHORT generate(const RelationLock& item)
		{
			return item.m_relation->getId();
		}

		jrd_rel* m_relation;
		Lock* m_lock;
		UCHAR m_level;		// original lock level
};

	thread_db* const m_tdbb;
	jrd_tra* const m_transaction;
	const bool m_release;
	Firebird::SortedArray<RelationLock, Firebird::InlineStorage<RelationLock, 2>, USHORT, RelationLock> m_locks;
};

} // namespace Jrd

#endif // FB_JRD_PROTECT_RELATIONS
