/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		tra.cpp
 *	DESCRIPTION:	Transaction manager
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
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 */

#include "firebird.h"
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/ods.h"
#include "../jrd/pag.h"
#include "../jrd/lck.h"
#include "../jrd/lls.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/extds/ExtDS.h"
#include "../jrd/rse.h"
#include "../jrd/intl_classes.h"
#include "../common/ThreadStart.h"
#include "../jrd/TimeZone.h"
#include "../jrd/UserManagement.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/idx_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/tpc_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/scl_proto.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/utils_proto.h"
#include "../lock/lock_proto.h"
#include "../dsql/dsql.h"
#include "../dsql/dsql_proto.h"
#include "../common/StatusArg.h"
#include "../jrd/replication/Publisher.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceJrdHelpers.h"
#include "../jrd/Function.h"
#include "../jrd/Collation.h"
#include "../jrd/Mapping.h"
#include "../jrd/DbCreators.h"
#include "../common/os/fbsyslog.h"


const int DYN_MSG_FAC	= 8;

using namespace Jrd;
using namespace Ods;
using namespace Firebird;

typedef Firebird::GenericMap<Firebird::Pair<Firebird::NonPooled<USHORT, UCHAR> > > RelationLockTypeMap;


#ifdef SUPERSERVER_V2
static TraNumber bump_transaction_id(thread_db*, WIN*);
#else
static header_page* bump_transaction_id(thread_db*, WIN*, bool);
#endif
static void retain_context(thread_db* tdbb, jrd_tra* transaction, bool commit, int state);
static void expand_view_lock(thread_db* tdbb, jrd_tra*, jrd_rel*, UCHAR lock_type,
	const char* option_name, RelationLockTypeMap& lockmap, const int level);
static tx_inv_page* fetch_inventory_page(thread_db*, WIN* window, ULONG sequence, USHORT lock_level);
static const char* get_lockname_v3(const UCHAR lock);
static ULONG inventory_page(thread_db*, ULONG);
static int limbo_transaction(thread_db*, TraNumber id);
static void release_temp_tables(thread_db*, jrd_tra*);
static void retain_temp_tables(thread_db*, jrd_tra*, TraNumber);
static void restart_requests(thread_db*, jrd_tra*);
static void start_sweeper(thread_db*);
static THREAD_ENTRY_DECLARE sweep_database(THREAD_ENTRY_PARAM);
static void transaction_flush(thread_db* tdbb, USHORT flush_flag, TraNumber tra_number);
static void transaction_options(thread_db*, jrd_tra*, const UCHAR*, USHORT);
static void transaction_start(thread_db* tdbb, jrd_tra* temp);

static const UCHAR sweep_tpb[] =
{
	isc_tpb_version1, isc_tpb_read,
	isc_tpb_read_committed, isc_tpb_rec_version
};


jrd_req* TRA_get_prior_request(thread_db* tdbb)
{
	// See if there is any request right above us in the call stack
	jrd_req* org_request;
	thread_db* jrd_ctx = tdbb;
	do
	{
		// Check regular request call stack
		org_request = jrd_ctx->getRequest();
		if (org_request)
			break;

		// Check for engine context switch (EXECUTE STATEMENT, etc)
		ThreadData* ctx = jrd_ctx;
		jrd_ctx = NULL;
		while( (ctx = ctx->getPriorContext()) )
		{
			if (ctx->getType() == ThreadData::tddDBB)
			{
				jrd_ctx = static_cast<thread_db*>(ctx);
				break;
			}
		}
	} while (jrd_ctx);

	return org_request;
}

void TRA_setup_request_snapshot(Jrd::thread_db* tdbb, Jrd::jrd_req* request)
{
	// This function is called whenever request is started in a transaction.
	// Setup context to preserve read consistency in READ COMMITTED transactions.

	Jrd::jrd_tra* transaction = request->req_transaction;

	// We assume that request is already attached to a transaction
	fb_assert(transaction);

	// If we are not READ COMMITTED or read consistency is not needed then nothing to do here
	if (!(transaction->tra_flags & TRA_read_committed) || !(transaction->tra_flags & TRA_read_consistency))
		return;

	// See if there is any request right above us in the call stack
	jrd_req* org_request = TRA_get_prior_request(tdbb);

	if (org_request && org_request->req_transaction == transaction)
	{
		fb_assert(org_request->req_snapshot.m_owner);
		request->req_snapshot.m_owner = org_request->req_snapshot.m_owner;
		return;
	}

	// If we are a top-level request or caller is executed in a different transaction,
	// we need to set up statement snapshot for read consistency and own it

	request->req_snapshot.m_owner = request;
	request->req_snapshot.m_number = 0;

	request->req_snapshot.m_handle =
		tdbb->getDatabase()->dbb_tip_cache->beginSnapshot(tdbb,
			tdbb->getAttachment()->att_attachment_id, request->req_snapshot.m_number);
}


void TRA_release_request_snapshot(Jrd::thread_db* tdbb, Jrd::jrd_req* request)
{
	// This function is called whenever request has completed processing
	// in a transaction (normally or abnormally)

	if (!request->req_snapshot.m_owner)
		return;

	if (request->req_snapshot.m_number)
	{
		tdbb->getDatabase()->dbb_tip_cache->endSnapshot(tdbb, request->req_snapshot.m_handle,
			request->req_attachment->att_attachment_id);
		request->req_snapshot.m_handle = 0;
		request->req_snapshot.m_number = 0;
	}

	request->req_snapshot.m_owner = nullptr;
}


void TRA_attach_request(Jrd::jrd_tra* transaction, Jrd::jrd_req* request)
{
	// When request finishes normally transaction reference is not cleared.
	// Then if afterwards request is restarted TRA_attach_request is called again.
	if (request->req_transaction)
	{
		if (request->req_transaction == transaction)
			return;
		TRA_detach_request(request);
	}

	fb_assert(request->req_transaction == NULL);
	fb_assert(request->req_tra_next == NULL);
	fb_assert(request->req_tra_prev == NULL);

	// Assign transaction reference
	request->req_transaction = transaction;

	// Add request to the doubly linked list
	if (transaction->tra_requests)
	{
		fb_assert(transaction->tra_requests->req_tra_prev == NULL);
		transaction->tra_requests->req_tra_prev = request;
		request->req_tra_next = transaction->tra_requests;
	}
	transaction->tra_requests = request;
}

void TRA_detach_request(Jrd::jrd_req* request)
{
	if (!request->req_transaction)
	{
		fb_assert(!request->req_savepoints);
		return;
	}

	// Release stored looper savepoints
	Savepoint::destroy(request->req_savepoints);

	// Remove request from the doubly linked list
	if (request->req_tra_next)
	{
		fb_assert(request->req_tra_next->req_tra_prev == request);
		request->req_tra_next->req_tra_prev = request->req_tra_prev;
	}

	if (request->req_tra_prev)
	{
		fb_assert(request->req_tra_prev->req_tra_next == request);
		request->req_tra_prev->req_tra_next = request->req_tra_next;
	}
	else
	{
		fb_assert(request->req_transaction->tra_requests == request);
		request->req_transaction->tra_requests = request->req_tra_next;
	}

	// Clear references
	request->req_transaction = NULL;
	request->req_tra_next = NULL;
	request->req_tra_prev = NULL;
}

bool TRA_active_transactions(thread_db* tdbb, Database* dbb)
{
/**************************************
 *
 *	T R A _ a c t i v e _ t r a n s a c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Determine if any transactions are active.
 *	Return true is active transactions; otherwise
 *	return false if no active transactions.
 *
 **************************************/
	SET_TDBB(tdbb);

	return LCK_query_data(tdbb, LCK_tra, LCK_ANY) ? true : false;
}

bool TRA_cleanup(thread_db* tdbb)
{
/**************************************
 *
 *	T R A _ c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	TRA_cleanup is called at startup while an exclusive lock is
 *	held on the database.  Because we haven't started a transaction,
 *	and we have an exclusive lock on the db, any transactions marked
 *	as active on the transaction inventory pages are indeed dead.
 *	Mark them so.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// Return without cleaning up the TIP's for a ReadOnly database
	if (dbb->readOnly())
		return false;

	// First, make damn sure there are no outstanding transactions

	for (Jrd::Attachment* attachment = dbb->dbb_attachments; attachment;
		 attachment = attachment->att_next)
	{
		if (attachment->att_transactions)
			return false;
	}

	const ULONG trans_per_tip = dbb->dbb_page_manager.transPerTIP;

	// Read header page and allocate transaction number.  Since
	// the transaction inventory page was initialized to zero, it
	// transaction is automatically marked active.

	WIN window(HEADER_PAGE_NUMBER);
	const header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_header);
	const TraNumber ceiling = Ods::getNT(header);
	const TraNumber active = Ods::getOAT(header);
	CCH_RELEASE(tdbb, &window);

	if (ceiling == 0)
		return false;

	// Zip thru transactions from the "oldest active" to the next looking for
	// active transactions.  When one is found, declare it dead.

	const ULONG last = ceiling / trans_per_tip;
	ULONG number = active % trans_per_tip;
	TraNumber limbo = 0;
	bool found = false;

	for (ULONG sequence = active / trans_per_tip; sequence <= last; sequence++, number = 0)
	{
		window.win_page = inventory_page(tdbb, sequence);
		tx_inv_page* tip = (tx_inv_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_transactions);
		TraNumber max = ceiling - (TraNumber) sequence * trans_per_tip;
		if (max >= trans_per_tip)
			max = trans_per_tip - 1;
		for (; number <= max; number++)
		{
			const ULONG trans_offset = TRANS_OFFSET(number);
			UCHAR* byte = tip->tip_transactions + trans_offset;
			const USHORT shift = TRANS_SHIFT(number);
			const int state = (*byte >> shift) & TRA_MASK;
			if (state == tra_limbo && limbo == 0)
				limbo = (TraNumber) sequence * trans_per_tip + number;
			else if (state == tra_active)
			{
				found = true;
				CCH_MARK(tdbb, &window);
				*byte &= ~(TRA_MASK << shift);

				// hvlad: mark system transaction as committed
				if (sequence == 0 && number == 0) {
					*byte |= tra_committed << shift;
				}
				else {
					*byte |= tra_dead << shift;
				}
			}
		}
#ifdef SUPERSERVER_V2
		if (sequence == last)
		{
			CCH_MARK(tdbb, &window);
			for (; number < trans_per_tip; number++)
			{
				const ULONG trans_offset = TRANS_OFFSET(number);
				UCHAR* byte = tip->tip_transactions + trans_offset;
				const USHORT shift = TRANS_SHIFT(number);
				*byte &= ~(TRA_MASK << shift);
				if (tip->tip_next)
					*byte |= tra_committed << shift;
				else
					*byte |= tra_active << shift;
			}
		}
#endif
		CCH_RELEASE(tdbb, &window);
	}

#ifdef SUPERSERVER_V2
	window.win_page = inventory_page(tdbb, last);
	tx_inv_page* tip = (tx_inv_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_transactions);

	while (tip->tip_next)
	{
		CCH_RELEASE(tdbb, &window);
		window.win_page = inventory_page(tdbb, ++last);
		tip = (tx_inv_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_transactions);
		CCH_MARK(tdbb, &window);
		for (number = 0; number < trans_per_tip; number++)
		{
			const ULONG trans_offset = TRANS_OFFSET(number);
			UCHAR* byte = tip->tip_transactions + trans_offset;
			const USHORT shift = TRANS_SHIFT(number);
			*byte &= ~(TRA_MASK << shift);
			if (tip->tip_next || !number)
				*byte |= tra_committed << shift;
			else
				*byte |= tra_active << shift;
		}

		if (!tip->tip_next)
			dbb->dbb_next_transaction = (TraNumber) last * trans_per_tip;
	}

	CCH_RELEASE(tdbb, &window);
#endif

	return found;
}


void TRA_commit(thread_db* tdbb, jrd_tra* transaction, const bool retaining_flag)
{
/**************************************
 *
 *	T R A _ c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit a transaction.
 *
 **************************************/
	SET_TDBB(tdbb);

	TraceTransactionEnd trace(transaction, true, retaining_flag);

	EDS::Transaction::jrdTransactionEnd(tdbb, transaction, true, retaining_flag, false);

	jrd_tra* const sysTran = tdbb->getAttachment()->getSysTransaction();

	// If this is a commit retaining, and no updates have been performed,
	// and no events have been posted (via stored procedures etc)
	// no-op the operation.

	if (retaining_flag && !((transaction->tra_flags & TRA_write) || transaction->tra_deferred_job))
	{
		if (sysTran->tra_flags & TRA_write)
			transaction_flush(tdbb, FLUSH_SYSTEM, 0);

		transaction->tra_flags &= ~TRA_prepared;

		// Get rid of all user savepoints
		while (transaction->tra_save_point && !transaction->tra_save_point->isRoot())
			transaction->rollforwardSavepoint(tdbb);

		trace.finish(ITracePlugin::RESULT_SUCCESS);
		return;
	}

	if (transaction->tra_flags & TRA_invalidated)
		ERR_post(Arg::Gds(isc_trans_invalid));

	Jrd::ContextPoolHolder context(tdbb, transaction->tra_pool);

	// Perform any meta data work deferred

	if (!(transaction->tra_flags & TRA_prepared))
		DFW_perform_work(tdbb, transaction);

	// Commit associated transaction in security DB

	SecDbContext* secContext = transaction->getSecDbContext();
	if (secContext && secContext->tra)
	{
		LocalStatus ls;
		CheckStatusWrapper st(&ls);
		secContext->tra->commit(&st);

		if (st.getState() & IStatus::STATE_ERRORS)
			status_exception::raise(&st);

		secContext->tra = NULL;
		Mapping::clearCache(tdbb->getDatabase()->dbb_config->getSecurityDatabase(), Mapping::MAPPING_CACHE);

		transaction->eraseSecDbContext();
	}

	if (transaction->tra_flags & (TRA_prepare2 | TRA_reconnected))
		MET_update_transaction(tdbb, transaction, true);

	// Flush pages if transaction logically modified data

	if (transaction->tra_flags & TRA_write)
	{
		// Get rid of user savepoints to allow intermediate garbage collection
		// in indices and BLOBs after in-place updates
		while (transaction->tra_save_point)
			transaction->rollforwardSavepoint(tdbb);

		transaction_flush(tdbb, FLUSH_TRAN, transaction->tra_number);
	}
	else if ((transaction->tra_flags & (TRA_prepare2 | TRA_reconnected)) ||
		(sysTran->tra_flags & TRA_write))
	{
		// If the transaction only read data but is a member of a
		// multi-database transaction with a transaction description
		// message then flush RDB$TRANSACTIONS.

		transaction_flush(tdbb, FLUSH_SYSTEM, 0);
	}

	if (retaining_flag)
	{
		retain_context(tdbb, transaction, true, tra_committed);
		trace.finish(ITracePlugin::RESULT_SUCCESS);
		return;
	}

	// Set the state on the inventory page to be committed

	TRA_set_state(tdbb, transaction, transaction->tra_number, tra_committed);
	REPL_trans_commit(tdbb, transaction);

	// Perform any post commit work

	DFW_perform_post_commit_work(transaction);

	// notify any waiting locks that this transaction is committing;
	// there could be no lock if this transaction is being reconnected

	++transaction->tra_use_count;
	Lock* lock = transaction->tra_lock;
	if (lock && (lock->lck_logical < LCK_write))
		LCK_convert(tdbb, lock, LCK_write, LCK_WAIT);
	--transaction->tra_use_count;

	TRA_release_transaction(tdbb, transaction, &trace);
}


