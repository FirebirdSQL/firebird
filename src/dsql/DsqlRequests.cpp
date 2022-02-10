/*
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
 *
 * 2022.02.07 Adriano dos Santos Fernandes: Refactored from dsql.cpp
 */

#include "firebird.h"
#include "../dsql/DsqlRequests.h"
#include "../dsql/dsql.h"
#include "../dsql/DsqlBatch.h"
#include "../dsql/Nodes.h"
#include "../jrd/Statement.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../jrd/replication/Publisher.h"
#include "../jrd/trace/TraceDSQLHelpers.h"
#include "../jrd/trace/TraceObjects.h"
#include "../dsql/errd_proto.h"
#include "../dsql/movd_proto.h"
#include "../jrd/exe_proto.h"

using namespace Firebird;
using namespace Jrd;


static void checkD(IStatus* st);


// DsqlRequest

DsqlRequest::DsqlRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aDsqlStatement)
	: PermanentStorage(pool),
	  req_dbb(dbb),
	  dsqlStatement(aDsqlStatement)
{
}

DsqlRequest::~DsqlRequest()
{
}

void DsqlRequest::setCursor(thread_db* /*tdbb*/, const TEXT* /*name*/)
{
	status_exception::raise(
		Arg::Gds(isc_sqlerr) << Arg::Num(-804) <<
		Arg::Gds(isc_dsql_sqlda_err) <<
		Arg::Gds(isc_req_sync));
}

void DsqlRequest::setDelayedFormat(thread_db* /*tdbb*/, IMessageMetadata* /*metadata*/)
{
	status_exception::raise(
		Arg::Gds(isc_sqlerr) << Arg::Num(-804) <<
		Arg::Gds(isc_dsql_sqlda_err) <<
		Arg::Gds(isc_req_sync));
}

bool DsqlRequest::fetch(thread_db* /*tdbb*/, UCHAR* /*msgBuffer*/)
{
	status_exception::raise(
		Arg::Gds(isc_sqlerr) << Arg::Num(-804) <<
		Arg::Gds(isc_dsql_sqlda_err) <<
		Arg::Gds(isc_req_sync));

	return false;	// avoid warning
}

unsigned int DsqlRequest::getTimeout()
{
	return req_timeout;
}

unsigned int DsqlRequest::getActualTimeout()
{
	if (req_timer)
		return req_timer->getValue();

	return 0;
}

void DsqlRequest::setTimeout(unsigned int timeOut)
{
	req_timeout = timeOut;
}

TimeoutTimer* DsqlRequest::setupTimer(thread_db* tdbb)
{
	auto request = getRequest();

	if (request)
	{
		if (request->hasInternalStatement())
			return req_timer;

		request->req_timeout = this->req_timeout;

		fb_assert(!request->req_caller);
		if (request->req_caller)
		{
			if (req_timer)
				req_timer->setup(0, 0);
			return req_timer;
		}
	}

	Database* dbb = tdbb->getDatabase();
	Attachment* att = tdbb->getAttachment();

	ISC_STATUS toutErr = isc_cfg_stmt_timeout;
	unsigned int timeOut = dbb->dbb_config->getStatementTimeout() * 1000;

	if (req_timeout)
	{
		if (!timeOut || req_timeout < timeOut)
		{
			timeOut = req_timeout;
			toutErr = isc_req_stmt_timeout;
		}
	}
	else
	{
		const unsigned int attTout = att->getStatementTimeout();

		if (!timeOut || attTout && attTout < timeOut)
		{
			timeOut = attTout;
			toutErr = isc_att_stmt_timeout;
		}
	}

	if (!req_timer && timeOut)
	{
		req_timer = FB_NEW TimeoutTimer();
		fb_assert(request);
		request->req_timer = this->req_timer;
	}

	if (req_timer)
	{
		req_timer->setup(timeOut, toutErr);
		req_timer->start();
	}

	return req_timer;
}

