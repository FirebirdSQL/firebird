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

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/lck.h"
#include "../jrd/tra.h"
#include "../jrd/met_proto.h"

#include "../jrd/Tablespace.h"

using namespace Firebird;
using namespace Jrd;


//
// Tablespace class
//

Tablespace::~Tablespace()
{
	fb_assert(m_useCount == 0);
}

void Tablespace::lock(thread_db* tdbb, ULONG pageSpaceId)
{
	if (PageSpace::isTablespace(pageSpaceId))
	{
		// Protect the tablespace from being modified or dropped until the current transaction ends
		if (const auto transaction = tdbb->getTransaction())
			transaction->lockTablespace(tdbb, pageSpaceId);
		// if transaction is unknown, at least ensure that the tablespace is initialized and usable
		else
			Tablespace::lookup(tdbb, pageSpaceId);
	}
}

Tablespace* Tablespace::create(thread_db* tdbb, ULONG id)
{
	// Check that pageSpaceId has reasonable value
	fb_assert(PageSpace::isTablespace(id));

	return Tablespace::init(tdbb, id, "", true);
}

Tablespace* Tablespace::lookup(thread_db* tdbb, ULONG id, bool open)
{
	const auto dbb = tdbb->getDatabase();
	auto tableSpace = dbb->dbb_tablespaces.get(id);

	if (tableSpace)
	{
		if (!tableSpace->isVisible(tdbb))
		{
			(Arg::Gds(isc_dyn_ts_not_found) << tableSpace->getName()).raise();
		}

		if (!tableSpace->isReady())
			tableSpace->rescan(tdbb, open);

		return tableSpace;
	}

	// Check that pageSpaceId has reasonable value
	fb_assert(PageSpace::isTablespace(id));

	return Tablespace::init(tdbb, id, "", open ? std::optional<bool>(false) : std::nullopt);
}

Tablespace* Tablespace::lookup(thread_db* tdbb, const MetaName& name)
{
	const auto dbb = tdbb->getDatabase();

	// Tablespace name can be empty for view, gtt, virtual tables
	if (name.isEmpty() || name == PRIMARY_TABLESPACE_NAME)
		return dbb->dbb_tablespaces.get(DB_PAGE_SPACE);

	auto tableSpace = dbb->dbb_tablespaces.get(name);

	if (tableSpace)
	{
		if (!tableSpace->isVisible(tdbb))
		{
			(Arg::Gds(isc_dyn_ts_not_found) << name).raise();
		}

		if (!tableSpace->isReady())
			tableSpace->rescan(tdbb, false);

		return tableSpace;
	}

	return Tablespace::init(tdbb, 0, name);
}

void Tablespace::mark(thread_db* tdbb, Operation operation, jrd_tra* transaction, const PathName& fileName)
{
	const auto dbb = tdbb->getDatabase();

	fb_assert(m_lock && m_lock->lck_logical >= LCK_SR);

	// Get the EX lock to prevent other attachments from using this tablespace
	if (isUsed() || !LCK_convert(tdbb, m_lock, LCK_EX, transaction->getLockWait()))
	{
		string name;
		name.printf("TABLESPACE \"%s\"", m_name.c_str());

		(Arg::Gds(isc_obj_in_use) << Arg::Str(name)).raise();
	}

	fb_assert(!m_transaction || m_transaction == transaction);

	m_transaction = transaction;

	if (operation == Operation::CREATE)
	{
		m_flags |= CREATED;
	}
	else if (operation == Operation::ALTER)
	{
		m_flags |= MODIFIED;

		dbb->dbb_page_manager.delPageSpace(m_id);
		dbb->dbb_page_manager.allocTableSpace(tdbb, m_id, false, fileName);
	}
	else if (operation == Operation::DROP)
	{
		m_flags |= DELETED;
	}
}