void TRA_extend_tip(thread_db* tdbb, ULONG sequence) //, WIN* precedence_window)
{
/**************************************
 *
 *	T R A _ e x t e n d _ t i p
 *
 **************************************
 *
 * Functional description
 *	Allocate and link in new TIP (transaction inventory page).
 *	This is called from TRA_start and from validate/repair.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// Start by fetching prior transaction page, if any
	tx_inv_page* prior_tip = NULL;
	WIN prior_window(DB_PAGE_SPACE, -1);
	if (sequence)
		prior_tip = fetch_inventory_page(tdbb, &prior_window, (sequence - 1), LCK_write);

	// Allocate and format new page
	WIN window(DB_PAGE_SPACE, -1);
	tx_inv_page* tip = (tx_inv_page*) DPM_allocate(tdbb, &window);
	tip->tip_header.pag_type = pag_transactions;

	CCH_must_write(tdbb, &window);
	CCH_RELEASE(tdbb, &window);

	// Release prior page

	if (sequence)
	{
		CCH_MARK_MUST_WRITE(tdbb, &prior_window);
		prior_tip->tip_next = window.win_page.getPageNum();
		CCH_RELEASE(tdbb, &prior_window);
	}

	// Link into internal data structures

	vcl* vector = dbb->dbb_t_pages =
		vcl::newVector(*dbb->dbb_permanent, dbb->dbb_t_pages, sequence + 1);
	(*vector)[sequence] = window.win_page.getPageNum();

	// Write into pages relation

	DPM_pages(tdbb, 0, pag_transactions, sequence, window.win_page.getPageNum());
}


int TRA_fetch_state(thread_db* tdbb, TraNumber number)
{
/**************************************
 *
 *	T R A _ f e t c h _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Physically fetch the state of a given
 *	transaction on the transaction inventory
 *	page.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// locate and fetch the proper TIP page

	const ULONG trans_per_tip = dbb->dbb_page_manager.transPerTIP;
	const ULONG tip_seq = number / trans_per_tip;
	WIN window(DB_PAGE_SPACE, -1);
	const tx_inv_page* tip = fetch_inventory_page(tdbb, &window, tip_seq, LCK_read);

	// calculate the state of the desired transaction

	const ULONG byte = TRANS_OFFSET(number % trans_per_tip);
	const USHORT shift = TRANS_SHIFT(number);
	const int state = (tip->tip_transactions[byte] >> shift) & TRA_MASK;

	CCH_RELEASE(tdbb, &window);

	return state;
}


void TRA_get_inventory(thread_db* tdbb, UCHAR* bit_vector, TraNumber base, TraNumber top)
{
/**************************************
 *
 *	T R A _ g e t _ i n v e n t o r y
 *
 **************************************
 *
 * Functional description
 *	Get an inventory of the state of all transactions
 *	between the base and top (included) transactions passed.
 *	To get a consistent view of the transaction
 *	inventory (in case we ever implement sub-transactions),
 *	do handoffs to read the pages in order.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// It does not make sence to call this function without bit_vector now
	fb_assert(bit_vector);

	const ULONG trans_per_tip = dbb->dbb_page_manager.transPerTIP;
	ULONG sequence = base / trans_per_tip;
	const ULONG last = top / trans_per_tip;

	// fetch the first inventory page

	WIN window(DB_PAGE_SPACE, -1);
	const tx_inv_page* tip = fetch_inventory_page(tdbb, &window, sequence++, LCK_read);

	// move the first page into the bit vector

	UCHAR* p = bit_vector;
	ULONG l = base % trans_per_tip;
	const UCHAR* q = tip->tip_transactions + TRANS_OFFSET(l);
	l = TRANS_OFFSET(MIN((top + TRA_MASK + 1 - base), trans_per_tip - l));
	memcpy(p, q, l);
	p += l;

	// move successive pages into the bit vector

	while (sequence <= last)
	{
		base = (TraNumber) sequence * trans_per_tip;

		// release the read lock as we go, so that some one else can
		// commit without having to signal all other transactions.

		tip = (tx_inv_page*) CCH_HANDOFF(tdbb, &window, inventory_page(tdbb, sequence++),
							  LCK_read, pag_transactions);

		l = TRANS_OFFSET(MIN((top + TRA_MASK + 1 - base), trans_per_tip));
		memcpy(p, tip->tip_transactions, l);
		p += l;
	}

	CCH_RELEASE(tdbb, &window);
}


int TRA_get_state(thread_db* tdbb, TraNumber number)
{
/**************************************
 *
 *	T R A _ g e t _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Get the state of a given transaction on the
 *	transaction inventory page.
 *
 **************************************/

	return TPC_snapshot_state(tdbb, number);
}


#ifdef SUPERSERVER_V2
void TRA_header_write(thread_db* tdbb, Database* dbb, TraNumber number)
{
/**************************************
 *
 *	T R A _ h e a d e r _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Force transaction ID on header to disk.
 *	Do post fetch check of the transaction
 *	ID header write as a concurrent thread
 *	might have written the header page
 *	while blocked on the latch.
 *
 *	The idea is to amortize the cost of
 *	header page I/O across multiple transactions.
 *
 **************************************/
	SET_TDBB(tdbb);

	// If transaction number is already on disk just return.

	if (!number || dbb->dbb_last_header_write < number)
	{
		WIN window(HEADER_PAGE_NUMBER);
		header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

		const TraNumber next_transaction = Ods::getNT(header);
		const TraNumber oldest_active = Ods::getOAT(header);
		const TraNumber oldest_transaction = Ods::getOIT(header);
		const TraNumber oldest_snapshot = Ods::getOST(header);

		if (next_transaction)
		{
			if (oldest_active > next_transaction)
				BUGCHECK(266);	//next transaction older than oldest active

			if (oldest_transaction > next_transaction)
				BUGCHECK(267);	// next transaction older than oldest transaction
		}

		// The header page might have been written while waiting
		// for the latch; perform a post fetch check and optimize
		// this case by not writing the page again.

		if (!number || dbb->dbb_last_header_write < number)
		{
			CCH_MARK_MUST_WRITE(tdbb, &window);

			if (dbb->dbb_next_transaction > next_transaction)
				Ods::writeNT(header, dbb->dbb_next_transaction);

			if (dbb->dbb_oldest_active > oldest_active)
				Ods::writeOAT(header, dbb->dbb_oldest_active);

			if (dbb->dbb_oldest_transaction > oldest_transaction)
				Ods::writeOIT(header, dbb->dbb_oldest_transaction);

			if (dbb->dbb_oldest_snapshot > oldest_snapshot)
				Ods::writeOST(header, dbb->dbb_oldest_snapshot);
		}

		CCH_RELEASE(tdbb, &window);
	}
}
#endif


void TRA_init(Jrd::Attachment* attachment)
{
/**************************************
 *
 *	T R A _ i n i t
 *
 **************************************
 *
 * Functional description
 *	"Start" the system transaction.
 *
 **************************************/
	Database* dbb = attachment->att_database;
	CHECK_DBB(dbb);

	MemoryPool* const pool = dbb->dbb_permanent;
	jrd_tra* const trans = FB_NEW_POOL(*pool) jrd_tra(pool, &dbb->dbb_memory_stats, NULL, NULL);
	trans->tra_attachment = attachment;
	attachment->setSysTransaction(trans);
	trans->tra_flags |= TRA_system | TRA_ignore_limbo;
}


void TRA_invalidate(thread_db* tdbb, ULONG mask)
{
/**************************************
 *
 *	T R A _ i n v a l i d a t e
 *
 **************************************
 *
 * Functional description
 *	Invalidate any active transactions that may have
 *	modified a page that couldn't be written.
 *
 **************************************/

	Database* const database = tdbb->getDatabase();

	EngineCheckout cout(tdbb, FB_FUNCTION, true);

	SyncLockGuard dbbSync(&database->dbb_sync, SYNC_SHARED, "TRA_invalidate");

	Jrd::Attachment* attachment = database->dbb_attachments;
	while (attachment)
	{
		Jrd::Attachment::SyncGuard attGuard(attachment, FB_FUNCTION);

		for (jrd_tra* transaction = attachment->att_transactions; transaction;
			 transaction = transaction->tra_next)
		{
			const ULONG transaction_mask = 1L << (transaction->tra_number & (BITS_PER_LONG - 1));
			if ((transaction_mask & mask) && (transaction->tra_flags & TRA_write))
				transaction->tra_flags |= TRA_invalidated;
		}

		attachment = attachment->att_next;
	}
}


void TRA_link_cursor(jrd_tra* transaction, DsqlCursor* cursor)
{
/**************************************
 *
 *	T R A _ l i n k _ c u r s o r
 *
 **************************************
 *
 * Functional description
 *	Add cursor to the list of open cursors belonging to this transaction.
 *
 **************************************/

	fb_assert(!transaction->tra_open_cursors.exist(cursor));
	transaction->tra_open_cursors.add(cursor);
}


void TRA_unlink_cursor(jrd_tra* transaction, DsqlCursor* cursor)
{
/**************************************
 *
 *	T R A _ u n l i n k _ c u r s o r
 *
 **************************************
 *
 * Functional description
 *	Remove cursor from the list of open cursors.
 *
 **************************************/

	FB_SIZE_T pos;
	if (transaction->tra_open_cursors.find(cursor, pos))
		transaction->tra_open_cursors.remove(pos);
}


void TRA_update_counters(thread_db* tdbb, Database* dbb)
{
/**************************************
*
*	T R A _ u p d a t e _ c o u n t e r s
*
**************************************
*
* Functional description
*  Update header page using cached values of transactions counters
*
**************************************/
	SET_TDBB(tdbb);

	if (!dbb || dbb->dbb_flags & DBB_read_only || dbb->dbb_flags & DBB_new ||
		dbb->dbb_oldest_transaction == 0)
	{
		return;
	}

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*)CCH_FETCH(tdbb, &window, LCK_write, pag_header);

	const TraNumber next_transaction = Ods::getNT(header);
	const TraNumber oldest_transaction = Ods::getOIT(header);
	const TraNumber oldest_active = Ods::getOAT(header);
	const TraNumber oldest_snapshot = Ods::getOST(header);

	fb_assert(dbb->dbb_next_transaction <= next_transaction);

	if (dbb->dbb_oldest_active > oldest_active ||
		dbb->dbb_oldest_transaction > oldest_transaction ||
		dbb->dbb_oldest_snapshot > oldest_snapshot ||
		dbb->dbb_next_transaction > next_transaction)
	{
		CCH_MARK_MUST_WRITE(tdbb, &window);

		if (dbb->dbb_oldest_active > oldest_active)
			Ods::writeOAT(header, dbb->dbb_oldest_active);

		if (dbb->dbb_oldest_transaction > oldest_transaction)
			Ods::writeOIT(header, dbb->dbb_oldest_transaction);

		if (dbb->dbb_oldest_snapshot > oldest_snapshot)
			Ods::writeOST(header, dbb->dbb_oldest_snapshot);

		if (dbb->dbb_next_transaction > next_transaction)
			Ods::writeNT(header, dbb->dbb_next_transaction);
	}

	CCH_RELEASE(tdbb, &window);
}


void TRA_post_resources(thread_db* tdbb, jrd_tra* transaction, ResourceList& resources)
{
/**************************************
 *
 *	T R A _ p o s t _ r e s o u r c e s
 *
 **************************************
 *
 * Functional description
 *	Post interest in relation/procedure/collation existence to transaction.
 *	This guarantees that the relation/procedure/collation won't be dropped
 *	out from under the transaction.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, transaction->tra_pool);

	for (Resource* rsc = resources.begin(); rsc < resources.end(); rsc++)
	{
		if (rsc->rsc_type == Resource::rsc_relation ||
			rsc->rsc_type == Resource::rsc_procedure ||
			rsc->rsc_type == Resource::rsc_function ||
			rsc->rsc_type == Resource::rsc_collation)
		{
			FB_SIZE_T i;
			if (!transaction->tra_resources.find(*rsc, i))
			{
				transaction->tra_resources.insert(i, *rsc);
				switch (rsc->rsc_type)
				{
				case Resource::rsc_relation:
					MET_post_existence(tdbb, rsc->rsc_rel);
					if (rsc->rsc_rel->rel_file) {
						EXT_tra_attach(rsc->rsc_rel->rel_file, transaction);
					}
					break;
				case Resource::rsc_procedure:
				case Resource::rsc_function:
					rsc->rsc_routine->addRef();
#ifdef DEBUG_PROCS
					{
						char buffer[256];
						sprintf(buffer,
								"Called from TRA_post_resources():\n\t Incrementing use count of %s\n",
								rsc->rsc_routine->prc_name->c_str());
						JRD_print_procedure_info(tdbb, buffer);
					}
#endif
					break;
				case Resource::rsc_collation:
					rsc->rsc_coll->incUseCount(tdbb);
					break;
				default:   // shut up compiler warning
					break;
				}
			}
		}
	}
}


bool TRA_is_active(thread_db* tdbb, TraNumber number)
{
/**************************************
 *
 *	T R A _ i s _ a c t i v e
 *
 **************************************
 *
 * Functional description
 *	Returns whether a given transaction
 *  owned by some other guy active or not.
 *
 * If function return false - you can trust this value and be sure that transaction
 * is not active. However if function returns true - it might be inactive already,
 * but two engine threads are checking for its status at once. Callers need to
 * behave correctly when facing this behavior.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	Lock temp_lock(tdbb, sizeof(TraNumber), LCK_tra);
	temp_lock.setKey(number);

	// If we can't get a lock on the transaction, it must be active

	if (!LCK_lock(tdbb, &temp_lock, LCK_read, LCK_NO_WAIT))
	{
		fb_utils::init_status(tdbb->tdbb_status_vector);
		return true;
	}

	LCK_release(tdbb, &temp_lock);
	return false;
}


void TRA_prepare(thread_db* tdbb, jrd_tra* transaction, USHORT length, const UCHAR* msg)
{
/**************************************
 *
 *	T R A _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Put a transaction into limbo.
 *
 **************************************/

	SET_TDBB(tdbb);

	if (transaction->tra_flags & TRA_prepared)
		return;

	if (transaction->tra_flags & TRA_invalidated)
		ERR_post(Arg::Gds(isc_trans_invalid));

	/* If there's a transaction description message, log it to RDB$TRANSACTION
	We should only log a message to RDB$TRANSACTION if there is a message
	to log (if the length = 0, we won't log the transaction in RDB$TRANSACTION)
	These messages are used to recover transactions in limbo.  The message indicates
	the action that is to be performed (hence, if nothing is getting logged, don't
	bother).
	*/

	/* Make sure that if msg is NULL there is no length.  The two
	should go hand in hand
	          msg == NULL || *msg == NULL
	*/
	fb_assert(!(!msg && length) || (msg && (!*msg && length)));

	if (msg && length)
	{
		MET_prepare(tdbb, transaction, length, msg);
		transaction->tra_flags |= TRA_prepare2;
	}

	// Prepare associated transaction in security DB

	SecDbContext* secContext = transaction->getSecDbContext();
	if (secContext && secContext->tra)
	{
		LocalStatus ls;
		CheckStatusWrapper st(&ls);
		secContext->tra->prepare(&st, length, msg);
		if (st.getState() & IStatus::STATE_ERRORS)
			status_exception::raise(&st);
	}

	// Perform any meta data work deferred

	DFW_perform_work(tdbb, transaction);

	// Flush pages if transaction logically modified data
	jrd_tra* sysTran = tdbb->getAttachment()->getSysTransaction();

	if (transaction->tra_flags & TRA_write)
		transaction_flush(tdbb, FLUSH_TRAN, transaction->tra_number);
	else if ((transaction->tra_flags & TRA_prepare2) || (sysTran->tra_flags & TRA_write))
	{
		// If the transaction only read data but is a member of a
		// multi-database transaction with a transaction description
		// message then flush RDB$TRANSACTIONS.

		transaction_flush(tdbb, FLUSH_SYSTEM, 0);
	}

	// Set the state on the inventory page to be limbo

	transaction->tra_flags |= TRA_prepared;
	TRA_set_state(tdbb, transaction, transaction->tra_number, tra_limbo);
}