// Release a dynamic request.
void DsqlRequest::destroy(thread_db* tdbb, DsqlRequest* dsqlRequest)
{
	SET_TDBB(tdbb);

	if (dsqlRequest->req_timer)
	{
		dsqlRequest->req_timer->stop();
		dsqlRequest->req_timer = nullptr;
	}

	// If request is parent, orphan the children and release a portion of their requests

	for (auto childStatement : dsqlRequest->cursors)
	{
		childStatement->addFlags(DsqlStatement::FLAG_ORPHAN);
		childStatement->setParentRequest(nullptr);

		// hvlad: lines below is commented out as
		// - child is already unlinked from its parent request
		// - we should not free child's sql text until its owner request is alive
		// It seems to me we should destroy owner request here, not a child
		// statement - as it always was before

		//Jrd::ContextPoolHolder context(tdbb, &childStatement->getPool());
		//releaseStatement(childStatement);
	}

	// If the request had an open cursor, close it

	if (dsqlRequest->req_cursor)
		DsqlCursor::close(tdbb, dsqlRequest->req_cursor);

	if (dsqlRequest->req_batch)
	{
		delete dsqlRequest->req_batch;
		dsqlRequest->req_batch = nullptr;
	}

	Jrd::Attachment* att = dsqlRequest->req_dbb->dbb_attachment;
	const bool need_trace_free = dsqlRequest->req_traced && TraceManager::need_dsql_free(att);
	if (need_trace_free)
	{
		TraceSQLStatementImpl stmt(dsqlRequest, NULL);
		TraceManager::event_dsql_free(att, &stmt, DSQL_drop);
	}

	if (dsqlRequest->req_cursor_name.hasData())
		dsqlRequest->req_dbb->dbb_cursors.remove(dsqlRequest->req_cursor_name);

	// If a request has been compiled, release it now
	if (dsqlRequest->getRequest())
		EXE_release(tdbb, dsqlRequest->getRequest());

	// Increase the statement refCount so its pool is not destroyed before the request is gone.
	auto dsqlStatement = dsqlRequest->getDsqlStatement();

	// Release the entire request
	delete dsqlRequest;

	dsqlStatement = nullptr;
}

// Parse the message of a request.
USHORT DsqlRequest::parseMetadata(IMessageMetadata* meta, const Array<dsql_par*>& parameters_list)
{
	HalfStaticArray<const dsql_par*, 16> parameters;

	for (FB_SIZE_T i = 0; i < parameters_list.getCount(); ++i)
	{
		dsql_par* param = parameters_list[i];

		if (param->par_index)
		{
			if (param->par_index > parameters.getCount())
				parameters.grow(param->par_index);
			fb_assert(!parameters[param->par_index - 1]);
			parameters[param->par_index - 1] = param;
		}
	}

	// If there's no metadata, then the format of the current message buffer
	// is identical to the format of the previous one.

	if (!meta)
		return parameters.getCount();

	FbLocalStatus st;
	unsigned count = meta->getCount(&st);
	checkD(&st);

	unsigned count2 = parameters.getCount();

	if (count != count2)
	{
		ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
				  Arg::Gds(isc_dsql_wrong_param_num) <<Arg::Num(count2) << Arg::Num(count));
	}

	unsigned offset = 0;

	for (USHORT index = 0; index < count; index++)
	{
		unsigned sqlType = meta->getType(&st, index);
		checkD(&st);
		unsigned sqlLength = meta->getLength(&st, index);
		checkD(&st);

		dsc desc;
		desc.dsc_flags = 0;

		unsigned dataOffset, nullOffset, dtype, dlength;
		offset = fb_utils::sqlTypeToDsc(offset, sqlType, sqlLength,
			&dtype, &dlength, &dataOffset, &nullOffset);
		desc.dsc_dtype = dtype;
		desc.dsc_length = dlength;

		desc.dsc_scale = meta->getScale(&st, index);
		checkD(&st);
		desc.dsc_sub_type = meta->getSubType(&st, index);
		checkD(&st);
		unsigned textType = meta->getCharSet(&st, index);
		checkD(&st);
		desc.setTextType(textType);
		desc.dsc_address = (UCHAR*)(IPTR) dataOffset;

		const dsql_par* const parameter = parameters[index];
		fb_assert(parameter);

		// ASF: Older than 2.5 engine hasn't validating strings in DSQL. After this has been
		// implemented in 2.5, selecting a NONE column with UTF-8 attachment charset started
		// failing. The real problem is that the client encodes SQL_TEXT/SQL_VARYING using
		// blr_text/blr_varying (i.e. with the connection charset). I'm reseting the charset
		// here at the server as a way to make older (and not yet changed) client work
		// correctly.
		if (desc.isText() && desc.getTextType() == ttype_dynamic)
			desc.setTextType(ttype_none);

		req_user_descs.put(parameter, desc);

		dsql_par* null = parameter->par_null;
		if (null)
		{
			desc.clear();
			desc.dsc_dtype = dtype_short;
			desc.dsc_scale = 0;
			desc.dsc_length = sizeof(SSHORT);
			desc.dsc_address = (UCHAR*)(IPTR) nullOffset;

			req_user_descs.put(null, desc);
		}
	}

	return count;
}


