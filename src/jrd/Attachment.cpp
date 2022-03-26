/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Attachment.cpp
 *      DESCRIPTION:    JRD Attachment class
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
 */

#include "firebird.h"
#include "../jrd/Attachment.h"
#include "../jrd/Database.h"
#include "../jrd/Function.h"
#include "../jrd/nbak.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/tra.h"
#include "../jrd/intl.h"

#include "../jrd/blb_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/tpc_proto.h"

#include "../jrd/extds/ExtDS.h"

#include "../jrd/replication/Applier.h"
#include "../jrd/replication/Manager.h"

#include "../common/classes/fb_string.h"
#include "../jrd/MetaName.h"
#include "../common/StatusArg.h"
#include "../common/TimeZoneUtil.h"
#include "../common/isc_proto.h"
#include "../common/classes/RefMutex.h"


using namespace Jrd;
using namespace Firebird;

/// class ActiveSnapshots

ActiveSnapshots::ActiveSnapshots(Firebird::MemoryPool& p) :
	m_snapshots(p),
	m_lastCommit(CN_ACTIVE),
	m_releaseCount(0),
	m_slots_used(0)
{
}


CommitNumber ActiveSnapshots::getSnapshotForVersion(CommitNumber version_cn)
{
	if (version_cn > m_lastCommit)
		return CN_ACTIVE;

	if (m_snapshots.locate(locGreatEqual, version_cn))
		return m_snapshots.current();

	return m_lastCommit;
}


/// class Attachment


// static method
Jrd::Attachment* Jrd::Attachment::create(Database* dbb, JProvider* provider)
{
	MemoryPool* const pool = dbb->createPool();

	try
	{
		Attachment* const attachment = FB_NEW_POOL(*pool) Attachment(pool, dbb, provider);
		pool->setStatsGroup(attachment->att_memory_stats);
		return attachment;
	}
	catch (const Firebird::Exception&)
	{
		dbb->deletePool(pool);
		throw;
	}
}


// static method
void Jrd::Attachment::destroy(Attachment* const attachment)
{
	if (!attachment)
		return;

	StableAttachmentPart* sAtt = attachment->getStable();
	fb_assert(sAtt);
	if (sAtt)
	{
		// break link between attachment and its stable part
		sAtt->cancel();
		attachment->setStable(NULL);

		sAtt->manualUnlock(attachment->att_flags);
	}

	thread_db* tdbb = JRD_get_thread_data();

	jrd_tra* sysTransaction = attachment->getSysTransaction();
	if (sysTransaction)
	{
		// unwind any active system requests
		while (sysTransaction->tra_requests)
			EXE_unwind(tdbb, sysTransaction->tra_requests);

		jrd_tra::destroy(NULL, sysTransaction);
	}

	Database* const dbb = attachment->att_database;
	MemoryPool* const pool = attachment->att_pool;
	Firebird::MemoryStats temp_stats;
	pool->setStatsGroup(temp_stats);

	delete attachment;

	dbb->deletePool(pool);
}


MemoryPool* Jrd::Attachment::createPool()
{
	MemoryPool* const pool = MemoryPool::createPool(att_pool, att_memory_stats);
	auto stats = FB_NEW_POOL(*pool) MemoryStats(&att_memory_stats);
	pool->setStatsGroup(*stats);
	att_pools.add(pool);
	return pool;
}


void Jrd::Attachment::deletePool(MemoryPool* pool)
{
	if (pool)
	{
		att_pools.findAndRemove(pool);

#ifdef DEBUG_LCK_LIST
		// hvlad: this could be slow, use only when absolutely necessary
		for (Lock* lock = att_long_locks; lock; )
		{
			Lock* next = lock->lck_next;
			if (BtrPageGCLock::checkPool(lock, pool))
			{
				gds__log("DEBUG_LCK_LIST: found not detached lock 0x%p in deleting pool 0x%p", lock, pool);

				//delete lock;
				lock->setLockAttachment(NULL);
			}
			lock = next;
		}
#endif
		MemoryPool::deletePool(pool);
	}
}