jrd_tra* TRA_reconnect(thread_db* tdbb, const UCHAR* id, USHORT length)
{
/**************************************
 *
 *	T R A _ r e c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Reconnect to a transaction in limbo.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	// Cannot work on limbo transactions for ReadOnly database
	if (dbb->readOnly())
		ERR_post(Arg::Gds(isc_read_only_database));

	const TraNumber number = isc_portable_integer(id, length);
	if (number > dbb->dbb_next_transaction)
		PAG_header(tdbb, true);

	const int state = (number > dbb->dbb_next_transaction) ?
		255 : limbo_transaction(tdbb, number);

	if (state != tra_limbo)
	{
		USHORT message;

		switch (state)
		{
		case tra_active:
			message = 262;		// ACTIVE
			break;
		case tra_dead:
			message = 264;		// ROLLED BACK
			break;
		case tra_committed:
			message = 263;		// COMMITTED
			break;
		default:
			message = 265;		// ILL DEFINED
			break;
		}

		TEXT text[128];
		USHORT flags = 0;
		gds__msg_lookup(NULL, JRD_BUGCHK, message, sizeof(text), text, &flags);

		// Cannot use Arg::Num here because transaction number is 64-bit unsigned integer
		ERR_post(Arg::Gds(isc_no_recon) <<
				 Arg::Gds(isc_tra_state) << Arg::Int64(number) << Arg::Str(text));
	}

	MemoryPool* const pool = attachment->createPool();
	Jrd::ContextPoolHolder context(tdbb, pool);
	jrd_tra* const trans = jrd_tra::create(pool, attachment, NULL);
	trans->tra_number = number;
	trans->tra_flags |= TRA_prepared | TRA_reconnected | TRA_write;

	trans->linkToAttachment(attachment);

	return trans;
}


void TRA_release_transaction(thread_db* tdbb, jrd_tra* transaction, Jrd::TraceTransactionEnd* trace)
{
/**************************************
 *
 *	T R A _ r e l e a s e _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Cleanup a transaction.  This is called by both COMMIT and
 *	ROLLBACK as well as code in JRD to get rid of remote
 *	transactions.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();
	Database* const dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	if (!transaction->tra_outer)
	{
		BlobUtilMap::Accessor blobUtilAccessor(&transaction->tra_blob_util_map);
		for (bool found = blobUtilAccessor.getFirst(); found; found = blobUtilAccessor.getNext())
		{
			auto blb = blobUtilAccessor.current()->second;

			// Let temporary blobs be cancelled in the block below.
			if (!(blb->blb_flags & BLB_temporary))
				blb->BLB_close(tdbb);

			blobUtilAccessor.fastRemove();
		}

		if (transaction->tra_blobs->getFirst())
		{
			while (true)
			{
				BlobIndex *current = &transaction->tra_blobs->current();
				if (current->bli_materialized)
				{
					if (!transaction->tra_blobs->getNext())
						break;
				}
				else
				{
					ULONG temp_id = current->bli_temp_id;
					current->bli_blob_object->BLB_cancel(tdbb);
					if (!transaction->tra_blobs->locate(Firebird::locGreat, temp_id))
						break;
				}
			}
		}

		while (transaction->tra_arrays)
			blb::release_array(transaction->tra_arrays);
	}

	if (transaction->tra_pool)
	{
		// Iterate the doubly linked list of requests for transaction and null out the transaction references
		while (transaction->tra_requests)
			TRA_detach_request(transaction->tra_requests);
	}

	// Release interest in relation/procedure existence for transaction

	for (Resource* rsc = transaction->tra_resources.begin();
		rsc < transaction->tra_resources.end(); rsc++)
	{
		switch (rsc->rsc_type)
		{
		case Resource::rsc_relation:
			MET_release_existence(tdbb, rsc->rsc_rel);
			if (rsc->rsc_rel->rel_file) {
				EXT_tra_detach(rsc->rsc_rel->rel_file, transaction);
			}
			break;
		case Resource::rsc_procedure:
		case Resource::rsc_function:
			rsc->rsc_routine->release(tdbb);
			break;
		case Resource::rsc_collation:
			rsc->rsc_coll->decUseCount(tdbb);
			break;
		default:
			fb_assert(false);
		}
	}

	release_temp_tables(tdbb, transaction);

	// Release the locks associated with the transaction

	if (transaction->tra_alter_db_lock)
		LCK_release(tdbb, transaction->tra_alter_db_lock);

	vec<Lock*>* vector = transaction->tra_relation_locks;
	if (vector)
	{
		vec<Lock*>::iterator lock = vector->begin();
		for (ULONG i = 0; i < vector->count(); ++i, ++lock)
		{
			if (*lock)
				LCK_release(tdbb, *lock);
		}
	}

	++transaction->tra_use_count;

	if (transaction->tra_lock)
		LCK_release(tdbb, transaction->tra_lock);

	--transaction->tra_use_count;

	// release the sparse bit map used for commit retain transaction

	delete transaction->tra_commit_sub_trans;

	if (trace)
		trace->finish(ITracePlugin::RESULT_SUCCESS);

	// Unlink the transaction from the attachment block

	transaction->unlinkFromAttachment();

	// Destroy the replicated transaction reference

	if (transaction->tra_replicator)
		transaction->tra_replicator->dispose();

	// Release transaction's under-modification-rpb list

	delete transaction->tra_rpblist;

	// Release the database snapshot, if any

	delete transaction->tra_mon_snapshot;

	// Close all open DSQL cursors

	while (transaction->tra_open_cursors.hasData())
		DsqlCursor::close(tdbb, transaction->tra_open_cursors.pop());

	if (!(transaction->tra_flags & TRA_read_committed) &&
		!(transaction->tra_flags & TRA_reconnected))
	{
		dbb->dbb_tip_cache->endSnapshot(tdbb, transaction->tra_snapshot_handle,
			transaction->tra_attachment->att_attachment_id);
	}

	// Release the transaction and its pool

	tdbb->setTransaction(NULL);
	JTransaction* jTra = transaction->getInterface(true);	// ASF: maybe it's better to pass false?
	if (jTra)
	{
		jTra->setHandle(NULL);
	}
	jrd_tra::destroy(attachment, transaction);
}


void TRA_rollback(thread_db* tdbb, jrd_tra* transaction, const bool retaining_flag,
				  const bool force_flag)
{
/**************************************
 *
 *	T R A _ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Rollback a transaction.
 *
 **************************************/
	SET_TDBB(tdbb);

	TraceTransactionEnd trace(transaction, false, retaining_flag);

	EDS::Transaction::jrdTransactionEnd(tdbb, transaction, false, retaining_flag, false /*force_flag ?*/);

	Jrd::ContextPoolHolder context(tdbb, transaction->tra_pool);

	if (transaction->tra_flags & (TRA_prepare2 | TRA_reconnected))
		MET_update_transaction(tdbb, transaction, false);

	// If force flag is true, get rid of all savepoints to mark the transaction as dead
	if (force_flag || (transaction->tra_flags & TRA_invalidated))
	{
		// Free all savepoint data
		// Undo data space and BLOBs will be released in destructor
		Savepoint::destroy(transaction->tra_save_point);
		fb_assert(!transaction->tra_save_point);
	}
	else if (!retaining_flag)
	{
		// Remove undo data for GTT ON COMMIT DELETE ROWS as their data will be released
		// at transaction end anyway and we don't need to waste time backing it out

		for (Savepoint::Iterator iter(transaction->tra_save_point); *iter; ++iter)
			(*iter)->cleanupTempData();
	}

	int state = tra_dead;

	if (transaction->tra_save_point)
	{
		// Make sure that any error during savepoint undo is handled by marking
		// the transaction as dead.

		try
		{
			// Release all user savepoints except transaction one
			// It will clean up blob ids and temporary space anyway but faster than rollback
			// because record data won't be updated with intermediate versions
			while (transaction->tra_save_point && !transaction->tra_save_point->isRoot())
				transaction->rollforwardSavepoint(tdbb);

			if (transaction->tra_save_point) // we still can use undo log for rollback, it wasn't reset because of no_auto_undo flag or size
			{
				// In an attempt to avoid deadlocks, clear the precedence by writing
				// all dirty buffers for this transaction.

				if (transaction->tra_flags & TRA_write)
				{
					transaction_flush(tdbb, FLUSH_TRAN, transaction->tra_number);
					transaction->rollbackSavepoint(tdbb);
					transaction_flush(tdbb, FLUSH_TRAN, transaction->tra_number);
				}
				else
					transaction->rollbackSavepoint(tdbb);

				// All changes are undone, so we may mark the transaction
				// as committed
				state = tra_committed;
			}
		}
		catch (const Firebird::Exception&)
		{
			// Prevent a bugcheck in TRA_set_state to cause a loop
			// Clear the error because the rollback will succeed.
			fb_utils::init_status(tdbb->tdbb_status_vector);
		}
	}
	else if (!(transaction->tra_flags & TRA_write))
	{
		// There were no changes within the transaction, so we may mark it
		// as committed
		state = tra_committed;
	}

	jrd_tra* const sysTran = tdbb->getAttachment()->getSysTransaction();
	if (sysTran->tra_flags & TRA_write)
		transaction_flush(tdbb, FLUSH_SYSTEM, 0);

	// If this is a rollback retain abort this transaction and start a new one.

	if (retaining_flag)
	{
		retain_context(tdbb, transaction, false, state);
		trace.finish(ITracePlugin::RESULT_SUCCESS);
		return;
	}

	TRA_set_state(tdbb, transaction, transaction->tra_number, state);
	REPL_trans_rollback(tdbb, transaction);

	TRA_release_transaction(tdbb, transaction, &trace);
}


