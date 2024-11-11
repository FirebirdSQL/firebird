/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		rlck.cpp
 *	DESCRIPTION:	Record and Relation Lock Manager
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
 * 2001.07.06 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, as the engine now fully supports
 *                         readonly databases.
 */

#include "firebird.h"
#include "../jrd/ini.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/err_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/rlck_proto.h"

using namespace Jrd;
using namespace Firebird;

Lock* RLCK_reserve_relation(thread_db* tdbb, jrd_tra* transaction, jrd_rel* relation, bool write_flag)
{
/**************************************
 *
 *	R L C K _ r e s e r v e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Lock a relation within a transaction.  If the relation
 *	is already locked at a lower level, upgrade the lock.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	if (transaction->tra_flags & TRA_system)
		return NULL;

	// hvlad: virtual relations are always writable, see below for the other rules

	if (write_flag && !relation->isVirtual())
	{
		// In read-only databases, only GTTs with ON COMMIT DELETE ROWS clause are writable

		if (dbb->readOnly() && !(relation->rel_flags & REL_temp_tran))
			ERR_post(Arg::Gds(isc_read_only_database));

		// No other limitations for GTTs

		if (!relation->isTemporary())
		{
			// Persistent tables are not writable in read-only transactions

			if (transaction->tra_flags & TRA_readonly)
				ERR_post(Arg::Gds(isc_read_only_trans));

			// Inside a read-only replica, only replicator sessions are expected to be writable.
			// However, we also allow not replicated DDL statements (e.g. ALTER DATABASE).
			// And insertions into RDB$BACKUP_HISTORY are also allowed to support nbackup, sigh.

			if (dbb->isReplica(REPLICA_READ_ONLY) &&
				!(tdbb->tdbb_flags & TDBB_replicator) &&
				!(tdbb->tdbb_flags & TDBB_repl_in_progress))
			{
				// This condition is a workaround for nbackup
				if (relation->rel_id != rel_backup_history)
					ERR_post(Arg::Gds(isc_read_only_trans));
			}
		}
	}

	Lock* lock = RLCK_transaction_relation_lock(tdbb, transaction, relation);

	// Next, figure out what kind of lock we need

	USHORT level;
	if (transaction->tra_flags & TRA_degree3)
	{
		if (write_flag)
			level = LCK_EX;
		else
			level = LCK_PR;
	}
	else
	{
		if (write_flag)
			level = LCK_SW;
		else
			level = LCK_none;
	}

	// If the lock is already "good enough", we're done

	if (level <= lock->lck_logical)
		return lock;

	// Get lock

	USHORT result;
	if (lock->lck_logical)
		result = LCK_convert(tdbb, lock, level, transaction->getLockWait());
	else
		result = LCK_lock(tdbb, lock, level, transaction->getLockWait());

	if (!result)
	{
		string err;
		err.printf("Acquire lock for relation (%s) failed", relation->rel_name.toString().c_str());

		ERR_append_status(tdbb->tdbb_status_vector, Arg::Gds(isc_random) << Arg::Str(err));
		ERR_punt();
	}

	return lock;
}


Lock* RLCK_transaction_relation_lock(thread_db* tdbb, jrd_tra* transaction, jrd_rel* relation)
{
/**************************************
 *
 *	R L C K _ t r a n s a c t i o n _ r e l a t i o n _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Take out a relation lock within the context of
 *	a transaction.
 *
 **************************************/
	SET_TDBB(tdbb);

	const ULONG relId = relation->rel_id;

	Lock* lock;
	vec<Lock*>* vector = transaction->tra_relation_locks;
	if (vector && (relId < vector->count()) && (lock = (*vector)[relId]))
	{
		return lock;
	}

	vector = transaction->tra_relation_locks =
		vec<Lock*>::newVector(*transaction->tra_pool, transaction->tra_relation_locks, relId + 1);
	lock = jrd_rel::createLock(tdbb, transaction->tra_pool, relation, LCK_relation, true);

	// enter all relation locks into the intra-process lock manager and treat
	// them as compatible within the attachment according to IPLM rules
	lock->lck_compatible = tdbb->getAttachment();
	// for relations locked within a transaction, add a second level of
	// compatibility within the intra-process lock manager which specifies
	// that relation locks are incompatible with locks taken out by other
	// transactions, if a transaction is specified
	lock->lck_compatible2 = transaction;

	(*vector)[relId] = lock;

	return lock;
}