// DsqlDmlRequest

DsqlDmlRequest::DsqlDmlRequest(thread_db* tdbb, MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aStatement)
	: DsqlRequest(pool, dbb, aStatement),
	  req_msg_buffers(pool)
{
	// Create the messages buffers
	for (auto message : aStatement->getPorts())
	{
		// Allocate buffer for message
		const ULONG newLen = message->msg_length + FB_DOUBLE_ALIGN - 1;
		UCHAR* msgBuffer = FB_NEW_POOL(getPool()) UCHAR[newLen];
		msgBuffer = FB_ALIGN(msgBuffer, FB_DOUBLE_ALIGN);
		fb_assert(message->msg_buffer_number == req_msg_buffers.getCount());
		req_msg_buffers.add(msgBuffer);
	}

	request = aStatement->getStatement()->findRequest(tdbb);
	tdbb->getAttachment()->att_requests.add(request);
}

Statement* DsqlDmlRequest::getStatement() const
{
	return request ? request->getStatement() : nullptr;
}

// Provide backward-compatibility
void DsqlDmlRequest::setDelayedFormat(thread_db* tdbb, IMessageMetadata* metadata)
{
	if (!needDelayedFormat)
	{
		status_exception::raise(
			Arg::Gds(isc_sqlerr) << Arg::Num(-804) <<
			Arg::Gds(isc_dsql_sqlda_err) <<
			Arg::Gds(isc_req_sync));
	}

	needDelayedFormat = false;
	delayedFormat = metadata;
}

// Fetch next record from a dynamic SQL cursor.
bool DsqlDmlRequest::fetch(thread_db* tdbb, UCHAR* msgBuffer)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &getPool());

	// if the cursor isn't open, we've got a problem
	if (dsqlStatement->isCursorBased())
	{
		if (!req_cursor)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					  Arg::Gds(isc_dsql_cursor_err) <<
					  Arg::Gds(isc_dsql_cursor_not_open));
		}
	}

	if (!request)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
				  Arg::Gds(isc_unprepared_stmt));
	}

	dsql_msg* message = (dsql_msg*) dsqlStatement->getReceiveMsg();

	if (delayedFormat && message)
	{
		parseMetadata(delayedFormat, message->msg_parameters);
		delayedFormat = NULL;
	}

	// Set up things for tracing this call
	Jrd::Attachment* att = req_dbb->dbb_attachment;
	TraceDSQLFetch trace(att, this);

	thread_db::TimerGuard timerGuard(tdbb, req_timer, false);
	if (req_timer && req_timer->expired())
		tdbb->checkCancelState();

	UCHAR* dsqlMsgBuffer = req_msg_buffers[message->msg_buffer_number];
	if (!firstRowFetched && needRestarts())
	{
		// Note: tra_handle can't be changed by executeReceiveWithRestarts below
		// and outMetadata and outMsg in not used there, so passing NULL's is safe.
		jrd_tra* tra = req_transaction;

		executeReceiveWithRestarts(tdbb, &tra, NULL, NULL, false, false, true);
		fb_assert(tra == req_transaction);
	}
	else
		JRD_receive(tdbb, request, message->msg_number, message->msg_length, dsqlMsgBuffer);

	firstRowFetched = true;

	const dsql_par* const eof = dsqlStatement->getEof();
	const USHORT* eofPtr = eof ? (USHORT*) (dsqlMsgBuffer + (IPTR) eof->par_desc.dsc_address) : NULL;
	const bool eofReached = eof && !(*eofPtr);

	if (eofReached)
	{
		if (req_timer)
			req_timer->stop();

		trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
		return false;
	}

	if (msgBuffer)
		mapInOut(tdbb, true, message, NULL, msgBuffer);

	trace.fetch(false, ITracePlugin::RESULT_SUCCESS);
	return true;
}