void TRA_set_state(thread_db* tdbb, jrd_tra* transaction, TraNumber number, int state)
{
/**************************************
 *
 *	T R A _ s e t _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Set the state of a transaction in the inventory page.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// If it is a ReadOnly DB, set the new state in the TIP cache and return
	if (dbb->readOnly() && dbb->dbb_tip_cache)
	{
		TPC_set_state(tdbb, number, state);
		return;
	}

	// If we're terminating ourselves and we've been precommitted then just return.

	if (transaction && transaction->tra_number == number &&
		(transaction->tra_flags & TRA_precommitted))
	{
		return;
	}

	const ULONG trans_per_tip = dbb->dbb_page_manager.transPerTIP;
	const ULONG sequence = number / trans_per_tip;
	const ULONG byte = TRANS_OFFSET(number % trans_per_tip);
	const USHORT shift = TRANS_SHIFT(number);

	WIN window(DB_PAGE_SPACE, -1);
	tx_inv_page* tip = fetch_inventory_page(tdbb, &window, sequence, LCK_write);

	UCHAR* address = tip->tip_transactions + byte;
	const int old_state = ((*address) >> shift) & TRA_MASK;

#ifdef SUPERSERVER_V2
	CCH_MARK(tdbb, &window);
	const ULONG generation = tip->tip_header.pag_generation;
#else
	if (!(dbb->dbb_flags & DBB_shared) || !transaction  ||
		(transaction->tra_flags & TRA_write) ||
		old_state != tra_active || state != tra_committed)
	{
		CCH_MARK_MUST_WRITE(tdbb, &window);
	}
	else
		CCH_MARK(tdbb, &window);
#endif

	// set the state on the TIP page

	*address &= ~(TRA_MASK << shift);
	*address |= state << shift;

	// set the new state in the TIP cache as well

	if (dbb->dbb_tip_cache)
		TPC_set_state(tdbb, number, state);

	CCH_RELEASE(tdbb, &window);

#ifdef SUPERSERVER_V2
	// Let the TIP be lazily updated for read-only queries.
	// To amortize write of TIP page for update transactions,
	// exit engine to allow other transactions to update the TIP
	// and use page generation to determine if page was written.

	if (transaction && !(transaction->tra_flags & TRA_write))
		return;

	{ //scope
		Database::Checkout dcoHolder(dbb, FB_FUNCTION);
		Thread::yield();
	}
	tip = reinterpret_cast<tx_inv_page*>(CCH_FETCH(tdbb, &window, LCK_write, pag_transactions));
	if (generation == tip->tip_header.pag_generation)
		CCH_MARK_MUST_WRITE(tdbb, &window);
	CCH_RELEASE(tdbb, &window);
#endif

}


int TRA_snapshot_state(thread_db* tdbb, const jrd_tra* trans, TraNumber number, CommitNumber* snapshot)
{
/**************************************
 *
 *	T R A _ s n a p s h o t _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Get the state of a numbered transaction when a
 *	transaction started.
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	Attachment* att = tdbb->getAttachment();

	// Inhibit intermediate GC by default

	if (snapshot)
		*snapshot = CN_ACTIVE;

	if (number == trans->tra_number)
		return tra_us;

	// If the transaction is older than the oldest
	// interesting transaction, it must be committed.

	if (number < trans->tra_oldest)
	{
		if (snapshot)
			*snapshot = att->att_active_snapshots.getSnapshotForVersion(CN_PREHISTORIC);
		return tra_committed;
	}

	// If the transaction is the system transaction, it is considered committed.

	if (number == TRA_system_transaction)
	{
		if (snapshot)
			*snapshot = att->att_active_snapshots.getSnapshotForVersion(CN_PREHISTORIC);
		return tra_committed;
	}

	// Determine transaction state and commit number if any

	int state;
	CommitNumber stateCn = CN_PREHISTORIC;

	if (TipCache* tip_cache = dbb->dbb_tip_cache)
	{
		stateCn = tip_cache->snapshotState(tdbb, number);
		switch (stateCn)
		{
			case CN_ACTIVE:
				state = tra_active;
				break;
			case CN_LIMBO:
				state = tra_limbo;
				break;
			case CN_DEAD:
				state = tra_dead;
				break;
			default:
				state = tra_committed;
				if (snapshot)
					*snapshot = att->att_active_snapshots.getSnapshotForVersion(stateCn);
				break;
		}
	}
	else
		state = TRA_fetch_state(tdbb, number);

	// If the transaction is a committed sub-transction - do the easy lookup.

	if (trans->tra_commit_sub_trans && trans->tra_commit_sub_trans->test(number))
		return tra_committed;

	if (trans->tra_flags & TRA_read_committed)
	{
		if ((trans->tra_flags & TRA_read_consistency) && state == tra_committed)
		{
			// GC thread accesses data directly without any request
			if (jrd_req* current_request = tdbb->getRequest())
			{
				// Notes:
				// 1) There is no request snapshot when we build expression index
				// 2) Disable read committed snapshot after we encountered update conflict
				jrd_req* snapshot_request = current_request->req_snapshot.m_owner;
				if (snapshot_request && !(snapshot_request->req_flags & req_update_conflict))
				{
					if (stateCn > snapshot_request->req_snapshot.m_number)
						return tra_active;
				}
			}
		}
		return state;
	}

	// The system transaction can read data from active transactions.

	if (trans->tra_flags & TRA_system)
	{
		if (state == tra_active)
			return tra_committed;

		return state;
	}

	if (state == tra_committed && stateCn > trans->tra_snapshot_number)
		return tra_active;

	return state;
}


jrd_tra* TRA_start(thread_db* tdbb, ULONG flags, SSHORT lock_timeout, Jrd::jrd_tra* outer)
{
/**************************************
 *
 *	T R A _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a user transaction.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	if (dbb->dbb_ast_flags & DBB_shut_tran)
	{
		ERR_post(Arg::Gds(isc_shutinprog) << Arg::Str(attachment->att_filename));
	}

	// To handle the problems of relation locks, allocate a temporary
	// transaction block first, seize relation locks, then go ahead and
	// make up the real transaction block.
	MemoryPool* const pool = outer ? outer->getAutonomousPool() : attachment->createPool();
	Jrd::ContextPoolHolder context(tdbb, pool);
	jrd_tra* const transaction = jrd_tra::create(pool, attachment, outer);

	transaction->tra_flags = flags & TRA_OPTIONS_MASK;
	transaction->tra_lock_timeout = lock_timeout;

	try
	{
		transaction_start(tdbb, transaction);
	}
	catch (const Exception&)
	{
		jrd_tra::destroy(attachment, transaction);
		throw;
	}

	if (attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_TRANSACTION_START))
	{
		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);
		attachment->att_trace_manager->event_transaction_start(&conn,
			&tran, 0, NULL, ITracePlugin::RESULT_SUCCESS);
	}

	return transaction;
}


jrd_tra* TRA_start(thread_db* tdbb, int tpb_length, const UCHAR* tpb, Jrd::jrd_tra* outer)
{
/**************************************
 *
 *	T R A _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a user transaction.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	Jrd::Attachment* attachment = tdbb->getAttachment();

	if (dbb->dbb_ast_flags & DBB_shut_tran)
	{
		ERR_post(Arg::Gds(isc_shutinprog) << Arg::Str(attachment->att_filename));
	}

	// To handle the problems of relation locks, allocate a temporary
	// transaction block first, seize relation locks, then go ahead and
	// make up the real transaction block.
	MemoryPool* const pool = outer ? outer->getAutonomousPool() : attachment->createPool();
	Jrd::ContextPoolHolder context(tdbb, pool);
	jrd_tra* const transaction = jrd_tra::create(pool, attachment, outer);

	try
	{
		transaction_options(tdbb, transaction, tpb, tpb_length);
		transaction_start(tdbb, transaction);
	}
	catch (const Exception&)
	{
		jrd_tra::destroy(attachment, transaction);
		throw;
	}

	if (attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_TRANSACTION_START))
	{
		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);
		attachment->att_trace_manager->event_transaction_start(&conn,
			&tran, tpb_length, tpb, ITracePlugin::RESULT_SUCCESS);
	}

	return transaction;
}


int TRA_state(const UCHAR* bit_vector, TraNumber oldest, TraNumber number)
{
/**************************************
 *
 *	T R A _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Get the state of a transaction from a cached
 *	bit vector.
 *	NOTE: This code is reproduced elsewhere in
 *	this module for speed.  If changes are made
 *	to this code make them in the replicated code also.
 *
 **************************************/
	const TraNumber base = oldest & ~TRA_MASK;
	const ULONG byte = TRANS_OFFSET(number - base);
	const USHORT shift = TRANS_SHIFT(number);

	return (bit_vector[byte] >> shift) & TRA_MASK;
}


void TRA_sweep(thread_db* tdbb)
{
/**************************************
 *
 *	T R A _ s w e e p
 *
 **************************************
 *
 * Functional description
 *	Make a garbage collection pass thru database.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	if (!dbb->allowSweepRun(tdbb))
	{
		dbb->clearSweepFlags(tdbb);
		return;
	}

	fb_assert(dbb->dbb_flags & DBB_sweep_in_progress);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	jrd_tra* const tdbb_old_trans = tdbb->getTransaction();

	jrd_tra* transaction = NULL;

	// Clean up the temporary locks we've gotten in case anything goes wrong

	try {

		// Identify ourselves as a sweeper thread. This accomplishes two goals:
		// 1) Sweep transaction is started "precommitted" and
		// 2) Execution is throttled in JRD_reschedule() by
		// yielding the processor when our quantum expires.

		ThreadSweepGuard sweepGuard(tdbb);

		TraceSweepEvent traceSweep(tdbb);

		// Start a transaction, if necessary, to perform the sweep.
		// Save the transaction's oldest snapshot as it is refreshed
		// during the course of the database sweep. Since it is used
		// below to advance the OIT we must save it before it changes.

		transaction = TRA_start(tdbb, sizeof(sweep_tpb), sweep_tpb);

		TraNumber transaction_oldest_active = transaction->tra_oldest_active;
		tdbb->setTransaction(transaction);

		// The garbage collector runs asynchronously with respect to
		// our database sweep. This isn't good enough since we must
		// be absolutely certain that all dead transactions have been
		// swept from the database before advancing the OIT. Turn off
		// the "notify garbage collector" flag for the attachment and
		// synchronously perform the garbage collection ourselves.

		attachment->att_flags &= ~ATT_notify_gc;

		if (VIO_sweep(tdbb, transaction, &traceSweep))
		{
			// At this point, we know that no record versions belonging to dead
			// transactions remain anymore. However, there may still be limbo
			// transactions, so we need to find the oldest one between tra_oldest and tra_top.
			// As our transaction is read-committed (see sweep_tpb), we have to scan
			// the global TIP cache.

			int oldest_state = 0;
			const TraNumber oldest_limbo =
				TPC_find_states(tdbb, transaction->tra_oldest, transaction->tra_top - 1,
								1 << tra_limbo, oldest_state);

			const TraNumber active = oldest_limbo ? oldest_limbo : transaction->tra_top;

			// Flush page buffers to insure that no dangling records from
			// dead transactions are left on-disk. This must be done before
			// the OIT is advanced and the header page is written to disk.
			// If the header page was written before flushing the page buffers
			// and there was a server crash, the dead records would appear
			// committed since their TID would now be less than the OIT recorded
			// in the database.

			CCH_flush(tdbb, FLUSH_SWEEP, 0);

			WIN window(HEADER_PAGE_NUMBER);
			header_page* const header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

			if (Ods::getOIT(header) < --transaction_oldest_active)
			{
				CCH_MARK_MUST_WRITE(tdbb, &window);
				Ods::writeOIT(header, MIN(active, transaction_oldest_active));
			}

			traceSweep.update(header);

			CCH_RELEASE(tdbb, &window);

			traceSweep.finish();
		}

		TRA_commit(tdbb, transaction, false);

		tdbb->setTransaction(tdbb_old_trans);
		dbb->clearSweepFlags(tdbb);
	}
	catch (const Firebird::Exception& ex)
	{
		PathName message = "Error during sweep of ";
		message += dbb->dbb_database_name;
		message += ':';
		iscLogException(message.c_str(), ex);

		ex.stuffException(tdbb->tdbb_status_vector);

		if (transaction)
		{
			try
			{
				TRA_commit(tdbb, transaction, false);
			}
			catch (const Firebird::Exception& ex2)
			{
				ex2.stuffException(tdbb->tdbb_status_vector);
			}
		}

		tdbb->setTransaction(tdbb_old_trans);
		dbb->clearSweepFlags(tdbb);

		throw;
	}
}


int TRA_wait(thread_db* tdbb, jrd_tra* trans, TraNumber number, jrd_tra::wait_t wait)
{
/**************************************
 *
 *	T R A _ w a i t
 *
 **************************************
 *
 * Functional description
 *	Wait for a given transaction to drop into a stable state (i.e. non-active)
 *	state.  To do this, we first wait on the transaction number.  When we
 *	are able to get the lock, the transaction is not longer bona fide
 *	active.  Next, we determine the state of the transaction from the
 *	transaction inventory page.  If either committed, dead, or limbo,
 *	we return the state.  If the transaction is still marked active,
 *	however, declare the transaction dead, and mark the transaction
 *	inventory page accordingly.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// Create, wait on, and release lock on target transaction.  If
	// we can't get the lock due to deadlock

	if (wait != jrd_tra::tra_no_wait)
	{
		Lock temp_lock(tdbb, sizeof(TraNumber), LCK_tra);
		temp_lock.setKey(number);

		const SSHORT timeout = (wait == jrd_tra::tra_wait) ? trans->getLockWait() : 0;

		if (!LCK_lock(tdbb, &temp_lock, LCK_read, timeout))
		{
			fb_utils::init_status(tdbb->tdbb_status_vector);
			return tra_active;
		}

		LCK_release(tdbb, &temp_lock);
	}

	int state = TRA_get_state(tdbb, number);

	if (wait != jrd_tra::tra_no_wait && state == tra_committed)
		return state;

	if (state == tra_precommitted)
		return state;

	// If the recorded state of the transaction is active, we know better.  If
	// it were active, he'd be alive now.  Mark him dead.

	if (state == tra_active)
	{
		state = tra_dead;
		TRA_set_state(tdbb, 0, number, tra_dead);
		REPL_trans_cleanup(tdbb, number);
	}

	// If the transaction disappeared into limbo, died, for constructively
	// died, tweak the transaction state snapshot to reflect the new state.
	// This is guaranteed safe.

	TPC_set_state(tdbb, number, state);

	return state;
}


#ifdef SUPERSERVER_V2
static TraNumber bump_transaction_id(thread_db* tdbb, WIN* window)
{
/**************************************
 *
 *	b u m p _ t r a n s a c t i o n _ i d
 *
 **************************************
 *
 * Functional description
 *	Fetch header and bump next transaction id.  If necessary,
 *	extend TIP.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	if (dbb->dbb_next_transaction >= MAX_TRA_NUMBER - 1)
	{
		CCH_RELEASE(tdbb, window);
		ERR_post(Arg::Gds(isc_imp_exc) <<
				 Arg::Gds(isc_tra_num_exc));
	}
	const TraNumber number = ++dbb->dbb_next_transaction;
	dbb->assignLastestTransactionId(number);

	// No need to write TID onto the TIP page, for a RO DB
	if (dbb->readOnly())
		return number;

	// If this is the first transaction on a TIP, allocate the TIP now.
	// Note, first TIP page is created with the database itself,
	// see JProvider::createDatabase.

	const bool new_tip = ((number % dbb->dbb_page_manager.transPerTIP) == 0);

	if (new_tip)
		TRA_extend_tip(tdbb, (number / dbb->dbb_page_manager.transPerTIP)); //, window);

	return number;
}
#else


static header_page* bump_transaction_id(thread_db* tdbb, WIN* window, bool dontWrite)
{
/**************************************
 *
 *	b u m p _ t r a n s a c t i o n _ i d
 *
 **************************************
 *
 * Functional description
 *	Fetch header and bump next transaction id.  If necessary,
 *	extend TIP.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	window->win_page = HEADER_PAGE_NUMBER;
	header_page* header = (header_page*) CCH_FETCH(tdbb, window, LCK_write, pag_header);

	const TraNumber next_transaction = Ods::getNT(header);
	const TraNumber oldest_active = Ods::getOAT(header);
	const TraNumber oldest_transaction = Ods::getOIT(header);
	const TraNumber oldest_snapshot = Ods::getOST(header);

	// Before incrementing the next transaction Id, make sure the current one is valid
	if (next_transaction)
	{
		if (oldest_active > next_transaction)
			BUGCHECK(266);		//next transaction older than oldest active

		if (oldest_transaction > next_transaction)
			BUGCHECK(267);		// next transaction older than oldest transaction
	}

	if (next_transaction >= MAX_TRA_NUMBER - 1)
	{
		CCH_RELEASE(tdbb, window);
		ERR_post(Arg::Gds(isc_imp_exc) <<
				 Arg::Gds(isc_tra_num_exc));
	}

	const TraNumber number = next_transaction + 1;

	// If this is the first transaction on a TIP, allocate the TIP now.
	// Note, first TIP page is created with the database itself,
	// see JProvider::createDatabase.

	const bool new_tip = ((number % dbb->dbb_page_manager.transPerTIP) == 0);

	if (new_tip)
		TRA_extend_tip(tdbb, (number / dbb->dbb_page_manager.transPerTIP)); //, window);

	// Extend, if necessary, has apparently succeeded.  Next, update header page

	if (dontWrite && !new_tip)
		CCH_MARK(tdbb, window);
	else
		CCH_MARK_MUST_WRITE(tdbb, window);

	//dbb->assignLatestTransactionId(number);
	dbb->dbb_next_transaction = number;

	Ods::writeNT(header, number);

	if (dbb->dbb_oldest_active > oldest_active)
		Ods::writeOAT(header, dbb->dbb_oldest_active);

	if (dbb->dbb_oldest_transaction > oldest_transaction)
		Ods::writeOIT(header, dbb->dbb_oldest_transaction);

	if (dbb->dbb_oldest_snapshot > oldest_snapshot)
		Ods::writeOST(header, dbb->dbb_oldest_snapshot);

	return header;
}
#endif


static void expand_view_lock(thread_db* tdbb, jrd_tra* transaction, jrd_rel* relation,
	UCHAR lock_type, const char* option_name, RelationLockTypeMap& lockmap, const int level)
{
/**************************************
 *
 *	e x p a n d _ v i e w _ l o c k
 *
 **************************************
 *
 * Functional description
 *	A view in a RESERVING will lead to all tables in the
 *	view being locked.
 *  Some checks only apply when the user reserved directly the table or view.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (level == 30)
	{
		ERR_post(Arg::Gds(isc_bad_tpb_content) <<
				 Arg::Gds(isc_tpb_reserv_max_recursion) << Arg::Num(30));
	}

	const char* const relation_name = relation->rel_name.c_str();

	// LCK_none < LCK_SR < LCK_PR < LCK_SW < LCK_EX
	UCHAR oldlock;
	const bool found = lockmap.get(relation->rel_id, oldlock);

	if (found && oldlock > lock_type)
	{
		const char* newname = get_lockname_v3(lock_type);
		const char* oldname = get_lockname_v3(oldlock);

		if (level)
		{
			lock_type = oldlock; // Preserve the old, more powerful lock.
			ERR_post_warning(Arg::Warning(isc_tpb_reserv_stronger_wng) << Arg::Str(relation_name) <<
																		  Arg::Str(oldname) <<
																		  Arg::Str(newname));
		}
		else
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_stronger) << Arg::Str(relation_name) <<
														  Arg::Str(oldname) <<
														  Arg::Str(newname));
		}
	}

	if (level == 0)
	{
		fb_assert(!relation->rel_view_rse && !relation->rel_view_contexts.getCount());
		// Reject explicit attempts to take locks on virtual tables.
		if (relation->isVirtual())
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_virtualtbl) << Arg::Str(relation_name));
		}

		// Reject explicit attempts to take locks on system tables.
		if (relation->isSystem())
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
		    		 Arg::Gds(isc_tpb_reserv_systbl) << Arg::Str(relation_name));
		}

		if (relation->isTemporary() && (lock_type == LCK_PR || lock_type == LCK_EX))
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_temptbl) << Arg::Str(get_lockname_v3(LCK_PR)) <<
					 									 Arg::Str(get_lockname_v3(LCK_EX)) <<
														 Arg::Str(relation_name));
		}
	}
	else
	{
		fb_assert(relation->rel_view_rse && relation->rel_view_contexts.getCount());
		// Ignore implicit attempts to take locks on special tables through views.
		if (relation->isVirtual() || relation->isSystem())
			return;

		// We can't propagate a view's LCK_PR or LCK_EX to a temporary table.
		if (relation->isTemporary())
		{
			switch (lock_type)
			{
			case LCK_PR:
				lock_type = LCK_SR;
				break;
			case LCK_EX:
				lock_type = LCK_SW;
				break;
			}
		}
	}

	// set up the lock on the relation/view
	Lock* lock = RLCK_transaction_relation_lock(tdbb, transaction, relation);
	lock->lck_logical = lock_type;

	if (!found)
		*lockmap.put(relation->rel_id) = lock_type;

	const ViewContexts& ctx = relation->rel_view_contexts;

	for (FB_SIZE_T i = 0; i < ctx.getCount(); ++i)
	{
		if (ctx[i]->vcx_type == VCT_PROCEDURE)
			continue;

		jrd_rel* base_rel = MET_lookup_relation(tdbb, ctx[i]->vcx_relation_name);
		if (!base_rel)
		{
			// should be a BUGCHECK
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_baserelnotfound) << Arg::Str(ctx[i]->vcx_relation_name) <<
																 Arg::Str(relation_name) <<
																 Arg::Str(option_name));
		}

		// force a scan to read view information
		MET_scan_relation(tdbb, base_rel);

		expand_view_lock(tdbb, transaction, base_rel, lock_type, option_name, lockmap, level + 1);
	}
}


static tx_inv_page* fetch_inventory_page(thread_db* tdbb,
										 WIN* window,
										 ULONG sequence,
										 USHORT lock_level)
{
/**************************************
 *
 *	f e t c h _ i n v e n t o r y _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Fetch a transaction inventory page.
 *	Use the opportunity to cache the info
 *	in the TIP cache.
 *
 **************************************/
	SET_TDBB(tdbb);

	window->win_page = inventory_page(tdbb, sequence);
	tx_inv_page* tip = (tx_inv_page*) CCH_FETCH(tdbb, window, lock_level, pag_transactions);

	return tip;
}


