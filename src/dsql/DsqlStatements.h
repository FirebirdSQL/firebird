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

#ifndef DSQL_STATEMENTS_H
#define DSQL_STATEMENTS_H

#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/NestConst.h"
#include "../jrd/jrd.h"
#include "../jrd/ntrace.h"
#include "../dsql/DsqlRequests.h"

namespace Jrd {


class DdlNode;
class dsql_dbb;
class dsql_msg;
class dsql_par;
class DsqlRequest;
class DsqlCompilerScratch;
class Statement;
class SessionManagementNode;
class TransactionNode;


// Compiled statement - shared by multiple requests.
class DsqlStatement : public Firebird::PermanentStorage
{
public:
	enum Type	// statement type
	{
		TYPE_SELECT, TYPE_SELECT_UPD, TYPE_INSERT, TYPE_DELETE, TYPE_UPDATE, TYPE_UPDATE_CURSOR,
		TYPE_DELETE_CURSOR, TYPE_COMMIT, TYPE_ROLLBACK, TYPE_CREATE_DB, TYPE_DDL, TYPE_START_TRANS,
		TYPE_EXEC_PROCEDURE, TYPE_COMMIT_RETAIN, TYPE_ROLLBACK_RETAIN, TYPE_SET_GENERATOR,
		TYPE_SAVEPOINT, TYPE_EXEC_BLOCK, TYPE_SELECT_BLOCK, TYPE_SESSION_MANAGEMENT,
		TYPE_RETURNING_CURSOR
	};

	// Statement flags.
	static const unsigned FLAG_ORPHAN		= 0x01;
	static const unsigned FLAG_NO_BATCH		= 0x02;
	//static const unsigned FLAG_BLR_VERSION4	= 0x04;
	//static const unsigned FLAG_BLR_VERSION5	= 0x08;
	static const unsigned FLAG_SELECTABLE	= 0x10;

	static void rethrowDdlException(Firebird::status_exception& ex, bool metadataUpdate, DdlNode* node);

public:
	DsqlStatement(MemoryPool& p, dsql_dbb* aDsqlAttachment)
		: PermanentStorage(p),
		  dsqlAttachment(aDsqlAttachment),
		  type(TYPE_SELECT),
		  flags(0),
		  blrVersion(5),
		  ports(p)
	{
	}

protected:
	virtual ~DsqlStatement() = default;

public:
	int addRef()
	{
		return ++refCounter;
	}

	int release();

	bool isCursorBased() const
	{
		switch (type)
		{
			case TYPE_SELECT:
			case TYPE_SELECT_BLOCK:
			case TYPE_SELECT_UPD:
			case TYPE_RETURNING_CURSOR:
				return true;
		}

		return false;
	}

	Type getType() const { return type; }
	void setType(Type value) { type = value; }

	ULONG getFlags() const { return flags; }
	void setFlags(ULONG value) { flags = value; }
	void addFlags(ULONG value) { flags |= value; }

	unsigned getBlrVersion() const { return blrVersion; }
	void setBlrVersion(unsigned value) { blrVersion = value; }

	Firebird::RefStrPtr& getSqlText() { return sqlText; }
	const Firebird::RefStrPtr& getSqlText() const { return sqlText; }
	void setSqlText(Firebird::RefString* value) { sqlText = value; }

	void setOrgText(const char* ptr, ULONG len);
	const Firebird::string& getOrgText() const { return *orgText; }

	Firebird::Array<dsql_msg*>& getPorts() { return ports; }

	dsql_msg* getSendMsg() { return sendMsg; }
	const dsql_msg* getSendMsg() const { return sendMsg; }
	void setSendMsg(dsql_msg* value) { sendMsg = value; }

	dsql_msg* getReceiveMsg() { return receiveMsg; }
	const dsql_msg* getReceiveMsg() const { return receiveMsg; }
	void setReceiveMsg(dsql_msg* value) { receiveMsg = value; }

	dsql_par* getEof() { return eof; }
	const dsql_par* getEof() const { return eof; }
	void setEof(dsql_par* value) { eof = value; }

public:
	virtual Statement* getStatement() const
	{
		return nullptr;
	}

	virtual bool mustBeReplicated() const
	{
		return false;
	}

	virtual bool shouldPreserveScratch() const
	{
		return true;
	}