// Set a cursor name for a dynamic request.
void DsqlDmlRequest::setCursor(thread_db* tdbb, const TEXT* name)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &getPool());

	const size_t MAX_CURSOR_LENGTH = 132 - 1;
	string cursor = name;

	if (cursor.hasData() && cursor[0] == '\"')
	{
		// Quoted cursor names eh? Strip'em.
		// Note that "" will be replaced with ".
		// The code is very strange, because it doesn't check for "" really
		// and thus deletes one isolated " in the middle of the cursor.
		for (string::iterator i = cursor.begin(); i < cursor.end(); ++i)
		{
			if (*i == '\"')
				cursor.erase(i);
		}
	}
	else	// not quoted name
	{
		const string::size_type i = cursor.find(' ');
		if (i != string::npos)
			cursor.resize(i);

		cursor.upper();
	}

	USHORT length = (USHORT) fb_utils::name_length(cursor.c_str());

	if (!length)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_decl_err) <<
				  Arg::Gds(isc_dsql_cursor_invalid));
	}

	if (length > MAX_CURSOR_LENGTH)
		length = MAX_CURSOR_LENGTH;

	cursor.resize(length);

	// If there already is a different cursor by the same name, bitch

	auto* const* symbol = req_dbb->dbb_cursors.get(cursor);
	if (symbol)
	{
		if (this == *symbol)
			return;

		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_decl_err) <<
				  Arg::Gds(isc_dsql_cursor_redefined) << cursor);
	}

	// If there already is a cursor and its name isn't the same, ditto.
	// We already know there is no cursor by this name in the hash table

	if (req_cursor && req_cursor_name.hasData())
	{
		fb_assert(!symbol);
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_decl_err) <<
				  Arg::Gds(isc_dsql_cursor_redefined) << req_cursor_name);
	}

	if (req_cursor_name.hasData())
		req_dbb->dbb_cursors.remove(req_cursor_name);
	req_cursor_name = cursor;
	req_dbb->dbb_cursors.put(cursor, this);
}

// Open a dynamic SQL cursor.
DsqlCursor* DsqlDmlRequest::openCursor(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* inMeta, const UCHAR* inMsg, IMessageMetadata* outMeta, ULONG flags)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &getPool());

	if (dsqlStatement->getFlags() & DsqlStatement::FLAG_ORPHAN)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
		          Arg::Gds(isc_bad_req_handle));
	}

	// Validate transaction handle

	if (!*traHandle)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
				  Arg::Gds(isc_bad_trans_handle));
	}

	// Validate statement type

	if (!dsqlStatement->isCursorBased())
		Arg::Gds(isc_no_cursor).raise();

	// Validate cursor or batch being not already open

	if (req_cursor)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_cursor_open_err));
	}

	if (req_batch)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_batch_open));
	}

	req_transaction = *traHandle;
	execute(tdbb, traHandle, inMeta, inMsg, outMeta, NULL, false);

	req_cursor = FB_NEW_POOL(getPool()) DsqlCursor(this, flags);

	return req_cursor;
}

bool DsqlDmlRequest::needRestarts()
{
	return (req_transaction && (req_transaction->tra_flags & TRA_read_consistency));
};

