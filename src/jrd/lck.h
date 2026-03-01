/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		lck.h
 *	DESCRIPTION:	Lock hander definitions (NOT lock manager)
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
 */

#ifndef JRD_LCK_H
#define JRD_LCK_H

//#define DEBUG_LCK

#include "../lock/lock_proto.h"

#ifdef DEBUG_LCK
#include "../common/classes/SyncObject.h"
#endif

#include "../jrd/Attachment.h"
#include "../common/classes/auto.h"

namespace Jrd {

class Database;
class thread_db;

// Lock types

enum lck_t : UCHAR {
	LCK_database = 1,			// Root of lock tree
	LCK_relation,				// Individual relation lock
	LCK_bdb,					// Individual buffer block
	LCK_tra,					// Individual transaction lock
	LCK_attachment,				// Attachment lock
	LCK_shadow,					// Lock to synchronize addition of shadows
	LCK_sweep,					// Sweep lock for single sweeper
	LCK_update_shadow,			// shadow update sync lock
	LCK_backup_alloc,           // Lock for page allocation table in backup spare file
	LCK_backup_database,        // Lock to protect writing to database file
	LCK_backup_end,				// Lock to protect end_backup consistency
	LCK_page_space,				// Page space ID lock
	LCK_dsql_cache,				// DSQL cache lock
	LCK_monitor,				// Lock to dump the monitoring data
	LCK_cancel,					// Cancellation lock
	LCK_btr_dont_gc,			// Prevent removal of b-tree page from index
	LCK_rel_gc,					// Allow garbage collection for relation
	LCK_tpc_init,				// TPC initializer lock
	LCK_tpc_block,				// TPC memory block file existence lock
	LCK_cs_rescan,				// Charset rescan lock
	LCK_rel_rescan,				// Relation forced rescan lock
	LCK_idx_rescan,				// Index rescan lock
	LCK_prc_rescan,				// Procedure rescan lock
	LCK_fun_rescan,				// Function existence lock
	LCK_constant_rescan,		// Constant existence lock
	LCK_rel_partners,			// Relation partners lock
	LCK_crypt,					// Crypt lock for single crypt thread
	LCK_crypt_status,			// Notifies about changed database encryption status
	LCK_record_gc,				// Record-level GC lock
	LCK_alter_database,			// ALTER DATABASE lock
	LCK_repl_state,				// Replication state lock
	LCK_repl_tables,			// Replication set lock
	LCK_dsql_statement_cache,	// DSQL statement cache lock
	LCK_profiler_listener,		// Remote profiler listener
	LCK_dbwide_triggers,		// Database wide triggers rescan lock
	LCK_idx_create				// Taken during index build process
};

// Lock owner types

enum lck_owner_t {
	LCK_OWNER_database = 1,		// A database is the owner of the lock
	LCK_OWNER_attachment		// An attachment is the owner of the lock
};

class LockManagerEngineCallbacks final : public LockManager::Callbacks
{
public:
	LockManagerEngineCallbacks(thread_db* aTdbb)
		: tdbb(aTdbb)
	{
	}

	ISC_STATUS getCancelState() const override;
	ULONG adjustWait(ULONG wait) const override;
	void checkoutRun(std::function<void()> func) const override;

private:
	thread_db* const tdbb;
};

class Lock : public pool_alloc_rpt<UCHAR, type_lck>
{
public:
	Lock(thread_db* tdbb, USHORT length, lck_t type, void* object = NULL, lock_ast_t ast = NULL);
	~Lock();

	Lock* detach();

	Firebird::RefPtr<StableAttachmentPart> getLockStable()
	{
		return lck_attachment;
	}

	Attachment* getLockAttachment();

	void setLockAttachment(Attachment* att);

#ifdef DEBUG_LCK
	Firebird::SyncObject	lck_sync;
#endif

	Database* lck_dbb;				// Database object is contained in

private:
	Firebird::RefPtr<StableAttachmentPart> lck_attachment;		// Attachment that owns lock, set only using set_lock_attachment()

public:
	void* lck_compatible;			// Enter into internal_enqueue() and treat as compatible
	void* lck_compatible2;			// Sub-level for internal compatibility

