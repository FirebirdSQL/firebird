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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "UdrCppExample.h"

using namespace Firebird;


//------------------------------------------------------------------------------


/***
Sample usage:

create database 'c:\temp\replica.fdb';
create table persons (
    id integer not null,
    name varchar(60) not null,
    address varchar(60),
    info blob sub_type text
);
commit;

create database 'c:\temp\main.fdb';
create table persons (
    id integer not null,
    name varchar(60) not null,
    address varchar(60),
    info blob sub_type text
);

create table replicate_config (
    name varchar(31) not null,
    data_source varchar(255) not null
);

insert into replicate_config (name, data_source)
   values ('ds1', 'c:\temp\replica.fdb');

create trigger persons_replicate
    after insert on persons
    external name 'udrcpp_example!replicate!ds1'
    engine udr;

create trigger persons_replicate2
    after insert on persons
    external name 'udrcpp_example!replicate_persons!ds1'
    engine udr;
***/
FB_UDR_BEGIN_TRIGGER(replicate)
	// Without FieldsMessage definition, messages will be byte-based.

	FB_UDR_CONSTRUCTOR
		, triggerMetadata(metadata->getTriggerMetadata(status))
	{
		ISC_STATUS_ARRAY statusVector = {0};
		isc_db_handle dbHandle = Helper::getIscDbHandle(status, context);
		isc_tr_handle trHandle = Helper::getIscTrHandle(status, context);

		isc_stmt_handle stmtHandle = 0;
		FbException::check(isc_dsql_allocate_statement(
			statusVector, &dbHandle, &stmtHandle), status, statusVector);
		FbException::check(isc_dsql_prepare(statusVector, &trHandle, &stmtHandle, 0,
			"select data_source from replicate_config where name = ?",
			SQL_DIALECT_CURRENT, NULL), status, statusVector);

		const char* table = metadata->getTriggerTable(status);

		// Skip the first exclamation point, separating the module name and entry point.
		const char* info = strchr(metadata->getEntryPoint(status), '!');

		// Skip the second exclamation point, separating the entry point and the misc info (config).
		if (info)
			info = strchr(info + 1, '!');

		if (info)
			++info;
		else
			info = "";

		XSQLDA* inSqlDa = reinterpret_cast<XSQLDA*>(new char[(XSQLDA_LENGTH(1))]);
		inSqlDa->version = SQLDA_VERSION1;
		inSqlDa->sqln = 1;
		FbException::check(isc_dsql_describe_bind(statusVector, &stmtHandle,
			SQL_DIALECT_CURRENT, inSqlDa), status, statusVector);
		inSqlDa->sqlvar[0].sqldata = new char[sizeof(short) + inSqlDa->sqlvar[0].sqllen];
		strncpy(inSqlDa->sqlvar[0].sqldata + sizeof(short), info, inSqlDa->sqlvar[0].sqllen);
		*reinterpret_cast<short*>(inSqlDa->sqlvar[0].sqldata) = strlen(info);

		XSQLDA* outSqlDa = reinterpret_cast<XSQLDA*>(new char[(XSQLDA_LENGTH(1))]);
		outSqlDa->version = SQLDA_VERSION1;
		outSqlDa->sqln = 1;
		FbException::check(isc_dsql_describe(statusVector, &stmtHandle,
			SQL_DIALECT_CURRENT, outSqlDa), status, statusVector);
		outSqlDa->sqlvar[0].sqldata = new char[sizeof(short) + outSqlDa->sqlvar[0].sqllen + 1];
		outSqlDa->sqlvar[0].sqldata[sizeof(short) + outSqlDa->sqlvar[0].sqllen] = '\0';

		FbException::check(isc_dsql_execute2(statusVector, &trHandle, &stmtHandle,
			SQL_DIALECT_CURRENT, inSqlDa, outSqlDa), status, statusVector);
		FbException::check(isc_dsql_free_statement(
			statusVector, &stmtHandle, DSQL_unprepare), status, statusVector);

		delete [] inSqlDa->sqlvar[0].sqldata;
		delete [] reinterpret_cast<char*>(inSqlDa);

		unsigned count = triggerMetadata->getCount(status);

		char buffer[65536];
		strcpy(buffer, "execute block (\n");

		for (unsigned i = 0; i < count; ++i)
		{
			if (i > 0)
				strcat(buffer, ",\n");

			const char* name = triggerMetadata->getField(status, i);

			strcat(buffer, "    p");
			sprintf(buffer + strlen(buffer), "%d type of column \"%s\".\"%s\" = ?", i, table, name);
		}

		strcat(buffer,
			")\n"
			"as\n"
			"begin\n"
			"    execute statement ('insert into \"");

		strcat(buffer, table);
		strcat(buffer, "\" (");

		for (unsigned i = 0; i < count; ++i)
		{
			if (i > 0)
				strcat(buffer, ", ");

			const char* name = triggerMetadata->getField(status, i);

			strcat(buffer, "\"");
			strcat(buffer, name);
			strcat(buffer, "\"");
		}

		strcat(buffer, ") values (");

		for (unsigned i = 0; i < count; ++i)
		{
			if (i > 0)
				strcat(buffer, ", ");
			strcat(buffer, "?");
		}

		strcat(buffer, ")') (");

		for (unsigned i = 0; i < count; ++i)
		{
			if (i > 0)
				strcat(buffer, ", ");
			strcat(buffer, ":p");
			sprintf(buffer + strlen(buffer), "%d", i);
		}

		strcat(buffer, ")\n        on external data source '");
		strcat(buffer, outSqlDa->sqlvar[0].sqldata + sizeof(short));
		strcat(buffer, "';\nend");

		AutoRelease<IAttachment> attachment(context->getAttachment(status));
		AutoRelease<ITransaction> transaction(context->getTransaction(status));

		stmt.reset(attachment->prepare(status, transaction, 0, buffer, SQL_DIALECT_CURRENT, 0));

		delete [] outSqlDa->sqlvar[0].sqldata;
		delete [] reinterpret_cast<char*>(outSqlDa);
	}

	/***
	FB_UDR_DESTRUCTOR
	{
	}
	***/

	FB_UDR_EXECUTE_TRIGGER
	{
		AutoRelease<ITransaction> transaction(context->getTransaction(status));

		// This will not work if the table has computed fields.
		stmt->execute(status, transaction, triggerMetadata, newFields, NULL, NULL);
	}

	AutoRelease<IMessageMetadata> triggerMetadata;
	AutoRelease<IStatement> stmt;
