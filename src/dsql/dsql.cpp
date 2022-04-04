/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		dsql.cpp
 *	DESCRIPTION:	Local processing for External entry points.
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
 * December 2001 Mike Nordell: Major overhaul to (try to) make it C++
 * 2001.6.3 Claudio Valderrama: fixed a bad behaved loop in get_plan_info()
 * and get_rsb_item() that caused a crash when plan info was requested.
 * 2001.6.9 Claudio Valderrama: Added nod_del_view, nod_current_role and nod_breakleave.
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 * 2004.01.16 Vlad Horsun: added support for EXECUTE BLOCK statement
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../dsql/dsql.h"
#include "ibase.h"
#include "../jrd/align.h"
#include "../jrd/intl.h"
#include "../common/intlobj_new.h"
#include "../jrd/jrd.h"
#include "../jrd/status.h"
#include "../common/CharSet.h"
#include "../dsql/Parser.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/dsql_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/movd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../dsql/metd_proto.h"
#include "../jrd/DataTypeUtil.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/ini_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/replication/Publisher.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceDSQLHelpers.h"
#include "../common/classes/init.h"
#include "../common/utils_proto.h"
#include "../common/StatusArg.h"
#include "../dsql/DsqlBatch.h"

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

using namespace Jrd;
using namespace Firebird;


static ULONG	get_request_info(thread_db*, DsqlRequest*, ULONG, UCHAR*);
static dsql_dbb*	init(Jrd::thread_db*, Jrd::Attachment*);
static DsqlRequest* prepareRequest(thread_db*, dsql_dbb*, jrd_tra*, ULONG, const TEXT*, USHORT, bool);
static RefPtr<DsqlStatement> prepareStatement(thread_db*, dsql_dbb*, jrd_tra*, ULONG, const TEXT*, USHORT,
	bool, ntrace_result_t* traceResult);
static UCHAR*	put_item(UCHAR, const USHORT, const UCHAR*, UCHAR*, const UCHAR* const);
static void		sql_info(thread_db*, DsqlRequest*, ULONG, const UCHAR*, ULONG, UCHAR*);
static UCHAR*	var_info(const dsql_msg*, const UCHAR*, const UCHAR* const, UCHAR*,
	const UCHAR* const, USHORT, bool);

#ifdef DSQL_DEBUG
unsigned DSQL_debug = 0;
#endif

namespace
{
	const UCHAR record_info[] =
	{
		isc_info_req_update_count, isc_info_req_delete_count,
		isc_info_req_select_count, isc_info_req_insert_count
	};
}	// namespace


#ifdef DSQL_DEBUG
IMPLEMENT_TRACE_ROUTINE(dsql_trace, "DSQL")
#endif

dsql_dbb::~dsql_dbb()
{
}


// Execute a dynamic SQL statement.
void DSQL_execute(thread_db* tdbb,
			  	  jrd_tra** tra_handle,
				  DsqlRequest* dsqlRequest,
				  IMessageMetadata* in_meta, const UCHAR* in_msg,
				  IMessageMetadata* out_meta, UCHAR* out_msg)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());

	const auto statement = dsqlRequest->getDsqlStatement();

	if (statement->getFlags() & DsqlStatement::FLAG_ORPHAN)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
		          Arg::Gds(isc_bad_req_handle));
	}

	// Only allow NULL trans_handle if we're starting a transaction or set session properties

	if (!*tra_handle &&
		statement->getType() != DsqlStatement::TYPE_START_TRANS &&
		statement->getType() != DsqlStatement::TYPE_SESSION_MANAGEMENT)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
				  Arg::Gds(isc_bad_trans_handle));
	}

	// A select with a non zero output length is a singleton select
	const bool singleton = statement->isCursorBased() && out_msg;

	// If the request is a SELECT or blob statement then this is an open.
	// Make sure the cursor is not already open.

	if (statement->isCursorBased())
	{
		if (dsqlRequest->req_cursor)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
					  Arg::Gds(isc_dsql_cursor_open_err));
		}

		if (!singleton)
			(Arg::Gds(isc_random) << "Cannot execute SELECT statement").raise();
	}

	dsqlRequest->req_transaction = *tra_handle;
	dsqlRequest->execute(tdbb, tra_handle, in_meta, in_msg, out_meta, out_msg, singleton);
}