// Execute a dynamic SQL statement
void DsqlDmlRequest::doExecute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton)
{
	firstRowFetched = false;
	const dsql_msg* message = dsqlStatement->getSendMsg();

	if (!message)
		JRD_start(tdbb, request, req_transaction);
	else
	{
		UCHAR* msgBuffer = req_msg_buffers[message->msg_buffer_number];
		JRD_start_and_send(tdbb, request, req_transaction, message->msg_number,
			message->msg_length, msgBuffer);
	}

	// Selectable execute block should get the "proc fetch" flag assigned,
	// which ensures that the savepoint stack is preserved while suspending
	if (dsqlStatement->getType() == DsqlStatement::TYPE_SELECT_BLOCK)
		request->req_flags |= req_proc_fetch;

	// TYPE_EXEC_BLOCK has no outputs so there are no out_msg
	// supplied from client side, but TYPE_EXEC_BLOCK requires
	// 2-byte message for EOS synchronization
	const bool isBlock = (dsqlStatement->getType() == DsqlStatement::TYPE_EXEC_BLOCK);

	message = dsqlStatement->getReceiveMsg();

	if (outMetadata == DELAYED_OUT_FORMAT)
	{
		needDelayedFormat = true;
		outMetadata = NULL;
	}

	if (outMetadata && message)
		parseMetadata(outMetadata, message->msg_parameters);

	if ((outMsg && message) || isBlock)
	{
		UCHAR temp_buffer[FB_DOUBLE_ALIGN * 2];
		dsql_msg temp_msg(*getDefaultMemoryPool());

		// Insure that the metadata for the message is parsed, regardless of
		// whether anything is found by the call to receive.

		UCHAR* msgBuffer = req_msg_buffers[message->msg_buffer_number];

		if (!outMetadata && isBlock)
		{
			message = &temp_msg;
			temp_msg.msg_number = 1;
			temp_msg.msg_length = 2;
			msgBuffer = FB_ALIGN(temp_buffer, FB_DOUBLE_ALIGN);
		}

		JRD_receive(tdbb, request, message->msg_number, message->msg_length, msgBuffer);

		if (outMsg)
			mapInOut(tdbb, true, message, NULL, outMsg);

		// if this is a singleton select, make sure there's in fact one record

		if (singleton)
		{
			USHORT counter;

			// Create a temp message buffer and try two more receives.
			// If both succeed then the first is the next record and the
			// second is either another record or the end of record message.
			// In either case, there's more than one record.

			UCHAR* message_buffer = (UCHAR*) gds__alloc(message->msg_length);

			ISC_STATUS status = FB_SUCCESS;
			FbLocalStatus localStatus;

			for (counter = 0; counter < 2 && !status; counter++)
			{
				localStatus->init();
				AutoSetRestore<Jrd::FbStatusVector*> autoStatus(&tdbb->tdbb_status_vector, &localStatus);

				try
				{
					JRD_receive(tdbb, request, message->msg_number,
						message->msg_length, message_buffer);
					status = FB_SUCCESS;
				}
				catch (Exception&)
				{
					status = tdbb->tdbb_status_vector->getErrors()[1];
				}
			}

			gds__free(message_buffer);

			// two successful receives means more than one record
			// a req_sync error on the first pass above means no records
			// a non-req_sync error on any of the passes above is an error

			if (!status)
				status_exception::raise(Arg::Gds(isc_sing_select_err));
			else if (status == isc_req_sync && counter == 1)
				status_exception::raise(Arg::Gds(isc_stream_eof));
			else if (status != isc_req_sync)
				status_exception::raise(&localStatus);
		}
	}

	switch (dsqlStatement->getType())
	{
		case DsqlStatement::TYPE_UPDATE_CURSOR:
			if (!request->req_records_updated)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-913) <<
						  Arg::Gds(isc_deadlock) <<
						  Arg::Gds(isc_update_conflict));
			}
			break;

		case DsqlStatement::TYPE_DELETE_CURSOR:
			if (!request->req_records_deleted)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-913) <<
						  Arg::Gds(isc_deadlock) <<
						  Arg::Gds(isc_update_conflict));
			}
			break;
	}
}

DsqlBatch* DsqlDmlRequest::openBatch(thread_db* tdbb, Firebird::IMessageMetadata* inMetadata,
	unsigned parLength, const UCHAR* par)
{
	return DsqlBatch::open(tdbb, this, inMetadata, parLength, par);
}

// Execute a dynamic SQL statement with tracing, restart and timeout handler
void DsqlDmlRequest::execute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* inMetadata, const UCHAR* inMsg,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton)
{
	if (!request)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
				  Arg::Gds(isc_unprepared_stmt));
	}

	// If there is no data required, just start the request

	const dsql_msg* message = dsqlStatement->getSendMsg();
	if (message)
		mapInOut(tdbb, false, message, inMetadata, NULL, inMsg);

	// we need to mapInOut() before tracing of execution start to let trace
	// manager know statement parameters values
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this);

	// Setup and start timeout timer
	const bool have_cursor = dsqlStatement->isCursorBased() && !singleton;

	setupTimer(tdbb);
	thread_db::TimerGuard timerGuard(tdbb, req_timer, !have_cursor);

	if (needRestarts())
		executeReceiveWithRestarts(tdbb, traHandle, outMetadata, outMsg, singleton, true, false);
	else {
		doExecute(tdbb, traHandle, outMetadata, outMsg, singleton);
	}

	trace.finish(have_cursor, ITracePlugin::RESULT_SUCCESS);
}

