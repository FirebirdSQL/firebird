/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Database.cpp
 *      DESCRIPTION:    Common descriptions
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
 * Sean Leyne
 * Claudio Valderrama C.
 */

#include "firebird.h"
// Definition of block types for data allocation in JRD
#include "../include/fb_blk.h"

#include "../jrd/ods.h"
#include "../jrd/lck.h"
#include "../jrd/Database.h"
#include "../jrd/nbak.h"
#include "../jrd/tra.h"
#include "../jrd/met_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/tpc_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/CryptoManager.h"
#include "../jrd/os/pio_proto.h"
#include "../common/os/os_utils.h"
//#include "../dsql/Parser.h"

// Thread data block
#include "../common/ThreadData.h"

using namespace Firebird;

namespace Jrd
{
	bool Database::onRawDevice() const
	{
		const auto pageSpace = dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
		return pageSpace->onRawDevice();
	}

	ULONG Database::getIOBlockSize() const
	{
		const auto pageSpace = dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
		fb_assert(pageSpace && pageSpace->file);

		if ((pageSpace->file->fil_flags & FIL_no_fs_cache) || pageSpace->onRawDevice())
			return DIRECT_IO_BLOCK_SIZE;

		return PAGE_ALIGNMENT;
	}

	AttNumber Database::generateAttachmentId()
	{
		fb_assert(dbb_tip_cache);
		return dbb_tip_cache->generateAttachmentId();
	}

	TraNumber Database::generateTransactionId()
	{
		fb_assert(dbb_tip_cache);
		return dbb_tip_cache->generateTransactionId();
	}

	/***
	void Database::assignLatestTransactionId(TraNumber number)
	{
		fb_assert(dbb_tip_cache);
		dbb_tip_cache->assignLatestTransactionId(number);
	}
	***/

	void Database::assignLatestAttachmentId(AttNumber number)
	{
		if (dbb_tip_cache)
			dbb_tip_cache->assignLatestAttachmentId(number);
	}

	StmtNumber Database::generateStatementId()
	{
		if (!dbb_tip_cache)
			return 0;
		return dbb_tip_cache->generateStatementId();
	}

	AttNumber Database::getLatestAttachmentId() const
	{
		if (!dbb_tip_cache)
			return 0;
		return dbb_tip_cache->getLatestAttachmentId();
	}

	StmtNumber Database::getLatestStatementId() const
	{
		if (!dbb_tip_cache)
			return 0;
		return dbb_tip_cache->getLatestStatementId();
	}

	ULONG Database::getMonitorGeneration() const
	{
		if (!dbb_tip_cache)
			return 0;
		return dbb_tip_cache->getMonitorGeneration();
	}

	ULONG Database::newMonitorGeneration() const
	{
		fb_assert(dbb_tip_cache);
		return dbb_tip_cache->newMonitorGeneration();
	}

	const Firebird::string& Database::getUniqueFileId()
	{
		if (dbb_file_id.isEmpty())
		{
			const PageSpace* const pageSpace = dbb_page_manager.findPageSpace(DB_PAGE_SPACE);

			UCharBuffer buffer;
			os_utils::getUniqueFileId(pageSpace->file->fil_desc, buffer);

			auto ptr = dbb_file_id.getBuffer(2 * buffer.getCount());
			for (const auto val : buffer)
			{
				sprintf(ptr, "%02x", (int) val);
				ptr += 2;
			}
		}

		return dbb_file_id;
	}

	Database::~Database()
	{
		if (dbb_linger_timer)
		{
			dbb_linger_timer->destroy();
		}

		{ // scope
			SyncLockGuard guard(&dbb_sortbuf_sync, SYNC_EXCLUSIVE, "Database::~Database");

			while (dbb_sort_buffers.hasData())
				delete[] dbb_sort_buffers.pop();
		}

		{ // scope
			SyncLockGuard guard(&dbb_pools_sync, SYNC_EXCLUSIVE, "Database::~Database");

			fb_assert(dbb_pools[0] == dbb_permanent);

			for (FB_SIZE_T i = 1; i < dbb_pools.getCount(); ++i)
				MemoryPool::deletePool(dbb_pools[i]);
		}

		delete dbb_tip_cache;
		delete dbb_monitoring_data;
		delete dbb_backup_manager;
		delete dbb_crypto_manager;
	}