void Tablespace::commit(thread_db* tdbb, jrd_tra* transaction)
{
	if (m_transaction != transaction)
		return;

	fb_assert(m_flags & (CREATED | MODIFIED | DELETED));
	fb_assert(m_lock && m_lock->lck_logical == LCK_EX);

	const auto dbb = tdbb->getDatabase();

	if (m_flags & DELETED)
	{
		release(tdbb, true);
		dbb->dbb_page_manager.delPageSpace(m_id);
		dbb->dbb_tablespaces.remove(m_id); // implicitly deletes this
		return;
	}

	m_flags = 0;
	m_transaction = nullptr;

	LCK_release(tdbb, m_lock);
	m_flags |= OBSOLETE;
}

void Tablespace::rollback(thread_db* tdbb, jrd_tra* transaction)
{
	if (!m_transaction || m_transaction != transaction)
		return;

	fb_assert(m_flags & (CREATED | MODIFIED | DELETED));
	fb_assert(m_lock && m_lock->lck_logical == LCK_EX);

	const auto dbb = tdbb->getDatabase();

	if (m_flags & CREATED)
	{
		release(tdbb, true);
		dbb->dbb_page_manager.delPageSpace(m_id, true);
		dbb->dbb_tablespaces.remove(m_id); // implicitly deletes this
		return;
	}

	if (m_flags & MODIFIED)
	{
		dbb->dbb_page_manager.delPageSpace(m_id);
	}

	m_flags = 0;
	m_transaction = nullptr;

	LCK_release(tdbb, m_lock);
	m_flags |= OBSOLETE;
}

void Tablespace::allocate(thread_db* tdbb, bool create)
{
	const auto dbb = tdbb->getDatabase();

	fb_assert(m_flags & NOALLOC);
	fb_assert(!(m_flags & OBSOLETE));

	try
	{
		dbb->dbb_page_manager.allocTableSpace(tdbb, m_id, create, m_fileName);
		m_flags &= ~NOALLOC;
	}
	catch (const Exception&)
	{
		LCK_release(tdbb, m_lock);
		m_flags |= OBSOLETE;
		throw;
	}
}

inline bool Tablespace::isVisible(thread_db* tdbb) const
{
	if (!m_transaction)
		return true;

	const auto transaction = tdbb->getTransaction();

	if (m_flags & DELETED)
		return (m_transaction != transaction);

	if (m_flags & CREATED)
		return (m_transaction == transaction);

	fb_assert(m_flags & MODIFIED);
	return true;
}

void Tablespace::astHandler()
{
	const auto dbb = m_lock->lck_dbb;

	AsyncContextHolder tdbb(dbb, FB_FUNCTION, m_lock);

	if (isUsed())
	{
		m_flags |= BLOCKING;
	}
	else
	{
		LCK_release(tdbb, m_lock);
		m_flags &= ~BLOCKING;
		m_flags |= OBSOLETE;
	}
}

void Tablespace::rescan(thread_db* tdbb, bool open)
{
	const auto dbb = tdbb->getDatabase();

	fb_assert(m_lock);

	if (m_flags & OBSOLETE)
	{
		// Get the SR lock to let other attachments know that we use the tablespace

		if (!LCK_lock(tdbb, m_lock, LCK_SR, LCK_WAIT))
			status_exception::raise(tdbb->tdbb_status_vector);

		MetaName tsName;
		if (!MET_tablespace(tdbb, m_id, tsName, m_fileName))
		{
			(Arg::Gds(isc_dyn_ts_not_found) << m_name).raise();
		}

		fb_assert(tsName == m_name);

		m_flags &= ~OBSOLETE;
	}

	if (open && (m_flags & NOALLOC))
		allocate(tdbb, false);
}

