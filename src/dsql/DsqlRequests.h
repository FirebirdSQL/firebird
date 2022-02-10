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
 * 2022.02.07 Adriano dos Santos Fernandes: Refactored from dsql.h
 */

#ifndef DSQL_REQUESTS_H
#define DSQL_REQUESTS_H

#include "firebird/Interface.h"
#include "../common/StatusArg.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/RefCounted.h"
#include "../jrd/jrd.h"

namespace Jrd {


class DdlNode;
class dsql_dbb;
class DsqlStatement;
class DsqlCompilerScratch;
class DsqlCursor;
class DsqlDmlStatement;
class dsql_par;
class jrd_req;
class jrd_tra;
class Statement;
class SessionManagementNode;
class TransactionNode;


class DsqlRequest : public Firebird::PermanentStorage
{
public:
	DsqlRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aStatement);
	virtual ~DsqlRequest();

public:
	jrd_tra* getTransaction()
	{
		return req_transaction;
	}

	Firebird::RefPtr<DsqlStatement> getDsqlStatement()
	{
		return dsqlStatement;
	}

	virtual Statement* getStatement() const
	{
		return nullptr;
	}

	virtual jrd_req* getRequest() const
	{
		return nullptr;
	}

	virtual bool isDml() const
	{
		return false;
	}

	virtual DsqlCursor* openCursor(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMeta, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMeta, ULONG flags)
	{
		Firebird::Arg::Gds(isc_no_cursor).raise();
	}

	virtual DsqlBatch* openBatch(thread_db* tdbb, Firebird::IMessageMetadata* inMetadata,
		unsigned parLength, const UCHAR* par)
	{
		(Firebird::Arg::Gds(isc_sqlerr) <<
			Firebird::Arg::Num(-504) <<
			Firebird::Arg::Gds(isc_unprepared_stmt)
		).raise();
	}

	virtual void execute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMetadata, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton) = 0;

	virtual void setCursor(thread_db* tdbb, const TEXT* name);

	virtual bool fetch(thread_db* tdbb, UCHAR* buffer);

	virtual void setDelayedFormat(thread_db* tdbb, Firebird::IMessageMetadata* metadata);

	// Get session-level timeout, milliseconds
	unsigned int getTimeout();

	// Set session-level timeout, milliseconds
	void setTimeout(unsigned int timeOut);

	// Get actual timeout, milliseconds
	unsigned int getActualTimeout();

	// Evaluate actual timeout value, consider config- and session-level timeout values,
	// setup and start timer
	TimeoutTimer* setupTimer(thread_db* tdbb);

	USHORT parseMetadata(Firebird::IMessageMetadata* meta, const Firebird::Array<dsql_par*>& parameters_list);

	static void destroy(thread_db* tdbb, DsqlRequest* request);

public:
	dsql_dbb* req_dbb;					// DSQL attachment
	Firebird::RefPtr<DsqlStatement> dsqlStatement;
	Firebird::Array<DsqlDmlStatement*> cursors{getPool()};	// Cursor update statements

	jrd_tra* req_transaction = nullptr;	// JRD transaction

	Firebird::string req_cursor_name{getPool()};	// Cursor name, if any
	DsqlCursor* req_cursor = nullptr;	// Open cursor, if any
	DsqlBatch* req_batch = nullptr;		// Active batch, if any
	Firebird::NonPooledMap<const dsql_par*, dsc> req_user_descs{getPool()}; // SQLDA data type

	Firebird::AutoPtr<Jrd::RuntimeStatistics> req_fetch_baseline; // State of request performance counters when we reported it last time
	SINT64 req_fetch_elapsed = 0;	// Number of clock ticks spent while fetching rows for this request since we reported it last time
	SINT64 req_fetch_rowcount = 0;	// Total number of rows returned by this request
	bool req_traced = false;		// request is traced via TraceAPI

protected:
	unsigned int req_timeout = 0;				// query timeout in milliseconds, set by the user
	Firebird::RefPtr<TimeoutTimer> req_timer;	// timeout timer
};


class DsqlDmlRequest final : public DsqlRequest
{
public:
	DsqlDmlRequest(thread_db* tdbb, MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aDsqlStatement);

	// Reintroduce method to fake covariant return type with RefPtr.
	auto getDsqlStatement()
	{
		return Firebird::RefPtr<DsqlDmlStatement>((DsqlDmlStatement*) dsqlStatement.getPtr());
	}

	Statement* getStatement() const override;

	jrd_req* getRequest() const override
	{
		return request;
	}

	bool isDml() const override
	{
		return true;
	}

	DsqlCursor* openCursor(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMeta, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMeta, ULONG flags) override;

	DsqlBatch* openBatch(thread_db* tdbb, Firebird::IMessageMetadata* inMetadata,
		unsigned parLength, const UCHAR* par) override;

	void execute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMetadata, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton) override;

	void setCursor(thread_db* tdbb, const TEXT* name) override;

	bool fetch(thread_db* tdbb, UCHAR* buffer) override;

	void setDelayedFormat(thread_db* tdbb, Firebird::IMessageMetadata* metadata) override;

	void mapInOut(Jrd::thread_db* tdbb, bool toExternal, const dsql_msg* message, Firebird::IMessageMetadata* meta,
		UCHAR* dsql_msg_buf, const UCHAR* in_dsql_msg_buf = nullptr);

private:
	// True, if request could be restarted
	bool needRestarts();

	void doExecute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton);

	// [Re]start part of "request restarts" algorithm
	void executeReceiveWithRestarts(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton, bool exec, bool fetch);

public:
	Firebird::Array<UCHAR*>	req_msg_buffers;

private:
	Firebird::RefPtr<Firebird::IMessageMetadata> delayedFormat;
	jrd_req* request = nullptr;
	bool needDelayedFormat = false;
	bool firstRowFetched = false;
};


class DsqlDdlRequest final : public DsqlRequest
{
public:
	DsqlDdlRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlCompilerScratch* aInternalScratch, DdlNode* aNode);

	void execute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMetadata, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton) override;

private:
	DsqlCompilerScratch* internalScratch;
	NestConst<DdlNode> node;
};


class DsqlTransactionRequest final : public DsqlRequest
{
public:
	DsqlTransactionRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aStatement, TransactionNode* aNode);

	void execute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMetadata, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton) override;

private:
	NestConst<TransactionNode> node;
};


class DsqlSessionManagementRequest final : public DsqlRequest
{
public:
	DsqlSessionManagementRequest(MemoryPool& pool, dsql_dbb* dbb, DsqlStatement* aStatement,
			SessionManagementNode* aNode)
		: DsqlRequest(pool, dbb, aStatement),
		  node(aNode)
	{
	}

	void execute(thread_db* tdbb, jrd_tra** traHandle,
		Firebird::IMessageMetadata* inMetadata, const UCHAR* inMsg,
		Firebird::IMessageMetadata* outMetadata, UCHAR* outMsg,
		bool singleton) override;

private:
	NestConst<SessionManagementNode> node;
};


}	// namespace Jrd

#endif // DSQL_REQUESTS_H