/**

 	DSQL_free_statement

    @brief	Release request for a dsql statement


    @param user_status
    @param req_handle
    @param option

 **/
void DSQL_free_statement(thread_db* tdbb, DsqlRequest* dsqlRequest, USHORT option)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());

	const auto dsqlStatement = dsqlRequest->getDsqlStatement();

	fb_assert(!(option & DSQL_unprepare));	// handled in y-valve

	if (option & DSQL_drop)
	{
		// Release everything associated with the request
		DsqlRequest::destroy(tdbb, dsqlRequest);
	}
	else if (option & DSQL_close)
	{
		// Just close the cursor associated with the request
		if (dsqlStatement->isCursorBased())
		{
			if (!dsqlRequest->req_cursor)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-501) <<
						  Arg::Gds(isc_dsql_cursor_close_err));
			}

			DsqlCursor::close(tdbb, dsqlRequest->req_cursor);
		}
	}
}


/**

 	DSQL_prepare

    @brief	Prepare a statement for execution.


    @param user_status
    @param trans_handle
    @param req_handle
    @param length
    @param string
    @param dialect
    @param item_length
    @param items
    @param buffer_length
    @param buffer

 **/
DsqlRequest* DSQL_prepare(thread_db* tdbb,
					   Attachment* attachment, jrd_tra* transaction,
					   ULONG length, const TEXT* string, USHORT dialect, unsigned prepareFlags,
					   Array<UCHAR>* items, Array<UCHAR>* buffer,
					   bool isInternalRequest)
{
	SET_TDBB(tdbb);

	dsql_dbb* database = init(tdbb, attachment);
	DsqlRequest* dsqlRequest = NULL;

	try
	{
		// Allocate a new request block and then prepare the request.

		dsqlRequest = prepareRequest(tdbb, database, transaction, length, string, dialect,
			isInternalRequest);

		// Can not prepare a CREATE DATABASE/SCHEMA statement

		const auto statement = dsqlRequest->getDsqlStatement();
		if (statement->getType() == DsqlStatement::TYPE_CREATE_DB)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-530) <<
					  Arg::Gds(isc_dsql_crdb_prepare_err));
		}

		if (items && buffer)
		{
			Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());
			sql_info(tdbb, dsqlRequest, items->getCount(), items->begin(),
				buffer->getCount(), buffer->begin());
		}

		return dsqlRequest;
	}
	catch (const Exception&)
	{
		if (dsqlRequest)
		{
			Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());
			DsqlRequest::destroy(tdbb, dsqlRequest);
		}
		throw;
	}
}


/**

 	DSQL_sql_info

    @brief	Provide information on dsql statement


    @param user_status
    @param req_handle
    @param item_length
    @param items
    @param info_length
    @param info

 **/
void DSQL_sql_info(thread_db* tdbb,
				   DsqlRequest* dsqlRequest,
				   ULONG item_length, const UCHAR* items,
				   ULONG info_length, UCHAR* info)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());

	sql_info(tdbb, dsqlRequest, item_length, items, info_length, info);
}


// Common part of prepare and execute a statement.
void DSQL_execute_immediate(thread_db* tdbb, Jrd::Attachment* attachment, jrd_tra** tra_handle,
	ULONG length, const TEXT* string, USHORT dialect,
	IMessageMetadata* in_meta, const UCHAR* in_msg,
	IMessageMetadata* out_meta, UCHAR* out_msg,
	bool isInternalRequest)
{
	SET_TDBB(tdbb);

	dsql_dbb* const database = init(tdbb, attachment);
	DsqlRequest* dsqlRequest = NULL;

	try
	{
		dsqlRequest = prepareRequest(tdbb, database, *tra_handle, length, string, dialect,
			isInternalRequest);

		const auto dsqlStatement = dsqlRequest->getDsqlStatement();

		// Only allow NULL trans_handle if we're starting a transaction or set session properties

		if (!*tra_handle &&
			dsqlStatement->getType() != DsqlStatement::TYPE_START_TRANS &&
			dsqlStatement->getType() != DsqlStatement::TYPE_SESSION_MANAGEMENT)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
					  Arg::Gds(isc_bad_trans_handle));
		}

		Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());

		// A select having cursor is a singleton select when executed immediate
		const bool singleton = dsqlStatement->isCursorBased();
		if (singleton && !(out_msg && out_meta))
		{
			ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
					  Arg::Gds(isc_dsql_no_output_sqlda));
		}

		dsqlRequest->req_transaction = *tra_handle;

		dsqlRequest->execute(tdbb, tra_handle, in_meta, in_msg, out_meta, out_msg, singleton);

		DsqlRequest::destroy(tdbb, dsqlRequest);
	}
	catch (const Exception&)
	{
		if (dsqlRequest)
		{
			Jrd::ContextPoolHolder context(tdbb, &dsqlRequest->getPool());
			DsqlRequest::destroy(tdbb, dsqlRequest);
		}
		throw;
	}
}


