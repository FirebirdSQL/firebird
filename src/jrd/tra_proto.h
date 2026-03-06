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

namespace Firebird::Jrd
{
	class Attachment;
	class Database;
	class TraceTransactionEnd;
	class DsqlCursor;
	class Request;

	class Resources;
	class thread_db;
	class jrd_tra;

	enum tra_wait_t {
		tra_no_wait,
		tra_probe,
		tra_wait
	};

	bool	TRA_active_transactions(thread_db* tdbb, Database*);
	bool	TRA_cleanup(thread_db*);
	void	TRA_commit(thread_db* tdbb, jrd_tra*, const bool);
	void	TRA_extend_tip(thread_db* tdbb, ULONG /*, struct win* */);
	int		TRA_fetch_state(thread_db* tdbb, TraNumber number);
	void	TRA_get_inventory(thread_db* tdbb, UCHAR*, TraNumber base, TraNumber top);
	int		TRA_get_state(thread_db* tdbb, TraNumber number);

	#ifdef SUPERSERVER_V2
	void	TRA_header_write(thread_db* tdbb, Database* dbb, TraNumber number);
	#endif
	void	TRA_init(Attachment*);
	void	TRA_invalidate(thread_db* tdbb, ULONG);
	void	TRA_link_cursor(jrd_tra*, DsqlCursor*);
	void	TRA_unlink_cursor(jrd_tra*, DsqlCursor*);

	void	TRA_post_resources(thread_db* tdbb, jrd_tra*, Resources&);

	bool	TRA_is_active(thread_db*, TraNumber);
	void	TRA_prepare(thread_db* tdbb, jrd_tra*, USHORT, const UCHAR*);
	jrd_tra*	TRA_reconnect(thread_db* tdbb, const UCHAR*, USHORT);
	void	TRA_release_transaction(thread_db* tdbb, jrd_tra*, TraceTransactionEnd*);
	void	TRA_rollback(thread_db* tdbb, jrd_tra*, const bool, const bool);
	void	TRA_set_state(thread_db* tdbb, jrd_tra* transaction, TraNumber number, int state);
	int		TRA_snapshot_state(thread_db* tdbb, const jrd_tra* trans, TraNumber number, CommitNumber* snapshot = NULL);
	jrd_tra*	TRA_start(thread_db* tdbb, ULONG flags, SSHORT lock_timeout, jrd_tra* outer = NULL);
	jrd_tra*	TRA_start(thread_db* tdbb, int, const UCHAR*, jrd_tra* outer = NULL);
	int		TRA_state(const UCHAR*, TraNumber oldest, TraNumber number) noexcept;
	void	TRA_sweep(thread_db* tdbb);
	void	TRA_update_counters(thread_db*, Database*);
	int		TRA_wait(thread_db* tdbb, jrd_tra* trans, TraNumber number, tra_wait_t wait);
	void	TRA_attach_request(jrd_tra* transaction, Request* request);
	void	TRA_detach_request(Request* request);
	void	TRA_setup_request_snapshot(thread_db*, Request* request);
	void	TRA_release_request_snapshot(thread_db*, Request* request);
	Request* TRA_get_prior_request(thread_db*);
	void	TRA_shutdown_sweep();
} // namespace Firebird::Jrd

#endif // JRD_TRA_PROTO_H