Tablespace* Tablespace::init(thread_db* tdbb, ULONG id, const MetaName& name,
							 std::optional<bool> alloc)
{
	const auto dbb = tdbb->getDatabase();

	MetaName tsName = name;
	PathName fileName;

	if (!id)
	{
		if (!(id = MET_tablespace_id(tdbb, name, fileName)))
		{
			(Arg::Gds(isc_dyn_ts_not_found) << name).raise();
		}
	}

	AutoPtr<Tablespace> tableSpace(FB_NEW_POOL(*dbb->dbb_permanent) Tablespace(*dbb->dbb_permanent, id));

	AutoPtr<Lock> lock(FB_NEW_RPT(*dbb->dbb_permanent, 0)
		Lock(tdbb, sizeof(ULONG), LCK_tablespace, tableSpace, blockingAst));
	lock->setKey(id);

	// Get the SR lock to let other attachments know that we use the tablespace

	if (!LCK_lock(tdbb, lock, LCK_SR, LCK_WAIT))
		status_exception::raise(tdbb->tdbb_status_vector);

	if (name.isEmpty() && !MET_tablespace(tdbb, id, tsName, fileName))
	{
		string errName;
		errName.printf("TABLESPACE (id=%)" ULONGFORMAT, id);

		(Arg::Gds(isc_dyn_ts_not_found) << errName).raise();
	}

	fb_assert(PageSpace::isTablespace(id) && tsName.hasData() && fileName.hasData());
	fb_assert(name.isEmpty() || tsName == name);

	// Reinitialize tablespace with proper metadata

	tableSpace->init(tsName, fileName, lock.release());

	// Allocate the pagespace, if requested

	if (alloc)
		tableSpace->allocate(tdbb, alloc.value());

	// Cache the tablespace
	dbb->dbb_tablespaces.store(tableSpace);

	return tableSpace.release();
}

void Tablespace::addRef(thread_db* tdbb)
{
	if (m_useCount++ == 0)
	{
		if (!isReady())
			rescan(tdbb, true);
	}

	fb_assert(!(m_flags & OBSOLETE));
}

void Tablespace::release(thread_db* tdbb, bool force)
{
	if (force)
	{
		fb_assert(m_useCount == 0);
		m_useCount = 0;

		if (m_lock)
			LCK_release(tdbb, m_lock);
	}
	else
	{
		fb_assert(m_useCount > 0);
		fb_assert(!(m_flags & OBSOLETE));

		if (--m_useCount == 0)
		{
			if (m_flags & BLOCKING)
			{
				LCK_release(tdbb, m_lock);
				m_flags &= ~BLOCKING;
				m_flags |= OBSOLETE;
			}
		}
	}
}


//
// Tablespace::Cache class
//

Tablespace* Tablespace::Cache::get(ULONG id)
{
	const auto accessor = m_tablespaces.readAccessor();

	// Tablespace IDs start from 1
	if (id <= accessor->getCount())
	{
		if (const auto tableSpace = accessor->value(id - 1))
		{
			fb_assert(tableSpace->getId() == id);
			return tableSpace;
		}
	}

	return nullptr;
}

Tablespace* Tablespace::Cache::get(const MetaName& name)
{
	const auto accessor = m_tablespaces.readAccessor();

	for (const auto tableSpace : *(accessor.getPointer()))
	{
		if (tableSpace && tableSpace->getName() == name)
			return tableSpace;
	}

	return nullptr;
}

void Tablespace::Cache::store(Tablespace* tableSpace)
{
	const auto id = tableSpace->getId();

	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	m_tablespaces.grow(id, true);

	const auto accessor = m_tablespaces.writeAccessor();

	auto& currentValue = accessor->value(id - 1);
	fb_assert(!currentValue);
	currentValue = tableSpace;
}

void Tablespace::Cache::remove(ULONG id)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	const auto accessor = m_tablespaces.writeAccessor();

	if (id <= accessor->getCount())
	{
		auto& tableSpace = accessor->value(id - 1);
		delete std::exchange(tableSpace, nullptr);
	}
}

void Tablespace::Cache::release(thread_db* tdbb)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	const auto accessor = m_tablespaces.writeAccessor();

	for (auto& tableSpace : *accessor)
	{
		if (tableSpace)
		{
			tableSpace->release(tdbb, true);
			delete std::exchange(tableSpace, nullptr);
		}
	}
}

void Tablespace::Cache::rollback(thread_db* tdbb, jrd_tra* transaction)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	const auto accessor = m_tablespaces.writeAccessor();

	for (const auto tableSpace : *accessor)
	{
		if (tableSpace)
			tableSpace->rollback(tdbb, transaction);
	}
}