/**

 	get_request_info

    @brief	Get the records updated/deleted for record


    @param request
    @param buffer_length
    @param buffer

 **/
static ULONG get_request_info(thread_db* tdbb, DsqlRequest* dsqlRequest, ULONG buffer_length, UCHAR* buffer)
{
	if (!dsqlRequest->getRequest())	// DDL
		return 0;

	// get the info for the request from the engine

	try
	{
		return INF_request_info(dsqlRequest->getRequest(), sizeof(record_info), record_info,
			buffer_length, buffer);
	}
	catch (Exception&)
	{
		return 0;
	}
}


/**

 	init

    @brief	Initialize dynamic SQL.  This is called only once.


    @param db_handle

 **/
static dsql_dbb* init(thread_db* tdbb, Jrd::Attachment* attachment)
{
	SET_TDBB(tdbb);

	if (attachment->att_dsql_instance)
		return attachment->att_dsql_instance;

	MemoryPool& pool = *attachment->createPool();
	dsql_dbb* const database = FB_NEW_POOL(pool) dsql_dbb(pool);
	database->dbb_attachment = attachment;
	attachment->att_dsql_instance = database;

	INI_init_dsql(tdbb, database);

#ifdef DSQL_DEBUG
	DSQL_debug = Config::getTraceDSQL();
#endif

	return attachment->att_dsql_instance;
}


// Prepare a request for execution.
// Note: caller is responsible for pool handling.
static DsqlRequest* prepareRequest(thread_db* tdbb, dsql_dbb* database, jrd_tra* transaction,
	ULONG textLength, const TEXT* text, USHORT clientDialect, bool isInternalRequest)
{
	TraceDSQLPrepare trace(database->dbb_attachment, transaction, textLength, text);

	ntrace_result_t traceResult = ITracePlugin::RESULT_SUCCESS;
	try
	{
		auto statement = prepareStatement(tdbb, database, transaction, textLength, text,
			clientDialect, isInternalRequest, &traceResult);

		auto dsqlRequest = statement->createRequest(tdbb, database);

		dsqlRequest->req_traced = true;
		trace.setStatement(dsqlRequest);
		trace.prepare(traceResult);

		return dsqlRequest;
	}
	catch (const Exception&)
	{
		trace.prepare(ITracePlugin::RESULT_FAILED);
		throw;
	}
}