bool Jrd::Attachment::backupStateWriteLock(thread_db* tdbb, SSHORT wait)
{
	if (att_backup_state_counter++)
		return true;

	if (att_database->dbb_backup_manager->lockStateWrite(tdbb, wait))
		return true;

	att_backup_state_counter--;
	return false;
}


void Jrd::Attachment::backupStateWriteUnLock(thread_db* tdbb)
{
	if (--att_backup_state_counter == 0)
		att_database->dbb_backup_manager->unlockStateWrite(tdbb);
}


bool Jrd::Attachment::backupStateReadLock(thread_db* tdbb, SSHORT wait)
{
	if (att_backup_state_counter++)
		return true;

	if (att_database->dbb_backup_manager->lockStateRead(tdbb, wait))
		return true;

	att_backup_state_counter--;
	return false;
}


void Jrd::Attachment::backupStateReadUnLock(thread_db* tdbb)
{
	if (--att_backup_state_counter == 0)
		att_database->dbb_backup_manager->unlockStateRead(tdbb);
}


Jrd::Attachment::Attachment(MemoryPool* pool, Database* dbb, JProvider* provider)
	: att_pool(pool),
	  att_memory_stats(&dbb->dbb_memory_stats),
	  att_database(dbb),
	  att_ss_user(NULL),
	  att_user_ids(*pool),
	  att_active_snapshots(*pool),
	  att_statements(*pool),
	  att_requests(*pool),
	  att_lock_owner_id(Database::getLockOwnerId()),
	  att_backup_state_counter(0),
	  att_stats(*pool),
	  att_base_stats(*pool),
	  att_working_directory(*pool),
	  att_filename(*pool),
	  att_timestamp(TimeZoneUtil::getCurrentSystemTimeStamp()),
	  att_context_vars(*pool),
	  ddlTriggersContext(*pool),
	  att_network_protocol(*pool),
	  att_remote_crypt(*pool),
	  att_remote_address(*pool),
	  att_remote_process(*pool),
	  att_client_version(*pool),
	  att_remote_protocol(*pool),
	  att_remote_host(*pool),
	  att_remote_os_user(*pool),
	  att_dsql_cache(*pool),
	  att_udf_pointers(*pool),
	  att_ext_connection(NULL),
	  att_ext_parent(NULL),
	  att_ext_call_depth(0),
	  att_trace_manager(FB_NEW_POOL(*att_pool) TraceManager(this)),
	  att_bindings(*pool),
	  att_dest_bind(&att_bindings),
	  att_original_timezone(TimeZoneUtil::getSystemTimeZone()),
	  att_current_timezone(att_original_timezone),
	  att_repl_appliers(*pool),
	  att_utility(UTIL_NONE),
	  att_procedures(*pool),
	  att_functions(*pool),
	  att_generators(*pool),
	  att_internal(*pool),
	  att_dyn_req(*pool),
	  att_dec_status(DecimalStatus::DEFAULT),
	  att_charsets(*pool),
	  att_charset_ids(*pool),
	  att_pools(*pool),
	  att_idle_timeout(0),
	  att_stmt_timeout(0),
	  att_batches(*pool),
	  att_initial_options(*pool),
	  att_provider(provider)
{
	att_internal.grow(irq_MAX);
	att_dyn_req.grow(drq_MAX);
}


Jrd::Attachment::~Attachment()
{
	if (att_idle_timer)
		att_idle_timer->stop();

	delete att_trace_manager;

	for (unsigned n = 0; n < att_batches.getCount(); ++n)
		att_batches[n]->resetHandle();

	for (Function** iter = att_functions.begin(); iter < att_functions.end(); ++iter)
	{
		Function* const function = *iter;
		if (function)
			delete function;
	}

	for (jrd_prc** iter = att_procedures.begin(); iter < att_procedures.end(); ++iter)
	{
		jrd_prc* const procedure = *iter;
		if (procedure)
			delete procedure;
	}

	while (att_pools.hasData())
		deletePool(att_pools.pop());

	// For normal attachments that happens in release_attachment(),
	// but for special ones like GC should be done also in dtor -
	// they do not (and should not) call release_attachment().
	// It's no danger calling detachLocks()
	// once more here because it nulls att_long_locks.
	//		AP 2007
	detachLocks();
}


