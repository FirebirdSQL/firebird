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

namespace Firebird::Jrd
{
	class thread_db;
	class jrd_tra;
	class Database;
	class Savepoint;
	struct record_param;
}

namespace Firebird::Jrd::Replication
{
	void REPL_attach(thread_db* tdbb, bool cleanupTransactions);
	void REPL_trans_prepare(thread_db* tdbb, jrd_tra* transaction);
	void REPL_trans_commit(thread_db* tdbb, jrd_tra* transaction);
	void REPL_trans_rollback(thread_db* tdbb, jrd_tra* transaction);
	void REPL_trans_cleanup(thread_db* tdbb, TraNumber number);
	void REPL_save_cleanup(thread_db* tdbb, jrd_tra* transaction,
						const Savepoint* savepoint, bool undo);
	void REPL_store(thread_db* tdbb, const record_param* rpb,
					jrd_tra* transaction);
	void REPL_modify(thread_db* tdbb, const record_param* orgRpb,
					const record_param* newRpb, jrd_tra* transaction);
	void REPL_erase(thread_db* tdbb, const record_param* rpb, jrd_tra* transaction);
	void REPL_gen_id(thread_db* tdbb, SLONG genId, SINT64 value, jrd_tra* transaction);
	void REPL_exec_sql(thread_db* tdbb, jrd_tra* transaction, const string& sql,
		const ObjectsArray<MetaString>& schemaSearchPath);
	void REPL_journal_switch(thread_db* tdbb);
	void REPL_journal_cleanup(Database* dbb);
} // namespace Firebird::Jrd::Replication

#endif // JRD_REPLICATION_PUBLISHER_H