// Prepare a statement for execution.
// Note: caller is responsible for pool handling.
static RefPtr<DsqlStatement> prepareStatement(thread_db* tdbb, dsql_dbb* database, jrd_tra* transaction,
	ULONG textLength, const TEXT* text, USHORT clientDialect, bool isInternalRequest, ntrace_result_t* traceResult)
{
	Database* const dbb = tdbb->getDatabase();

	if (text && textLength == 0)
		textLength = static_cast<ULONG>(strlen(text));

	if (clientDialect > SQL_DIALECT_CURRENT)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
				  Arg::Gds(isc_wish_list));
	}

	if (!text || textLength == 0)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Unexpected end of command
				  // CVC: Nothing will be line 1, column 1 for the user.
				  Arg::Gds(isc_command_end_err2) << Arg::Num(1) << Arg::Num(1));
	}

	// Get rid of the trailing ";" if there is one.

	for (const TEXT* p = text + textLength; p-- > text;)
	{
		if (*p != ' ')
		{
			if (*p == ';')
				textLength = p - text;
			break;
		}
	}

	if (textLength > MAX_SQL_LENGTH)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-902) <<
				  Arg::Gds(isc_imp_exc) <<
				  Arg::Gds(isc_sql_too_long) << Arg::Num(MAX_SQL_LENGTH));
	}

	// allocate the statement block, then prepare the statement

	MemoryPool* scratchPool = nullptr;
	DsqlCompilerScratch* scratch = nullptr;
	MemoryPool* statementPool = database->createPool();
	RefPtr<DsqlStatement> dsqlStatement;

	Jrd::ContextPoolHolder statementContext(tdbb, statementPool);
	try
	{
		scratchPool = database->createPool();

		if (!transaction)		// Useful for session management statements
			transaction = database->dbb_attachment->getSysTransaction();

		const auto dbDialect =
			(dbb->dbb_flags & DBB_DB_SQL_dialect_3) ? SQL_DIALECT_V6 : SQL_DIALECT_V5;

		const auto charSetId = database->dbb_attachment->att_charset;

		string transformedText;

		{	// scope to delete parser before the scratch pool is gone
			Jrd::ContextPoolHolder scratchContext(tdbb, scratchPool);

			scratch = FB_NEW_POOL(*scratchPool) DsqlCompilerScratch(*scratchPool, database, transaction);
			scratch->clientDialect = clientDialect;

			if (isInternalRequest)
				scratch->flags |= DsqlCompilerScratch::FLAG_INTERNAL_REQUEST;

			Parser parser(tdbb, *scratchPool, statementPool, scratch, clientDialect,
				dbDialect, text, textLength, charSetId);

			// Parse the SQL statement.  If it croaks, return
			dsqlStatement = parser.parse();

			scratch->setDsqlStatement(dsqlStatement);

			if (parser.isStmtAmbiguous())
				scratch->flags |= DsqlCompilerScratch::FLAG_AMBIGUOUS_STMT;

			transformedText = parser.getTransformedString();
		}

		// If the attachment charset is NONE, replace non-ASCII characters by question marks, so
		// that engine internals doesn't receive non-mappeable data to UTF8. If an attachment
		// charset is used, validate the string.
		if (charSetId == CS_NONE)
		{
			for (char* p = transformedText.begin(), *end = transformedText.end(); p < end; ++p)
			{
				if (UCHAR(*p) > 0x7F)
					*p = '?';
			}
		}
		else
		{
			CharSet* charSet = INTL_charset_lookup(tdbb, charSetId);

			if (!charSet->wellFormed(transformedText.length(),
					(const UCHAR*) transformedText.begin(), NULL))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_malformed_string));
			}

			UCharBuffer temp;

			CsConvert conversor(charSet->getStruct(),
				INTL_charset_lookup(tdbb, CS_METADATA)->getStruct());
			conversor.convert(transformedText.length(), (const UCHAR*) transformedText.c_str(), temp);

			transformedText.assign(temp.begin(), temp.getCount());
		}

		dsqlStatement->setSqlText(FB_NEW_POOL(*statementPool) RefString(*statementPool, transformedText));

		// allocate the send and receive messages

		dsqlStatement->setSendMsg(FB_NEW_POOL(*statementPool) dsql_msg(*statementPool));
		dsql_msg* message = FB_NEW_POOL(*statementPool) dsql_msg(*statementPool);
		dsqlStatement->setReceiveMsg(message);
		message->msg_number = 1;

		dsqlStatement->setType(DsqlStatement::TYPE_SELECT);
		dsqlStatement->dsqlPass(tdbb, scratch, traceResult);

		if (!dsqlStatement->shouldPreserveScratch())
			database->deletePool(scratchPool);

		scratchPool = nullptr;

		if (!isInternalRequest && dsqlStatement->mustBeReplicated())
			dsqlStatement->setOrgText(text, textLength);

		return dsqlStatement;
	}
	catch (const Exception&)
	{
		if (scratchPool)
			database->deletePool(scratchPool);

		if (!dsqlStatement)
			database->deletePool(statementPool);

		throw;
	}
}


/**

 	put_item

    @brief	Put information item in output buffer if there is room, and
 	return an updated pointer.  If there isn't room for the item,
 	indicate truncation and return NULL.


    @param item
    @param length
    @param string
    @param ptr
    @param end

 **/
static UCHAR* put_item(	UCHAR	item,
						const USHORT	length,
						const UCHAR* string,
						UCHAR*	ptr,
						const UCHAR* const end)
{
	if (ptr + length + 3 >= end)
	{
		*ptr = isc_info_truncated;
		return NULL;
	}

	*ptr++ = item;

	*ptr++ = (UCHAR) length;
	*ptr++ = length >> 8;

	if (length)
		memcpy(ptr, string, length);

	return ptr + length;
}


