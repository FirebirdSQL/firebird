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
#include "../dsql/DsqlStatementCache.h"
#include "../dsql/Nodes.h"
#include "../dsql/StmtNodes.h"
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

void DsqlRequest::releaseRequest(thread_db* tdbb)
{
	if (req_timer)
	{
		req_timer->stop();
		req_timer = nullptr;
	}

	// Prevent new children from appear
	if (req_cursor_name.hasData())
		req_dbb->dbb_cursors.remove(req_cursor_name);

	// If request is parent, orphan the children and release a portion of their requests

	for (auto childStatement : cursors)
	{
		// Without parent request prepared DsqlRequest will throw error on execute which is exactly what one would expect
		childStatement->onReferencedCursorClose();

		// hvlad: lines below is commented out as
		// - child is already unlinked from its parent request
		// - we should not free child's sql text until its owner request is alive
		// It seems to me we should destroy owner request here, not a child
		// statement - as it always was before

		//Jrd::ContextPoolHolder context(tdbb, &childStatement->getPool());
		//releaseStatement(childStatement);
	}

	// If the request had an open cursor, close it

	if (req_cursor)
		DsqlCursor::close(tdbb, req_cursor);

	if (req_batch)
	{
		delete req_batch;
		req_batch = nullptr;
	}

	Jrd::Attachment* att = req_dbb->dbb_attachment;
	const bool need_trace_free = req_traced && TraceManager::need_dsql_free(att);
	if (need_trace_free)
	{
		TraceSQLStatementImpl stmt(this, nullptr, nullptr);
		TraceManager::event_dsql_free(att, &stmt, DSQL_drop);
	}

	// If a request has been compiled, release it now
	if (getRequest())
		EXE_release(tdbb, getRequest());
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

	// Increase the statement refCount so its pool is not destroyed before the request is gone.
	auto dsqlStatement = dsqlRequest->getDsqlStatement();

	// Let request to clean itself
	try
	{
		dsqlRequest->releaseRequest(tdbb);
	}
	catch(...)
	{
		fb_assert(false);
	}

	// Release the entire request
	delete dsqlRequest;
	dsqlStatement = nullptr;
}

// DsqlDmlRequest

DsqlDmlRequest::DsqlDmlRequest(thread_db* tdbb, MemoryPool& pool, dsql_dbb* dbb, DsqlDmlStatement* aStatement)
	: DsqlRequest(pool, dbb, aStatement)
{
	request = aStatement->getStatement()->findRequest(tdbb);
	tdbb->getAttachment()->att_requests.add(request);

	// If this is positional DML - subscribe to parent cursor as well

	if (aStatement->parentCursorName.hasData())
	{
		const auto* const symbol = dbb->dbb_cursors.get(aStatement->parentCursorName.c_str());

		if (!symbol)
		{
			// cursor is not found
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					  Arg::Gds(isc_dsql_cursor_err) <<
					  Arg::Gds(isc_dsql_cursor_not_found) << aStatement->parentCursorName);
		}

		parentRequest = *symbol;
		fb_assert(parentRequest != nullptr);

		// Verify that the cursor is appropriate and updatable

		if (parentRequest->getDsqlStatement()->getType() != DsqlStatement::TYPE_SELECT_UPD)
		{
			// cursor is not updatable
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-510) <<
					  Arg::Gds(isc_dsql_cursor_update_err) << aStatement->parentCursorName);
		}

		// Check that it contains this relation name
		Request* request = parentRequest->getRequest();
		fb_assert(request->req_rpb.getCount() > 0 && request->req_rpb[0].rpb_relation != nullptr);

		const auto& relName = request->req_rpb[0].rpb_relation->rel_name;
		bool found = false;
		for (FB_SIZE_T i = 0; i < request->req_rpb.getCount(); ++i)
		{
			jrd_rel* relation = request->req_rpb[i].rpb_relation;

			if (relation && relation->rel_name == relName)
			{
				if (found)
				{
					// Relation is used twice in cursor
					ERRD_post(
						Arg::Gds(isc_dsql_cursor_err) <<
						Arg::Gds(isc_dsql_cursor_rel_ambiguous) <<
							relName.toQuotedString() << aStatement->parentCursorName.toQuotedString());
				}
				parentContext = i;
				found = true;
			}
		}

		if (!found)
		{
			// Relation is not in cursor
			ERRD_post(
				Arg::Gds(isc_dsql_cursor_err) <<
				Arg::Gds(isc_dsql_cursor_rel_not_found) <<
					relName.toQuotedString() << aStatement->parentCursorName.toQuotedString());
		}
		parentRequest->cursors.add(this);
	}
}

