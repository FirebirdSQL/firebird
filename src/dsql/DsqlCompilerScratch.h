/*
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
 * Adriano dos Santos Fernandes
 */

#ifndef DSQL_COMPILER_SCRATCH_H
#define DSQL_COMPILER_SCRATCH_H

#include "../jrd/jrd.h"
#include "../dsql/dsql.h"
#include "../dsql/DsqlStatements.h"
#include "../dsql/BlrDebugWriter.h"
#include "../common/classes/array.h"
#include "../jrd/MetaName.h"
#include "../common/classes/stack.h"
#include "../common/classes/alloc.h"
#include <initializer_list>
#include <optional>
#include <variant>

namespace Jrd
{

class BinaryBoolNode;
class LocalDeclarationsNode;
class DeclareCursorNode;
class DeclareLocalTableNode;
class DeclareVariableNode;
class ParameterClause;
class RseNode;
class SelectExprNode;
class TypeClause;
class VariableNode;
class WithClause;

typedef Firebird::Pair<
	Firebird::NonPooled<NestConst<ValueListNode>, NestConst<ValueListNode>>> ReturningClause;


// DSQL Compiler scratch block - may be discarded after compilation in the future.
class DsqlCompilerScratch : public BlrDebugWriter
{
public:
	static const unsigned FLAG_IN_AUTO_TRANS_BLOCK	= 0x0001;
	static const unsigned FLAG_RETURNING_INTO		= 0x0002;
	static const unsigned FLAG_METADATA_SAVED		= 0x0004;
	static const unsigned FLAG_PROCEDURE			= 0x0008;
	static const unsigned FLAG_TRIGGER				= 0x0010;
	static const unsigned FLAG_BLOCK				= 0x0020;
	static const unsigned FLAG_RECURSIVE_CTE		= 0x0040;
	static const unsigned FLAG_UPDATE_OR_INSERT		= 0x0080;
	static const unsigned FLAG_FUNCTION				= 0x0200;
	static const unsigned FLAG_SUB_ROUTINE			= 0x0400;
	static const unsigned FLAG_INTERNAL_REQUEST		= 0x0800;
	static const unsigned FLAG_AMBIGUOUS_STMT		= 0x1000;
	static const unsigned FLAG_DDL					= 0x2000;
	static const unsigned FLAG_FETCH				= 0x4000;
	static const unsigned FLAG_VIEW_WITH_CHECK		= 0x8000;

	static const unsigned MAX_NESTING = 512;

public:
	DsqlCompilerScratch(MemoryPool& p, dsql_dbb* aDbb, jrd_tra* aTransaction,
				DsqlStatement* aDsqlStatement = nullptr, DsqlCompilerScratch* aMainScratch = nullptr)
		: BlrDebugWriter(p),
		  dbb(aDbb),
		  transaction(aTransaction),
		  dsqlStatement(aDsqlStatement),
		  mainContext(p),
		  context(&mainContext),
		  unionContext(p),
		  derivedContext(p),
		  labels(p),
		  cursors(p),
		  localTables(p),
		  aliasRelationPrefix(p),
		  package(p),
		  currCtes(p),
		  hiddenVariables(p),
		  variables(p),
		  outputVariables(p),
		  mainScratch(aMainScratch),
		  outerMessagesMap(p),
		  outerVarsMap(p),
		  ddlSchema(p),
		  ctes(p),
		  cteAliases(p),
		  subFunctions(p),
		  subProcedures(p)
	{
	}

protected:
	// DsqlCompilerScratch should never be destroyed using delete.
	// It dies together with it's pool in release_request().
	~DsqlCompilerScratch()
	{
	}

public:
#ifdef DSQL_DEBUG
	static void dumpContextStack(const DsqlContextStack* stack);
#endif

public:
	virtual bool isVersion4()
	{
		return dsqlStatement->getBlrVersion() == 4;
	}

	MemoryPool& getPool()
	{
		return PermanentStorage::getPool();
	}

	dsql_dbb* getAttachment()
	{
		return dbb;
	}

	jrd_tra* getTransaction()
	{
		return transaction;
	}

	void setTransaction(jrd_tra* value)
	{
		transaction = value;
	}

	DsqlStatement* getDsqlStatement() const
	{
		return dsqlStatement;
	}

	void setDsqlStatement(DsqlStatement* aDsqlStatement)
	{
		dsqlStatement = aDsqlStatement;
	}