	void Database::deletePool(MemoryPool* pool)
	{
		if (pool)
		{
			{
				SyncLockGuard guard(&dbb_pools_sync, SYNC_EXCLUSIVE, "Database::deletePool");
				FB_SIZE_T pos;

				if (dbb_pools.find(pool, pos))
					dbb_pools.remove(pos);
			}

			MemoryPool::deletePool(pool);
		}
	}

	int Database::blocking_ast_sweep(void* ast_object)
	{
		try
		{
			Database* dbb = static_cast<Database*>(ast_object);
			AsyncContextHolder tdbb(dbb, FB_FUNCTION);

			while (true)
			{
				AtomicCounter::counter_type old = dbb->dbb_flags;
				if ((old & DBB_sweep_in_progress) || !(old & DBB_sweep_starting))
				{
					SPTHR_DEBUG(fprintf(stderr, "blocking_ast_sweep %p false wrong flags %lx\n", dbb, old));
					break;
				}

				if (dbb->dbb_flags.compareExchange(old, old & ~DBB_sweep_starting))
				{
					SPTHR_DEBUG(fprintf(stderr, "blocking_ast_sweep true %p\n", dbb));
					dbb->dbb_thread_mutex.leave();
					LCK_release(tdbb, dbb->dbb_sweep_lock);
					break;
				}
			}
		}
		catch (const Exception&)
		{} // no-op

		return 0;
	}

	Lock* Database::createSweepLock(thread_db* tdbb)
	{
		if (!dbb_sweep_lock)
		{
			dbb_sweep_lock = FB_NEW_RPT(*dbb_permanent, 0)
				Lock(tdbb, 0, LCK_sweep, this, blocking_ast_sweep);
		}

		return dbb_sweep_lock;
	}

	bool Database::allowSweepThread(thread_db* tdbb)
	{
		SPTHR_DEBUG(fprintf(stderr, "allowSweepThread %p\n", this));
		if (readOnly())
			return false;

		Jrd::Attachment* const attachment = tdbb->getAttachment();
		if (attachment->att_flags & ATT_no_cleanup)
			return false;

		if (!dbb_thread_mutex.tryEnter(FB_FUNCTION))
		{
			SPTHR_DEBUG(fprintf(stderr, "allowSweepThread %p false, dbb_thread_mutex busy\n", this));
			return false;
		}

		while (true)
		{
			AtomicCounter::counter_type old = dbb_flags;
			if ((old & (DBB_sweep_in_progress | DBB_sweep_starting)) || (dbb_ast_flags & DBB_shutdown))
			{
				dbb_thread_mutex.leave();
				return false;
			}

			if (dbb_flags.compareExchange(old, old | DBB_sweep_starting))
				break;
		}

        SPTHR_DEBUG(fprintf(stderr, "allowSweepThread - set DBB_sweep_starting\n"));

		createSweepLock(tdbb);
		if (!LCK_lock(tdbb, dbb_sweep_lock, LCK_EX, LCK_NO_WAIT))
		{
			// clear lock error from status vector
			fb_utils::init_status(tdbb->tdbb_status_vector);

			clearSweepStarting();
			SPTHR_DEBUG(fprintf(stderr, "allowSweepThread - !LCK_lock\n"));
			return false;
		}

        SPTHR_DEBUG(fprintf(stderr, "allowSweepThread - TRUE\n"));
		return true;
	}

	bool Database::clearSweepStarting()
	{
		while (true)
		{
			AtomicCounter::counter_type old = dbb_flags;
			if (!(old & DBB_sweep_starting))
			{
				SPTHR_DEBUG(fprintf(stderr, "clearSweepStarting false %p\n", this));
				return false;
			}

			if (dbb_flags.compareExchange(old, old & ~DBB_sweep_starting))
			{
				SPTHR_DEBUG(fprintf(stderr, "clearSweepStarting true %p\n", this));
				dbb_thread_mutex.leave();
				return true;
			}
		}
	}