void DsqlDmlRequest::releaseRequest(thread_db* tdbb)
{
	// If request is a child - unsubscribe
	if (parentRequest)
	{
		parentRequest->cursors.findAndRemove(this);
		parentRequest = nullptr;
	}

	// Let ancestor do cleanup as well
	DsqlRequest::releaseRequest(tdbb);
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

	metadataToFormat(metadata, dsqlStatement->getReceiveMsg());
	needDelayedFormat = false;
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

	// At this point we have to have output metadata from client
	if (needDelayedFormat)
	{
		ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
				  Arg::Gds(isc_dsql_no_output_sqlda));
	}

	// Set up things for tracing this call
	Jrd::Attachment* att = req_dbb->dbb_attachment;
	TraceDSQLFetch trace(att, this);

	thread_db::TimerGuard timerGuard(tdbb, req_timer, false);
	if (req_timer && req_timer->expired())
		tdbb->checkCancelState();

	// Fetch from already finished request should not produce error
	if (!(request->req_flags & req_active))
	{
		if (req_timer)
			req_timer->stop();

		trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
		return false;
	}

	if (!firstRowFetched && needRestarts())
	{
		// Note: tra_handle can't be changed by executeReceiveWithRestarts below
		// and outMetadata and outMsg in not used there, so passing NULL's is safe.
		jrd_tra* tra = req_transaction;

		executeReceiveWithRestarts(tdbb, &tra, nullptr, nullptr, msgBuffer, false, false, true);
		fb_assert(tra == req_transaction);
	}
	else
	{
		MessageNode* msg = dsqlStatement->getStatement()->getMessage(dsqlStatement->getReceiveMsg()->msg_number);
		const Format* fmt = msg->getFormat(request);

		JRD_receive(tdbb, request, msg->messageNumber, fmt->fmt_length, msgBuffer);
	}

	firstRowFetched = true;

	if (!(request->req_flags & req_active))
	{
		if (req_timer)
			req_timer->stop();

		trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
		return false;
	}

	trace.fetch(false, ITracePlugin::RESULT_SUCCESS);
	return true;
}

// Set a cursor name for a dynamic request.
void DsqlDmlRequest::setCursor(thread_db* tdbb, const TEXT* name)
{
	SET_TDBB(tdbb);

	Jrd::ContextPoolHolder context(tdbb, &getPool());

	constexpr size_t MAX_CURSOR_LENGTH = 132 - 1;
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
	const UCHAR* inMsg, IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton)
{
	firstRowFetched = false;
	const dsql_msg* message = dsqlStatement->getSendMsg();

	if (!message)
	{
		JRD_start(tdbb, request, req_transaction);
	}
	else
	{
		fb_assert(inMsg != nullptr);

		const ULONG inMsgLength = dsqlStatement->getStatement()->getMessage(message->msg_number)->getFormat(request)->fmt_length;
		JRD_start_and_send(tdbb, request, req_transaction, message->msg_number,
			inMsgLength, inMsg);
	}

	// Selectable execute block should get the "proc fetch" flag assigned,
	// which ensures that the savepoint stack is preserved while suspending
	if (dsqlStatement->getType() == DsqlStatement::TYPE_SELECT_BLOCK)
		request->req_flags |= req_proc_fetch;

	message = dsqlStatement->getReceiveMsg();

	if (outMetadata == DELAYED_OUT_FORMAT)
	{
		needDelayedFormat = true;
		outMetadata = NULL;
	}

	if (message && (request->req_flags & req_active))
	{
		if (outMetadata)
		{
			metadataToFormat(outMetadata, message);
		}

		if (outMsg)
		{
			MessageNode* msg = dsqlStatement->getStatement()->getMessage(message->msg_number);
			// outMetadata can be nullptr. If not - it is already converted to message above
			const ULONG outMsgLength = msg->getFormat(request)->fmt_length;

			JRD_receive(tdbb, request, message->msg_number, outMsgLength, outMsg);

			// if this is a singleton select that return some data, make sure there's in fact one record

			if (singleton)
			{
				if (outMsgLength > 0)
				{
					// No record returned though expected
					if (!(request->req_flags & req_active))
					{
						status_exception::raise(Arg::Gds(isc_stream_eof));
					}

					// Create a temp message buffer and try one more receive.
					// If it succeed then the next record exists.

					std::unique_ptr<UCHAR[]> message_buffer(FB_NEW_POOL(getPool()) UCHAR[outMsgLength]);

					JRD_receive(tdbb, request, message->msg_number, outMsgLength, message_buffer.get());

					// Still active request means that second record exists
					if ((request->req_flags & req_active))
					{
						status_exception::raise(Arg::Gds(isc_sing_select_err));
					}
				}
			}
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

		default:
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
	{
		if (!inMetadata)
		{
			ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
					  Arg::Gds(isc_dsql_no_input_sqlda));
		}

		// If this is not first call of execute(), metadata most likely is already converted to message
		// but there is no easy way to check if they match so conversion is unconditional.
		// Even if value of inMetadata is the same, other instance could be placed in the same memory.
		// Even if the instance is the same, its content may be different from previous call.
		metadataToFormat(inMetadata, message);
	}

	mapCursorKey(tdbb);

	// we need to set new format before tracing of execution start to let trace
	// manager know statement parameters values
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this, inMsg);

	// Setup and start timeout timer
	const bool have_cursor = dsqlStatement->isCursorBased() && !singleton;

	setupTimer(tdbb);
	thread_db::TimerGuard timerGuard(tdbb, req_timer, !have_cursor);

	if (needRestarts())
		executeReceiveWithRestarts(tdbb, traHandle, inMsg, outMetadata, outMsg, singleton, true, false);
	else {
		doExecute(tdbb, traHandle, inMsg, outMetadata, outMsg, singleton);
	}

	trace.finish(have_cursor, ITracePlugin::RESULT_SUCCESS);
}