void DsqlDmlRequest::executeReceiveWithRestarts(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton, bool exec, bool fetch)
{
	request->req_flags &= ~req_update_conflict;
	int numTries = 0;
	const int MAX_RESTARTS = 10;

	while (true)
	{
		AutoSavePoint savePoint(tdbb, req_transaction);

		// Don't set req_restart_ready flag at last attempt to restart request.
		// It allows to raise update conflict error (if any) as usual and
		// handle error by PSQL handler.
		const ULONG flag = (numTries >= MAX_RESTARTS) ? 0 : req_restart_ready;
		AutoSetRestoreFlag<ULONG> restartReady(&request->req_flags, flag, true);
		try
		{
			if (exec)
				doExecute(tdbb, traHandle, outMetadata, outMsg, singleton);

			if (fetch)
			{
				fb_assert(dsqlStatement->isCursorBased());

				const dsql_msg* message = dsqlStatement->getReceiveMsg();

				UCHAR* dsqlMsgBuffer = req_msg_buffers[message->msg_buffer_number];
				JRD_receive(tdbb, request, message->msg_number, message->msg_length, dsqlMsgBuffer);
			}
		}
		catch (const status_exception&)
		{
			if (!(req_transaction->tra_flags & TRA_ex_restart))
			{
				request->req_flags &= ~req_update_conflict;
				throw;
			}
		}

		if (!(request->req_flags & req_update_conflict))
		{
			fb_assert((req_transaction->tra_flags & TRA_ex_restart) == 0);
			req_transaction->tra_flags &= ~TRA_ex_restart;

#ifdef DEV_BUILD
			if (numTries > 0)
			{
				string s;
				s.printf("restarts = %d", numTries);

				ERRD_post_warning(Arg::Warning(isc_random) << Arg::Str(s));
			}
#endif
			savePoint.release();	// everything is ok
			break;
		}

		fb_assert((req_transaction->tra_flags & TRA_ex_restart) != 0);

		request->req_flags &= ~req_update_conflict;
		req_transaction->tra_flags &= ~TRA_ex_restart;
		fb_utils::init_status(tdbb->tdbb_status_vector);

		// Undo current savepoint but preserve already taken locks.
		// Savepoint will be restarted at the next loop iteration.
		savePoint.rollback(true);

		numTries++;
		if (numTries >= MAX_RESTARTS)
		{
			gds__log("Update conflict: unable to get a stable set of rows in the source tables\n"
				"\tafter %d attempts of restart.\n"
				"\tQuery:\n%s\n", numTries, request->getStatement()->sqlText->c_str() );
		}

		// When restart we must execute query
		exec = true;
	}
}