Jrd::PreparedStatement* Jrd::Attachment::prepareStatement(thread_db* tdbb, jrd_tra* transaction,
	const string& text, Firebird::MemoryPool* pool)
{
	pool = pool ? pool : tdbb->getDefaultPool();
	return FB_NEW_POOL(*pool) PreparedStatement(tdbb, *pool, this, transaction, text, true);
}


Jrd::PreparedStatement* Jrd::Attachment::prepareStatement(thread_db* tdbb, jrd_tra* transaction,
	const PreparedStatement::Builder& builder, Firebird::MemoryPool* pool)
{
	pool = pool ? pool : tdbb->getDefaultPool();
	return FB_NEW_POOL(*pool) PreparedStatement(tdbb, *pool, this, transaction, builder, true);
}


PreparedStatement* Jrd::Attachment::prepareUserStatement(thread_db* tdbb, jrd_tra* transaction,
	const string& text, Firebird::MemoryPool* pool)
{
	pool = pool ? pool : tdbb->getDefaultPool();
	return FB_NEW_POOL(*pool) PreparedStatement(tdbb, *pool, this, transaction, text, false);
}


MetaName Jrd::Attachment::nameToMetaCharSet(thread_db* tdbb, const MetaName& name)
{
	if (att_charset == CS_METADATA || att_charset == CS_NONE)
		return name;

	UCHAR buffer[MAX_SQL_IDENTIFIER_SIZE];
	ULONG len = INTL_convert_bytes(tdbb, CS_METADATA, buffer, MAX_SQL_IDENTIFIER_LEN,
		att_charset, (const BYTE*) name.c_str(), name.length(), ERR_post);
	buffer[len] = '\0';

	return MetaName((const char*) buffer);
}


MetaName Jrd::Attachment::nameToUserCharSet(thread_db* tdbb, const MetaName& name)
{
	if (att_charset == CS_METADATA || att_charset == CS_NONE)
		return name;

	UCHAR buffer[MAX_SQL_IDENTIFIER_SIZE];
	ULONG len = INTL_convert_bytes(tdbb, att_charset, buffer, MAX_SQL_IDENTIFIER_LEN,
		CS_METADATA, (const BYTE*) name.c_str(), name.length(), ERR_post);
	buffer[len] = '\0';

	return MetaName((const char*) buffer);
}


string Jrd::Attachment::stringToMetaCharSet(thread_db* tdbb, const string& str,
	const char* charSet)
{
	USHORT charSetId = att_charset;

	if (charSet)
	{
		if (!MET_get_char_coll_subtype(tdbb, &charSetId, (const UCHAR*) charSet,
				static_cast<USHORT>(strlen(charSet))))
		{
			(Arg::Gds(isc_charset_not_found) << Arg::Str(charSet)).raise();
		}
	}

	if (charSetId == CS_METADATA || charSetId == CS_NONE)
		return str;

	HalfStaticArray<UCHAR, BUFFER_MEDIUM> buffer(str.length() * sizeof(ULONG));
	ULONG len = INTL_convert_bytes(tdbb, CS_METADATA, buffer.begin(), buffer.getCapacity(),
		charSetId, (const BYTE*) str.c_str(), str.length(), ERR_post);

	return string((char*) buffer.begin(), len);
}


string Jrd::Attachment::stringToUserCharSet(thread_db* tdbb, const string& str)
{
	if (att_charset == CS_METADATA || att_charset == CS_NONE)
		return str;

	HalfStaticArray<UCHAR, BUFFER_MEDIUM> buffer(str.length() * sizeof(ULONG));
	ULONG len = INTL_convert_bytes(tdbb, att_charset, buffer.begin(), buffer.getCapacity(),
		CS_METADATA, (const BYTE*) str.c_str(), str.length(), ERR_post);

	return string((char*) buffer.begin(), len);
}