static const char* get_lockname_v3(const UCHAR lock)
{
/**************************************
 *
 * g e t _ l o c k n a m e _ v 3
 *
 **************************************
 *
 * Functional description
 *	Get the lock mnemonic, given its binary value.
 *	This is for TPB versions 1 & 3.
 *
 **************************************/
	const char* typestr = "unknown";
	switch (lock)
	{
	case LCK_none:
	case LCK_SR:
		typestr = "isc_tpb_lock_read, isc_tpb_shared";
		break;
	case LCK_PR:
		typestr = "isc_tpb_lock_read, isc_tpb_protected/isc_tpb_exclusive";
		break;
	case LCK_SW:
		typestr = "isc_tpb_lock_write, isc_tpb_shared";
		break;
	case LCK_EX:
		typestr = "isc_tpb_lock_write, isc_tpb_protected/isc_tpb_exclusive";
		break;
	}
	return typestr;
}


static ULONG inventory_page(thread_db* tdbb, ULONG sequence)
{
/**************************************
 *
 *	i n v e n t o r y _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Get the physical page number of the n-th transaction inventory
 *	page. If not found, try to reconstruct using sibling pointer
 *	from last known TIP page.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	WIN window(DB_PAGE_SPACE, -1);
	vcl* vector = dbb->dbb_t_pages;
	while (!vector || sequence >= vector->count())
	{
		DPM_scan_pages(tdbb);

		if ((vector = dbb->dbb_t_pages) && sequence < vector->count())
			break;

		if (!vector)
			BUGCHECK(165);		// msg 165 cannot find tip page

		window.win_page = (*vector)[vector->count() - 1];
		tx_inv_page* tip = (tx_inv_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_transactions);
		const ULONG next = tip->tip_next;
		CCH_RELEASE(tdbb, &window);
		if (!(window.win_page = next))
			BUGCHECK(165);		// msg 165 cannot find tip page

		// Type check it
		tip = (tx_inv_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_transactions);
		CCH_RELEASE(tdbb, &window);
		DPM_pages(tdbb, 0, pag_transactions, vector->count(), window.win_page.getPageNum());
	}

	return (*vector)[sequence];
}


static int limbo_transaction(thread_db* tdbb, TraNumber id)
{
/**************************************
 *
 *	l i m b o _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *
 *	limbo_state is called when reconnecting
 *	to an existing transaction to assure that
 *	the transaction is actually in limbo.
 *	It returns the transaction state.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	const ULONG trans_per_tip = dbb->dbb_page_manager.transPerTIP;

	const ULONG page = id / trans_per_tip;
	const ULONG number = id % trans_per_tip;

	WIN window(DB_PAGE_SPACE, -1);
	const tx_inv_page* tip = fetch_inventory_page(tdbb, &window, page, LCK_write);

	const ULONG trans_offset = TRANS_OFFSET(number);
	const UCHAR* byte = tip->tip_transactions + trans_offset;
	const USHORT shift = TRANS_SHIFT(number);
	const int state = (*byte >> shift) & TRA_MASK;
	CCH_RELEASE(tdbb, &window);

	return state;
}


void jrd_tra::unlinkFromAttachment()
{
	for (jrd_tra** ptr = &tra_attachment->att_transactions; *ptr; ptr = &(*ptr)->tra_next)
	{
		if (*ptr == this)
		{
			*ptr = tra_next;
			return;
		}
	}

	tra_abort("transaction to unlink is missing in the attachment");
}


void jrd_tra::linkToAttachment(Attachment* attachment)
{
/**************************************
 *
 *	l i n k _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Link transaction block into database attachment.
 *
 **************************************/
	tra_next = attachment->att_transactions;
	attachment->att_transactions = this;
}


void jrd_tra::tra_abort(const char* reason)
{
	string buff;
	buff.printf("Failure working with transactions list: %s", reason);
	Syslog::Record(Syslog::Error, buff.c_str());
	gds__log(buff.c_str());
#ifdef DEV_BUILD
	abort();
#endif
}


static void release_temp_tables(thread_db* tdbb, jrd_tra* transaction)
{
/**************************************
 *
 *	r e l e a s e _ t e m p _ t a b l e s
 *
 **************************************
 *
 * Functional description
 *	Release data of temporary tables with transaction lifetime
 *
 **************************************/
	Attachment* att = tdbb->getAttachment();
	vec<jrd_rel*>& rels = *att->att_relations;

	for (FB_SIZE_T i = 0; i < rels.count(); i++)
	{
		jrd_rel* relation = rels[i];

		if (relation && (relation->rel_flags & REL_temp_tran))
			relation->delPages(tdbb, transaction->tra_number);
	}
}


static void retain_temp_tables(thread_db* tdbb, jrd_tra* transaction, TraNumber new_number)
{
/**************************************
 *
 *	r e t a i n _ t e m p _ t a b l e s
 *
 **************************************
 *
 * Functional description
 *	Reassign instance of temporary tables with transaction lifetime to the new
 *  transaction number (see retain_context).
 *
 **************************************/
	Attachment* att = tdbb->getAttachment();
	vec<jrd_rel*>& rels = *att->att_relations;

	for (FB_SIZE_T i = 0; i < rels.count(); i++)
	{
		jrd_rel* relation = rels[i];

		if (relation && (relation->rel_flags & REL_temp_tran))
			relation->retainPages(tdbb, transaction->tra_number, new_number);
	}
}


static void restart_requests(thread_db* tdbb, jrd_tra* trans)
{
/**************************************
 *
 *	r e s t a r t _ r e q u e s t s
 *
 **************************************
 *
 * Functional description
 *	Restart all requests in the current
 *	attachment to utilize the passed
 *	transaction.
 *
 **************************************/
	SET_TDBB(tdbb);

	for (jrd_req** i = trans->tra_attachment->att_requests.begin();
		 i != trans->tra_attachment->att_requests.end();
		 ++i)
	{
		Array<jrd_req*>& requests = (*i)->getStatement()->requests;

		for (jrd_req** j = requests.begin(); j != requests.end(); ++j)
		{
			jrd_req* request = *j;

			if (request && request->req_transaction)
			{
				EXE_unwind(tdbb, request);
				EXE_start(tdbb, request, trans);
			}
		}
	}
}


static void retain_context(thread_db* tdbb, jrd_tra* transaction, bool commit, int state)
{
/**************************************
 *
 *	r e t a i n _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	If 'commit' flag is true, commit the transaction,
 *	else rollback the transaction.
 *
 *	Commit/rollback a transaction while preserving the
 *	context, in particular, its snapshot. The
 *	trick is to insure that the transaction's
 *	oldest active is seen by other transactions
 *	simultaneously starting up.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// The new transaction needs to remember the 'commit-retained' transaction
	// because it must see the operations of the 'commit-retained' transaction and
	// its snapshot doesn't contain these operations.

	if (commit)
		TBM_SET(tdbb->getDefaultPool(), &transaction->tra_commit_sub_trans, transaction->tra_number);

	// Create a new transaction lock, inheriting oldest active from transaction being committed.

	WIN window(DB_PAGE_SPACE, -1);
	TraNumber new_number;
#ifdef SUPERSERVER_V2
	new_number = bump_transaction_id(tdbb, &window);
#else
	if (dbb->readOnly())
		new_number = dbb->generateTransactionId();
	else
	{
		const bool dontWrite = (dbb->dbb_flags & DBB_shared) &&
			(transaction->tra_flags & TRA_readonly);

		const header_page* const header = bump_transaction_id(tdbb, &window, dontWrite);
		new_number = Ods::getNT(header);
	}
#endif

	Lock* new_lock = NULL;
	Lock* old_lock = transaction->tra_lock;
	if (old_lock)
	{
		new_lock = FB_NEW_RPT(*tdbb->getDefaultPool(), 0) Lock(tdbb, sizeof(TraNumber), LCK_tra);
		new_lock->setKey(new_number);
		new_lock->lck_data = transaction->tra_lock->lck_data;

		if (!LCK_lock(tdbb, new_lock, LCK_write, LCK_WAIT))
		{
#ifndef SUPERSERVER_V2
			if (!dbb->readOnly())
				CCH_RELEASE(tdbb, &window);
#endif
			ERR_post(Arg::Gds(isc_lock_conflict));
		}
	}

#ifndef SUPERSERVER_V2
	if (!dbb->readOnly())
		CCH_RELEASE(tdbb, &window);
#endif

	// Update database notion of the youngest commit retaining
	// transaction before committing the first transaction. This
	// secures the original snapshot by insuring the oldest active
	// is seen by other transactions.

	const TraNumber old_number = transaction->tra_number;

	if (!dbb->readOnly())
	{
		// Set the state on the inventory page
		TRA_set_state(tdbb, transaction, old_number, state);

		if (commit)
			REPL_trans_commit(tdbb, transaction);
		else
			REPL_trans_rollback(tdbb, transaction);
	}
	if (dbb->dbb_config->getClearGTTAtRetaining())
		release_temp_tables(tdbb, transaction);
	else
		retain_temp_tables(tdbb, transaction, new_number);

	transaction->tra_number = new_number;

	// Release transaction lock since it isn't needed
	// anymore and the new one is already in place.

	if (old_lock)
	{
		++transaction->tra_use_count;
		LCK_release(tdbb, old_lock);
		transaction->tra_lock = new_lock;
		--transaction->tra_use_count;
		delete old_lock;
	}

	// Perform any post commit work OR delete entries from deferred list

	if (commit)
		DFW_perform_post_commit_work(transaction);
	else
		DFW_delete_deferred(transaction, -1);

	transaction->tra_flags &= ~(TRA_write | TRA_prepared);

	// All savepoint were already released in TRA_commit/TRA_rollback except, may be, empty transaction-level one
	if (!transaction->tra_save_point && !(transaction->tra_flags & TRA_no_auto_undo))
		transaction->startSavepoint(true);	// start new savepoint if necessary

	if (transaction->tra_flags & TRA_precommitted)
	{
		if (!dbb->readOnly())
		{
			transaction->tra_flags &= ~TRA_precommitted;
			TRA_set_state(tdbb, transaction, new_number, tra_committed);
			transaction->tra_flags |= TRA_precommitted;
		}
	}
}


static void start_sweeper(thread_db* tdbb)
{
/**************************************
 *
 *	s t a r t _ s w e e p e r
 *
 **************************************
 *
 * Functional description
 *	Start a thread to sweep the database.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	if (!dbb->allowSweepThread(tdbb))
		return;

	TRA_update_counters(tdbb, dbb);

	// allocate space for the string and a null at the end
	const char* pszFilename = tdbb->getAttachment()->att_filename.c_str();

	char* database = (char*) gds__alloc(static_cast<SLONG>(strlen(pszFilename)) + 1);

	if (database)
	{
		strcpy(database, pszFilename);

		try
		{
			Thread::start(sweep_database, database, THREAD_medium);
			return;
		}
		catch (const Firebird::Exception& ex)
		{
			gds__free(database);
			iscLogException("cannot start sweep thread", ex);
		}
	}
	else
	{
		ERR_log(0, 0, "cannot start sweep thread, Out of Memory");
	}
	dbb->clearSweepFlags(tdbb);
}


static THREAD_ENTRY_DECLARE sweep_database(THREAD_ENTRY_PARAM database)
{
/**************************************
 *
 *	s w e e p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Sweep database.
 *
 **************************************/
	Firebird::ClumpletWriter dpb(Firebird::ClumpletReader::dpbList, MAX_DPB_SIZE);

	dpb.insertByte(isc_dpb_sweep, isc_dpb_records);
	// use embedded authentication to attach database
	const char* szAuthenticator = "sweeper";
	dpb.insertString(isc_dpb_user_name, szAuthenticator, fb_strlen(szAuthenticator));

	ISC_STATUS_ARRAY status_vector = {0};
	isc_db_handle db_handle = 0;

	isc_attach_database(status_vector, 0, (const char*) database,
						&db_handle, dpb.getBufferLength(),
						reinterpret_cast<const char*>(dpb.getBuffer()));

	if (db_handle)
	{
		isc_detach_database(status_vector, &db_handle);
	}

	gds__free(database);
	return 0;
}