	bool Database::allowSweepRun(thread_db* tdbb)
	{
		SPTHR_DEBUG(fprintf(stderr, "allowSweepRun %p\n", this));

		if (readOnly())
			return false;

		Jrd::Attachment* const attachment = tdbb->getAttachment();
		if (attachment->att_flags & ATT_no_cleanup)
			return false;

		while (true)
		{
			AtomicCounter::counter_type old = dbb_flags;
			if (old & DBB_sweep_in_progress)
			{
				clearSweepStarting();
				return false;
			}

			if (dbb_flags.compareExchange(old, old | DBB_sweep_in_progress))
				break;
		}

		SPTHR_DEBUG(fprintf(stderr, "allowSweepRun - set DBB_sweep_in_progress\n"));

		if (!(dbb_flags & DBB_sweep_starting))
		{
			SPTHR_DEBUG(fprintf(stderr, "allowSweepRun - createSweepLock\n"));

			createSweepLock(tdbb);
			if (!LCK_lock(tdbb, dbb_sweep_lock, LCK_EX, -1))
			{
				// clear lock error from status vector
				fb_utils::init_status(tdbb->tdbb_status_vector);

				dbb_flags &= ~DBB_sweep_in_progress;
				return false;
			}
		}
		else
		{
			SPTHR_DEBUG(fprintf(stderr, "allowSweepRun - clearSweepStarting\n"));
			attachment->att_flags |= ATT_from_thread;
			clearSweepStarting();
		}

		return true;
	}

	void Database::clearSweepFlags(thread_db* tdbb)
	{
		if (!(dbb_flags & DBB_sweep_in_progress))
			return;

		if (dbb_sweep_lock)
			LCK_release(tdbb, dbb_sweep_lock);

		dbb_flags &= ~DBB_sweep_in_progress;
	}

	void Database::registerModule(Module& module)
	{
		Sync sync(&dbb_modules_sync, FB_FUNCTION);
		sync.lock(SYNC_SHARED);
		if (dbb_modules.exist(module))
			return;

		sync.unlock();
		sync.lock(SYNC_EXCLUSIVE);
		if (!dbb_modules.exist(module))
			dbb_modules.add(module);
	}

	FB_UINT64 Database::getReplSequence(thread_db* tdbb)
	{
		USHORT length = sizeof(FB_UINT64);
		if (!PAG_get_clump(tdbb, Ods::HDR_repl_seq, &length, (UCHAR*) &dbb_repl_sequence))
			return 0;

		return dbb_repl_sequence;
	}

	void Database::setReplSequence(thread_db* tdbb, FB_UINT64 sequence)
	{
		if (dbb_repl_sequence != sequence)
		{
			PAG_set_repl_sequence(tdbb, sequence);
			dbb_repl_sequence = sequence;
		}
	}

	bool Database::isReplicating(thread_db* tdbb)
	{
		if (!replConfig())
			return false;

		Sync sync(&dbb_repl_sync, FB_FUNCTION);
		sync.lock(SYNC_SHARED);

		if (dbb_repl_state.isUnknown())
		{
			sync.unlock();
			sync.lock(SYNC_EXCLUSIVE);

			if (dbb_repl_state.isUnknown())
			{
				if (!dbb_repl_lock)
				{
					dbb_repl_lock = FB_NEW_RPT(*dbb_permanent, 0)
						Lock(tdbb, 0, LCK_repl_state, this, replStateAst);
				}

				dbb_repl_state = MET_get_repl_state(tdbb, {});

				fb_assert(dbb_repl_lock->lck_logical == LCK_none);
				LCK_lock(tdbb, dbb_repl_lock, LCK_SR, LCK_WAIT);
			}
		}

		return dbb_repl_state.asBool();
	}