// We store in CS_METADATA.
void Jrd::Attachment::storeMetaDataBlob(thread_db* tdbb, jrd_tra* transaction,
	bid* blobId, const string& text, USHORT fromCharSet)
{
	UCharBuffer bpb;
	if (fromCharSet != CS_METADATA)
		BLB_gen_bpb(isc_blob_text, isc_blob_text, fromCharSet, CS_METADATA, bpb);

	blb* blob = blb::create2(tdbb, transaction, blobId, bpb.getCount(), bpb.begin());
	try
	{
		blob->BLB_put_data(tdbb, (const UCHAR*) text.c_str(), text.length());
	}
	catch (const Exception&)
	{
		blob->BLB_close(tdbb);
		throw;
	}

	blob->BLB_close(tdbb);
}


// We store raw stuff; don't attempt to translate.
void Jrd::Attachment::storeBinaryBlob(thread_db* tdbb, jrd_tra* transaction,
	bid* blobId, const ByteChunk& chunk)
{
	blb* blob = blb::create2(tdbb, transaction, blobId, 0, NULL);
	try
	{
		blob->BLB_put_data(tdbb, chunk.data, chunk.length);
	}
	catch (const Exception&)
	{
		blob->BLB_close(tdbb);
		throw;
	}

	blob->BLB_close(tdbb);
}

void Jrd::Attachment::releaseGTTs(thread_db* tdbb)
{
	if (!att_relations)
		return;

	for (FB_SIZE_T i = 1; i < att_relations->count(); i++)
	{
		jrd_rel* relation = (*att_relations)[i];
		if (relation && (relation->rel_flags & REL_temp_conn) &&
			!(relation->rel_flags & (REL_deleted | REL_deleting)))
		{
			relation->delPages(tdbb);
		}
	}
}

static void runDBTriggers(thread_db* tdbb, TriggerAction action)
{
	fb_assert(action == TRIGGER_CONNECT || action == TRIGGER_DISCONNECT);

	Database* dbb = tdbb->getDatabase();
	Attachment* att = tdbb->getAttachment();
	fb_assert(dbb);
	fb_assert(att);

	const unsigned trgKind = (action == TRIGGER_CONNECT) ? DB_TRIGGER_CONNECT : DB_TRIGGER_DISCONNECT;

	const TrigVector* const triggers =	att->att_triggers[trgKind];
	if (!triggers || triggers->isEmpty())
		return;

	ThreadStatusGuard temp_status(tdbb);
	jrd_tra* transaction = NULL;

	try
	{
		transaction = TRA_start(tdbb, 0, NULL);
		EXE_execute_db_triggers(tdbb, transaction, action);
		TRA_commit(tdbb, transaction, false);
		return;
	}
	catch (const Exception& /*ex*/)
	{
		if (!(dbb->dbb_flags & DBB_bugcheck) && transaction)
		{
			try
			{
				TRA_rollback(tdbb, transaction, false, false);
			}
			catch (const Exception& /*ex2*/)
			{
			}
		}
		throw;
	}
}