// Return as UTF8
string IntlString::toUtf8(jrd_tra* transaction) const
{
	CHARSET_ID id = CS_dynamic;

	if (charset.hasData())
	{
		const dsql_intlsym* resolved = METD_get_charset(transaction, charset.length(), charset.c_str());

		if (!resolved)
		{
			// character set name is not defined
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					  Arg::Gds(isc_charset_not_found) << charset);
		}

		id = resolved->intlsym_charset_id;
	}

	string utf;
	return DataTypeUtil::convertToUTF8(s, utf, id, ERRD_post) ? utf : s;
}


/**

	sql_info

	@brief	Return DSQL information buffer.

	@param dsqlRequest
	@param item_length
	@param items
	@param info_length
	@param info

 **/

static void sql_info(thread_db* tdbb,
					 DsqlRequest* dsqlRequest,
					 ULONG item_length,
					 const UCHAR* items,
					 ULONG info_length,
					 UCHAR* info)
{
	if (!item_length || !items || !info_length || !info)
		return;

	UCHAR buffer[BUFFER_SMALL];
	memset(buffer, 0, sizeof(buffer));

	// Pre-initialize buffer. This is necessary because we don't want to transfer rubbish over the wire
	memset(info, 0, info_length);

	const UCHAR* const end_items = items + item_length;
	const UCHAR* const end_info = info + info_length;
	UCHAR *start_info;

	if (*items == isc_info_length)
	{
		start_info = info;
		items++;
	}
	else
		start_info = NULL;

	// CVC: Is it the idea that this pointer remains with its previous value
	// in the loop or should it be made NULL in each iteration?
	const dsql_msg* message = NULL;
	bool messageFound = false;
	USHORT first_index = 0;

	const auto dsqlStatement = dsqlRequest->getDsqlStatement();

	while (items < end_items && *items != isc_info_end && info < end_info)
	{
		ULONG length;
		USHORT number;
		ULONG value;
		const UCHAR item = *items++;

		switch (item)
		{
		case isc_info_sql_select:
		case isc_info_sql_bind:
			message = (item == isc_info_sql_select) ?
				dsqlStatement->getReceiveMsg() : dsqlStatement->getSendMsg();
			messageFound = true;
			if (info + 1 >= end_info)
			{
				*info = isc_info_truncated;
				return;
			}
			*info++ = item;
			break;

		case isc_info_sql_stmt_flags:
			value = IStatement::FLAG_REPEAT_EXECUTE;
			switch (dsqlStatement->getType())
			{
			case DsqlStatement::TYPE_CREATE_DB:
			case DsqlStatement::TYPE_DDL:
				value &= ~IStatement::FLAG_REPEAT_EXECUTE;
				break;
			case DsqlStatement::TYPE_SELECT:
			case DsqlStatement::TYPE_SELECT_UPD:
			case DsqlStatement::TYPE_SELECT_BLOCK:
			case DsqlStatement::TYPE_RETURNING_CURSOR:
				value |= IStatement::FLAG_HAS_CURSOR;
				break;
			}
			length = put_vax_long(buffer, value);
			info = put_item(item, length, buffer, info, end_info);
			if (!info)
				return;
			break;

		case isc_info_sql_stmt_type:
			switch (dsqlStatement->getType())
			{
			case DsqlStatement::TYPE_SELECT:
			case DsqlStatement::TYPE_RETURNING_CURSOR:
				number = isc_info_sql_stmt_select;
				break;
			case DsqlStatement::TYPE_SELECT_UPD:
				number = isc_info_sql_stmt_select_for_upd;
				break;
			case DsqlStatement::TYPE_CREATE_DB:
			case DsqlStatement::TYPE_DDL:
				number = isc_info_sql_stmt_ddl;
				break;
			case DsqlStatement::TYPE_COMMIT:
			case DsqlStatement::TYPE_COMMIT_RETAIN:
				number = isc_info_sql_stmt_commit;
				break;
			case DsqlStatement::TYPE_ROLLBACK:
			case DsqlStatement::TYPE_ROLLBACK_RETAIN:
				number = isc_info_sql_stmt_rollback;
				break;
			case DsqlStatement::TYPE_START_TRANS:
				number = isc_info_sql_stmt_start_trans;
				break;
			case DsqlStatement::TYPE_SESSION_MANAGEMENT:
				number = isc_info_sql_stmt_ddl;		// ?????????????????
				break;
			case DsqlStatement::TYPE_INSERT:
				number = isc_info_sql_stmt_insert;
				break;
			case DsqlStatement::TYPE_UPDATE:
			case DsqlStatement::TYPE_UPDATE_CURSOR:
				number = isc_info_sql_stmt_update;
				break;
			case DsqlStatement::TYPE_DELETE:
			case DsqlStatement::TYPE_DELETE_CURSOR:
				number = isc_info_sql_stmt_delete;
				break;
			case DsqlStatement::TYPE_EXEC_PROCEDURE:
				number = isc_info_sql_stmt_exec_procedure;
				break;
			case DsqlStatement::TYPE_SET_GENERATOR:
				number = isc_info_sql_stmt_set_generator;
				break;
			case DsqlStatement::TYPE_SAVEPOINT:
				number = isc_info_sql_stmt_savepoint;
				break;
			case DsqlStatement::TYPE_EXEC_BLOCK:
				number = isc_info_sql_stmt_exec_procedure;
				break;
			case DsqlStatement::TYPE_SELECT_BLOCK:
				number = isc_info_sql_stmt_select;
				break;
			default:
				number = 0;
				break;
			}
			length = put_vax_long(buffer, (SLONG) number);
			info = put_item(item, length, buffer, info, end_info);
			if (!info)
				return;
			break;

		case isc_info_sql_sqlda_start:
			if (items < end_items)
			{
				length = *items++;

				if (end_items - items >= length)
				{
					first_index = static_cast<USHORT>(gds__vax_integer(items, length));
					items += length;
					break;
				}
			}

			buffer[0] = item;
			length = 1 + INF_convert(isc_inf_invalid_args, buffer + 1);

			if (!(info = put_item(isc_info_error, length, buffer, info, end_info)))
				return;

			items = end_items;
			break;

		case isc_info_sql_batch_fetch:
			if (dsqlStatement->getFlags() & DsqlStatement::FLAG_NO_BATCH)
				number = 0;
			else
				number = 1;
			length = put_vax_long(buffer, (SLONG) number);
			if (!(info = put_item(item, length, buffer, info, end_info)))
				return;
			break;

		case isc_info_sql_records:
			length = get_request_info(tdbb, dsqlRequest, sizeof(buffer), buffer);
			if (length && !(info = put_item(item, length, buffer, info, end_info)))
				return;
			break;

		case isc_info_sql_stmt_timeout_user:
		case isc_info_sql_stmt_timeout_run:
			value = (item == isc_info_sql_stmt_timeout_user) ? dsqlRequest->getTimeout() : dsqlRequest->getActualTimeout();

			length = put_vax_long(buffer, value);
			if (!(info = put_item(item, length, buffer, info, end_info)))
				return;
			break;

		case isc_info_sql_stmt_blob_align:
			value = DsqlBatch::BLOB_STREAM_ALIGN;

			length = put_vax_long(buffer, value);
			if (!(info = put_item(item, length, buffer, info, end_info)))
				return;
			break;

		case isc_info_sql_get_plan:
		case isc_info_sql_explain_plan:
			{
				const bool detailed = (item == isc_info_sql_explain_plan);
				string plan = tdbb->getAttachment()->stringToUserCharSet(tdbb,
					Optimizer::getPlan(tdbb, dsqlRequest->getStatement(), detailed));

				if (plan.hasData())
				{
					// 1-byte item + 2-byte length + isc_info_end/isc_info_truncated == 4
					const ULONG buffer_length = end_info - info - 4;
					const ULONG max_length = MIN(buffer_length, MAX_USHORT);

					if (plan.length() > max_length)
					{
						// If the plan doesn't fit the supplied buffer or exceeds the API limits,
						// truncate it to the rightmost space and add ellipsis to the end
						plan.resize(max_length);

						while (plan.length() > max_length - 4)
						{
							const FB_SIZE_T pos = plan.find_last_of(' ');
							if (pos == string::npos)
								break;
							plan.resize(pos);
						}

						plan += " ...";

						if (plan.length() <= max_length)
						{
							info = put_item(item, plan.length(), reinterpret_cast<const UCHAR*>(plan.c_str()),
											info, end_info);
						}

						*info = isc_info_truncated;
						info = NULL;
					}
					else
					{
						info = put_item(item, plan.length(), reinterpret_cast<const UCHAR*>(plan.c_str()),
										info, end_info);
					}
				}

				if (!info)
					return;
			}
			break;

		case isc_info_sql_exec_path_blr_bytes:
		case isc_info_sql_exec_path_blr_text:
			{
				HalfStaticArray<UCHAR, 128> path;

				if (dsqlRequest->getStatement())
				{
					const auto& blr = dsqlRequest->getStatement()->blr;

					if (blr.hasData())
					{
						if (item == isc_info_sql_exec_path_blr_bytes)
							path.push(blr.begin(), blr.getCount());
						else if (item == isc_info_sql_exec_path_blr_text)
						{
							fb_print_blr(blr.begin(), (ULONG) blr.getCount(),
								[](void* arg, SSHORT offset, const char* line)
								{
									auto& localPath = *static_cast<decltype(path)*>(arg);
									auto lineLen = strlen(line);

									// Trim trailing spaces.
									while (lineLen > 0 && line[lineLen - 1] == ' ')
										--lineLen;

									char offsetStr[10];
									const auto offsetLen = sprintf(offsetStr, "%5d", (int) offset);

									localPath.push(reinterpret_cast<const UCHAR*>(offsetStr), offsetLen);
									localPath.push(' ');
									localPath.push(reinterpret_cast<const UCHAR*>(line), lineLen);
									localPath.push('\n');
								},
								&path, 0);
						}
					}
				}

				if (path.hasData())
				{
					// 1-byte item + 2-byte length + isc_info_end/isc_info_truncated == 4
					const ULONG bufferLength = end_info - info - 4;
					const ULONG maxLength = MIN(bufferLength, MAX_USHORT);

					if (path.getCount() > maxLength)
					{
						*info = isc_info_truncated;
						info = NULL;
					}
					else
						info = put_item(item, path.getCount(), path.begin(), info, end_info);
				}

				if (!info)
					return;
			}
			break;

		case isc_info_sql_num_variables:
		case isc_info_sql_describe_vars:
			if (messageFound)
			{
				number = message ? message->msg_index : 0;
				length = put_vax_long(buffer, (SLONG) number);
				if (!(info = put_item(item, length, buffer, info, end_info)))
					return;
				if (item == isc_info_sql_num_variables)
					continue;

				const UCHAR* end_describe = items;
				while (end_describe < end_items &&
					*end_describe != isc_info_end && *end_describe != isc_info_sql_describe_end)
				{
					end_describe++;
				}

				info = var_info(message, items, end_describe, info, end_info, first_index,
					message == dsqlStatement->getSendMsg());
				if (!info)
					return;

				items = end_describe;
				if (*items == isc_info_sql_describe_end)
					items++;
				break;
			}
			// else fall into

		default:
			buffer[0] = item;
			length = 1 + put_vax_long(buffer + 1, (SLONG) isc_infunk);
			if (!(info = put_item(isc_info_error, length, buffer, info, end_info)))
				return;
		}
	}

	if (info < end_info)
		*info++ = isc_info_end;

	if (start_info && (end_info - info >= 7))
	{
		const SLONG number = info - start_info;
		fb_assert(number > 0);
		memmove(start_info + 7, start_info, number);
		USHORT length = put_vax_long(buffer, number);
		fb_assert(length == 4); // We only accept SLONG
		put_item(isc_info_length, length, buffer, start_info, end_info);
	}
}


