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
 * Adriano dos Santos Fernandes
 */

#ifndef JRD_STATEMENT_H
#define JRD_STATEMENT_H

#include "../include/fb_blk.h"
#include "../jrd/exe.h"
#include "../jrd/req.h"
#include "../jrd/EngineInterface.h"
#include "../jrd/SharedReadVector.h"
#include "../jrd/intl.h"
#include <functional>
#include "../common/sha2/sha2.h"

namespace Jrd {

class PlanEntry;

// Compiled statement.
class Statement : public pool_alloc<type_req>
{
	typedef SharedReadVector<Request*, 16> Requests;

public:
	static const unsigned FLAG_SYS_TRIGGER	= 0x01;
	static const unsigned FLAG_INTERNAL		= 0x02;
	static const unsigned FLAG_IGNORE_PERM	= 0x04;
	//static const unsigned FLAG_VERSION4	= 0x08;
	static const unsigned FLAG_POWERFUL		= FLAG_SYS_TRIGGER | FLAG_INTERNAL | FLAG_IGNORE_PERM;

	//static const unsigned MAP_LENGTH;		// CVC: Moved to dsql/Nodes.h as STREAM_MAP_LENGTH
	static const unsigned MAX_CLONES = 1000;
	static const unsigned MAX_REQUEST_SIZE = 50 * 1048576;	// 50 MB - just to be safe

private:
	Statement(thread_db* tdbb, MemoryPool* p, CompilerScratch* csb);
	Request* getRequest(thread_db* tdbb, const Requests::ReadAccessor& g, USHORT level);

public:
	static Statement* makeStatement(thread_db* tdbb, CompilerScratch* csb, bool internalFlag,
		std::function<void ()> beforeCsbRelease = nullptr);

	static Statement* makeBoolExpression(thread_db* tdbb, BoolExprNode*& node,
		CompilerScratch* csb, bool internalFlag);

	static Statement* makeValueExpression(thread_db* tdbb, ValueExprNode*& node, dsc& desc,
		CompilerScratch* csb, bool internalFlag);

	static Request* makeRequest(thread_db* tdbb, CompilerScratch* csb, bool internalFlag);

	Request* makeRootRequest(thread_db* tdbb)
	{
		auto g = requests.readAccessor();
		if (g->getCount())
			return g->value(0);

		return getRequest(tdbb, g, 0);
	}

	StmtNumber getStatementId() const
	{
		if (!id)
			id = JRD_get_thread_data()->getDatabase()->generateStatementId();
		return id;
	}

	unsigned getSize() const
	{
		return (unsigned) pool->getStatsGroup().getCurrentUsage();
	}

	const Routine* getRoutine() const;
	//bool isActive() const;

	Request* findRequest(thread_db* tdbb, bool unique = false);
	Request* getUserRequest(thread_db* tdbb, USHORT level);

	Request* rootRequest()
	{
		auto g = requests.readAccessor();
		return g->getCount() == 0 ? nullptr : g->value(0);
	}

	void restartRequests(thread_db* tdbb, jrd_tra* trans);

	void verifyAccess(thread_db* tdbb);
	Request* verifyRequestSynchronization(USHORT level);
	void release(thread_db* tdbb);

	Firebird::string getPlan(thread_db* tdbb, bool detailed) const;
	void getPlan(thread_db* tdbb, PlanEntry& planEntry) const;

	const Resources* getResources()
	{
		return resources;
	}
	MessageNode* getMessage(USHORT messageNumber) const;

private:
	static void verifyTriggerAccess(thread_db* tdbb, const jrd_rel* ownerRelation, const Triggers& triggers,
		MetaName userName);
	static void triggersExternalAccess(thread_db* tdbb, ExternalAccessList& list, const Triggers& tvec, const MetaName &user);
	void buildExternalAccess(thread_db* tdbb, ExternalAccessList& list, const MetaName& user);

	void loadResources(thread_db* tdbb, Request* req, bool withLock);

public:
	MemoryPool* pool;
	unsigned flags;						// statement flags
	unsigned blrVersion;
	ULONG impureSize;					// Size of impure area
	mutable StmtNumber id;				// statement identifier
	CSetId charSetId;					// client character set (CS_METADATA for internal statements)
	Request::RecordParameters rpbsSetup;

private:
	Requests requests;					// vector of requests
	Firebird::Mutex requestsGrow;		// requests' vector protection when adding new element

public:
	ExternalAccessList externalList;	// Access to procedures/triggers to be checked
	AccessItemList accessList;			// Access items to be checked
	const jrd_prc* procedure;			// procedure, if any
	const Function* function;			// function, if any
	QualifiedName triggerName;		// name of request (trigger), if any
	Jrd::UserId* triggerInvoker;		// user name if trigger run with SQL SECURITY DEFINER
	Statement* parentStatement;		// Sub routine's parent statement
	Firebird::Array<Statement*> subStatements;	// Array of subroutines' statements
	const StmtNode* topNode;			// top of execution tree
	Firebird::Array<const Select*> fors;	// select expressions
	Firebird::Array<const DeclareLocalTableNode*> localTables;	// local tables
	Firebird::Array<ULONG*> invariants;	// pointer to nodes invariant offsets
	Firebird::RefStrPtr sqlText;		// SQL text (encoded in the metadata charset)
	Firebird::Array<UCHAR> blr;			// BLR for non-SQL query
	MapFieldInfo mapFieldInfo;			// Map field name to field info

private:
	Resources* resources;				// Resources (relations, routines, etc.)
	Firebird::RefPtr<VersionedObjects> latestVer;
	Firebird::Mutex lvMutex;			// Protects upgrade of latestVer
	Firebird::Array<MessageNode*> messages;	// Input/output messages
};


} // namespace Jrd

#endif // JRD_STATEMENT_H