void Jrd::Attachment::resetSession(thread_db* tdbb, jrd_tra** traHandle)
{
	jrd_tra* oldTran = traHandle ? *traHandle : nullptr;
	if (att_transactions)
	{
		int n = 0;
		bool err = false;
		for (const jrd_tra* tra = att_transactions; tra; tra = tra->tra_next)
		{
			n++;
			if (tra != oldTran && !(tra->tra_flags & TRA_prepared))
				err = true;
		}

		// Cannot reset user session
		// There are open transactions (@1 active)
		if (err)
		{
			ERR_post(Arg::Gds(isc_ses_reset_err) <<
				Arg::Gds(isc_ses_reset_open_trans) << Arg::Num(n));
		}
	}

	AutoSetRestoreFlag<ULONG> flags(&att_flags, ATT_resetting, true);

	ULONG oldFlags = 0;
	SSHORT oldTimeout = 0;
	RefPtr<JTransaction> jTran;
	bool shutAtt = false;
	try
	{
		// Run ON DISCONNECT trigger before reset
		if (!(att_flags & ATT_no_db_triggers))
			runDBTriggers(tdbb, TRIGGER_DISCONNECT);

		// shutdown attachment on any error after this point
		shutAtt = true;

		if (oldTran)
		{
			oldFlags = oldTran->tra_flags;
			oldTimeout = oldTran->tra_lock_timeout;
			jTran = oldTran->getInterface(false);

			// It will also run run ON TRANSACTION ROLLBACK triggers
			JRD_rollback_transaction(tdbb, oldTran);
			*traHandle = nullptr;
		}

		// Session was reset with warning(s)
		// Transaction is rolled back due to session reset, all changes are lost
		if (oldFlags & TRA_write)
		{
			ERR_post_warning(Arg::Warning(isc_ses_reset_warn) <<
				Arg::Gds(isc_ses_reset_tran_rollback));
		}

		att_initial_options.resetAttachment(this);

		// reset timeouts
		setIdleTimeout(0);
		setStatementTimeout(0);

		// reset context variables
		att_context_vars.clear();

		// reset role
		if (att_user->resetRole())
			SCL_release_all(att_security_classes);

		// reset GTT's
		releaseGTTs(tdbb);

		// Run ON CONNECT trigger after reset
		if (!(att_flags & ATT_no_db_triggers))
			runDBTriggers(tdbb, TRIGGER_CONNECT);

		if (oldTran)
		{
			jrd_tra* newTran = TRA_start(tdbb, oldFlags, oldTimeout);
			if (jTran)
			{
				fb_assert(jTran->getHandle() == NULL);

				newTran->setInterface(jTran);
				jTran->setHandle(newTran);
			}

			// run ON TRANSACTION START triggers
			JRD_run_trans_start_triggers(tdbb, newTran);

			tdbb->setTransaction(newTran);
			*traHandle = newTran;
		}
	}
	catch (const Exception& ex)
	{
		if (shutAtt)
			signalShutdown(isc_ses_reset_failed);

		Arg::StatusVector error;
		error.assign(ex);
		error.prepend(Arg::Gds(shutAtt ? isc_ses_reset_failed : isc_ses_reset_err));
		error.raise();
	}
}


void Jrd::Attachment::signalCancel()
{
	att_flags |= ATT_cancel_raise;

	if (att_ext_connection && att_ext_connection->isConnected())
		att_ext_connection->cancelExecution(false);

	LCK_cancel_wait(this);
}


void Jrd::Attachment::signalShutdown(ISC_STATUS code)
{
	att_flags |= ATT_shutdown;
	if (getStable())
		getStable()->setShutError(code);

	if (att_ext_connection && att_ext_connection->isConnected())
		att_ext_connection->cancelExecution(true);

	LCK_cancel_wait(this);
}


void Jrd::Attachment::mergeStats()
{
	MutexLockGuard guard(att_database->dbb_stats_mutex, FB_FUNCTION);
	att_database->dbb_stats.adjust(att_base_stats, att_stats, true);
	att_base_stats.assign(att_stats);
}


bool Attachment::hasActiveRequests() const
{
	for (const jrd_tra* transaction = att_transactions;
		transaction; transaction = transaction->tra_next)
	{
		for (const Request* request = transaction->tra_requests;
			request; request = request->req_tra_next)
		{
			if (request->req_transaction && (request->req_flags & req_active))
				return true;
		}
	}

	return false;
}


// Find an inactive incarnation of a system request. If necessary, clone it.
Request* Jrd::Attachment::findSystemRequest(thread_db* tdbb, USHORT id, USHORT which)
{
	static const int MAX_RECURSION = 100;

	// If the request hasn't been compiled or isn't active, there're nothing to do.

	//Database::CheckoutLockGuard guard(this, dbb_cmp_clone_mutex);

	fb_assert(which == IRQ_REQUESTS || which == DYN_REQUESTS);

	Statement* statement = (which == IRQ_REQUESTS ? att_internal[id] : att_dyn_req[id]);

	if (!statement)
		return NULL;

	// Look for requests until we find one that is available.

	for (int n = 0;; ++n)
	{
		if (n > MAX_RECURSION)
		{
			ERR_post(Arg::Gds(isc_no_meta_update) <<
						Arg::Gds(isc_req_depth_exceeded) << Arg::Num(MAX_RECURSION));
			// Msg363 "request depth exceeded. (Recursive definition?)"
		}

		Request* clone = statement->getRequest(tdbb, n);

		if (!(clone->req_flags & (req_active | req_reserved)))
		{
			clone->req_flags |= req_reserved;
			return clone;
		}
	}
}