// Map data from external world into message or from message to external world.
void DsqlDmlRequest::mapInOut(thread_db* tdbb, bool toExternal, const dsql_msg* message,
	IMessageMetadata* meta, UCHAR* dsql_msg_buf, const UCHAR* in_dsql_msg_buf)
{
	USHORT count = parseMetadata(meta, message->msg_parameters);

	// Sanity check

	if (count)
	{
		if (toExternal)
		{
			if (dsql_msg_buf == NULL)
			{
				ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
						  Arg::Gds(isc_dsql_no_output_sqlda));
			}
		}
		else
		{
			if (in_dsql_msg_buf == NULL)
			{
				ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
						  Arg::Gds(isc_dsql_no_input_sqlda));
			}
		}
	}

	USHORT count2 = 0;

	for (FB_SIZE_T i = 0; i < message->msg_parameters.getCount(); ++i)
	{
		dsql_par* parameter = message->msg_parameters[i];

		if (parameter->par_index)
		{
			 // Make sure the message given to us is long enough

			dsc desc;
			if (!req_user_descs.get(parameter, desc))
				desc.clear();

			/***
			ULONG length = (IPTR) desc.dsc_address + desc.dsc_length;
			if (length > msg_length)
			{
				ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
					Arg::Gds(isc_random) << "Message buffer too short");
			}
			***/
			if (!desc.dsc_dtype)
			{
				ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
					Arg::Gds(isc_dsql_datatype_err) <<
					Arg::Gds(isc_dsql_sqlvar_index) << Arg::Num(parameter->par_index-1));
			}

			UCHAR* msgBuffer = req_msg_buffers[parameter->par_message->msg_buffer_number];

			SSHORT* flag = NULL;
			dsql_par* const null_ind = parameter->par_null;
			if (null_ind != NULL)
			{
				dsc userNullDesc;
				if (!req_user_descs.get(null_ind, userNullDesc))
					userNullDesc.clear();

				const ULONG null_offset = (IPTR) userNullDesc.dsc_address;

				/***
				length = null_offset + sizeof(SSHORT);
				if (length > msg_length)
				{
					ERRD_post(Arg::Gds(isc_dsql_sqlda_err)
						<< Arg::Gds(isc_random) << "Message buffer too short");
				}
				***/

				dsc nullDesc = null_ind->par_desc;
				nullDesc.dsc_address = msgBuffer + (IPTR) nullDesc.dsc_address;

				if (toExternal)
				{
					flag = reinterpret_cast<SSHORT*>(dsql_msg_buf + null_offset);
					*flag = *reinterpret_cast<const SSHORT*>(nullDesc.dsc_address);
				}
				else
				{
					flag = reinterpret_cast<SSHORT*>(nullDesc.dsc_address);
					*flag = *reinterpret_cast<const SSHORT*>(in_dsql_msg_buf + null_offset);
				}
			}

			const bool notNull = (!flag || *flag >= 0);

			dsc parDesc = parameter->par_desc;
			parDesc.dsc_address = msgBuffer + (IPTR) parDesc.dsc_address;

			if (toExternal)
			{
				desc.dsc_address = dsql_msg_buf + (IPTR) desc.dsc_address;

				if (notNull)
					MOVD_move(tdbb, &parDesc, &desc);
				else
					memset(desc.dsc_address, 0, desc.dsc_length);
			}
			else if (notNull && !parDesc.isNull())
			{
				// Safe cast because desc is used as source only.
				desc.dsc_address = const_cast<UCHAR*>(in_dsql_msg_buf) + (IPTR) desc.dsc_address;
				MOVD_move(tdbb, &desc, &parDesc);
			}
			else
				memset(parDesc.dsc_address, 0, parDesc.dsc_length);

			++count2;
		}
	}

	if (count != count2)
	{
		ERRD_post(
			Arg::Gds(isc_dsql_sqlda_err) <<
			Arg::Gds(isc_dsql_wrong_param_num) << Arg::Num(count) <<Arg::Num(count2));
	}

	const auto dsqlStatement = getDsqlStatement();
	const dsql_par* parameter;

	const dsql_par* dbkey;
	if (!toExternal && (dbkey = dsqlStatement->getParentDbKey()) &&
		(parameter = dsqlStatement->getDbKey()))
	{
		UCHAR* parentMsgBuffer = dsqlStatement->getParentRequest() ?
			dsqlStatement->getParentRequest()->req_msg_buffers[dbkey->par_message->msg_buffer_number] : NULL;
		UCHAR* msgBuffer = req_msg_buffers[parameter->par_message->msg_buffer_number];

		fb_assert(parentMsgBuffer);

		dsc parentDesc = dbkey->par_desc;
		parentDesc.dsc_address = parentMsgBuffer + (IPTR) parentDesc.dsc_address;

		dsc desc = parameter->par_desc;
		desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

		MOVD_move(tdbb, &parentDesc, &desc);

		dsql_par* null_ind = parameter->par_null;
		if (null_ind != NULL)
		{
			desc = null_ind->par_desc;
			desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

			SSHORT* flag = (SSHORT*) desc.dsc_address;
			*flag = 0;
		}
	}

	const dsql_par* rec_version;
	if (!toExternal && (rec_version = dsqlStatement->getParentRecVersion()) &&
		(parameter = dsqlStatement->getRecVersion()))
	{
		UCHAR* parentMsgBuffer = dsqlStatement->getParentRequest() ?
			dsqlStatement->getParentRequest()->req_msg_buffers[rec_version->par_message->msg_buffer_number] :
			NULL;
		UCHAR* msgBuffer = req_msg_buffers[parameter->par_message->msg_buffer_number];

		fb_assert(parentMsgBuffer);

		dsc parentDesc = rec_version->par_desc;
		parentDesc.dsc_address = parentMsgBuffer + (IPTR) parentDesc.dsc_address;

		dsc desc = parameter->par_desc;
		desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

		MOVD_move(tdbb, &parentDesc, &desc);

		dsql_par* null_ind = parameter->par_null;
		if (null_ind != NULL)
		{
			desc = null_ind->par_desc;
			desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

			SSHORT* flag = (SSHORT*) desc.dsc_address;
			*flag = 0;
		}
	}
}


