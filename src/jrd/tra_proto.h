/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		tra_proto.h
 *	DESCRIPTION:	Prototype header file for tra.cpp
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

#ifndef JRD_TRA_PROTO_H
#define JRD_TRA_PROTO_H

namespace Jrd {
class Attachment;
class Database;
class TraceTransactionEnd;
class DsqlCursor;
class Request;

class Resources;
class thread_db;
class jrd_tra;
/*
// System transaction is always transaction 0.
const TraNumber TRA_system_transaction = 0;

// Flag definitions for tra_flags.

const ULONG TRA_system				= 0x1L;			// system transaction
const ULONG TRA_prepared			= 0x2L;			// transaction is in limbo
const ULONG TRA_reconnected			= 0x4L;			// reconnect in progress
const ULONG TRA_degree3				= 0x8L;			// serializeable transaction
const ULONG TRA_write				= 0x10L;		// transaction has written
const ULONG TRA_readonly			= 0x20L;		// transaction is readonly
const ULONG TRA_prepare2			= 0x40L;		// transaction has updated RDB$TRANSACTIONS
const ULONG TRA_ignore_limbo		= 0x80L;		// ignore transactions in limbo
const ULONG TRA_invalidated 		= 0x100L;		// transaction invalidated by failed write
const ULONG TRA_deferred_meta 		= 0x200L;		// deferred meta work posted
const ULONG TRA_read_committed		= 0x400L;		// can see latest committed records
const ULONG TRA_autocommit			= 0x800L;		// autocommits all updates
const ULONG TRA_perform_autocommit	= 0x1000L;		// indicates autocommit is necessary
const ULONG TRA_rec_version			= 0x2000L;		// don't wait for uncommitted versions
const ULONG TRA_restart_requests	= 0x4000L;		// restart all requests in attachment
const ULONG TRA_no_auto_undo		= 0x8000L;		// don't start a savepoint in TRA_start
const ULONG TRA_precommitted		= 0x10000L;		// transaction committed at startup
const ULONG TRA_own_interface		= 0x20000L;		// tra_interface was created for internal needs
const ULONG TRA_read_consistency	= 0x40000L; 	// ensure read consistency in this transaction
const ULONG TRA_ex_restart			= 0x80000L; 	// Exception was raised to restart request
const ULONG TRA_replicating			= 0x100000L;	// transaction is allowed to be replicated
const ULONG TRA_no_blob_check		= 0x200000L;	// disable blob access checking
const ULONG TRA_auto_release_temp_blobid	= 0x400000L;	// remove temp ids of materialized user blobs from tra_blobs

// flags derived from TPB, see also transaction_options() at tra.cpp
const ULONG TRA_OPTIONS_MASK = (TRA_degree3 | TRA_readonly | TRA_ignore_limbo | TRA_read_committed |
	TRA_autocommit | TRA_rec_version | TRA_read_consistency | TRA_no_auto_undo | TRA_restart_requests | TRA_auto_release_temp_blobid);
*/
enum tra_wait_t {
	tra_no_wait,
	tra_probe,
	tra_wait
};
} // namespace Jrd

bool	TRA_active_transactions(Jrd::thread_db* tdbb, Jrd::Database*);
bool	TRA_cleanup(Jrd::thread_db*);
void	TRA_commit(Jrd::thread_db* tdbb, Jrd::jrd_tra*, const bool);
void	TRA_extend_tip(Jrd::thread_db* tdbb, ULONG /*, struct Jrd::win* */);
int		TRA_fetch_state(Jrd::thread_db* tdbb, TraNumber number);
void	TRA_get_inventory(Jrd::thread_db* tdbb, UCHAR*, TraNumber base, TraNumber top);
int		TRA_get_state(Jrd::thread_db* tdbb, TraNumber number);

#ifdef SUPERSERVER_V2
void	TRA_header_write(Jrd::thread_db* tdbb, Jrd::Database* dbb, TraNumber number);
#endif
void	TRA_init(Jrd::Attachment*);
void	TRA_invalidate(Jrd::thread_db* tdbb, ULONG);
void	TRA_link_cursor(Jrd::jrd_tra*, Jrd::DsqlCursor*);
void	TRA_unlink_cursor(Jrd::jrd_tra*, Jrd::DsqlCursor*);

void	TRA_post_resources(Jrd::thread_db* tdbb, Jrd::jrd_tra*, Jrd::Resources&);

bool	TRA_is_active(Jrd::thread_db*, TraNumber);
void	TRA_prepare(Jrd::thread_db* tdbb, Jrd::jrd_tra*, USHORT, const UCHAR*);
Jrd::jrd_tra*	TRA_reconnect(Jrd::thread_db* tdbb, const UCHAR*, USHORT);
void	TRA_release_transaction(Jrd::thread_db* tdbb, Jrd::jrd_tra*, Jrd::TraceTransactionEnd*);
void	TRA_rollback(Jrd::thread_db* tdbb, Jrd::jrd_tra*, const bool, const bool);
void	TRA_set_state(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction, TraNumber number, int state);
int		TRA_snapshot_state(Jrd::thread_db* tdbb, const Jrd::jrd_tra* trans, TraNumber number, CommitNumber* snapshot = NULL);
Jrd::jrd_tra*	TRA_start(Jrd::thread_db* tdbb, ULONG flags, SSHORT lock_timeout, Jrd::jrd_tra* outer = NULL);
Jrd::jrd_tra*	TRA_start(Jrd::thread_db* tdbb, int, const UCHAR*, Jrd::jrd_tra* outer = NULL);
int		TRA_state(const UCHAR*, TraNumber oldest, TraNumber number) noexcept;
void	TRA_sweep(Jrd::thread_db* tdbb);
void	TRA_update_counters(Jrd::thread_db*, Jrd::Database*);
int		TRA_wait(Jrd::thread_db* tdbb, Jrd::jrd_tra* trans, TraNumber number, Jrd::tra_wait_t wait);
void	TRA_attach_request(Jrd::jrd_tra* transaction, Jrd::Request* request);
void	TRA_detach_request(Jrd::Request* request);
void	TRA_setup_request_snapshot(Jrd::thread_db*, Jrd::Request* request);
void	TRA_release_request_snapshot(Jrd::thread_db*, Jrd::Request* request);
Jrd::Request* TRA_get_prior_request(Jrd::thread_db*);
void	TRA_shutdown_sweep();

#endif // JRD_TRA_PROTO_H