	virtual void dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult) = 0;
	virtual DsqlRequest* createRequest(thread_db* tdbb, dsql_dbb* dbb) = 0;

protected:
	virtual void doRelease();

protected:
	dsql_dbb* dsqlAttachment;
	Type type;					// Type of statement
	ULONG flags;				// generic flag
	unsigned blrVersion;
	Firebird::RefStrPtr sqlText;
	Firebird::RefStrPtr orgText;
	Firebird::Array<dsql_msg*> ports;			// Port messages
	dsql_msg* sendMsg = nullptr;				// Message to be sent to start request
	dsql_msg* receiveMsg = nullptr;				// Per record message to be received
	dsql_par* eof = nullptr;					// End of file parameter

private:
	Firebird::AtomicCounter refCounter;
};


class DsqlDmlStatement final : public DsqlStatement
{
public:
	DsqlDmlStatement(MemoryPool& p, dsql_dbb* aDsqlAttachment, StmtNode* aNode)
		: DsqlStatement(p, aDsqlAttachment),
		  node(aNode)
	{
	}

public:
	Statement* getStatement() const override
	{
		return statement;
	}

	void dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult) override;
	DsqlDmlRequest* createRequest(thread_db* tdbb, dsql_dbb* dbb) override;

	dsql_par* getDbKey() { return dbKey; }
	const dsql_par* getDbKey() const { return dbKey; }
	void setDbKey(dsql_par* value) { dbKey = value; }

	dsql_par* getRecVersion() { return recVersion; }
	const dsql_par* getRecVersion() const { return recVersion; }
	void setRecVersion(dsql_par* value) { recVersion = value; }

	dsql_par* getParentRecVersion() { return parentRecVersion; }
	const dsql_par* getParentRecVersion() const { return parentRecVersion; }
	void setParentRecVersion(dsql_par* value) { parentRecVersion = value; }

	dsql_par* getParentDbKey() { return parentDbKey; }
	const dsql_par* getParentDbKey() const { return parentDbKey; }
	void setParentDbKey(dsql_par* value) { parentDbKey = value; }

	DsqlDmlRequest* getParentRequest() const { return parentRequest; }
	void setParentRequest(DsqlDmlRequest* value) { parentRequest = value; }

protected:
	void doRelease() override;

private:
	NestConst<StmtNode> node;
	Statement* statement = nullptr;
	dsql_par* dbKey = nullptr;					// Database key for current of
	dsql_par* recVersion = nullptr;				// Record Version for current of
	dsql_par* parentRecVersion = nullptr;		// parent record version
	dsql_par* parentDbKey = nullptr;			// Parent database key for current of
	DsqlDmlRequest* parentRequest = nullptr;	// Source request, if cursor update
};


class DsqlDdlStatement final : public DsqlStatement
{
public:
	DsqlDdlStatement(MemoryPool& p, dsql_dbb* aDsqlAttachment, DdlNode* aNode)
		: DsqlStatement(p, aDsqlAttachment),
		  node(aNode)
	{
	}

	~DsqlDdlStatement();

public:
	bool mustBeReplicated() const override;
	void dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult) override;
	DsqlDdlRequest* createRequest(thread_db* tdbb, dsql_dbb* dbb) override;

private:
	NestConst<DdlNode> node;
	DsqlCompilerScratch* scratch = nullptr;
};


class DsqlTransactionStatement final : public DsqlStatement
{
public:
	DsqlTransactionStatement(MemoryPool& p, dsql_dbb* aDsqlAttachment, TransactionNode* aNode)
		: DsqlStatement(p, aDsqlAttachment),
		  node(aNode)
	{
	}

	~DsqlTransactionStatement();

public:
	void dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult) override;
	DsqlTransactionRequest* createRequest(thread_db* tdbb, dsql_dbb* dbb) override;

private:
	NestConst<TransactionNode> node;
	DsqlCompilerScratch* scratch = nullptr;
};


class DsqlSessionManagementStatement final : public DsqlStatement
{
public:
	DsqlSessionManagementStatement(MemoryPool& p, dsql_dbb* aDsqlAttachment, SessionManagementNode* aNode)
		: DsqlStatement(p, aDsqlAttachment),
		  node(aNode)
	{
	}

	~DsqlSessionManagementStatement();

public:
	void dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult) override;
	DsqlSessionManagementRequest* createRequest(thread_db* tdbb, dsql_dbb* dbb) override;

private:
	NestConst<SessionManagementNode> node;
	DsqlCompilerScratch* scratch = nullptr;
};


}	// namespace Jrd

#endif // DSQL_STATEMENTS_H