// DsqlDdlRequest

DsqlDdlRequest::DsqlDdlRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlCompilerScratch* aInternalScratch, DdlNode* aNode)
	: DsqlRequest(pool, dbb, aInternalScratch->getDsqlStatement()),
	  internalScratch(aInternalScratch),
	  node(aNode)
{
}

// Execute a dynamic SQL statement.
void DsqlDdlRequest::execute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* inMetadata, const UCHAR* inMsg,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton)
{
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this);

	fb_utils::init_status(tdbb->tdbb_status_vector);

	// run all statements under savepoint control
	{	// scope
		AutoSavePoint savePoint(tdbb, req_transaction);

		try
		{
			AutoSetRestoreFlag<ULONG> execDdl(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);

			node->executeDdl(tdbb, internalScratch, req_transaction);

			const bool isInternalRequest =
				(internalScratch->flags & DsqlCompilerScratch::FLAG_INTERNAL_REQUEST);

			if (!isInternalRequest && node->mustBeReplicated())
				REPL_exec_sql(tdbb, req_transaction, getDsqlStatement()->getOrgText());
		}
		catch (status_exception& ex)
		{
			DsqlStatement::rethrowDdlException(ex, true, node);
		}

		savePoint.release();	// everything is ok
	}

	JRD_autocommit_ddl(tdbb, req_transaction);

	trace.finish(false, ITracePlugin::RESULT_SUCCESS);
}


// DsqlTransactionRequest

DsqlTransactionRequest::DsqlTransactionRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aStatement, TransactionNode* aNode)
	: DsqlRequest(pool, dbb, aStatement),
	  node(aNode)
{
	// Don't trace anything except savepoint statements
	req_traced = (aStatement->getType() == DsqlStatement::TYPE_SAVEPOINT);
}

// Execute a dynamic SQL statement.
void DsqlTransactionRequest::execute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* /*inMetadata*/, const UCHAR* /*inMsg*/,
	IMessageMetadata* /*outMetadata*/, UCHAR* /*outMsg*/,
	bool /*singleton*/)
{
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this);
	node->execute(tdbb, this, traHandle);
	trace.finish(false, ITracePlugin::RESULT_SUCCESS);
}


// DsqlSessionManagementStatement

DsqlSessionManagementStatement::~DsqlSessionManagementStatement()
{
	dsqlAttachment->deletePool(&scratch->getPool());
}

void DsqlSessionManagementStatement::dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch,
	ntrace_result_t* /*traceResult*/)
{
	node = Node::doDsqlPass(scratch, node);

	this->scratch = scratch;
}

DsqlSessionManagementRequest* DsqlSessionManagementStatement::createRequest(thread_db* tdbb, dsql_dbb* dbb)
{
	return FB_NEW_POOL(getPool()) DsqlSessionManagementRequest(getPool(), dbb, this, node);
}

// Execute a dynamic SQL statement.
void DsqlSessionManagementRequest::execute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* inMetadata, const UCHAR* inMsg,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton)
{
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this);
	node->execute(tdbb, this, traHandle);
	trace.finish(false, ITracePlugin::RESULT_SUCCESS);
}


// Utility functions

// raise error if one present
static void checkD(IStatus* st)
{
	if (st->getState() & IStatus::STATE_ERRORS)
		ERRD_post(Arg::StatusVector(st));
}
