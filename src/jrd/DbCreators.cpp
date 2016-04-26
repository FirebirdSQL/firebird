/*
 *	PROGRAM:		JRD access method
 *	MODULE:			DbCreators.cpp
 *	DESCRIPTION:	Checks CREATE DATABASE right (in security.db)
 *
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "firebird/Interface.h"
#include "../auth/SecureRemotePassword/Message.h"
#include "gen/iberror.h"

#include "../jrd/constants.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/init.h"
#include "../common/classes/Hash.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/RefMutex.h"
#include "../common/classes/SyncObject.h"
#include "../common/classes/MetaName.h"
#include "../common/isc_s_proto.h"
#include "../common/isc_proto.h"
#include "../common/ThreadStart.h"
#include "../common/db_alias.h"

#include "../jrd/DbCreators.h"
#include "../jrd/tra.h"
#include "../jrd/ini.h"
#include "../jrd/status.h"
#include "gen/ids.h"

#define DBC_DEBUG(A)

using namespace Firebird;
using namespace Jrd;

namespace {

void check(const char* s, IStatus* st)
{
	if (!(st->getState() & IStatus::STATE_ERRORS))
		return;

	Arg::StatusVector newStatus(st);
	newStatus << Arg::Gds(isc_crdb_load) << s;
	newStatus.raise();
}

bool openDb(const char* securityDb, RefPtr<IAttachment>& att, RefPtr<ITransaction>& tra)
{
	DispatcherPtr prov;

	ClumpletWriter embeddedSysdba(ClumpletWriter::Tagged, MAX_DPB_SIZE, isc_dpb_version1);
	embeddedSysdba.insertString(isc_dpb_user_name, SYSDBA_USER_NAME, fb_strlen(SYSDBA_USER_NAME));
	embeddedSysdba.insertByte(isc_dpb_sec_attach, TRUE);
	embeddedSysdba.insertByte(isc_dpb_no_db_triggers, TRUE);

	FbLocalStatus st;
	att = prov->attachDatabase(&st, securityDb,
		embeddedSysdba.getBufferLength(), embeddedSysdba.getBuffer());
	if (st->getState() & IStatus::STATE_ERRORS)
	{
		if (!fb_utils::containsErrorCode(st->getErrors(), isc_io_error))
			check("IProvider::attachDatabase", &st);

		// missing security DB - checking granted rights not possible
		return false;
	}

	ClumpletWriter readOnly(ClumpletWriter::Tpb, MAX_DPB_SIZE, isc_tpb_version1);
	readOnly.insertTag(isc_tpb_read);
	readOnly.insertTag(isc_tpb_wait);
	tra = att->startTransaction(&st, readOnly.getBufferLength(), readOnly.getBuffer());
	check("IAttachment::startTransaction", &st);

	return true;
}

} // anonymous namespace


namespace Jrd {

bool checkCreateDatabaseGrant(const MetaName& userName, const MetaName& trustedRole,
	const MetaName& sqlRole, const char* securityDb)
{
	if (userName == SYSDBA_USER_NAME)
		return true;

	RefPtr<IAttachment> att;
	RefPtr<ITransaction> tra;
	if (!openDb(securityDb, att, tra))
		return false;

	FbLocalStatus st;
	MetaName role(sqlRole);
	if (role.hasData())
	{
		const UCHAR info[] = { isc_info_db_sql_dialect, isc_info_end };
		UCHAR buffer[BUFFER_TINY];
		att->getInfo(&st, sizeof(info), info, sizeof(buffer), buffer);
		check("IAttachment::getInfo", &st);

		int dialect = SQL_DIALECT_V5;		// reasonable default
		const UCHAR* p = buffer;
		while (*p != isc_info_end && *p != isc_info_truncated && p < buffer + sizeof(buffer))
		{
			const UCHAR item = (UCHAR) *p++;
			const USHORT length = gds__vax_integer(p, sizeof(USHORT));
			p += sizeof(USHORT);

			switch (item)
			{
			case isc_info_db_sql_dialect:
				dialect = gds__vax_integer(p, length);
				break;
			}

			p += length;
		}

		JRD_make_role_name(role, dialect);

		// We need to check is admin role granted to userName in security DB
		const char* sql = "select count(*) from RDB$USER_PRIVILEGES "
			"where RDB$USER = ? and RDB$RELATION_NAME = ? and RDB$PRIVILEGE = 'M'";

		Message prm;
		Field<Varying> u(prm, MAX_SQL_IDENTIFIER_LEN);
		Field<Varying> r(prm, MAX_SQL_IDENTIFIER_LEN);
		u = userName.c_str();
		r = role.c_str();

		Message result;
		Field<ISC_INT64> cnt(result);

		att->execute(&st, tra, 0, sql, SQL_DIALECT_V6, prm.getMetadata(), prm.getBuffer(),
			result.getMetadata(), result.getBuffer());

		if (st->getState() & IStatus::STATE_ERRORS)
		{
			// isc_dsql_relation_err when exec SQL - i.e. table RDB$USER_PRIVILEGES
			// is missing due to non-FB security DB
			if (!fb_utils::containsErrorCode(st->getErrors(), isc_dsql_relation_err))
				check("IAttachment::execute", &st);

			role = "";
		}
		else if (cnt == 0)
			role = "";
	}
	else
		role = trustedRole;

	if (role == ADMIN_ROLE)
		return true;

	Message gr;
	Field<ISC_SHORT> uType(gr);
	Field<Varying> u(gr, MAX_SQL_IDENTIFIER_LEN);
	Field<ISC_SHORT> rType(gr);
	Field<Varying> r(gr, MAX_SQL_IDENTIFIER_LEN);
	uType = obj_user;
	u = userName.c_str();
	rType = role.hasData() ? obj_sql_role : 255;
	r = role.c_str();

	Message result;
	Field<ISC_INT64> cnt(result);

	att->execute(&st, tra, 0,
		"select count(*) from RDB$DB_CREATORS"
		" where (RDB$USER_TYPE = ? and RDB$USER = ?) or (RDB$USER_TYPE = ? and RDB$USER = ?)",
		SQL_DIALECT_V6, gr.getMetadata(), gr.getBuffer(), result.getMetadata(), result.getBuffer());
	if (st->getState() & IStatus::STATE_ERRORS)
	{
		if (fb_utils::containsErrorCode(st->getErrors(), isc_dsql_relation_err))
		{
			// isc_dsql_relation_err when exec SQL - i.e. table RDB$DB_CREATORS
			// is missing due to non-FB3 security DB
			return false;
		}
		check("IAttachment::execute", &st);
	}

	return cnt > 0;
}


const Format* DbCreatorsScan::getFormat(thread_db* tdbb, jrd_rel* relation) const
{
	jrd_tra* const transaction = tdbb->getTransaction();
	return transaction->getDbCreatorsList()->getList(tdbb, relation)->getFormat();
}

bool DbCreatorsScan::retrieveRecord(thread_db* tdbb, jrd_rel* relation,
									FB_UINT64 position, Record* record) const
{
	jrd_tra* const transaction = tdbb->getTransaction();
	return transaction->getDbCreatorsList()->getList(tdbb, relation)->fetch(position, record);
}

DbCreatorsList::DbCreatorsList(jrd_tra* tra)
	: SnapshotData(*tra->tra_pool)
{ }

RecordBuffer* DbCreatorsList::makeBuffer(thread_db* tdbb)
{
	MemoryPool* const pool = tdbb->getTransaction()->tra_pool;
	allocBuffer(tdbb, *pool, rel_sec_db_creators);
	return getData(rel_sec_db_creators);
}

RecordBuffer* DbCreatorsList::getList(thread_db* tdbb, jrd_rel* relation)
{
	fb_assert(relation);
	fb_assert(relation->rel_id == rel_sec_db_creators);

	RecordBuffer* buffer = getData(relation);
	if (buffer)
	{
		return buffer;
	}

	RefPtr<IAttachment> att;
	RefPtr<ITransaction> tra;
	const char* dbName = tdbb->getDatabase()->dbb_config->getSecurityDatabase();
	if (!openDb(dbName, att, tra))
	{
		// In embedded mode we are not raising any errors - silent return
		if (MasterInterfacePtr()->serverMode(-1) < 0)
			return makeBuffer(tdbb);

		(Arg::Gds(isc_crdb_nodb) << dbName).raise();
	}

	Message gr;
	Field<ISC_SHORT> uType(gr);
	Field<Varying> u(gr, MAX_SQL_IDENTIFIER_LEN);

	FbLocalStatus st;
	RefPtr<IResultSet> curs(att->openCursor(&st, tra, 0,
		"select RDB$USER_TYPE, RDB$USER from RDB$DB_CREATORS",
		SQL_DIALECT_V6, NULL, NULL, gr.getMetadata(), NULL, 0));

	if (st->getState() & IStatus::STATE_ERRORS)
	{
		if (!fb_utils::containsErrorCode(st->getErrors(), isc_dsql_relation_err))
			check("IAttachment::openCursor", &st);

		// isc_dsql_relation_err when opening cursor - i.e. table
		// is missing due to non-FB3 security DB

		// In embedded mode we are not raising any errors - silent return
		if (MasterInterfacePtr()->serverMode(-1) < 0)
			return makeBuffer(tdbb);

		(Arg::Gds(isc_crdb_notable) << dbName).raise();
	}

	try
	{
		buffer = makeBuffer(tdbb);
		while (curs->fetchNext(&st, gr.getBuffer()) == IStatus::RESULT_OK)
		{
			Record* record = buffer->getTempRecord();
			record->nullify();

			putField(tdbb, record,
					 DumpField(f_sec_crt_user, VALUE_STRING, u->len, u->data));

			SINT64 v = uType;
			putField(tdbb, record,
					 DumpField(f_sec_crt_u_type, VALUE_INTEGER, sizeof(v), &v));

			buffer->store(record);
		}
		check("IResultSet::fetchNext", &st);
	}
	catch (const Exception&)
	{
		clearSnapshot();
		throw;
	}

	return getData(relation);
}

} // namespace Jrd