	lock_ast_t lck_ast;				// Blocking AST routine
	void* lck_object;				// Argument to be passed to AST

//private:
	Lock* lck_next;					// lck_next and lck_prior form a doubly linked list of locks
	Lock* lck_prior;				// bound to attachment

#ifdef DEBUG_LCK_LIST
	UCHAR lck_next_type;			// Lock type of next lock in list
	UCHAR lck_prev_type;			// Lock type of prev lock in list
#endif

	Lock* lck_collision;			// Collisions in compatibility table
	Lock* lck_identical;			// Identical locks in compatibility table

	SLONG lck_id;					// Lock id from the lock manager
	SLONG lck_owner_handle;			// Lock owner handle from the lock manager's point of view
	USHORT lck_length;				// Length of lock key string
	lck_t lck_type;					// Lock type

public:
	UCHAR lck_logical;				// Logical lock level
	UCHAR lck_physical;				// Physical lock level
	LOCK_DATA_T lck_data;			// Data associated with a lock

	static constexpr size_t KEY_STATIC_SIZE = sizeof(SINT64);

private:
	union
	{
		UCHAR key_string[KEY_STATIC_SIZE];
		SINT64 key_long;
	} lck_key;						// Lock key string

	static_assert(KEY_STATIC_SIZE >= sizeof(lck_key), "Wrong KEY_STATIC_SIZE");

public:

	UCHAR* getKeyPtr()
	{
#ifdef WORDS_BIGENDIAN
		if (lck_length < KEY_STATIC_SIZE)
			return &lck_key.key_string[KEY_STATIC_SIZE - lck_length];
#endif
		return &lck_key.key_string[0];
	}

	SINT64 getKey() const
	{
		return lck_key.key_long;
	}

	void setKey(SINT64 value)
	{
		lck_key.key_long = value;
	}
};

} // namespace Jrd

void	LCK_assert(Jrd::thread_db*, Jrd::Lock*);
bool	LCK_cancel_wait(Jrd::Attachment*);
bool	LCK_convert(Jrd::thread_db*, Jrd::Lock*, USHORT, SSHORT);
bool	LCK_convert_opt(Jrd::thread_db*, Jrd::Lock*, USHORT);
void	LCK_downgrade(Jrd::thread_db*, Jrd::Lock*);
void	LCK_fini(Jrd::thread_db*, Jrd::lck_owner_t);
void	LCK_init(Jrd::thread_db*, Jrd::lck_owner_t);
bool	LCK_lock(Jrd::thread_db*, Jrd::Lock*, USHORT, SSHORT);
bool	LCK_lock_opt(Jrd::thread_db*, Jrd::Lock*, USHORT, SSHORT);
LOCK_DATA_T LCK_query_data(Jrd::thread_db*, Jrd::lck_t, USHORT);
LOCK_DATA_T LCK_read_data(Jrd::thread_db*, Jrd::Lock*);
void	LCK_release(Jrd::thread_db*, Jrd::Lock*);
void	LCK_re_post(Jrd::thread_db*, Jrd::Lock*);
void	LCK_write_data(Jrd::thread_db*, Jrd::Lock*, LOCK_DATA_T);


namespace Jrd {

class AutoLock
{
public:
	explicit AutoLock(thread_db* tdbb, Lock* lck = NULL)
		: m_tdbb(tdbb),
		  m_lock(lck)
	{
	}

	~AutoLock()
	{
		release();
	}

	void release()
	{
		if (m_lock)
		{
			if (m_lock->lck_id)
				LCK_release(m_tdbb, m_lock);
			delete m_lock;
			m_lock = NULL;
		}
	}

	Lock* operator-> ()
	{
		return m_lock;
	}

	operator Lock* ()
	{
		return m_lock;
	}

	Lock* operator= (Lock* lck)
	{
		release();
		m_lock = lck;
		return m_lock;
	}

private:
	thread_db* m_tdbb;
	Lock* m_lock;
};

} // namespace Jrd

#endif // JRD_LCK_H