	void Database::invalidateReplState(thread_db* tdbb, bool broadcast)
	{
		SyncLockGuard guard(&dbb_repl_sync, SYNC_EXCLUSIVE, FB_FUNCTION);

		dbb_repl_state.reset();

		if (broadcast)
		{
			if (!dbb_repl_lock)
			{
				dbb_repl_lock = FB_NEW_RPT(*dbb_permanent, 0)
					Lock(tdbb, 0, LCK_repl_state, this, replStateAst);
			}

			// Signal other processes about the changed state
			if (dbb_repl_lock->lck_logical == LCK_none)
				LCK_lock(tdbb, dbb_repl_lock, LCK_EX, LCK_WAIT);
			else
				LCK_convert(tdbb, dbb_repl_lock, LCK_EX, LCK_WAIT);
		}

		LCK_release(tdbb, dbb_repl_lock);
	}

	int Database::replStateAst(void* ast_object)
	{
		Database* const dbb = static_cast<Database*>(ast_object);

		try
		{
			AsyncContextHolder tdbb(dbb, FB_FUNCTION);

			dbb->invalidateReplState(tdbb, false);
		}
		catch (const Exception&)
		{} // no-op

		return 0;
	}

	void Database::initGlobalObjects()
	{
		dbb_gblobj_holder.assignRefNoIncr(GlobalObjectHolder::init(getUniqueFileId(),
			dbb_filename, dbb_config));
	}

	// Methods encapsulating operations with vectors of known pages

	ULONG Database::getKnownPagesCount(SCHAR ptype)
	{
		fb_assert(ptype == pag_transactions || ptype == pag_ids);

		SyncLockGuard guard(&dbb_pages_sync, SYNC_SHARED, FB_FUNCTION);

		const auto vector =
			(ptype == pag_transactions) ? dbb_tip_pages :
			(ptype == pag_ids) ? dbb_gen_pages :
			nullptr;

		return vector ? (ULONG) vector->count() : 0;
	}

	ULONG Database::getKnownPage(SCHAR ptype, ULONG sequence)
	{
		fb_assert(ptype == pag_transactions || ptype == pag_ids);

		SyncLockGuard guard(&dbb_pages_sync, SYNC_SHARED, FB_FUNCTION);

		const auto vector =
			(ptype == pag_transactions) ? dbb_tip_pages :
			(ptype == pag_ids) ? dbb_gen_pages :
			nullptr;

		if (!vector || sequence >= vector->count())
			return 0;

		return (*vector)[sequence];
	}

	void Database::setKnownPage(SCHAR ptype, ULONG sequence, ULONG value)
	{
		fb_assert(ptype == pag_transactions || ptype == pag_ids);

		SyncLockGuard guard(&dbb_pages_sync, SYNC_EXCLUSIVE, FB_FUNCTION);

		auto& rvector = (ptype == pag_transactions) ? dbb_tip_pages : dbb_gen_pages;

		rvector = vcl::newVector(*dbb_permanent, rvector, sequence + 1);

		(*rvector)[sequence] = value;
	}

	void Database::copyKnownPages(SCHAR ptype, ULONG count, ULONG* data)
	{
		fb_assert(ptype == pag_transactions || ptype == pag_ids);

		SyncLockGuard guard(&dbb_pages_sync, SYNC_EXCLUSIVE, FB_FUNCTION);

		auto& rvector = (ptype == pag_transactions) ? dbb_tip_pages : dbb_gen_pages;

		rvector = vcl::newVector(*dbb_permanent, rvector, count);

		memcpy(rvector->memPtr(), data, count * sizeof(ULONG));
	}

	// Database::Linger class implementation

	void Database::Linger::handler()
	{
		JRD_shutdown_database(dbb, SHUT_DBB_RELEASE_POOLS);
	}

	void Database::Linger::reset()
	{
		if (active)
		{
			FbLocalStatus s;
			TimerInterfacePtr()->stop(&s, this);
			if (!(s->getState() & IStatus::STATE_ERRORS))
				active = false;
		}
	}

