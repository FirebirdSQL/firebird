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

#include "../jrd/tra.h"

namespace Jrd {
	class Attachment;
	class Database;
	class TraceTransactionEnd;
}

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
void	TRA_post_resources(Jrd::thread_db* tdbb, Jrd::jrd_tra*, Jrd::ResourceList&);
bool	TRA_is_active(Jrd::thread_db*, TraNumber);
void	TRA_prepare(Jrd::thread_db* tdbb, Jrd::jrd_tra*, USHORT, const UCHAR*);
Jrd::jrd_tra*	TRA_reconnect(Jrd::thread_db* tdbb, const UCHAR*, USHORT);
void	TRA_release_transaction(Jrd::thread_db* tdbb, Jrd::jrd_tra*, Jrd::TraceTransactionEnd*);
void	TRA_rollback(Jrd::thread_db* tdbb, Jrd::jrd_tra*, const bool, const bool);
void	TRA_set_state(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction, TraNumber number, int state);
int		TRA_snapshot_state(Jrd::thread_db* tdbb, const Jrd::jrd_tra* trans, TraNumber number, CommitNumber* snapshot = NULL);
Jrd::jrd_tra*	TRA_start(Jrd::thread_db* tdbb, ULONG flags, SSHORT lock_timeout, Jrd::jrd_tra* outer = NULL);
Jrd::jrd_tra*	TRA_start(Jrd::thread_db* tdbb, int, const UCHAR*, Jrd::jrd_tra* outer = NULL);
int		TRA_state(const UCHAR*, TraNumber oldest, TraNumber number);
void	TRA_sweep(Jrd::thread_db* tdbb);
void	TRA_update_counters(Jrd::thread_db*, Jrd::Database*);
int		TRA_wait(Jrd::thread_db* tdbb, Jrd::jrd_tra* trans, TraNumber number, Jrd::jrd_tra::wait_t wait);
void	TRA_attach_request(Jrd::jrd_tra* transaction, Jrd::jrd_req* request);
void	TRA_detach_request(Jrd::jrd_req* request);
void	TRA_setup_request_snapshot(Jrd::thread_db*, Jrd::jrd_req* request, bool no_prior = false);
void	TRA_release_request_snapshot(Jrd::thread_db*, Jrd::jrd_req* request);
Jrd::jrd_req* TRA_get_prior_request(Jrd::thread_db*);

#endif // JRD_TRA_PROTO_H