FB_UDR_END_TRIGGER


FB_UDR_BEGIN_TRIGGER(replicate_persons)
	// Order of fields does not need to match the fields order in the table, but it should match
	// the order of fields in the SQL command constructed in the initialization.
	FB_UDR_TRIGGER_MESSAGE(FieldsMessage,
		(FB_INTEGER, id, "ID")
		(FB_BLOB, info, "INFO")
		///(FB_VARCHAR(60 * 4), address, "ADDRESS")
		(FB_VARCHAR(60 * 4), name, "NAME")
	);

	FB_UDR_CONSTRUCTOR
		, triggerMetadata(metadata->getTriggerMetadata(status))
	{
		ISC_STATUS_ARRAY statusVector = {0};
		isc_db_handle dbHandle = Helper::getIscDbHandle(status, context);
		isc_tr_handle trHandle = Helper::getIscTrHandle(status, context);

		isc_stmt_handle stmtHandle = 0;
		FbException::check(isc_dsql_allocate_statement(
			statusVector, &dbHandle, &stmtHandle), status, statusVector);
		FbException::check(isc_dsql_prepare(statusVector, &trHandle, &stmtHandle, 0,
			"select data_source from replicate_config where name = ?",
			SQL_DIALECT_CURRENT, NULL), status, statusVector);

		const char* table = metadata->getTriggerTable(status);

		// Skip the first exclamation point, separating the module name and entry point.
		const char* info = strchr(metadata->getEntryPoint(status), '!');

		// Skip the second exclamation point, separating the entry point and the misc info (config).
		if (info)
			info = strchr(info + 1, '!');

		if (info)
			++info;
		else
			info = "";

		XSQLDA* inSqlDa = reinterpret_cast<XSQLDA*>(new char[(XSQLDA_LENGTH(1))]);
		inSqlDa->version = SQLDA_VERSION1;
		inSqlDa->sqln = 1;
		FbException::check(isc_dsql_describe_bind(
			statusVector, &stmtHandle, SQL_DIALECT_CURRENT, inSqlDa), status, statusVector);
		inSqlDa->sqlvar[0].sqldata = new char[sizeof(short) + inSqlDa->sqlvar[0].sqllen];
		strncpy(inSqlDa->sqlvar[0].sqldata + sizeof(short), info, inSqlDa->sqlvar[0].sqllen);
		*reinterpret_cast<short*>(inSqlDa->sqlvar[0].sqldata) = strlen(info);

		XSQLDA* outSqlDa = reinterpret_cast<XSQLDA*>(new char[(XSQLDA_LENGTH(1))]);
		outSqlDa->version = SQLDA_VERSION1;
		outSqlDa->sqln = 1;
		FbException::check(isc_dsql_describe(
			statusVector, &stmtHandle, SQL_DIALECT_CURRENT, outSqlDa), status, statusVector);
		outSqlDa->sqlvar[0].sqldata = new char[sizeof(short) + outSqlDa->sqlvar[0].sqllen + 1];
		outSqlDa->sqlvar[0].sqldata[sizeof(short) + outSqlDa->sqlvar[0].sqllen] = '\0';

		FbException::check(isc_dsql_execute2(statusVector, &trHandle, &stmtHandle,
			SQL_DIALECT_CURRENT, inSqlDa, outSqlDa), status, statusVector);
		FbException::check(isc_dsql_free_statement(
			statusVector, &stmtHandle, DSQL_unprepare), status, statusVector);

		delete [] inSqlDa->sqlvar[0].sqldata;
		delete [] reinterpret_cast<char*>(inSqlDa);

		char buffer[65536];
		strcpy(buffer,
			"execute block (\n"
			"    id type of column PERSONS.ID = ?,\n"
			"    info type of column PERSONS.INFO = ?,\n"
			///"    address type of column PERSONS.ADDRESS = ?,\n"
			"    name type of column PERSONS.NAME = ?\n"
			")"
			"as\n"
			"begin\n"
			"    execute statement ('insert into persons (id, name/***, address***/, info)\n"
			"        values (?, ?/***, ?***/, ?)') (:id, :name/***, :address***/, :info)\n"
			"        on external data source '");
		strcat(buffer, outSqlDa->sqlvar[0].sqldata + sizeof(short));
		strcat(buffer, "';\nend");

		AutoRelease<IAttachment> attachment(context->getAttachment(status));
		AutoRelease<ITransaction> transaction(context->getTransaction(status));

		stmt.reset(attachment->prepare(status, transaction, 0, buffer, SQL_DIALECT_CURRENT, 0));

		delete [] outSqlDa->sqlvar[0].sqldata;
		delete [] reinterpret_cast<char*>(outSqlDa);
	}

	/***
	FB_UDR_DESTRUCTOR
	{
	}
	***/

	FB_UDR_EXECUTE_TRIGGER
	{
		AutoRelease<ITransaction> transaction(context->getTransaction(status));

		stmt->execute(status, transaction, triggerMetadata, newFields, NULL, NULL);
	}

	AutoRelease<IMessageMetadata> triggerMetadata;
	AutoRelease<IStatement> stmt;
FB_UDR_END_TRIGGER
