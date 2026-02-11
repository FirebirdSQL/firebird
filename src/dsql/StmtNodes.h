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
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_STMT_NODES_H
#define DSQL_STMT_NODES_H

#include <optional>
#include "../jrd/MetaName.h"
#include "firebird/impl/blr.h"
#include "../jrd/Function.h"
#include "../jrd/extds/ExtDS.h"
#include "../dsql/Nodes.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/NodePrinter.h"
#include "../common/DecFloat.h"
#include "../common/classes/TriState.h"

namespace Jrd {

class CompoundStmtNode;
class ExecBlockNode;
class ForNode;
class PlanNode;
class RecordBuffer;
class RelationSourceNode;
class SelectNode;
class GeneratorItem;


class ExceptionItem final : public Firebird::PermanentStorage, public Printable
{
public:
	enum Type : UCHAR
	{
		SQL_CODE = 1,
		SQL_STATE = 2,
		GDS_CODE = 3,
		XCP_CODE = 4,
		XCP_DEFAULT = 5
	};

	ExceptionItem(MemoryPool& pool, const ExceptionItem& o)
		: PermanentStorage(pool),
		  type(o.type),
		  code(o.code),
		  name(pool, o.name),
		  secName(pool, o.secName)
	{
	}

	explicit ExceptionItem(MemoryPool& pool)
		: PermanentStorage(pool),
		  code(0),
		  name(pool),
		  secName(pool)
	{
	}