	void qualifyNewName(QualifiedName& name) const;
	void qualifyExistingName(QualifiedName& name, std::initializer_list<ObjectType> objectTypes);

	void qualifyExistingName(QualifiedName& name, ObjectType objectType)
	{
		qualifyExistingName(name, {objectType});
	}

	std::variant<std::monostate, dsql_prc*, dsql_rel*, dsql_udf*> resolveRoutineOrRelation(QualifiedName& name,
		std::initializer_list<ObjectType> objectTypes);

	void putBlrMarkers(ULONG marks);
	void putDtype(const TypeClause* field, bool useSubType);
	void putType(const TypeClause* type, bool useSubType);
	void putLocalVariableDecl(dsql_var* variable, DeclareVariableNode* hostParam, QualifiedName& collationName);
	void putLocalVariableInit(dsql_var* variable, const DeclareVariableNode* hostParam);

	void putLocalVariable(dsql_var* variable)
	{
		QualifiedName dummyCollationName;
		putLocalVariableDecl(variable, nullptr, dummyCollationName);
		putLocalVariableInit(variable, nullptr);
	}

	void putOuterMaps();
	dsql_var* makeVariable(dsql_fld*, const char*, const dsql_var::Type type, USHORT,
		USHORT, std::optional<USHORT> = std::nullopt);
	dsql_var* resolveVariable(const MetaName& varName);
	void genReturn(bool eosFlag = false);

	void genParameters(Firebird::Array<NestConst<ParameterClause> >& parameters,
		Firebird::Array<NestConst<ParameterClause> >& returns);

	// Get rid of any predefined contexts created for a view or trigger definition.
	// Also reset hidden variables.
	void resetContextStack()
	{
		context->clear();
		contextNumber = 0;
		derivedContextNumber = 0;
		nextVarNumber = 0;
		hiddenVariables.clear();
	}

	void resetTriggerContextStack()
	{
		context->clear();
		contextNumber = 0;
	}

	void addCTEs(WithClause* withClause);
	SelectExprNode* findCTE(const MetaName& name);
	void clearCTEs();
	void checkUnusedCTEs();

	// hvlad: each member of recursive CTE can refer to CTE itself (only once) via
	// CTE name or via alias. We need to substitute this aliases when processing CTE
	// member to resolve field names. Therefore we store all aliases in order of
	// occurrence and later use it in backward order (since our parser is right-to-left).
	// Also we put CTE name after all such aliases to distinguish aliases for
	// different CTE's.
	// We also need to repeat this process if main select expression contains union with
	// recursive CTE
	void addCTEAlias(const Firebird::string& alias)
	{
		thread_db* tdbb = JRD_get_thread_data();
		fb_assert(currCteAlias == NULL);
		cteAliases.add(FB_NEW_POOL(*tdbb->getDefaultPool()) Firebird::string(*tdbb->getDefaultPool(), alias));
	}

	const Firebird::string* getNextCTEAlias()
	{
		return *(--currCteAlias);
	}

	void resetCTEAlias(const Firebird::string& alias)
	{
#ifdef DEV_BUILD
		const Firebird::string* const* begin = cteAliases.begin();
#endif

		currCteAlias = cteAliases.end() - 1;
		fb_assert(currCteAlias >= begin);

		const Firebird::string* curr = *(currCteAlias);

		while (strcmp(curr->c_str(), alias.c_str()))
		{
			currCteAlias--;
			fb_assert(currCteAlias >= begin);

			curr = *(currCteAlias);
		}
	}

	USHORT reserveVarNumber()
	{
		return nextVarNumber++;
	}

	void reserveInitialVarNumbers(USHORT count)
	{
		fb_assert(nextVarNumber == 0);
		nextVarNumber = count;
	}

	bool isPsql() const { return psql; }
	void setPsql(bool value) { psql = value; }

	const auto& getSubFunctions() const
	{
		return subFunctions;
	}

	DeclareSubFuncNode* getSubFunction(const MetaName& name);
	void putSubFunction(DeclareSubFuncNode* subFunc, bool replace = false);

	const auto& getSubProcedures() const
	{
		return subProcedures;
	}

	DeclareSubProcNode* getSubProcedure(const MetaName& name);
	void putSubProcedure(DeclareSubProcNode* subProc, bool replace = false);

private:
	SelectExprNode* pass1RecursiveCte(SelectExprNode* input);
	RseNode* pass1RseIsRecursive(RseNode* input);
	bool pass1RelProcIsRecursive(RecordSourceNode* input);
	BoolExprNode* pass1JoinIsRecursive(RecordSourceNode*& input);