void Jrd::Attachment::initLocks(thread_db* tdbb)
{
	// Take out lock on attachment id

	const lock_ast_t ast = (att_flags & ATT_system) ? NULL : blockingAstShutdown;

	Lock* lock = FB_NEW_RPT(*att_pool, 0)
		Lock(tdbb, sizeof(AttNumber), LCK_attachment, this, ast);
	att_id_lock = lock;
	lock->setKey(att_attachment_id);
	LCK_lock(tdbb, lock, LCK_EX, LCK_WAIT);

	// Allocate and take the monitoring lock

	lock = FB_NEW_RPT(*att_pool, 0)
		Lock(tdbb, sizeof(AttNumber), LCK_monitor, this, blockingAstMonitor);
	att_monitor_lock = lock;
	lock->setKey(att_attachment_id);
	LCK_lock(tdbb, lock, LCK_EX, LCK_WAIT);

	// Unless we're a system attachment, allocate cancellation and replication locks

	if (!(att_flags & ATT_system))
	{
		lock = FB_NEW_RPT(*att_pool, 0)
			Lock(tdbb, sizeof(AttNumber), LCK_cancel, this, blockingAstCancel);
		att_cancel_lock = lock;
		lock->setKey(att_attachment_id);

		lock = FB_NEW_RPT(*att_pool, 0)
			Lock(tdbb, 0, LCK_repl_tables, this, blockingAstReplSet);
		att_repl_lock = lock;
	}
}

void Jrd::Attachment::releaseLocks(thread_db* tdbb)
{
	// Go through relations and indices and release
	// all existence locks that might have been taken.

	vec<jrd_rel*>* rvector = att_relations;

	if (rvector)
	{
		vec<jrd_rel*>::iterator ptr, end;

		for (ptr = rvector->begin(), end = rvector->end(); ptr < end; ++ptr)
		{
			jrd_rel* relation = *ptr;

			if (relation)
			{
				if (relation->rel_existence_lock)
				{
					LCK_release(tdbb, relation->rel_existence_lock);
					relation->rel_flags |= REL_check_existence;
					relation->rel_use_count = 0;
				}

				if (relation->rel_partners_lock)
				{
					LCK_release(tdbb, relation->rel_partners_lock);
					relation->rel_flags |= REL_check_partners;
				}

				if (relation->rel_rescan_lock)
				{
					LCK_release(tdbb, relation->rel_rescan_lock);
					relation->rel_flags &= ~REL_scanned;
				}

				if (relation->rel_gc_lock)
				{
					LCK_release(tdbb, relation->rel_gc_lock);
					relation->rel_flags |= REL_gc_lockneed;
				}

				for (IndexLock* index = relation->rel_index_locks; index; index = index->idl_next)
				{
					if (index->idl_lock)
					{
						index->idl_count = 0;
						LCK_release(tdbb, index->idl_lock);
					}
				}

				for (IndexBlock* index = relation->rel_index_blocks; index; index = index->idb_next)
				{
					if (index->idb_lock)
						LCK_release(tdbb, index->idb_lock);
				}
			}
		}
	}

	// Release all procedure existence locks that might have been taken

	for (jrd_prc** iter = att_procedures.begin(); iter < att_procedures.end(); ++iter)
	{
		jrd_prc* const procedure = *iter;

		if (procedure)
		{
			if (procedure->existenceLock)
			{
				LCK_release(tdbb, procedure->existenceLock);
				procedure->flags |= Routine::FLAG_CHECK_EXISTENCE;
				procedure->useCount = 0;
			}
		}
	}

	// Release all function existence locks that might have been taken

	for (Function** iter = att_functions.begin(); iter < att_functions.end(); ++iter)
	{
		Function* const function = *iter;

		if (function)
			function->releaseLocks(tdbb);
	}

	// Release collation existence locks

	releaseIntlObjects(tdbb);

	// Release the DSQL cache locks

	DSqlCache::Accessor accessor(&att_dsql_cache);
	for (bool getResult = accessor.getFirst(); getResult; getResult = accessor.getNext())
		LCK_release(tdbb, accessor.current()->second.lock);

	// Release the remaining locks

	if (att_id_lock)
		LCK_release(tdbb, att_id_lock);

	if (att_cancel_lock)
		LCK_release(tdbb, att_cancel_lock);

	if (att_monitor_lock)
		LCK_release(tdbb, att_monitor_lock);

	if (att_temp_pg_lock)
		LCK_release(tdbb, att_temp_pg_lock);

	if (att_repl_lock)
		LCK_release(tdbb, att_repl_lock);

	// And release the system requests

	for (Statement** itr = att_internal.begin(); itr != att_internal.end(); ++itr)
	{
		if (*itr)
			(*itr)->release(tdbb);
	}

	for (Statement** itr = att_dyn_req.begin(); itr != att_dyn_req.end(); ++itr)
	{
		if (*itr)
			(*itr)->release(tdbb);
	}
}