/**

 	var_info

    @brief	Provide information on an internal message.


    @param message
    @param items
    @param end_describe
    @param info
    @param end
    @param first_index

 **/
static UCHAR* var_info(const dsql_msg* message,
					   const UCHAR* items,
					   const UCHAR* const end_describe,
					   UCHAR* info,
					   const UCHAR* const end,
					   USHORT first_index,
					   bool input_message)
{
	if (!message || !message->msg_index)
		return info;

	thread_db* tdbb = JRD_get_thread_data();
	Jrd::Attachment* attachment = tdbb->getAttachment();

	HalfStaticArray<const dsql_par*, 16> parameters;

	for (FB_SIZE_T i = 0; i < message->msg_parameters.getCount(); ++i)
	{
		const dsql_par* param = message->msg_parameters[i];

		if (param->par_index)
		{
			if (param->par_index > parameters.getCount())
				parameters.grow(param->par_index);
			fb_assert(!parameters[param->par_index - 1]);
			parameters[param->par_index - 1] = param;
		}
	}

	UCHAR buf[128];

	for (FB_SIZE_T i = 0; i < parameters.getCount(); i++)
	{
		const dsql_par* param = parameters[i];
		fb_assert(param);

		if (param->par_index >= first_index)
		{
			dsc desc = param->par_desc;

			// Scan sources of coercion rules in reverse order to observe
			// 'last entered in use' rule. Start with dynamic binding rules ...
			if (!attachment->att_bindings.coerce(&desc))
			{
				// next - given in DPB ...
				if (!attachment->getInitialBindings()->coerce(&desc))
				{
					Database* dbb = tdbb->getDatabase();
					// and finally - rules from .conf files.
					dbb->getBindings()->coerce(&desc, dbb->dbb_compatibility_index);
				}
			}

			SLONG sql_len, sql_sub_type, sql_scale, sql_type;
			desc.getSqlInfo(&sql_len, &sql_sub_type, &sql_scale, &sql_type);

			if (input_message &&
				(desc.dsc_dtype == dtype_text || param->par_is_text) &&
				(desc.dsc_flags & DSC_null))
			{
				sql_type = SQL_NULL;
				sql_len = 0;
				sql_sub_type = 0;
			}
			else if (desc.dsc_dtype == dtype_varying && param->par_is_text)
				sql_type = SQL_TEXT;

			if (sql_type && (desc.dsc_flags & DSC_nullable))
				sql_type |= 0x1;

			for (const UCHAR* describe = items; describe < end_describe;)
			{
				USHORT length;
				MetaName name;
				const UCHAR* buffer = buf;
				UCHAR item = *describe++;

				switch (item)
				{
				case isc_info_sql_sqlda_seq:
					length = put_vax_long(buf, (SLONG) param->par_index);
					break;

				case isc_info_sql_message_seq:
					length = 0;
					break;

				case isc_info_sql_type:
					length = put_vax_long(buf, (SLONG) sql_type);
					break;

				case isc_info_sql_sub_type:
					length = put_vax_long(buf, (SLONG) sql_sub_type);
					break;

				case isc_info_sql_scale:
					length = put_vax_long(buf, (SLONG) sql_scale);
					break;

				case isc_info_sql_length:
					length = put_vax_long(buf, (SLONG) sql_len);
					break;

				case isc_info_sql_null_ind:
					length = put_vax_long(buf, (SLONG) (sql_type & 1));
					break;

				case isc_info_sql_field:
					if (param->par_name.hasData())
					{
						name = attachment->nameToUserCharSet(tdbb, param->par_name);
						length = name.length();
						buffer = reinterpret_cast<const UCHAR*>(name.c_str());
					}
					else
						length = 0;
					break;

				case isc_info_sql_relation:
					if (param->par_rel_name.hasData())
					{
						name = attachment->nameToUserCharSet(tdbb, param->par_rel_name);
						length = name.length();
						buffer = reinterpret_cast<const UCHAR*>(name.c_str());
					}
					else
						length = 0;
					break;

				case isc_info_sql_owner:
					if (param->par_owner_name.hasData())
					{
						name = attachment->nameToUserCharSet(tdbb, param->par_owner_name);
						length = name.length();
						buffer = reinterpret_cast<const UCHAR*>(name.c_str());
					}
					else
						length = 0;
					break;

				case isc_info_sql_relation_alias:
					if (param->par_rel_alias.hasData())
					{
						name = attachment->nameToUserCharSet(tdbb, param->par_rel_alias);
						length = name.length();
						buffer = reinterpret_cast<const UCHAR*>(name.c_str());
					}
					else
						length = 0;
					break;

				case isc_info_sql_alias:
					if (param->par_alias.hasData())
					{
						name = attachment->nameToUserCharSet(tdbb, param->par_alias);
						length = name.length();
						buffer = reinterpret_cast<const UCHAR*>(name.c_str());
					}
					else
						length = 0;
					break;

				default:
					buf[0] = item;
					item = isc_info_error;
					length = 1 + put_vax_long(buf + 1, (SLONG) isc_infunk);
					break;
				}

				if (!(info = put_item(item, length, buffer, info, end)))
					return info;
			}

			if (info + 1 >= end)
			{
				*info = isc_info_truncated;
				return NULL;
			}
			*info++ = isc_info_sql_describe_end;
		} // if()
	} // for()

	return info;
}