	ExceptionItem& operator =(const ExceptionItem& o)
	{
		code = o.code;
		name = o.name;
		secName = o.secName;
		return *this;
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		NODE_PRINT(printer, type);
		NODE_PRINT(printer, code);
		NODE_PRINT(printer, name);
		NODE_PRINT(printer, secName);

		return "ExceptionItem";
	}

public:
	Type type;
	SLONG code;
	QualifiedName name;
	QualifiedName secName;
};

typedef Firebird::ObjectsArray<ExceptionItem> ExceptionArray;


struct ValidateInfo
{
	NestConst<BoolExprNode> boolean;
	NestConst<ValueExprNode> value;
};


enum OverrideClause : UCHAR
{
	USER_VALUE = blr_store_override_user,
	SYSTEM_VALUE = blr_store_override_system
};


class AssignmentNode final : public TypedNode<StmtNode, StmtNode::TYPE_ASSIGNMENT>
{
public:
	explicit AssignmentNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_ASSIGNMENT>(pool),
		  asgnFrom(NULL),
		  asgnTo(NULL),
		  missing(NULL),
		  missing2(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	static void validateTarget(thread_db* tdbb, CompilerScratch* csb, const ValueExprNode* target);
	static void dsqlValidateTarget(const ValueExprNode* target);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	AssignmentNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	AssignmentNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AssignmentNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	AssignmentNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<ValueExprNode> asgnFrom;
	NestConst<ValueExprNode> asgnTo;
	NestConst<ValueExprNode> missing;
	NestConst<ValueExprNode> missing2;
};


class BlockNode final : public TypedNode<StmtNode, StmtNode::TYPE_BLOCK>
{
public:
	explicit BlockNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_BLOCK>(pool),
		  action(NULL),
		  handlers(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	BlockNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	BlockNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	static bool testAndFixupError(thread_db* tdbb, Request* request, const ExceptionArray& conditions);

public:
	NestConst<StmtNode> action;
	NestConst<CompoundStmtNode> handlers;
};


class CompoundStmtNode : public TypedNode<StmtNode, StmtNode::TYPE_COMPOUND_STMT>	// blr_begin
{
public:
	explicit CompoundStmtNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_COMPOUND_STMT>(pool),
		  statements(pool),
		  onlyAssignments(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	bool isProfileAware() const override
	{
		return false;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	CompoundStmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	CompoundStmtNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	CompoundStmtNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	CompoundStmtNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	Firebird::Array<NestConst<StmtNode> > statements;
	bool onlyAssignments;
};


class ContinueLeaveNode final : public TypedNode<StmtNode, StmtNode::TYPE_CONTINUE_LEAVE>
{
public:
	explicit ContinueLeaveNode(MemoryPool& pool, UCHAR aBlrOp)
		: TypedNode<StmtNode, StmtNode::TYPE_CONTINUE_LEAVE>(pool),
		  blrOp(aBlrOp),
		  labelNumber(0),
		  dsqlLabelName(NULL)
	{
		fb_assert(blrOp == blr_continue_loop || blrOp == blr_leave);
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ContinueLeaveNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	ContinueLeaveNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		return this;
	}

	ContinueLeaveNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	UCHAR blrOp;
	USHORT labelNumber;
	MetaName* dsqlLabelName;
};


class CursorStmtNode final : public TypedNode<StmtNode, StmtNode::TYPE_CURSOR_STMT>
{
public:
	explicit CursorStmtNode(MemoryPool& pool, UCHAR aCursorOp, const MetaName& aDsqlName = "",
				ValueListNode* aDsqlIntoStmt = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_CURSOR_STMT>(pool),
		  dsqlName(pool, aDsqlName),
		  dsqlIntoStmt(aDsqlIntoStmt),
		  scrollExpr(NULL),
		  intoStmt(NULL),
		  cursorNumber(0),
		  cursorOp(aCursorOp),
		  scrollOp(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	CursorStmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	CursorStmtNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	CursorStmtNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	MetaName dsqlName;
	ValueListNode* dsqlIntoStmt;
	NestConst<ValueExprNode> scrollExpr;
	NestConst<StmtNode> intoStmt;
	USHORT cursorNumber;
	UCHAR cursorOp;
	UCHAR scrollOp;
};


class DeclareCursorNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_CURSOR>
{
public:
	static constexpr USHORT CUR_TYPE_NONE = 0;
	static constexpr USHORT CUR_TYPE_EXPLICIT = 1;
	static constexpr USHORT CUR_TYPE_FOR = 2;
	static constexpr USHORT CUR_TYPE_ALL = CUR_TYPE_EXPLICIT | CUR_TYPE_FOR;

	explicit DeclareCursorNode(MemoryPool& pool, const MetaName& aDsqlName = NULL,
				USHORT aDsqlCursorType = CUR_TYPE_NONE)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_CURSOR>(pool),
		  dsqlName(aDsqlName),
		  dsqlSelect(NULL),
		  rse(NULL),
		  refs(NULL),
		  cursor(NULL),
		  dsqlCursorType(aDsqlCursorType),
		  cursorNumber(0),
		  dsqlScroll(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	DeclareCursorNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	DeclareCursorNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	DeclareCursorNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	MetaName dsqlName;
	NestConst<SelectNode> dsqlSelect;
	NestConst<RseNode> rse;
	NestConst<ValueListNode> refs;
	NestConst<Cursor> cursor;
	USHORT dsqlCursorType;
	USHORT cursorNumber;
	bool dsqlScroll;
};


class DeclareLocalTableNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_LOCAL_TABLE>
{
public:
	struct Impure
	{
		RecordBuffer* recordBuffer;
	};

public:
	explicit DeclareLocalTableNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_LOCAL_TABLE>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	DeclareLocalTableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	DeclareLocalTableNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	DeclareLocalTableNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	DeclareLocalTableNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	Impure* getImpure(thread_db* tdbb, Request* request, bool createWhenDead = true) const;

public:
	NestConst<Format> format;
	USHORT tableNumber = 0;
};


class DeclareSubFuncNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBFUNC>
{
public:
	explicit DeclareSubFuncNode(MemoryPool& pool, const MetaName& aName)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBFUNC>(pool),
		  name(pool, aName),
		  dsqlSignature(pool, aName)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	bool isProfileAware() const override
	{
		return false;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	DeclareSubFuncNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	DeclareSubFuncNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	DeclareSubFuncNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	bool isForwardDecl() const;

private:
	static void parseParameters(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
		Firebird::Array<NestConst<Parameter> >& paramArray, USHORT* defaultCount = NULL);

	void genParameters(DsqlCompilerScratch* dsqlScratch,
		Firebird::Array<NestConst<ParameterClause> >& paramArray);

public:
	MetaName name;
	Signature dsqlSignature;
	NestConst<ExecBlockNode> dsqlBlock;
	DsqlCompilerScratch* blockScratch = nullptr;
	dsql_udf* dsqlFunction = nullptr;
	const UCHAR* blrStart = nullptr;
	CompilerScratch* subCsb = nullptr;
	Function* routine = nullptr;
	ULONG blrLength = 0;
	bool dsqlDeterministic = false;
};


class DeclareSubProcNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBPROC>
{
public:
	explicit DeclareSubProcNode(MemoryPool& pool, const MetaName& aName)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBPROC>(pool),
		  name(pool, aName),
		  dsqlSignature(pool, aName)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	bool isProfileAware() const override
	{
		return false;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	DeclareSubProcNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	DeclareSubProcNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	DeclareSubProcNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	bool isForwardDecl() const;

private:
	static void parseParameters(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
		Firebird::Array<NestConst<Parameter> >& paramArray, USHORT* defaultCount = NULL);

	void genParameters(DsqlCompilerScratch* dsqlScratch,
		Firebird::Array<NestConst<ParameterClause> >& paramArray);

public:
	MetaName name;
	Signature dsqlSignature;
	NestConst<ExecBlockNode> dsqlBlock;
	DsqlCompilerScratch* blockScratch = nullptr;
	dsql_prc* dsqlProcedure = nullptr;
	const UCHAR* blrStart = nullptr;
	CompilerScratch* subCsb = nullptr;
	jrd_prc* routine = nullptr;
	ULONG blrLength = 0;
};


class DeclareVariableNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_VARIABLE>
{
public:
	explicit DeclareVariableNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_VARIABLE>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	DeclareVariableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	DeclareVariableNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	DeclareVariableNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	DeclareVariableNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<ParameterClause> dsqlDef;
	dsc varDesc;
	USHORT varId = 0;
	bool usedInSubRoutines = false;
};


class EraseNode final : public TypedNode<StmtNode, StmtNode::TYPE_ERASE>
{
public:
	explicit EraseNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_ERASE>(pool),
		  dsqlCursorName(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	EraseNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	EraseNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	static void pass1Erase(thread_db* tdbb, CompilerScratch* csb, EraseNode* node);
	const StmtNode* erase(thread_db* tdbb, Request* request, WhichTrigger whichTrig) const;

public:
	NestConst<RelationSourceNode> dsqlRelation;
	NestConst<BoolExprNode> dsqlBoolean;
	NestConst<PlanNode> dsqlPlan;
	NestConst<ValueListNode> dsqlOrder;
	NestConst<RowsClause> dsqlRows;
	MetaName dsqlCursorName;
	NestConst<ReturningClause> dsqlReturning;
	NestConst<RseNode> dsqlRse;
	bool dsqlSkipLocked = false;
	dsql_ctx* dsqlContext = nullptr;
	NestConst<StmtNode> statement;
	NestConst<StmtNode> subStatement;
	NestConst<StmtNode> returningStatement;
	NestConst<ForNode> forNode;			// parent implicit cursor, if present
	StreamType stream = 0;
	unsigned marks = 0;					// see StmtNode::IUD_MARK_xxx
};


class ErrorHandlerNode final : public TypedNode<StmtNode, StmtNode::TYPE_ERROR_HANDLER>
{
public:
	explicit ErrorHandlerNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_ERROR_HANDLER>(pool),
		  action(NULL),
		  conditions(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ErrorHandlerNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ErrorHandlerNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ErrorHandlerNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<StmtNode> action;
	ExceptionArray conditions;
};


class ExecProcedureNode final : public TypedNode<StmtNode, StmtNode::TYPE_EXEC_PROCEDURE>
{
public:
	explicit ExecProcedureNode(MemoryPool& pool,
				const QualifiedName& aDsqlName = {},
				ValueListNode* aInputs = nullptr, ValueListNode* aOutputs = nullptr,
				Firebird::ObjectsArray<MetaName>* aDsqlInputArgNames = nullptr)
		: TypedNode<StmtNode, StmtNode::TYPE_EXEC_PROCEDURE>(pool),
		  dsqlName(pool, aDsqlName),
		  inputSources(aInputs),
		  outputTargets(aOutputs),
		  dsqlInputArgNames(aDsqlInputArgNames)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	ExecProcedureNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ExecProcedureNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ExecProcedureNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	ValueListNode* explodeOutputs(DsqlCompilerScratch* dsqlScratch, const dsql_prc* procedure);
	void executeProcedure(thread_db* tdbb, Request* request) const;

public:
	QualifiedName dsqlName;
	dsql_prc* dsqlProcedure = nullptr;
	NestConst<ValueListNode> inputSources;
	NestConst<ValueListNode> inputTargets;
	NestConst<MessageNode> inputMessage;
	NestConst<ValueListNode> outputSources;
	NestConst<ValueListNode> outputTargets;
	NestConst<MessageNode> outputMessage;
	SubRoutine<jrd_prc> procedure;
	NestConst<Firebird::ObjectsArray<MetaName>> dsqlInputArgNames;
	NestConst<Firebird::ObjectsArray<MetaName>> dsqlOutputArgNames;
	bool dsqlCallSyntax = false;
};


class ExecStatementNode final : public TypedNode<StmtNode, StmtNode::TYPE_EXEC_STATEMENT>
{
public:
	explicit ExecStatementNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_EXEC_STATEMENT>(pool),
		  dsqlLabelName(NULL),
		  sql(NULL),
		  dataSource(NULL),
		  userName(NULL),
		  password(NULL),
		  role(NULL),
		  innerStmt(NULL),
		  inputs(NULL),
		  outputs(NULL),
		  inputNames(NULL),
		  excessInputs(NULL),
		  dsqlLabelNumber(0),
		  useCallerPrivs(false),
		  traScope(EDS::traNotSet)	// not defined
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ExecStatementNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ExecStatementNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	static void genOptionalExpr(DsqlCompilerScratch* dsqlScratch, const UCHAR code, ValueExprNode* node);

	void getString(thread_db* tdbb, Request* request, const ValueExprNode* node,
		Firebird::string& str, bool useAttCS = false) const;

public:
	MetaName* dsqlLabelName;
	NestConst<ValueExprNode> sql;
	NestConst<ValueExprNode> dataSource;
	NestConst<ValueExprNode> userName;
	NestConst<ValueExprNode> password;
	NestConst<ValueExprNode> role;
	NestConst<StmtNode> innerStmt;
	NestConst<ValueListNode> inputs;
	NestConst<ValueListNode> outputs;
	EDS::ParamNames* inputNames;
	EDS::ParamNumbers* excessInputs;
	USHORT dsqlLabelNumber;
	bool useCallerPrivs;
	EDS::TraScope traScope;
};


class IfNode final : public TypedNode<StmtNode, StmtNode::TYPE_IF>
{
public:
	explicit IfNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_IF>(pool),
		  condition(NULL),
		  trueAction(NULL),
		  falseAction(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	IfNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	IfNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	IfNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<BoolExprNode> condition;
	NestConst<StmtNode> trueAction;
	NestConst<StmtNode> falseAction;
};


class InAutonomousTransactionNode final : public TypedNode<StmtNode, StmtNode::TYPE_IN_AUTO_TRANS>
{
	struct Impure
	{
		TraNumber traNumber;
		SavNumber savNumber;
	};

public:
	explicit InAutonomousTransactionNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_IN_AUTO_TRANS>(pool),
		  action(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	InAutonomousTransactionNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	InAutonomousTransactionNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	InAutonomousTransactionNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<StmtNode> action;
};


class InitVariableNode final : public TypedNode<StmtNode, StmtNode::TYPE_INIT_VARIABLE>
{
public:
	explicit InitVariableNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_INIT_VARIABLE>(pool),
		  varId(0),
		  varDecl(NULL),
		  varInfo(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	InitVariableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	InitVariableNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	InitVariableNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	InitVariableNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	USHORT varId;
	NestConst<DeclareVariableNode> varDecl;
	NestConst<ItemInfo> varInfo;
};


class ExecBlockNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_EXEC_BLOCK>
{
public:
	explicit ExecBlockNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_EXEC_BLOCK>(pool),
		  parameters(pool),
		  returns(pool)
	{
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ExecBlockNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

private:
	static void revertParametersOrder(Firebird::Array<dsql_par*>& parameters);

public:
	Firebird::Array<NestConst<ParameterClause>> parameters;
	Firebird::Array<NestConst<ParameterClause>> returns;
	NestConst<LocalDeclarationsNode> localDeclList;
	NestConst<StmtNode> body;
};


class ExceptionNode final : public TypedNode<StmtNode, StmtNode::TYPE_EXCEPTION>
{
public:
	ExceptionNode(MemoryPool& pool, const QualifiedName& name,
				ValueExprNode* aMessageExpr = NULL, ValueListNode* aParameters = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_EXCEPTION>(pool),
		  messageExpr(aMessageExpr),
		  parameters(aParameters)
	{
		exception = FB_NEW_POOL(pool) ExceptionItem(pool);
		exception->type = ExceptionItem::XCP_CODE;
		exception->name = name;
	}

	explicit ExceptionNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_EXCEPTION>(pool),
		  messageExpr(NULL),
		  parameters(NULL),
		  exception(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ExceptionNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ExceptionNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	void setError(thread_db* tdbb) const;

public:
	NestConst<ValueExprNode> messageExpr;
	NestConst<ValueListNode> parameters;
	NestConst<ExceptionItem> exception;
};


class ExitNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_EXIT>
{
public:
	explicit ExitNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_EXIT>(pool)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
};


class ForNode final : public TypedNode<StmtNode, StmtNode::TYPE_FOR>
{
public:
	explicit ForNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_FOR>(pool),
		  dsqlSelect(NULL),
		  dsqlInto(NULL),
		  dsqlCursor(NULL),
		  dsqlLabelName(NULL),
		  stall(NULL),
		  rse(NULL),
		  statement(NULL),
		  cursor(NULL),
		  parBlrBeginCnt(0),
		  marks(0),
		  dsqlLabelNumber(0),
		  dsqlForceSingular(false),
		  withLock(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ForNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	StmtNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	StmtNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

	bool isWriteLockMode(Request* request) const;
	void setWriteLockMode(Request* request) const;

	// Used by UPDATE and DELETE sub-statements of MERGE
	void checkRecordUpdated(thread_db* tdbb, Request* request, record_param* rpb) const;
	void setRecordUpdated(thread_db* tdbb, Request* request, record_param* rpb) const;

public:
	struct Impure
	{
		SavNumber savepoint;
		bool writeLockMode;		// true - driven statement (UPDATE\DELETE\SELECT WITH LOCK) works in "write lock" mode, false - normal mode
	};

	struct ImpureMerge : Impure
	{
		RecordBitmap* recUpdated;	// updated and deleted records by MERGE statement
	};

	NestConst<SelectNode> dsqlSelect;
	NestConst<ValueListNode> dsqlInto;
	DeclareCursorNode* dsqlCursor;
	MetaName* dsqlLabelName;
	NestConst<StmtNode> stall;
	NestConst<RseNode> rse;
	NestConst<StmtNode> statement;
	NestConst<Cursor> cursor;
	int parBlrBeginCnt;
	unsigned marks;
	USHORT dsqlLabelNumber;
	bool dsqlForceSingular;
	bool withLock;				// part of SELECT ... WITH LOCK	statement
};


class ForRangeNode final : public TypedNode<StmtNode, StmtNode::TYPE_FOR_RANGE>
{
public:
	enum class Direction : UCHAR
	{
		TO = blr_for_range_direction_to,
		DOWNTO = blr_for_range_direction_downto
	};

	struct Impure
	{
		impure_value finalValue;
		impure_value byValue;
	};

public:
	explicit ForRangeNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_FOR_RANGE>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ForRangeNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ForRangeNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<ValueExprNode> variable;
	NestConst<ValueExprNode> initialExpr;
	NestConst<ValueExprNode> finalExpr;
	NestConst<ValueExprNode> byExpr;
	NestConst<StmtNode> statement;
	MetaName* dsqlLabelName = nullptr;
	USHORT dsqlLabelNumber = 0;
	Direction direction = Direction::TO;
	SCHAR incDecScale = 0;
	USHORT incDecFlags = 0;
};


class HandlerNode final : public TypedNode<StmtNode, StmtNode::TYPE_HANDLER>
{
public:
	explicit HandlerNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_HANDLER>(pool),
		  statement(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	HandlerNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	HandlerNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	HandlerNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<StmtNode> statement;
};


class LabelNode final : public TypedNode<StmtNode, StmtNode::TYPE_LABEL>
{
public:
	explicit LabelNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_LABEL>(pool),
		  statement(NULL),
		  labelNumber(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	LabelNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	LabelNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	LabelNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<StmtNode> statement;
	USHORT labelNumber;
};


class LineColumnNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_LINE_COLUMN>
{
public:
	explicit LineColumnNode(MemoryPool& pool, ULONG aLine, ULONG aColumn, StmtNode* aStatement)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_LINE_COLUMN>(pool),
		  statement(aStatement)
	{
		line = aLine;
		column = aColumn;
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	LineColumnNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

private:
	NestConst<StmtNode> statement;
};


class LocalDeclarationsNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_LOCAL_DECLARATIONS>
{
public:
	explicit LocalDeclarationsNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_LOCAL_DECLARATIONS>(pool),
		  statements(pool)
	{
	}

public:
	static void checkUniqueFieldsNames(const LocalDeclarationsNode* node,
		const Firebird::Array<NestConst<ParameterClause>>* inputParameters,
		const Firebird::Array<NestConst<ParameterClause>>* outputParameters);

public:
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

public:
	Firebird::Array<NestConst<StmtNode>> statements;
};


class LoopNode final : public TypedNode<StmtNode, StmtNode::TYPE_LOOP>
{
public:
	explicit LoopNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_LOOP>(pool),
		  dsqlLabelName(NULL),
		  dsqlExpr(NULL),
		  statement(NULL),
		  dsqlLabelNumber(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	LoopNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	LoopNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	LoopNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	MetaName* dsqlLabelName;
	NestConst<BoolExprNode> dsqlExpr;
	NestConst<StmtNode> statement;
	USHORT dsqlLabelNumber;
};


class MergeNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_MERGE>
{
public:
	struct Matched
	{
		explicit Matched(MemoryPool& pool)
			: processedFields(pool),
			  processedValues(pool)
		{
		}

		NestConst<CompoundStmtNode> assignments;
		NestConst<BoolExprNode> condition;

		NestConst<Jrd::RecordSourceNode> modifyRelation;
		NestValueArray processedFields;
		NestValueArray processedValues;
		NestConst<ReturningClause> processedReturning;
	};

	struct NotMatched
	{
		explicit NotMatched(MemoryPool& pool)
			: fields(pool),
			  processedFields(pool)
		{
		}

		Firebird::Array<NestConst<FieldNode>> fields;
		NestConst<ValueListNode> values;
		NestConst<BoolExprNode> condition;
		std::optional<OverrideClause> overrideClause;

		NestConst<Jrd::RecordSourceNode> storeRelation;
		NestValueArray processedFields;
		NestConst<ReturningClause> processedReturning;
	};

	explicit MergeNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_MERGE>(pool),
		  whenMatched(pool),
		  whenNotMatchedByTarget(pool),
		  whenNotMatchedBySource(pool),
		  usingContexts(pool)
	{
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

public:
	NestConst<RelationSourceNode> relation;
	NestConst<RecordSourceNode> usingClause;
	NestConst<BoolExprNode> condition;
	Firebird::ObjectsArray<Matched> whenMatched;
	Firebird::ObjectsArray<NotMatched> whenNotMatchedByTarget;
	Firebird::ObjectsArray<Matched> whenNotMatchedBySource;
	NestConst<PlanNode> plan;
	NestConst<ValueListNode> order;
	NestConst<ReturningClause> returning;

	NestConst<RseNode> rse;
	dsql_ctx* oldContext = nullptr;
	Firebird::SortedArray<USHORT> usingContexts;
};


class MessageNode : public TypedNode<StmtNode, StmtNode::TYPE_MESSAGE>
{
	struct MessageBuffer
	{
		const Format* format; // Message format derived from user MessageMetadata. Overrides default format.
		UCHAR* buffer = nullptr;
	};

public:
	explicit MessageNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_MESSAGE>(pool)
	{
	}
	// This constructor is temporary workaround for copying of existing format.
	// For details look at comment in CMP_procedure_arguments()
	explicit MessageNode(MemoryPool& pool, const Format& oldFormat)
		: TypedNode<StmtNode, StmtNode::TYPE_MESSAGE>(pool)
	{
		format = Format::newFormat(pool, oldFormat.fmt_count);
		*format = oldFormat;
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	void setup(thread_db* tdbb, CompilerScratch* csb, USHORT message, USHORT count);

	virtual USHORT setupDesc(thread_db* tdbb, CompilerScratch* csb, USHORT index,
		dsc* desc, ItemInfo* itemInfo);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	MessageNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	MessageNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	MessageNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	MessageNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

	UCHAR* getBuffer(Request* request) const;
	const Format* getFormat(const Request* request) const;
	void setFormat(Request* request, Format* newFormat);

public:
	ULONG impureFlags = 0;
	USHORT messageNumber = 0;

private:
	using StmtNode::impureOffset; // Made private to incapsulate it's interpretation logic
	NestConst<Format> format;
};


class ModifyNode final : public TypedNode<StmtNode, StmtNode::TYPE_MODIFY>
{
public:
	explicit ModifyNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_MODIFY>(pool),
		  dsqlCursorName(pool),
		  validations(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* internalDsqlPass(DsqlCompilerScratch* dsqlScratch, bool updateOrInsert);
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ModifyNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ModifyNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	static void pass1Modify(thread_db* tdbb, CompilerScratch* csb, ModifyNode* node);
	const StmtNode* modify(thread_db* tdbb, Request* request, WhichTrigger whichTrig) const;

public:
	NestConst<RecordSourceNode> dsqlRelation;
	NestConst<BoolExprNode> dsqlBoolean;
	NestConst<PlanNode> dsqlPlan;
	NestConst<ValueListNode> dsqlOrder;
	NestConst<RowsClause> dsqlRows;
	MetaName dsqlCursorName;
	NestConst<ReturningClause> dsqlReturning;
	NestConst<RecordSourceNode> dsqlRse;
	dsql_ctx* dsqlContext = nullptr;
	bool dsqlSkipLocked = false;
	NestConst<StmtNode> statement;
	NestConst<StmtNode> statement2;
	NestConst<StmtNode> subMod;
	Firebird::Array<ValidateInfo> validations;
	NestConst<StmtNode> mapView;
	NestConst<ForNode> forNode;			// parent implicit cursor, if present
	StreamType orgStream = 0;
	StreamType newStream = 0;
	unsigned marks = 0;						// see StmtNode::IUD_MARK_xxx
	USHORT dsqlRseFlags = 0;
	std::optional<USHORT> dsqlReturningLocalTableNumber;
};


class OuterMapNode final : public TypedNode<StmtNode, StmtNode::TYPE_OUTER_MAP>
{
public:
	explicit OuterMapNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_OUTER_MAP>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& /*printer*/) const override
	{
		return "OuterMapNode";
	}

	void genBlr(DsqlCompilerScratch* /*dsqlScratch*/) override
	{
	}

	OuterMapNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override;

	OuterMapNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;
};


class PostEventNode final : public TypedNode<StmtNode, StmtNode::TYPE_POST_EVENT>
{
public:
	explicit PostEventNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_POST_EVENT>(pool),
		  event(NULL),
		  argument(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	PostEventNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	PostEventNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	PostEventNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<ValueExprNode> event;
	NestConst<ValueExprNode> argument;
};


class ReceiveNode final : public TypedNode<StmtNode, StmtNode::TYPE_RECEIVE>
{
public:
	explicit ReceiveNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_RECEIVE>(pool),
		  statement(NULL),
		  message(NULL),
		  batchFlag(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ReceiveNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	ReceiveNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ReceiveNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<StmtNode> statement;
	NestConst<MessageNode> message;
	bool batchFlag;
};


class StoreNode final : public TypedNode<StmtNode, StmtNode::TYPE_STORE>
{
public:
	explicit StoreNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_STORE>(pool),
		  dsqlFields(pool),
		  validations(pool),
		  marks(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* internalDsqlPass(DsqlCompilerScratch* dsqlScratch, bool updateOrInsert, bool& needSavePoint);
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	StoreNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	StoreNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	static bool pass1Store(thread_db* tdbb, CompilerScratch* csb, StoreNode* node);
	void makeDefaults(thread_db* tdbb, CompilerScratch* csb);
	const StmtNode* store(thread_db* tdbb, Request* request, WhichTrigger whichTrig) const;

public:
	NestConst<RecordSourceNode> target;
	Firebird::Array<NestConst<FieldNode>> dsqlFields;
	NestConst<ValueListNode> dsqlValues;
	NestConst<ReturningClause> dsqlReturning;
	NestConst<RecordSourceNode> dsqlRse;
	NestConst<StmtNode> statement;
	NestConst<StmtNode> statement2;
	NestConst<StmtNode> subStore;
	Firebird::Array<ValidateInfo> validations;
	unsigned marks;
	std::optional<USHORT> dsqlReturningLocalTableNumber;
	std::optional<OverrideClause> overrideClause;
};


class SelectNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_SELECT>
{
public:
	explicit SelectNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_SELECT>(pool)
	{
	}

public:
	RseNode* dsqlProcess(DsqlCompilerScratch* dsqlScratch);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	SelectNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

public:
	NestConst<SelectExprNode> selectExpr;
	NestConst<RseNode> rse;
	Firebird::TriState optimizeForFirstRows;
	bool forUpdate = false;
	bool withLock = false;
	bool skipLocked = false;
};


class SelectMessageNode final : public TypedNode<StmtNode, StmtNode::TYPE_SELECT_MESSAGE>
{
public:
	explicit SelectMessageNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_SELECT_MESSAGE>(pool),
		  statements(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;

	SelectMessageNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		fb_assert(false);
		return nullptr;
	}

	void genBlr(DsqlCompilerScratch* dsqlScratch) override
	{
		fb_assert(false);
	}

	SelectMessageNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	SelectMessageNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	Firebird::Array<NestConst<StmtNode>> statements;
};


// This is only for GPRE's cmp_set_generator().
class SetGeneratorNode final : public TypedNode<StmtNode, StmtNode::TYPE_SET_GENERATOR>
{
public:
	SetGeneratorNode(MemoryPool& pool, const QualifiedName& name, ValueExprNode* aValue = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_SET_GENERATOR>(pool),
		  generator(pool, name), value(aValue)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;

	// DSQL support is implemented in CreateAlterSequenceNode.
	void genBlr(DsqlCompilerScratch* dsqlScratch) override
	{
		fb_assert(false);
	}

	SetGeneratorNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	SetGeneratorNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	GeneratorItem generator;
	NestConst<ValueExprNode> value;
};


class StallNode final : public TypedNode<StmtNode, StmtNode::TYPE_STALL>
{
public:
	explicit StallNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_STALL>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StallNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	StallNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	StallNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;
};


class SuspendNode final : public TypedNode<StmtNode, StmtNode::TYPE_SUSPEND>
{
public:
	explicit SuspendNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_SUSPEND>(pool),
		  message(NULL),
		  statement(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	SuspendNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	SuspendNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	SuspendNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	NestConst<MessageNode> message;
	NestConst<StmtNode> statement;
};


class ReturnNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_RETURN>
{
public:
	explicit ReturnNode(MemoryPool& pool, ValueExprNode* val = NULL)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_RETURN>(pool),
		  value(val)
	{
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	ReturnNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

public:
	NestConst<ValueExprNode> value;
};


class SavepointEncloseNode final : public TypedNode<StmtNode, StmtNode::TYPE_SAVEPOINT>
{
public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	static StmtNode* make(MemoryPool& pool, DsqlCompilerScratch* dsqlScratch, StmtNode* node, bool force = false);

	Firebird::string internalPrint(NodePrinter& printer) const override;
	SavepointEncloseNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	SavepointEncloseNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	SavepointEncloseNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

private:
	explicit SavepointEncloseNode(MemoryPool& pool, StmtNode* stmt)
		: TypedNode<StmtNode, StmtNode::TYPE_SAVEPOINT>(pool),
		  statement(stmt)
	{
	}

	NestConst<StmtNode> statement;
};


class SessionManagementWrapperNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_SESSION_MANAGEMENT_WRAPPER>
{
public:
	explicit SessionManagementWrapperNode(MemoryPool& pool, SessionManagementNode* aWrapped,
				const Firebird::string& aText)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_SESSION_MANAGEMENT_WRAPPER>(pool),
		  wrapped(aWrapped),
		  text(pool, aText)
	{
	}

public:
	SessionManagementWrapperNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		Node::dsqlPass(dsqlScratch);

		// Save and reset the statement type, as SessionManagementNode sets it to TYPE_SESSION_MANAGEMENT but
		// we are a DML statement.
		DsqlStatement::Type statementType = dsqlScratch->getDsqlStatement()->getType();
		wrapped->dsqlPass(dsqlScratch);
		dsqlScratch->getDsqlStatement()->setType(statementType);

		return this;
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		DsqlOnlyStmtNode::internalPrint(printer);

		NODE_PRINT(printer, wrapped);
		NODE_PRINT(printer, text);

		return "SessionManagementWrapperNode";
	}

	void genBlr(DsqlCompilerScratch* dsqlScratch) override
	{
		dsqlScratch->appendUChar(blr_exec_sql);
		dsqlScratch->appendUChar(blr_literal);
		dsqlScratch->appendUChar(blr_text2);
		dsqlScratch->appendUShort(CS_METADATA);
		dsqlScratch->appendUShort((USHORT) text.length());
		dsqlScratch->appendBytes((const UCHAR*) text.c_str(), text.length());
	}

public:
	SessionManagementNode* wrapped;
	const Firebird::string text;
};


class SetTransactionNode final : public TransactionNode
{
public:
	struct RestrictionOption : Firebird::PermanentStorage
	{
		RestrictionOption(MemoryPool& p, Firebird::ObjectsArray<QualifiedName>* aTables,
					unsigned aLockMode)
			: PermanentStorage(p),
			  tables(aTables),
			  lockMode(aLockMode)
		{
		}

		Firebird::ObjectsArray<QualifiedName>* tables;
		unsigned lockMode;
	};

	enum
	{
		ISO_LEVEL_CONCURRENCY,
		ISO_LEVEL_CONSISTENCY,
		ISO_LEVEL_READ_COMMITTED_REC_VERSION,
		ISO_LEVEL_READ_COMMITTED_NO_REC_VERSION,
		ISO_LEVEL_READ_COMMITTED_READ_CONSISTENCY
	};

	static constexpr unsigned LOCK_MODE_SHARED 		= 0x1;
	static constexpr unsigned LOCK_MODE_PROTECTED	= 0x2;
	static constexpr unsigned LOCK_MODE_READ		= 0x4;
	static constexpr unsigned LOCK_MODE_WRITE		= 0x8;

public:
	explicit SetTransactionNode(MemoryPool& pool)
		: TransactionNode(pool),
		  reserveList(pool),
		  tpb(pool)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		TransactionNode::internalPrint(printer);

		NODE_PRINT(printer, readOnly);
		NODE_PRINT(printer, wait);
		NODE_PRINT(printer, isoLevel);
		NODE_PRINT(printer, noAutoUndo);
		NODE_PRINT(printer, ignoreLimbo);
		NODE_PRINT(printer, restartRequests);
		NODE_PRINT(printer, autoCommit);
		NODE_PRINT(printer, autoReleaseTempBlobID);
		NODE_PRINT(printer, lockTimeout);
		//// FIXME-PRINT: NODE_PRINT(printer, reserveList);
		NODE_PRINT(printer, tpb);

		return "SetTransactionNode";
	}

	SetTransactionNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const override;

private:
	void genTableLock(DsqlCompilerScratch* dsqlScratch, const RestrictionOption& tblLock,
		USHORT lockLevel);

public:
	Firebird::Array<RestrictionOption*> reserveList;
	Firebird::UCharBuffer tpb;
	std::optional<CommitNumber> atSnapshotNumber;
	std::optional<unsigned> isoLevel;
	std::optional<USHORT> lockTimeout;
	Firebird::TriState readOnly;
	Firebird::TriState wait;
	Firebird::TriState noAutoUndo;
	Firebird::TriState ignoreLimbo;
	Firebird::TriState restartRequests;
	Firebird::TriState autoCommit;
	Firebird::TriState autoReleaseTempBlobID;
};


class CommitRollbackNode final : public TransactionNode
{
public:
	enum Command : UCHAR
	{
		CMD_COMMIT,
		CMD_ROLLBACK
	};

public:
	explicit CommitRollbackNode(MemoryPool& pool, Command aCommand, bool aRetain)
		: TransactionNode(pool),
		  command(aCommand),
		  retain(aRetain)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	CommitRollbackNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const override;

private:
	const Command command;
	const bool retain;
};


class UserSavepointNode final : public TransactionNode
{
public:
	enum Command : UCHAR
	{
		CMD_SET,
		CMD_RELEASE,
		CMD_RELEASE_ONLY,
		CMD_ROLLBACK
	};

public:
	explicit UserSavepointNode(MemoryPool& pool, Command aCommand, const MetaName& aName)
		: TransactionNode(pool),
		  command(aCommand),
		  name(pool, aName)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	UserSavepointNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const override;

public:
	const Command command;
	const MetaName name;
};


class SessionResetNode final : public SessionManagementNode
{
public:
	explicit SessionResetNode(MemoryPool& pool)
		: SessionManagementNode(pool)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		return "SessionResetNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;
};


class SetRoleNode final : public SessionManagementNode
{
public:
	explicit SetRoleNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  trusted(true),
		  roleName(pool)
	{
	}

	SetRoleNode(MemoryPool& pool, MetaName* name)
		: SessionManagementNode(pool),
		  trusted(false),
		  roleName(pool, *name)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, trusted);
		NODE_PRINT(printer, roleName);

		return "SetRoleNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	bool trusted;
	MetaName roleName;
};


class SetSessionNode final : public SessionManagementNode
{
public:
	enum Type : UCHAR
	{
		TYPE_IDLE_TIMEOUT,
		TYPE_STMT_TIMEOUT
	};

	SetSessionNode(MemoryPool& pool, Type aType, ULONG aVal, UCHAR blr_timepart);

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

private:
	Type m_type;
	ULONG m_value;
};


class SetDebugOptionNode final : public SessionManagementNode
{
public:
	SetDebugOptionNode(MemoryPool& pool, MetaName* aName, ExprNode* aValue);

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, name);
		NODE_PRINT(printer, value);

		return "SetDebugOptionNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

private:
	MetaName name;
	ExprNode* value;
};


class SetDecFloatRoundNode final : public SessionManagementNode
{
public:
	SetDecFloatRoundNode(MemoryPool& pool, MetaName* name);

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, rndMode);

		return "SetDecFloatRoundNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	USHORT rndMode;
};


class SetDecFloatTrapsNode final : public SessionManagementNode
{
public:
	explicit SetDecFloatTrapsNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  traps(0u)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, traps);

		return "SetDecFloatTrapsNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

	void trap(MetaName* name);

public:
	USHORT traps;
};


class SetBindNode final : public SessionManagementNode
{
public:
	explicit SetBindNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  from(nullptr),
		  to(nullptr)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, from);
		NODE_PRINT(printer, to);

		return "SetBindNode";
	}

	SessionManagementNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	dsql_fld* from;
	dsql_fld* to;
};


class SetOptimizeNode final : public SessionManagementNode
{
public:
	explicit SetOptimizeNode(MemoryPool& pool)
		: SessionManagementNode(pool)
	{
	}

	SetOptimizeNode(MemoryPool& pool, bool mode)
		: SessionManagementNode(pool),
		  optimizeMode(mode)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, optimizeMode);

		return "SetOptimizeNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	Firebird::TriState optimizeMode;
};


class SetSearchPathNode final : public SessionManagementNode
{
public:
	SetSearchPathNode(MemoryPool& pool, Firebird::ObjectsArray<MetaName>* aSchemas)
		: SessionManagementNode(pool),
		  schemas(aSchemas)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, schemas);

		return "SetSearchPathNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	NestConst<Firebird::ObjectsArray<MetaName>> schemas;
};


class SetTimeZoneNode final : public SessionManagementNode
{
public:
	explicit SetTimeZoneNode(MemoryPool& pool, const Firebird::string& aStr)
		: SessionManagementNode(pool),
		  str(pool, aStr),
		  local(false)
	{
	}

	explicit SetTimeZoneNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  str(pool),
		  local(true)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, str);
		NODE_PRINT(printer, local);

		return "SetTimeZoneNode";
	}

	void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const override;

public:
	Firebird::string str;
	bool local;
};


class TruncateLocalTableNode final : public TypedNode<StmtNode, StmtNode::TYPE_TRUNCATE_LOCAL_TABLE>
{
public:
	explicit TruncateLocalTableNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_TRUNCATE_LOCAL_TABLE>(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	Firebird::string internalPrint(NodePrinter& printer) const override;

	TruncateLocalTableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		return this;
	}

	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	TruncateLocalTableNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	TruncateLocalTableNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	TruncateLocalTableNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const override;

public:
	USHORT tableNumber = 0;
};


class UpdateOrInsertNode final : public TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_UPDATE_OR_INSERT>
{
public:
	explicit UpdateOrInsertNode(MemoryPool& pool)
		: TypedNode<DsqlOnlyStmtNode, StmtNode::TYPE_UPDATE_OR_INSERT>(pool),
		  fields(pool),
		  matching(pool),
		  varAssignments(pool)
	{
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

public:
	NestConst<RelationSourceNode> relation;
	Firebird::Array<NestConst<FieldNode>> fields;
	NestConst<ValueListNode> values;
	Firebird::Array<NestConst<FieldNode>> matching;
	NestConst<PlanNode> plan;
	NestConst<ValueListNode> order;
	NestConst<RowsClause> rows;
	NestConst<ReturningClause> returning;
	std::optional<OverrideClause> overrideClause;
	NestConst<StoreNode> storeNode;
	NestConst<ModifyNode> modifyNode;
	Firebird::Array<NestConst<AssignmentNode>> varAssignments;
};


class ConstantExprNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_CONSTANT>
{
public:
	explicit ConstantExprNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_CONSTANT>(pool)
	{ }

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ConstantExprNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ConstantExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual ConstantExprNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ConstantExprNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const;

	dsc* getImpureDsc(Request* request) const;

public:
	NestConst<ValueExprNode> value;
	ULONG impureOffset;	// Impure offset from request block.
};

} // namespace

#endif // DSQL_STMT_NODES_H