void Jrd::Attachment::detachLocks()
{
/**************************************
 *
 *	d e t a c h L o c k s
 *
 **************************************
 *
 * Functional description
 * Bug #7781, need to null out the attachment pointer of all locks which
 * were hung off this attachment block, to ensure that the attachment
 * block doesn't get dereferenced after it is released
 *
 **************************************/
	if (!att_long_locks)
		return;

	Lock* long_lock = att_long_locks;
	while (long_lock)
	{
#ifdef DEBUG_LCK_LIST
		att_long_locks_type = long_lock->lck_next_type;
#endif
		long_lock = long_lock->detach();
	}

	att_long_locks = NULL;
}

void Jrd::Attachment::releaseRelations(thread_db* tdbb)
{
	if (att_relations)
	{
		vec<jrd_rel*>* vector = att_relations;

		for (vec<jrd_rel*>::iterator ptr = vector->begin(), end = vector->end(); ptr < end; ++ptr)
		{
			jrd_rel* relation = *ptr;

			if (relation)
			{
				if (relation->rel_file)
					EXT_fini(relation, false);

				delete relation;
			}
		}
	}
}

int Jrd::Attachment::blockingAstShutdown(void* ast_object)
{
	Jrd::Attachment* const attachment = static_cast<Jrd::Attachment*>(ast_object);

	try
	{
		Database* const dbb = attachment->att_database;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, attachment->att_id_lock);

		attachment->signalShutdown(isc_att_shut_killed);

		JRD_shutdown_attachment(attachment);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

int Jrd::Attachment::blockingAstCancel(void* ast_object)
{
	Jrd::Attachment* const attachment = static_cast<Jrd::Attachment*>(ast_object);

	try
	{
		Database* const dbb = attachment->att_database;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, attachment->att_cancel_lock);

		attachment->signalCancel();

		LCK_release(tdbb, attachment->att_cancel_lock);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

int Jrd::Attachment::blockingAstMonitor(void* ast_object)
{
	Jrd::Attachment* const attachment = static_cast<Jrd::Attachment*>(ast_object);

	try
	{
		Database* const dbb = attachment->att_database;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, attachment->att_monitor_lock);

		if (!(attachment->att_flags & ATT_monitor_done))
		{
			try
			{
				Monitoring::dumpAttachment(tdbb, attachment);
			}
			catch (const Exception& ex)
			{
				iscLogException("Cannot dump the monitoring data", ex);
			}

			LCK_downgrade(tdbb, attachment->att_monitor_lock);
			attachment->att_flags |= ATT_monitor_done;
		}
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

void Jrd::Attachment::SyncGuard::init(const char* f, bool
#ifdef DEV_BUILD
	optional
#endif
	)
{
	fb_assert(optional || jStable);

	if (jStable)
	{
		jStable->getSync()->enter(f);
		if (!jStable->getHandle())
		{
			jStable->getSync()->leave();
			Arg::Gds(isc_att_shutdown).raise();
		}
	}
}

void StableAttachmentPart::manualLock(ULONG& flags, const ULONG whatLock)
{
	fb_assert(!(flags & whatLock));

	if (whatLock & ATT_async_manual_lock)
	{
		async.enter(FB_FUNCTION);
		flags |= ATT_async_manual_lock;
	}

	if (whatLock & ATT_manual_lock)
	{
		mainSync.enter(FB_FUNCTION);
		flags |= ATT_manual_lock;
	}
}

void StableAttachmentPart::manualUnlock(ULONG& flags)
{
	if (flags & ATT_manual_lock)
	{
		flags &= ~ATT_manual_lock;
		mainSync.leave();
	}
	manualAsyncUnlock(flags);
}

void StableAttachmentPart::manualAsyncUnlock(ULONG& flags)
{
	if (flags & ATT_async_manual_lock)
	{
		flags &= ~ATT_async_manual_lock;
		async.leave();
	}
}

void StableAttachmentPart::onIdleTimer(TimerImpl*)
{
	// Ensure attachment is still alive and still idle

	EnsureUnlock<Sync, NotRefCounted> guard(*this->getSync(), FB_FUNCTION);
	if (!guard.tryEnter())
		return;

	Attachment* att = this->getHandle();
	att->signalShutdown(isc_att_shut_idle);
	JRD_shutdown_attachment(att);
}

JAttachment* Attachment::getInterface() throw()
{
	return att_stable->getInterface();
}

unsigned int Attachment::getActualIdleTimeout() const
{
	unsigned int timeout = att_database->dbb_config->getConnIdleTimeout() * 60;
	if (att_idle_timeout && (att_idle_timeout < timeout || !timeout))
		timeout = att_idle_timeout;

	return timeout;
}

void Attachment::setupIdleTimer(bool clear)
{
	unsigned int timeout = clear ? 0 : getActualIdleTimeout();
	if (!timeout || hasActiveRequests())
	{
		if (att_idle_timer)
			att_idle_timer->reset(0);
	}
	else
	{
		if (!att_idle_timer)
		{
			att_idle_timer = FB_NEW IdleTimer(getStable());
			att_idle_timer->setOnTimer(&StableAttachmentPart::onIdleTimer);
		}

		att_idle_timer->reset(timeout);
	}
}

UserId* Attachment::getUserId(const MetaString& userName)
{
	// It's necessary to keep specified sql role of user
	if (att_user && att_user->getUserName() == userName)
		return att_user;

	UserId* result = NULL;
	if (!att_user_ids.get(userName, result))
	{
		result = FB_NEW_POOL(*att_pool) UserId(*att_pool);
		result->setUserName(userName);
		att_user_ids.put(userName, result);
	}
	return result;
}

void Attachment::checkReplSetLock(thread_db* tdbb)
{
	if (att_flags & ATT_repl_reset)
	{
		fb_assert(att_repl_lock->lck_logical == LCK_none);
		LCK_lock(tdbb, att_repl_lock, LCK_SR, LCK_WAIT);
		att_flags &= ~ATT_repl_reset;
	}
}

void Attachment::invalidateReplSet(thread_db* tdbb, bool broadcast)
{
	att_flags |= ATT_repl_reset;

	if (att_relations)
	{
		for (auto relation : *att_relations)
		{
			if (relation)
				relation->rel_repl_state.invalidate();
		}
	}

	if (broadcast)
	{
		// Signal other attachments about the changed state
		if (att_repl_lock->lck_logical == LCK_none)
			LCK_lock(tdbb, att_repl_lock, LCK_EX, LCK_WAIT);
		else
			LCK_convert(tdbb, att_repl_lock, LCK_EX, LCK_WAIT);
	}

	LCK_release(tdbb, att_repl_lock);
}

int Attachment::blockingAstReplSet(void* ast_object)
{
	Attachment* const attachment = static_cast<Attachment*>(ast_object);

	try
	{
		Database* const dbb = attachment->att_database;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, attachment->att_repl_lock);

		attachment->invalidateReplSet(tdbb, false);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}