void DsqlDmlRequest::executeReceiveWithRestarts(thread_db* tdbb, jrd_tra** traHandle,
	const UCHAR* inMsg,
	IMessageMetadata* outMetadata, UCHAR* outMsg,
	bool singleton, bool exec, bool fetch)
{
	request->req_flags &= ~req_update_conflict;
	int numTries = 0;
	constexpr int MAX_RESTARTS = 10;

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
				doExecute(tdbb, traHandle, inMsg, outMetadata, outMsg, singleton);

			if (fetch)
			{
				fb_assert(dsqlStatement->isCursorBased());

				const dsql_msg* message = dsqlStatement->getReceiveMsg();
				const Format* fmt = dsqlStatement->getStatement()->getMessage(message->msg_number)->getFormat(request);

				JRD_receive(tdbb, request, message->msg_number, fmt->fmt_length, outMsg);
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

		TraceManager::event_dsql_restart(req_dbb->dbb_attachment, req_transaction, this, inMsg, numTries);

		// When restart we must execute query
		exec = true;
		// Next fetch will be performed by doExecute(), so do not call JRD_receive again
		fetch = false;
	}
}

void DsqlDmlRequest::metadataToFormat(Firebird::IMessageMetadata* meta, const dsql_msg* message)
{
	if (!message)
	{
		fb_assert(false);
		return;
	}
	if (!meta)
	{
		fb_assert(false);
		return;
	}

	MessageNode* msg = dsqlStatement->getStatement()->getMessage(message->msg_number);

	FbLocalStatus st;
	unsigned count = meta->getCount(&st);
	checkD(&st);

	const Format* oldFormat = msg->getFormat(nullptr);
	unsigned count2 = oldFormat->fmt_count;

	if (count * 2 != count2)
	{
		ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
				  Arg::Gds(isc_dsql_wrong_param_num) <<Arg::Num(count2 / 2) << Arg::Num(count));
	}

	if (count == 0)
	{
		// No point to continue, old format is fine
		return;
	}

	// Dumb pointer is fine: on error request pool will be destructed anyway
	Format* newFormat = Format::newFormat(*request->req_pool, count2);

	newFormat->fmt_length = meta->getMessageLength(&st);
	checkD(&st);

	unsigned assigned = 0; // Counter of initialized dsc in newFormat
	for (unsigned i = 0; i < count2; ++i)
	{
		const dsql_par* param = message->msg_parameters[i];
		if (param->par_index == 0)
		{
			// Skip parameters not bound to SQLDA: they must be null indicators handled later.
			continue;
		}

		unsigned index = param->par_index - 1;
		unsigned sqlType = meta->getType(&st, index);
		checkD(&st);
		unsigned sqlLength = meta->getLength(&st, index);
		checkD(&st);

		// For unknown reason parameters in Format has reversed order
		dsc& desc = newFormat->fmt_desc[param->par_parameter];
		desc.dsc_flags = 0;
		desc.dsc_dtype = fb_utils::sqlTypeToDscType(sqlType);
		desc.dsc_length = sqlLength;
		if (sqlType == SQL_VARYING)
			desc.dsc_length += sizeof(USHORT);
		desc.dsc_scale = meta->getScale(&st, index);
		checkD(&st);
		desc.dsc_sub_type = meta->getSubType(&st, index);
		checkD(&st);
		unsigned textType = meta->getCharSet(&st, index);
		checkD(&st);
		desc.setTextType(textType);
		desc.dsc_address = (UCHAR*)(IPTR) meta->getOffset(&st, index);
		checkD(&st);
		++assigned;

		if (param->par_null != nullptr)
		{
			dsc& null = newFormat->fmt_desc[param->par_null->par_parameter];
			null.dsc_dtype = dtype_short;
			null.dsc_scale = 0;
			null.dsc_length = sizeof(SSHORT);
			null.dsc_address = (UCHAR*)(IPTR) meta->getNullOffset(&st, index);
			checkD(&st);
			++assigned;
		}
	}
	// Last sanity check
	if (assigned != newFormat->fmt_count)
	{
		fb_assert(false);
		ERRD_post(Arg::Gds(isc_dsql_sqlda_err) <<
				  Arg::Gds(isc_dsql_wrong_param_num) <<Arg::Num(newFormat->fmt_count) << Arg::Num(assigned));
	}

	msg->setFormat(request, newFormat);
}