	void Database::Linger::set(unsigned seconds)
	{
		if (dbb && !active)
		{
			FbLocalStatus s;
			TimerInterfacePtr()->start(&s, this, seconds * 1000 * 1000);
			check(&s);
			active = true;
		}
	}

	void Database::Linger::destroy()
	{
		dbb = NULL;
		reset();
	}

	// Database::GlobalObjectHolder class implementation

	int Database::GlobalObjectHolder::release() const
	{
		// Release should be executed under g_mutex protection
		// in order to modify reference counter & hash table atomically
		MutexLockGuard guard(g_mutex, FB_FUNCTION);

		return RefCounted::release();
	}

	Database::GlobalObjectHolder* Database::GlobalObjectHolder::init(const string& id,
																	 const PathName& filename,
																	 RefPtr<const Config> config)
	{
		MutexLockGuard guard(g_mutex, FB_FUNCTION);

		Database::GlobalObjectHolder::DbId* entry = g_hashTable->lookup(id);
		if (!entry)
		{
			const auto holder = FB_NEW Database::GlobalObjectHolder(id, filename, config);
			entry = FB_NEW Database::GlobalObjectHolder::DbId(id, holder);
			g_hashTable->add(entry);
		}

		entry->holder->addRef();
		return entry->holder;
	}

	Database::GlobalObjectHolder::~GlobalObjectHolder()
	{
		// dtor is executed under g_mutex protection
		Database::GlobalObjectHolder::DbId* entry = g_hashTable->lookup(m_id);
		if (!g_hashTable->remove(m_id))
			fb_assert(false);

		{ // scope
			// here we cleanup what should not be globally protected
			MutexUnlockGuard guard(g_mutex, FB_FUNCTION);
			if (m_replMgr)
				m_replMgr->shutdown();
		}

		m_lockMgr = nullptr;
		m_eventMgr = nullptr;
		m_replMgr = nullptr;

		delete entry;

		fb_assert(m_tempCacheUsage == 0);
	}

	LockManager* Database::GlobalObjectHolder::getLockManager()
	{
		if (!m_lockMgr)
		{
			MutexLockGuard guard(m_mutex, FB_FUNCTION);

			if (!m_lockMgr)
				m_lockMgr = FB_NEW LockManager(m_id, m_config);
		}
		return m_lockMgr;
	}

	EventManager* Database::GlobalObjectHolder::getEventManager()
	{
		if (!m_eventMgr)
		{
			MutexLockGuard guard(m_mutex, FB_FUNCTION);

			if (!m_eventMgr)
				m_eventMgr = FB_NEW EventManager(m_id, m_config);
		}
		return m_eventMgr;
	}

	Replication::Manager* Database::GlobalObjectHolder::getReplManager(bool create)
	{
		if (!m_replConfig)
			return nullptr;

		if (!m_replMgr && create)
		{
			MutexLockGuard guard(m_mutex, FB_FUNCTION);

			if (!m_replMgr)
				m_replMgr = FB_NEW Replication::Manager(m_id, m_replConfig);
		}
		return m_replMgr;
	}

	bool Database::GlobalObjectHolder::incTempCacheUsage(FB_SIZE_T size)
	{
		if (m_tempCacheUsage + size > m_tempCacheLimit)
			return false;

		const auto old = m_tempCacheUsage.fetch_add(size);
		if (old + size > m_tempCacheLimit)
		{
			m_tempCacheUsage.fetch_sub(size);
			return false;
		}

		return true;
	}

	void Database::GlobalObjectHolder::decTempCacheUsage(FB_SIZE_T size)
	{
		fb_assert(m_tempCacheUsage >= size);

		m_tempCacheUsage.fetch_sub(size);
	}

	GlobalPtr<Database::GlobalObjectHolder::DbIdHash>
		Database::GlobalObjectHolder::g_hashTable;
	GlobalPtr<Mutex> Database::GlobalObjectHolder::g_mutex;

} // namespace
