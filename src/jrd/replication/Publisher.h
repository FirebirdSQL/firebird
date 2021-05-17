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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2013 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_REPLICATION_PUBLISHER_H
#define JRD_REPLICATION_PUBLISHER_H

namespace Jrd
{
	class thread_db;
	class jrd_tra;
	class Savepoint;
	struct record_param;
}

void REPL_attach(Jrd::thread_db* tdbb, bool cleanupTransactions);
void REPL_trans_prepare(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void REPL_trans_commit(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void REPL_trans_rollback(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void REPL_trans_cleanup(Jrd::thread_db* tdbb, TraNumber number);
void REPL_save_cleanup(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction,
				  	   const Jrd::Savepoint* savepoint, bool undo);
void REPL_store(Jrd::thread_db* tdbb, const Jrd::record_param* rpb,
				Jrd::jrd_tra* transaction);
void REPL_modify(Jrd::thread_db* tdbb, const Jrd::record_param* orgRpb,
				 const Jrd::record_param* newRpb, Jrd::jrd_tra* transaction);
void REPL_erase(Jrd::thread_db* tdbb, const Jrd::record_param* rpb, Jrd::jrd_tra* transaction);
void REPL_gen_id(Jrd::thread_db* tdbb, SLONG genId, SINT64 value);
void REPL_exec_sql(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction, const Firebird::string& sql);
void REPL_journal_switch(Jrd::thread_db* tdbb);

#endif // JRD_REPLICATION_PUBLISHER_H