void DsqlDmlRequest::mapCursorKey(thread_db* tdbb)
{
	const auto dsqlStatement = getDsqlStatement();
	if (!dsqlStatement->parentCursorName.hasData())
		return;

	RecordKey dbKey = {};

	if (!parentRequest || !parentRequest->req_cursor) // It has been already closed
	{
		// Here may be code to re-establish link to parent cursor.
		ERRD_post(Arg::Gds(isc_dsql_cursor_err) <<
				  Arg::Gds(isc_dsql_cursor_not_found) << dsqlStatement->parentCursorName);
	}

	if (!parentRequest->req_cursor->getCurrentRecordKey(parentContext, dbKey))
	{
		ERRD_post(Arg::Gds(isc_cursor_not_positioned) << dsqlStatement->parentCursorName);
	}

	fb_assert(request);

	// Assign record key
	MessageNode* message = request->getStatement()->getMessage(2);
	fb_assert(message);

	dsc desc = message->getFormat(request)->fmt_desc[0];
	UCHAR* msgBuffer = message->getBuffer(request);
	desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

	dsc parentDesc;
	parentDesc.makeDbkey(&dbKey.recordNumber);

	MOVD_move(tdbb, &parentDesc, &desc);

	// Assign version number

	desc = message->getFormat(request)->fmt_desc[1];
	desc.dsc_address = msgBuffer + (IPTR) desc.dsc_address;

	// This is not a mistake, record version is represented as a DbKey in RecordKeyNode::make() to allow (theoretically now) positional DML on views
	parentDesc.makeDbkey(&dbKey.recordVersion);

	MOVD_move(tdbb, &parentDesc, &desc);
}

void DsqlDmlRequest::gatherRecordKey(RecordKey* buffer) const
{
	fb_assert(request->req_rpb.getCount() > 0);
	memset(buffer, 0, request->req_rpb.getCount() * sizeof(RecordKey));
	for (unsigned i = 0; i < request->req_rpb.getCount(); ++i)
	{
		record_param& rpb = request->req_rpb[i];
		if (rpb.rpb_relation && rpb.rpb_number.isValid() && !rpb.rpb_number.isBof())
		{
			buffer[i].recordNumber.bid_encode(rpb.rpb_number.getValue() + 1);
			buffer[i].recordNumber.bid_relation_id = rpb.rpb_relation->rel_id;
			buffer[i].recordVersion = rpb.rpb_transaction_nr;
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
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this, inMsg);

	fb_utils::init_status(tdbb->tdbb_status_vector);

	// run all statements under savepoint control
	{	// scope
		AutoSavePoint savePoint(tdbb, req_transaction);

		try
		{
			AutoSetRestoreFlag<ULONG> execDdl(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);

			//// Doing it in DFW_perform_work to avoid problems with DDL+DML in the same transaction.
			///req_dbb->dbb_attachment->att_dsql_instance->dbb_statement_cache->purgeAllAttachments(tdbb);

			node->executeDdl(tdbb, internalScratch, req_transaction);

			const bool isInternalRequest =
				(internalScratch->flags & DsqlCompilerScratch::FLAG_INTERNAL_REQUEST);

			if (!isInternalRequest && node->mustBeReplicated())
			{
				REPL_exec_sql(tdbb, req_transaction, getDsqlStatement()->getOrgText(),
					*getDsqlStatement()->getSchemaSearchPath());
			}
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
}

// Execute a dynamic SQL statement.
void DsqlTransactionRequest::execute(thread_db* tdbb, jrd_tra** traHandle,
	IMessageMetadata* /*inMetadata*/, const UCHAR* inMsg,
	IMessageMetadata* /*outMetadata*/, UCHAR* /*outMsg*/,
	bool /*singleton*/)
{
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this, inMsg);
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
	TraceDSQLExecute trace(req_dbb->dbb_attachment, this, inMsg);
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