	dsql_dbb* dbb = nullptr;				// DSQL attachment
	jrd_tra* transaction = nullptr;			// Transaction
	DsqlStatement* dsqlStatement = nullptr;	// DSQL statement

public:
	unsigned flags = 0;						// flags
	unsigned nestingLevel = 0;				// begin...end nesting level
	dsql_rel* relation = nullptr;			// relation created by this request (for DDL)
	DsqlContextStack mainContext;
	DsqlContextStack* context = nullptr;
	DsqlContextStack unionContext;			// Save contexts for views of unions
	DsqlContextStack derivedContext;		// Save contexts for views of derived tables
	dsql_ctx* outerAggContext = nullptr;	// agg context for outer ref
	// CVC: I think the two contexts may need a bigger var, too.
	USHORT contextNumber = 0;				// Next available context number
	USHORT derivedContextNumber = 0;		// Next available context number for derived tables
	USHORT scopeLevel = 0;					// Scope level for parsing aliases in subqueries
	USHORT loopLevel = 0;					// Loop level
	Firebird::Stack<MetaName*> labels;		// Loop labels
	USHORT cursorNumber = 0;				// Cursor number
	Firebird::Array<DeclareCursorNode*> cursors; // Cursors
	USHORT localTableNumber = 0;			// Local table number
	Firebird::Array<DeclareLocalTableNode*> localTables; // Local tables
	USHORT inSelectList = 0;				// now processing "select list"
	USHORT inWhereClause = 0;				// processing "where clause"
	USHORT inGroupByClause = 0;				// processing "group by clause"
	USHORT inHavingClause = 0;				// processing "having clause"
	USHORT inOrderByClause = 0;				// processing "order by clause"
	USHORT errorHandlers = 0;				// count of active error handlers
	USHORT clientDialect = 0;				// dialect passed into the API call
	USHORT inOuterJoin = 0;					// processing inside outer-join part
	Firebird::ObjectsArray<QualifiedName> aliasRelationPrefix;	// prefix for every relation-alias.
	QualifiedName package;				// package being defined
	Firebird::Stack<SelectExprNode*> currCtes;	// current processing CTE's
	dsql_ctx* recursiveCtx = nullptr;		// context of recursive CTE
	USHORT recursiveCtxId = 0;				// id of recursive union stream context
	bool processingWindow = false;			// processing window functions
	bool checkConstraintTrigger = false;	// compiling a check constraint trigger
	dsc domainValue;						// VALUE in the context of domain's check constraint
	Firebird::Array<dsql_var*> hiddenVariables;	// hidden variables
	Firebird::Array<dsql_var*> variables;
	Firebird::Array<dsql_var*> outputVariables;
	ReturningClause* returningClause = nullptr;
	const Firebird::string* const* currCteAlias = nullptr;
	DsqlCompilerScratch* mainScratch = nullptr;
	Firebird::NonPooledMap<USHORT, USHORT> outerMessagesMap;	// <outer, inner>
	Firebird::NonPooledMap<USHORT, USHORT> outerVarsMap;		// <outer, inner>
	MetaName ddlSchema;
	Firebird::AutoPtr<Firebird::ObjectsArray<Firebird::MetaString>> cachedDdlSchemaSearchPath;

private:
	Firebird::HalfStaticArray<SelectExprNode*, 4> ctes; // common table expressions
	Firebird::HalfStaticArray<const Firebird::string*, 4> cteAliases; // CTE aliases in recursive members
	USHORT nextVarNumber = 0;				// Next available variable number
	bool psql = false;
	Firebird::LeftPooledMap<MetaName, DeclareSubFuncNode*> subFunctions;
	Firebird::LeftPooledMap<MetaName, DeclareSubProcNode*> subProcedures;
};

class PsqlChanger
{
public:
	PsqlChanger(DsqlCompilerScratch* aDsqlScratch, bool value)
		: dsqlScratch(aDsqlScratch),
		  oldValue(dsqlScratch->isPsql())
	{
		dsqlScratch->setPsql(value);
	}

	~PsqlChanger()
	{
		dsqlScratch->setPsql(oldValue);
	}

private:
	// copying is prohibited
	PsqlChanger(const PsqlChanger&);
	PsqlChanger& operator =(const PsqlChanger&);

	DsqlCompilerScratch* dsqlScratch;
	const bool oldValue;
};

}	// namespace Jrd

#endif // DSQL_COMPILER_SCRATCH_H