static void transaction_flush(thread_db* tdbb, USHORT flush_flag, TraNumber tra_number)
{
/**************************************
 *
 *	t r a n s a c t i o n _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush pages modified by user and/or system transaction.
 *  Note, flush of user transaction also flushed pages,
 *  changed by system transaction.
 *
 **************************************/
	fb_assert(flush_flag == FLUSH_TRAN || flush_flag == FLUSH_SYSTEM);

	CCH_flush(tdbb, flush_flag, tra_number);

	jrd_tra* const sysTran = tdbb->getAttachment()->getSysTransaction();
	sysTran->tra_flags &= ~TRA_write;
}


static void transaction_options(thread_db* tdbb,
								jrd_tra* transaction,
								const UCHAR* tpb, USHORT tpb_length)
{
/**************************************
 *
 *	t r a n s a c t i o n _ o p t i o n s
 *
 **************************************
 *
 * Functional description
 *	Process transaction options.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (!tpb_length)
		return;

	const UCHAR* const end = tpb + tpb_length;

	if (*tpb != isc_tpb_version3 && *tpb != isc_tpb_version1)
		ERR_post(Arg::Gds(isc_bad_tpb_form) <<
				 Arg::Gds(isc_wrotpbver));

	Attachment* const attachment = tdbb->getAttachment();

	RelationLockTypeMap	lockmap;

	TriState wait, lock_timeout;
	TriState isolation, read_only, rec_version, read_consistency;
	bool anylock_write = false;
	bool shared_snapshot = false;

	++tpb;

	while (tpb < end)
	{
		const USHORT op = *tpb++;
		switch (op)
		{
		case isc_tpb_consistency:
			if (!isolation.assignOnce(true))
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_multiple_txn_isolation));

			if (shared_snapshot)
			{
				ERR_post(
					Arg::Gds(isc_bad_tpb_content) <<
					Arg::Gds(isc_tpb_conflicting_options) <<
						Arg::Str("isc_tpb_consistency") << Arg::Str("isc_tpb_at_snapshot_number"));
			}

			transaction->tra_flags |= TRA_degree3;
			transaction->tra_flags &= ~TRA_read_committed;
			break;

		case isc_tpb_concurrency:
			if (!isolation.assignOnce(true))
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_multiple_txn_isolation));

			transaction->tra_flags &= ~TRA_degree3;
			transaction->tra_flags &= ~TRA_read_committed;
			break;

		case isc_tpb_read_committed:
			if (!isolation.assignOnce(true))
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_multiple_txn_isolation));

			if (shared_snapshot)
			{
				ERR_post(
					Arg::Gds(isc_bad_tpb_content) <<
					Arg::Gds(isc_tpb_conflicting_options) <<
						Arg::Str("isc_tpb_read_committed") << Arg::Str("isc_tpb_at_snapshot_number"));
			}

			transaction->tra_flags &= ~TRA_degree3;
			transaction->tra_flags |= TRA_read_committed;
			break;

		case isc_tpb_shared:
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_before_table) << Arg::Str("isc_tpb_shared"));
			break;

		case isc_tpb_protected:
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_before_table) << Arg::Str("isc_tpb_protected"));
			break;

		case isc_tpb_exclusive:
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_reserv_before_table) << Arg::Str("isc_tpb_exclusive"));
			break;

		case isc_tpb_wait:
			if (!wait.assignOnce(true))
			{
				if (!wait.value)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_wait") <<
																	  Arg::Str("isc_tpb_nowait"));
				}
				else
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_wait"));
				}
			}
			break;

		case isc_tpb_rec_version:
			if (isolation.isAssigned() && !(transaction->tra_flags & TRA_read_committed))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_option_without_rc) << Arg::Str("isc_tpb_rec_version"));
			}

			if (!rec_version.assignOnce(true))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_rec_version"));
			}

			if (read_consistency.isAssigned())
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					// 'Option @1 is not valid if @2 was used previously in TPB'
					Arg::Gds(isc_tpb_conflicting_options) <<
					Arg::Str("isc_tpb_rec_version") << Arg::Str("isc_tpb_read_consistency"));
			}

			transaction->tra_flags &= ~TRA_read_consistency;
			transaction->tra_flags |= TRA_rec_version;
			break;

		case isc_tpb_no_rec_version:
			if (isolation.isAssigned() && !(transaction->tra_flags & TRA_read_committed))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_option_without_rc) << Arg::Str("isc_tpb_no_rec_version"));
			}

			if (!rec_version.assignOnce(false))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_no_rec_version"));
			}

			if (read_consistency.isAssigned())
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					// 'Option @1 is not valid if @2 was used previously in TPB'
					Arg::Gds(isc_tpb_conflicting_options) <<
					Arg::Str("isc_tpb_no_rec_version") << Arg::Str("isc_tpb_read_consistency"));
			}

			transaction->tra_flags &= ~(TRA_rec_version | TRA_read_consistency);
			break;

		case isc_tpb_read_consistency:
			if (isolation.isAssigned() && !(transaction->tra_flags & TRA_read_committed))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					Arg::Gds(isc_tpb_option_without_rc) << Arg::Str("isc_tpb_read_consistency"));
			}

			if (!read_consistency.assignOnce(true))
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_read_consistency"));
			}

			if (rec_version.isAssigned())
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					// 'Option @1 is not valid if @2 was used previously in TPB'
					Arg::Gds(isc_tpb_conflicting_options) <<
					Arg::Str("isc_tpb_read_consistency") << (rec_version.value ?
						Arg::Str("isc_tpb_rec_version") : Arg::Str("isc_tpb_no_rec_version")) );
			}

			transaction->tra_flags |= TRA_read_consistency | TRA_rec_version;
			break;

		case isc_tpb_nowait:
			if (lock_timeout.value)
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_nowait") <<
																  Arg::Str("isc_tpb_lock_timeout"));
			}

			if (!wait.assignOnce(false))
			{
				if (wait.value)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_nowait") <<
																	  Arg::Str("isc_tpb_wait"));
				}
				else
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_nowait"));
				}
			}

			transaction->tra_lock_timeout = 0;
			break;

		case isc_tpb_read:
			if (!read_only.assignOnce(true))
			{
				if (!read_only.value)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_read") <<
																	  Arg::Str("isc_tpb_write"));
				}
				else
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_read"));
				}
			}

			// Cannot set the whole txn to R/O if we already saw a R/W table reservation.
			if (anylock_write)
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_readtxn_after_writelock));
			}

			transaction->tra_flags |= TRA_readonly;
			break;

		case isc_tpb_write:
			if (!read_only.assignOnce(false))
			{
				if (read_only.value)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_write") <<
																	  Arg::Str("isc_tpb_read"));
				}
				else
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_write"));
				}
			}

			transaction->tra_flags &= ~TRA_readonly;
			break;

		case isc_tpb_ignore_limbo:
			transaction->tra_flags |= TRA_ignore_limbo;
			break;

		case isc_tpb_no_auto_undo:
			transaction->tra_flags |= TRA_no_auto_undo;
			break;

		case isc_tpb_lock_write:
			// Cannot set a R/W table reservation if the whole txn is R/O.
			if (read_only.value)
			{
				ERR_post(Arg::Gds(isc_bad_tpb_content) <<
						 Arg::Gds(isc_tpb_writelock_after_readtxn));
			}
			anylock_write = true;
			// fall into
		case isc_tpb_lock_read:
			{
				const char* option_name = (op == isc_tpb_lock_read) ?
					"isc_tpb_lock_read" : "isc_tpb_lock_write";

				// Do we have space for the identifier length?
				if (tpb >= end)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_missing_tlen) << Arg::Str(option_name));
				}

				const USHORT len = *tpb++;
				if (len > MAX_SQL_IDENTIFIER_LEN)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_long_tlen) << Arg::Num(len) <<
																   Arg::Str(option_name));
				}

				if (!len)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_null_tlen) << Arg::Str(option_name));
				}

				// Does the identifier length surpasses the remaining of the TPB?
				if (tpb >= end)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_missing_tname) << Arg::Num(len) <<
							 										   Arg::Str(option_name));
				}

				if (end - tpb < len)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_corrup_tlen) << Arg::Num(len) <<
							 										 Arg::Str(option_name));
				}

				const MetaName orgName(reinterpret_cast<const char*>(tpb), len);
				const MetaName metaName = attachment->nameToMetaCharSet(tdbb, orgName);

				tpb += len;
				jrd_rel* relation = MET_lookup_relation(tdbb, metaName);
				if (!relation)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_reserv_relnotfound) << Arg::Str(metaName) <<
																	 Arg::Str(option_name));
				}

				// force a scan to read view information
				MET_scan_relation(tdbb, relation);

				UCHAR lock_type = (op == isc_tpb_lock_read) ? LCK_none : LCK_SW;
				if (tpb < end)
				{
					switch (*tpb)
					{
					case isc_tpb_shared:
						++tpb;
						break;
					case isc_tpb_protected:
					case isc_tpb_exclusive:
						++tpb;
						lock_type = (lock_type == LCK_SW) ? LCK_EX : LCK_PR;
						break;
					// We'll assume table reservation doesn't make the concurrency type mandatory.
					//default:
					//    ERR_post(isc-arg-end);
					}
				}

				expand_view_lock(tdbb, transaction, relation, lock_type, option_name, lockmap, 0);
			}
			break;

		case isc_tpb_verb_time:
		case isc_tpb_commit_time:
			{
				const char* option_name = (op == isc_tpb_verb_time) ?
					"isc_tpb_verb_time" : "isc_tpb_commit_time";
				// Harmless for now even if formally invalid.
				if (tpb >= end)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_missing_len) << Arg::Str(option_name));
				}

				const USHORT len = *tpb++;

				if (tpb >= end && len > 0)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_missing_value) << Arg::Num(len) << Arg::Str(option_name));
				}

				if (end - tpb < len)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_corrupt_len) << Arg::Num(len) << Arg::Str(option_name));
				}

				tpb += len;
			}
			break;

		case isc_tpb_autocommit:
			transaction->tra_flags |= TRA_autocommit;
			break;

		case isc_tpb_restart_requests:
			transaction->tra_flags |= TRA_restart_requests;
			break;

		case isc_tpb_lock_timeout:
			{
				if (wait.isAssigned() && !wait.value)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_conflicting_options) << Arg::Str("isc_tpb_lock_timeout") <<
																	  Arg::Str("isc_tpb_nowait"));
				}

				if (!lock_timeout.assignOnce(true))
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_multiple_spec) << Arg::Str("isc_tpb_lock_timeout"));
				}

				// Do we have space for the identifier length?
				if (tpb >= end)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_missing_len) << Arg::Str("isc_tpb_lock_timeout"));
				}

				const USHORT len = *tpb++;

				// Does the encoded number's length surpasses the remaining of the TPB?
				if (tpb >= end)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_missing_value) << Arg::Num(len) <<
																Arg::Str("isc_tpb_lock_timeout"));
				}

				if (end - tpb < len)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_corrupt_len) << Arg::Num(len) <<
															  Arg::Str("isc_tpb_lock_timeout"));
				}

				if (!len)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_null_len) << Arg::Str("isc_tpb_lock_timeout"));
				}

				if (len > sizeof(ULONG))
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_overflow_len) << Arg::Num(len) << Arg::Str("isc_tpb_lock_timeout"));
				}

				const SLONG value = gds__vax_integer(tpb, len);

				if (value <= 0 || value > MAX_SSHORT)
				{
					ERR_post(Arg::Gds(isc_bad_tpb_content) <<
							 Arg::Gds(isc_tpb_invalid_value) << Arg::Num(value) << Arg::Str("isc_tpb_lock_timeout"));
				}

				transaction->tra_lock_timeout = (SSHORT) value;

				tpb += len;
			}
			break;

		case isc_tpb_at_snapshot_number:
			{
				const char* option_name = "isc_tpb_at_snapshot_number";

				if (shared_snapshot)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_multiple_spec) << Arg::Str(option_name));
				}

				if (transaction->tra_flags & TRA_read_committed)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_conflicting_options) <<
							Arg::Str(option_name) << Arg::Str("isc_tpb_read_committed"));
				}

				if (transaction->tra_flags & TRA_degree3)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_conflicting_options) <<
							Arg::Str(option_name) << Arg::Str("isc_tpb_consistency"));
				}

				if (tpb >= end)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_missing_len) << Arg::Str(option_name));
				}

				const USHORT len = *tpb++;

				if (tpb >= end && len > 0)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_missing_value) << Arg::Num(len) << Arg::Str(option_name));
				}

				if (end - tpb < len || len == 0)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Gds(isc_tpb_corrupt_len) << Arg::Num(len) << Arg::Str(option_name));
				}

				shared_snapshot = true;
				transaction->tra_snapshot_number = isc_portable_integer(tpb, len);

				if (transaction->tra_snapshot_number == 0)
				{
					ERR_post(
						Arg::Gds(isc_bad_tpb_content) <<
						Arg::Str(option_name));
				}

				tpb += len;
			}
			break;

		default:
			ERR_post(Arg::Gds(isc_bad_tpb_form));
		}
	}

	if (rec_version.isAssigned() && !(transaction->tra_flags & TRA_read_committed))
	{
		if (rec_version.value)
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_option_without_rc) << Arg::Str("isc_tpb_rec_version"));
		}
		else
		{
			ERR_post(Arg::Gds(isc_bad_tpb_content) <<
					 Arg::Gds(isc_tpb_option_without_rc) << Arg::Str("isc_tpb_no_rec_version"));
		}
	}

	if ((transaction->tra_flags & TRA_read_committed) && !(tdbb->tdbb_flags & TDBB_sweeper))
	{
		if (tdbb->getDatabase()->dbb_config->getReadConsistency())
			transaction->tra_flags |= TRA_read_consistency | TRA_rec_version;
	}

	// If there aren't any relation locks to seize, we're done.

	vec<Lock*>* vector = transaction->tra_relation_locks;
	if (!vector)
		return;

	// Try to seize all relation locks. If any can't be seized, release all and try again.

	for (ULONG id = 0; id < vector->count(); id++)
	{
		Lock* lock = (*vector)[id];
		if (!lock)
			continue;

		USHORT level = lock->lck_logical;

		if (level == LCK_none || LCK_lock(tdbb, lock, level, transaction->getLockWait()))
			continue;

		for (ULONG l = 0; l < id; l++)
		{
			if ( (lock = (*vector)[l]) )
			{
				level = lock->lck_logical;
				LCK_release(tdbb, lock);
				lock->lck_logical = level;
			}
		}

		id = 0;
		ERR_punt();
	}
}


static void transaction_start(thread_db* tdbb, jrd_tra* trans)
{
/**************************************
 *
 *	t r a n s a c t i o n _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a transaction.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	Jrd::Attachment* const attachment = tdbb->getAttachment();
	WIN window(DB_PAGE_SPACE, -1);

	Lock* lock = FB_NEW_RPT(*tdbb->getDefaultPool(), 0) Lock(tdbb, sizeof(TraNumber), LCK_tra);

	// Read header page and allocate transaction number.  Since
	// the transaction inventory page was initialized to zero, it
	// transaction is automatically marked active.

	TraNumber oldest, number, active, oldest_active;

#ifdef SUPERSERVER_V2
	number = bump_transaction_id(tdbb, &window);
	oldest = dbb->dbb_oldest_transaction;
	active = MAX(dbb->dbb_oldest_active, dbb->dbb_oldest_transaction);
	oldest_active = dbb->dbb_oldest_active;

#else // SUPERSERVER_V2
	if (dbb->readOnly())
	{
		number = dbb->generateTransactionId();
		oldest = dbb->dbb_oldest_transaction;
		oldest_active = dbb->dbb_oldest_active;
	}
	else
	{
		const bool dontWrite = (dbb->dbb_flags & DBB_shared) &&
			(trans->tra_flags & TRA_readonly);

		const header_page* header = bump_transaction_id(tdbb, &window, dontWrite);
		number = Ods::getNT(header);
		oldest = Ods::getOIT(header);
		oldest_active = Ods::getOAT(header);
	}

	// oldest (OIT) > oldest_active (OAT) if OIT was advanced by sweep
	// and no transactions was started after the sweep starts
	active = MAX(oldest_active, oldest);

#endif // SUPERSERVER_V2

	// Allocate pool and transactions block.  Since, by policy,
	// all transactions older than the oldest are either committed
	// or cleaned up, they can be all considered as committed.  To
	// make everything simpler, round down the oldest to a multiple
	// of four, which puts the transaction on a byte boundary.

	trans->tra_number = number;
	trans->tra_initial_number = number;
	trans->tra_top = number;
	trans->tra_oldest = oldest;
	trans->tra_oldest_active = active;

	trans->tra_lock = lock;
	lock->setKey(number);

	// Put the TID of the oldest active transaction (from the header page)
	// in the new transaction's lock.
	// hvlad: it is important to put transaction number for read-committed
	// transaction instead of oldest active to correctly calculate new oldest
	// active value (look at call to LCK_query_data below which will take into
	// account this new lock too)

	lock->lck_data = ((trans->tra_flags & TRA_read_committed) &&
		!(trans->tra_flags & TRA_read_consistency)) ? number : active;
	lock->lck_object = trans;

	if (!LCK_lock(tdbb, lock, LCK_write, LCK_WAIT))
	{
#ifndef SUPERSERVER_V2
		if (!dbb->readOnly())
			CCH_RELEASE(tdbb, &window);
#endif
		ERR_post(Arg::Gds(isc_lock_conflict));
	}

	// Link the transaction to the attachment block before releasing
	// header page for handling signals.

	trans->linkToAttachment(attachment);

	try
	{
#ifndef SUPERSERVER_V2
		if (!dbb->readOnly())
			CCH_RELEASE(tdbb, &window);
#endif

		if (dbb->readOnly())
		{
			// Set transaction flags to TRA_precommitted, TRA_readonly
			trans->tra_flags |= (TRA_readonly | TRA_precommitted);
		}

		// Next, take a snapshot of all transactions between the oldest interesting
		// transaction and the current.  Don't bother to get a snapshot for
		// read-committed transactions; they use the snapshot off the dbb block
		// since they need to know what is currently committed.

		if (!(trans->tra_flags & TRA_read_committed))
		{
			try
			{
				trans->tra_snapshot_handle = dbb->dbb_tip_cache->beginSnapshot(
					tdbb, attachment->att_attachment_id, trans->tra_snapshot_number);
			}
			catch (const Firebird::Exception&)
			{
				LCK_release(tdbb, lock);
				throw;
			}
		}

		// Next task is to find the oldest active transaction on the system.  This
		// is needed for garbage collection.  Things are made ever so slightly
		// more complicated by the fact that existing transaction may have oldest
		// actives older than they are.

		Lock temp_lock(tdbb, sizeof(TraNumber), LCK_tra, trans);

		trans->tra_oldest_active = number;
		oldest_active = number;
		bool cleanup = !(number % TRA_ACTIVE_CLEANUP);
		int oldest_state;

		for (; active < number; active++)
		{
			//oldest_state = TPC_cache_state(tdbb, active);
			const ULONG mask = (1 << tra_active);
			active = TPC_find_states(tdbb, active, number, mask, oldest_state);
			if (!active)
			{
				active = number;
				break;
			}
			fb_assert(oldest_state == tra_active);

			if (oldest_state == tra_active)
			{
				temp_lock.setKey(active);
				TraNumber data = LCK_read_data(tdbb, &temp_lock);
				if (!data)
				{
					if (cleanup)
					{
						if (TRA_wait(tdbb, trans, active, jrd_tra::tra_no_wait) == tra_committed)
							cleanup = false;
						continue;
					}

					data = active;
				}

				oldest_active = active;
				break;
			}
		}

		// Calculate attachment-local oldest active and oldest snapshot numbers
		// looking at current attachment's transactions only. Calculated values
		// are used to determine garbage collection threshold for attachment-local
		// data such as temporary tables (GTT's).

		trans->tra_att_oldest_active = number;
		TraNumber att_oldest_active = number;
		TraNumber att_oldest_snapshot = number;

		for (jrd_tra* tx_att = attachment->att_transactions; tx_att; tx_att = tx_att->tra_next)
		{
			att_oldest_active = MIN(att_oldest_active, tx_att->tra_number);
			att_oldest_snapshot = MIN(att_oldest_snapshot, tx_att->tra_att_oldest_active);
		}

		trans->tra_att_oldest_active = ((trans->tra_flags & TRA_read_committed) &&
			!(trans->tra_flags & TRA_read_consistency)) ? number : att_oldest_active;

		if (attachment->att_oldest_snapshot < att_oldest_snapshot)
			attachment->att_oldest_snapshot = att_oldest_snapshot;

		// Put the TID of the oldest active transaction (just calculated)
		// in the new transaction's lock.
		// hvlad: for read-committed transaction put tra_number to prevent
		// unnecessary blocking of garbage collection by read-committed
		// transactions

		const TraNumber lck_data = ((trans->tra_flags & TRA_read_committed) &&
			!(trans->tra_flags & TRA_read_consistency)) ? number : oldest_active;

		static_assert(sizeof(lock->lck_data) == sizeof(lck_data), "Check lock data type !");
		if (lock->lck_data != (SINT64) lck_data)
			LCK_write_data(tdbb, lock, lck_data);

		// Query the minimum lock data for all active transaction locks.
		// This will be the oldest active snapshot used for regulating garbage collection.

		const TraNumber data = LCK_query_data(tdbb, LCK_tra, LCK_MIN);
		if (data && data < trans->tra_oldest_active)
			trans->tra_oldest_active = data;

		// Finally, scan transactions looking for the oldest interesting transaction -- the oldest
		// non-commited transaction.  This will not be updated immediately, but saved until the
		// next update access to the header page

		oldest_state = tra_committed;

		for (oldest = trans->tra_oldest; oldest < number; oldest++)
		{
			//oldest_state = TPC_cache_state(tdbb, oldest);
			const ULONG mask = ~((1 << tra_committed) | (1 << tra_precommitted));
			oldest = TPC_find_states(tdbb, trans->tra_oldest, number, mask, oldest_state);
			if (!oldest)
			{
				oldest = number;
				break;
			}
			fb_assert(oldest_state != tra_committed && oldest_state != tra_precommitted);

			if (oldest_state != tra_committed && oldest_state != tra_precommitted)
				break;
		}

		if (oldest > number && dbb->dbb_flags & DBB_read_only)
			oldest = number;

		if (--oldest > dbb->dbb_oldest_transaction)
			dbb->dbb_oldest_transaction = oldest;

		if (oldest_active > dbb->dbb_oldest_active)
			dbb->dbb_oldest_active = oldest_active;

		if (trans->tra_oldest_active > dbb->dbb_oldest_snapshot)
		{
			dbb->dbb_oldest_snapshot = trans->tra_oldest_active;

			if (!(dbb->dbb_flags & DBB_gc_active) && (dbb->dbb_flags & DBB_gc_background))
			{
				dbb->dbb_flags |= DBB_gc_pending;
				dbb->dbb_gc_sem.release();
			}
		}

		// Release TPC shared memory if counters moved sufficently forward
		dbb->dbb_tip_cache->updateOldestTransaction(tdbb,
			dbb->dbb_oldest_transaction, dbb->dbb_oldest_snapshot);

		// If the transaction block is getting out of hand, force a sweep

		if (dbb->dbb_sweep_interval &&
			(trans->tra_oldest_active > oldest) &&
			(trans->tra_oldest_active - oldest > dbb->dbb_sweep_interval) &&
			oldest_state != tra_limbo)
		{
			start_sweeper(tdbb);
		}

		// Start a 'transaction-level' savepoint, unless this is the
		// system transaction, or unless the transactions doesn't want
		// a savepoint to be started.  This savepoint will be used to
		// undo the transaction if it rolls back.

		if (!(trans->tra_flags & TRA_system) && !(trans->tra_flags & TRA_no_auto_undo))
			trans->startSavepoint(true);

		// if the user asked us to restart all requests in this attachment,
		// do so now using the new transaction

		if (trans->tra_flags & TRA_restart_requests)
			restart_requests(tdbb, trans);

		// If the transaction is read-only and read committed, it can be
		// precommitted because it can't modify any records.
		// 2014-08-26 NS XXX: with latest changes in TIP cache semantics and read
		// consistency changes precommitted transactions offer almost no benefit, but
		// complicate implementation considerably. It might make sense to remove
		// precommitted transactions logic completely.

		if ((trans->tra_flags & TRA_readonly) && (trans->tra_flags & TRA_read_committed))
		{
			TRA_set_state(tdbb, trans, trans->tra_number, tra_committed);
			if (!(trans->tra_flags & TRA_read_consistency))
				LCK_write_data(tdbb, lock, 0); // Fully disinhibit GC for this transaction
			trans->tra_flags |= TRA_precommitted;
		}

		if (dbb->isReplicating(tdbb))
			trans->tra_flags |= TRA_replicating;
	}
	catch (const Firebird::Exception&)
	{
		trans->unlinkFromAttachment();
		throw;
 	}
}


jrd_tra::~jrd_tra()
{
	while (tra_undo_records.hasData())
		delete tra_undo_records.pop();

	delete tra_undo_space;
	delete tra_user_management;
	delete tra_timezone_snapshot;
	delete tra_mapping_list;
	delete tra_gen_ids;

	if (!tra_outer)
		delete tra_blob_space;
	else
		fb_assert(!tra_arrays);

	DFW_delete_deferred(this, -1);

	if (tra_flags & TRA_own_interface)
	{
		tra_interface->setHandle(NULL);
		tra_interface->release();
	}

	if (tra_autonomous_pool)
		MemoryPool::deletePool(tra_autonomous_pool);

	delete tra_sec_db_context;
}


JTransaction* jrd_tra::getInterface(bool create)
{
	if (!tra_interface && create)
	{
		tra_flags |= TRA_own_interface;
		tra_interface = FB_NEW JTransaction(this, tra_attachment->getStable());
		tra_interface->addRef();
	}

	return tra_interface;
}


void jrd_tra::setInterface(JTransaction* jt)
{
	fb_assert(tra_interface == NULL || tra_interface == jt);
	tra_interface = jt;
}


TimeZoneSnapshot* jrd_tra::getTimeZoneSnapshot(thread_db* tdbb)
{
	if (!tra_timezone_snapshot)
		tra_timezone_snapshot = FB_NEW_POOL(*tra_pool) TimeZoneSnapshot(tdbb, *tra_pool);

	return tra_timezone_snapshot;
}


UserManagement* jrd_tra::getUserManagement()
{
	if (!tra_user_management)
		tra_user_management = FB_NEW_POOL(*tra_pool) UserManagement(this);

	return tra_user_management;
}


MappingList* jrd_tra::getMappingList()
{
	if (!tra_mapping_list)
		tra_mapping_list = FB_NEW_POOL(*tra_pool) MappingList(this);

	return tra_mapping_list;
}

DbCreatorsList* jrd_tra::getDbCreatorsList()
{
	if (!tra_dbcreators_list)
		tra_dbcreators_list = FB_NEW_POOL(*tra_pool) DbCreatorsList(this);

	return tra_dbcreators_list;
}


jrd_tra* jrd_tra::getOuter()
{
	jrd_tra* tra = this;

	while (tra->tra_outer)
		tra = tra->tra_outer;

	return tra;
}


MemoryPool* jrd_tra::getAutonomousPool()
{
	if (!tra_autonomous_pool)
	{
		MemoryPool* pool = tra_pool;
		jrd_tra* outer = tra_outer;
		while (outer)
		{
			pool = outer->tra_pool;
			outer = outer->tra_outer;
		}
		tra_autonomous_pool = MemoryPool::createPool(pool, tra_memory_stats);
		tra_autonomous_cnt = 0;
	}

	return tra_autonomous_pool;
}

Record* jrd_tra::findNextUndo(VerbAction* stopAction, jrd_rel* relation, SINT64 number)
/**************************************
 *
 *	f i n d N e x t U n d o
 *
 **************************************
 *
 * Functional description
 *	For given record find next undo data in stack of savepoint (if any).
 *
 **************************************/
{
	UndoItem* result = NULL;

	for (Savepoint::Iterator iter(tra_save_point); *iter; ++iter)
	{
		VerbAction* const action = (*iter)->getAction(relation);

		if (action == stopAction)
			return result ? result->setupRecord(this) : NULL;

		if (action && action->vct_undo && action->vct_undo->locate(number))
			result = &(action->vct_undo->current());
	}

	fb_assert(false);	// verb_action disappeared from savepoint stack
	return NULL;
}

void jrd_tra::listStayingUndo(jrd_rel* relation, SINT64 number, RecordStack &staying)
/**************************************
 *
 *	l i s t S t a y i n g U n d o
 *
 **************************************
 *
 * Functional description
 *	For given record find undo data in stack of savepoint (if any) and push it into list.
 *  Except one from given verb action.
 *
 **************************************/
{
	for (Savepoint::Iterator iter(tra_save_point); *iter; ++iter)
	{
		VerbAction* const action = (*iter)->getAction(relation);

		if (action && action->vct_undo && action->vct_undo->locate(number))
			staying.push(action->vct_undo->current().setupRecord(this));
	}
}

void jrd_tra::releaseAutonomousPool(MemoryPool* toRelease)
{
	fb_assert(tra_autonomous_pool == toRelease);
	if (++tra_autonomous_cnt > TRA_AUTONOMOUS_PER_POOL)
	{
		MemoryPool::deletePool(tra_autonomous_pool);
		tra_autonomous_pool = NULL;
	}
}

Savepoint* jrd_tra::startSavepoint(bool root)
/**************************************
 *
 *	 s t a r t S a v e p o i n t
 *
 **************************************
 *
 * Functional description
 *	Start a new savepoint. Reuse some priorly allocated one, if exists.
 *
 **************************************/
{
	Savepoint* savepoint = tra_save_free;

	if (savepoint)
		tra_save_free = savepoint->getNext();
	else
		savepoint = FB_NEW_POOL(*tra_pool) Savepoint(this);

	const SavNumber number = ++tra_save_point_number;
	savepoint->init(number, root, tra_save_point);
	tra_save_point = savepoint;

	return savepoint;
}

void jrd_tra::rollbackSavepoint(thread_db* tdbb, bool preserveLocks)
/**************************************
 *
 *	 r o l l b a c k S a v e p o i n t
 *
 **************************************
 *
 * Functional description
 *	Rollback last savepoint and free it.
 *
 **************************************/
{
	if (tra_save_point && !(tra_flags & TRA_system))
	{
		REPL_save_cleanup(tdbb, this, tra_save_point, true);

		if (tra_flags & TRA_ex_restart)
			preserveLocks = true;

		Jrd::ContextPoolHolder context(tdbb, tra_pool);
		tra_save_point = tra_save_point->rollback(tdbb, NULL, preserveLocks);
	}
}

void jrd_tra::rollbackToSavepoint(thread_db* tdbb, SavNumber number)
/**************************************
 *
 *	 r o l l b a c k T o S a v e p o i n t
 *
 **************************************
 *
 * Functional description
 *	Rollback savepoints up to one with given number.
 *  There may be cases when savepoint with given number does not exist.
 *  Rollback all savepoints with bigger numbers then, but not more.
 *  These cases most likely is a bug in logic somewhere, so assert is here.
 *
 **************************************/
{
	// Merge all but one folowing savepoints into one
	while (tra_save_point && tra_save_point->getNumber() > number &&
		tra_save_point->getNext() && tra_save_point->getNext()->getNumber() >= number)
	{
		rollforwardSavepoint(tdbb);
	}

	// Check that savepoint with given number really exists
	fb_assert(tra_save_point && tra_save_point->getNumber() == number);

	if (tra_save_point && tra_save_point->getNumber() >= number) // second line of defence
	// under no circumstances a savepoint with smaller number should be rolled back
	{
		// Undo the savepoint
		rollbackSavepoint(tdbb);
	}
}


void jrd_tra::rollforwardSavepoint(thread_db* tdbb)
/**************************************
 *
 *	 r o l l f o r w a r d S a v e p o i n t
 *
 **************************************
 *
 * Functional description
 *	Apply last savepoint and free it.
 *
 **************************************/
{
	if (tra_save_point && !(tra_flags & TRA_system))
	{
		REPL_save_cleanup(tdbb, this, tra_save_point, false);

		Jrd::ContextPoolHolder context(tdbb, tra_pool);
		tra_save_point = tra_save_point->rollforward(tdbb);
	}
}

void jrd_tra::checkBlob(thread_db* tdbb, const bid* blob_id, jrd_fld* fld, bool punt)
{
	USHORT rel_id = blob_id->bid_internal.bid_relation_id;

	if (tra_attachment->isGbak() ||
		(tra_attachment->locksmith(tdbb, SELECT_ANY_OBJECT_IN_DATABASE)) ||
		rel_id == 0)
	{
		return;
	}

	if (!tra_blobs->locate(blob_id->bid_temp_id()) &&
		!tra_fetched_blobs.locate(*blob_id))
	{
		vec<jrd_rel*>* vector = tra_attachment->att_relations;
		jrd_rel* blb_relation;

		if (rel_id < vector->count() &&	(blb_relation = (*vector)[rel_id]))
		{
			const MetaName security_name = fld ?
				fld->fld_security_name : blb_relation->rel_security_name;

			if (security_name.isEmpty())
				MET_scan_relation(tdbb, blb_relation);

			SecurityClass* s_class = SCL_get_class(tdbb, security_name.c_str());

			if (!s_class)
				return;

			switch (s_class->scl_blb_access)
			{
			case SecurityClass::BA_UNKNOWN:
				// Relation (or field) has not been checked for access rights
				try
				{
					ThreadStatusGuard status_vector(tdbb);

					if (fld)
					{
						SCL_check_access(tdbb, s_class, 0, 0, SCL_select, SCL_object_column,
							false, fld->fld_name, blb_relation->rel_name);
					}
					else
					{
						SCL_check_access(tdbb, s_class, 0, 0, SCL_select, SCL_object_table,
							false, blb_relation->rel_name);
					}

					s_class->scl_blb_access = SecurityClass::BA_SUCCESS;
				}
				catch (const Exception& ex)
				{
					StaticStatusVector status;
					ex.stuffException(status);
					if (status[1] != isc_no_priv)
						throw;

					// We don't have access to this relation
					s_class->scl_blb_access = SecurityClass::BA_FAILURE;

					if (punt)
						throw;

					// but someone else has (SP, view)
					// store Blob ID as allowed in this transaction
					tra_fetched_blobs.add(*blob_id);
				}
				break;

			case SecurityClass::BA_FAILURE:
				// Relation has been checked earlier and check was failed
				if (punt)
				{
					ERR_post(Arg::Gds(isc_no_priv) << Arg::Str("SELECT") <<
						(fld ? Arg::Str("COLUMN") : Arg::Str("TABLE")) <<
						(fld ? Arg::Str(fld->fld_name) : Arg::Str(blb_relation->rel_name)));
				}
				else
					tra_fetched_blobs.add(*blob_id);
				break;

			case SecurityClass::BA_SUCCESS:
				// do nothing
				break;

			default:
				fb_assert(false);
			}
		}
	}
}

/// class TraceSweepEvent

TraceSweepEvent::TraceSweepEvent(thread_db* tdbb)
{
	m_tdbb = tdbb;

	WIN window(HEADER_PAGE_NUMBER);
	Ods::header_page* header = (Ods::header_page*) CCH_FETCH(m_tdbb, &window, LCK_read, pag_header);

	m_sweep_info.update(header);
	CCH_RELEASE(m_tdbb, &window);

	Attachment* att = m_tdbb->getAttachment();

	gds__log("Sweep is started by %s\n"
		"\tDatabase \"%s\" \n"
		"\tOIT %" SQUADFORMAT", OAT %" SQUADFORMAT", OST %" SQUADFORMAT", Next %" SQUADFORMAT,
		att->att_user ? att->att_user->getUserName().c_str() : "<Unknown user>",
		att->att_filename.c_str(),
		m_sweep_info.getOIT(),
		m_sweep_info.getOAT(),
		m_sweep_info.getOST(),
		m_sweep_info.getNext());

	TraceManager* trace_mgr = att->att_trace_manager;

	m_need_trace = trace_mgr->needs(ITraceFactory::TRACE_EVENT_SWEEP);

	if (!m_need_trace)
		return;

	m_start_clock = fb_utils::query_performance_counter();

	TraceConnectionImpl conn(att);
	trace_mgr->event_sweep(&conn, &m_sweep_info, ITracePlugin::SWEEP_STATE_STARTED);
}


TraceSweepEvent::~TraceSweepEvent()
{
	m_tdbb->setRequest(NULL);
	report(ITracePlugin::SWEEP_STATE_FAILED);
}


void TraceSweepEvent::beginSweepRelation(jrd_rel* relation)
{
	if (!m_need_trace)
		return;

	if (relation && relation->rel_name.isEmpty())
	{
		// don't accumulate per-relation stats for metadata query below
		MET_lookup_relation_id(m_tdbb, relation->rel_id, false);
	}

	m_relation_clock = fb_utils::query_performance_counter();
	m_base_stats.assign(m_tdbb->getTransaction()->tra_stats);
}


void TraceSweepEvent::endSweepRelation(jrd_rel* relation)
{
	if (!m_need_trace)
		return;

	Attachment* att = m_tdbb->getAttachment();
	jrd_tra* tran = m_tdbb->getTransaction();

	// don't report empty relation
	if (m_base_stats.getValue(RuntimeStatistics::RECORD_SEQ_READS) ==
		tran->tra_stats.getValue(RuntimeStatistics::RECORD_SEQ_READS) &&

		m_base_stats.getValue(RuntimeStatistics::RECORD_BACKOUTS) ==
		tran->tra_stats.getValue(RuntimeStatistics::RECORD_BACKOUTS) &&

		m_base_stats.getValue(RuntimeStatistics::RECORD_PURGES) ==
		tran->tra_stats.getValue(RuntimeStatistics::RECORD_PURGES) &&

		m_base_stats.getValue(RuntimeStatistics::RECORD_EXPUNGES) ==
		tran->tra_stats.getValue(RuntimeStatistics::RECORD_EXPUNGES) )
	{
		return;
	}

	TraceRuntimeStats stats(att, &m_base_stats, &tran->tra_stats,
		fb_utils::query_performance_counter() - m_relation_clock,
		0);

	m_sweep_info.setPerf(stats.getPerf());

	TraceConnectionImpl conn(att);
	TraceManager* trace_mgr = att->att_trace_manager;
	trace_mgr->event_sweep(&conn, &m_sweep_info, ITracePlugin::SWEEP_STATE_PROGRESS);
}


void TraceSweepEvent::report(ntrace_process_state_t state)
{
	Attachment* att = m_tdbb->getAttachment();

	if (state == ITracePlugin::SWEEP_STATE_FINISHED)
	{
		gds__log("Sweep is finished\n"
			"\tDatabase \"%s\" \n"
			"\tOIT %" SQUADFORMAT", OAT %" SQUADFORMAT", OST %" SQUADFORMAT", Next %" SQUADFORMAT,
			att->att_filename.c_str(),
			m_sweep_info.getOIT(),
			m_sweep_info.getOAT(),
			m_sweep_info.getOST(),
			m_sweep_info.getNext());
	}

	if (!m_need_trace)
		return;

	Database* dbb = m_tdbb->getDatabase();
	TraceManager* trace_mgr = att->att_trace_manager;

	TraceConnectionImpl conn(att);

	// we need to compare stats against zero base
	if (state != ITracePlugin::SWEEP_STATE_PROGRESS)
		m_base_stats.reset();

	jrd_tra* tran = m_tdbb->getTransaction();

	TraceRuntimeStats stats(att, &m_base_stats, &att->att_stats,
		fb_utils::query_performance_counter() - m_start_clock,
		0);

	m_sweep_info.setPerf(stats.getPerf());
	trace_mgr->event_sweep(&conn, &m_sweep_info, state);

	if (state == ITracePlugin::SWEEP_STATE_FAILED || state == ITracePlugin::SWEEP_STATE_FINISHED)
		m_need_trace = false;
}

SecDbContext::SecDbContext(IAttachment* a, ITransaction* t)
	: att(a), tra(t), savePoint(0)
{ }

SecDbContext::~SecDbContext()
{
	LocalStatus ls;
	CheckStatusWrapper st(&ls);
	if (tra)
	{
		tra->rollback(&st);
		tra = NULL;
	}
	if (att)
	{
		att->detach(&st);
		att = NULL;
	}
}

SecDbContext* jrd_tra::getSecDbContext()
{
	return tra_sec_db_context;
}

SecDbContext* jrd_tra::setSecDbContext(IAttachment* att, ITransaction* tra)
{
	fb_assert(!tra_sec_db_context);

	tra_sec_db_context = FB_NEW_POOL(*getDefaultMemoryPool()) SecDbContext(att, tra);
	return tra_sec_db_context;
}

void jrd_tra::eraseSecDbContext()
{
	delete tra_sec_db_context;
	tra_sec_db_context = NULL;
}
