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

#include "../jrd/MetaName.h"
#include "firebird/impl/blr.h"
#include "../jrd/Function.h"
#include "../jrd/extds/ExtDS.h"
#include "../dsql/Nodes.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/NodePrinter.h"
#include "../common/DecFloat.h"

namespace Jrd {

class CompoundStmtNode;
class ExecBlockNode;
class ForNode;
class PlanNode;
class RecordBuffer;
class RelationSourceNode;
class SelectNode;
class GeneratorItem;


class ExceptionItem : public Firebird::PermanentStorage, public Printable
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
	virtual Firebird::string internalPrint(NodePrinter& printer) const
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
	// ASF: There are some inconsistencies in the type of 'name'. Metanames have maximum of 31 chars,
	// while there are system exceptions with 32 chars. The parser always expects metanames, but
	// I'm following the legacy code and making this a string.
	Firebird::string name;
	MetaName secName;
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

	static void validateTarget(CompilerScratch* csb, const ValueExprNode* target);
	static void dsqlValidateTarget(const ValueExprNode* target);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual AssignmentNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual AssignmentNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AssignmentNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual AssignmentNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual BlockNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual BlockNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static bool testAndFixupError(thread_db* tdbb, jrd_req* request, const ExceptionArray& conditions);

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual CompoundStmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual CompoundStmtNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual CompoundStmtNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual CompoundStmtNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ContinueLeaveNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

	virtual ContinueLeaveNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/)
	{
		return this;
	}

	virtual ContinueLeaveNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/)
	{
		return this;
	}

	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual CursorStmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual CursorStmtNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual CursorStmtNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
	static const USHORT CUR_TYPE_NONE = 0;
	static const USHORT CUR_TYPE_EXPLICIT = 1;
	static const USHORT CUR_TYPE_FOR = 2;
	static const USHORT CUR_TYPE_ALL = (CUR_TYPE_EXPLICIT | CUR_TYPE_FOR);

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DeclareCursorNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual DeclareCursorNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual DeclareCursorNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
	const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const override;

public:
	Impure* getImpure(thread_db* tdbb, jrd_req* request, bool createWhenDead = true) const;

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
		  dsqlParameters(pool),
		  dsqlReturns(pool),
		  dsqlSignature(pool, aName),
		  dsqlBlock(NULL),
		  blockScratch(NULL),
		  dsqlFunction(NULL),
		  blrStart(NULL),
		  subCsb(NULL),
		  routine(NULL),
		  blrLength(0),
		  dsqlDeterministic(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DeclareSubFuncNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

	virtual DeclareSubFuncNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual DeclareSubFuncNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static void parseParameters(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
		Firebird::Array<NestConst<Parameter> >& paramArray, USHORT* defaultCount = NULL);

	void genParameters(DsqlCompilerScratch* dsqlScratch,
		Firebird::Array<NestConst<ParameterClause> >& paramArray);

public:
	MetaName name;
	Firebird::Array<NestConst<ParameterClause> > dsqlParameters;
	Firebird::Array<NestConst<ParameterClause> > dsqlReturns;
	Signature dsqlSignature;
	NestConst<ExecBlockNode> dsqlBlock;
	DsqlCompilerScratch* blockScratch;
	dsql_udf* dsqlFunction;
	const UCHAR* blrStart;
	CompilerScratch* subCsb;
	Function* routine;
	ULONG blrLength;
	bool dsqlDeterministic;
};


class DeclareSubProcNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBPROC>
{
public:
	explicit DeclareSubProcNode(MemoryPool& pool, const MetaName& aName)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_SUBPROC>(pool),
		  name(pool, aName),
		  dsqlParameters(pool),
		  dsqlReturns(pool),
		  dsqlSignature(pool, aName),
		  dsqlBlock(NULL),
		  blockScratch(NULL),
		  dsqlProcedure(NULL),
		  blrStart(NULL),
		  subCsb(NULL),
		  routine(NULL),
		  blrLength(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DeclareSubProcNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

	virtual DeclareSubProcNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual DeclareSubProcNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static void parseParameters(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
		Firebird::Array<NestConst<Parameter> >& paramArray, USHORT* defaultCount = NULL);

	void genParameters(DsqlCompilerScratch* dsqlScratch,
		Firebird::Array<NestConst<ParameterClause> >& paramArray);

public:
	MetaName name;
	Firebird::Array<NestConst<ParameterClause> > dsqlParameters;
	Firebird::Array<NestConst<ParameterClause> > dsqlReturns;
	Signature dsqlSignature;
	NestConst<ExecBlockNode> dsqlBlock;
	DsqlCompilerScratch* blockScratch;
	dsql_prc* dsqlProcedure;
	const UCHAR* blrStart;
	CompilerScratch* subCsb;
	jrd_prc* routine;
	ULONG blrLength;
};


class DeclareVariableNode final : public TypedNode<StmtNode, StmtNode::TYPE_DECLARE_VARIABLE>
{
public:
	explicit DeclareVariableNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_DECLARE_VARIABLE>(pool)
	{
		varDesc.clear();
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DeclareVariableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual DeclareVariableNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual DeclareVariableNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual DeclareVariableNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
		  dsqlRelation(NULL),
		  dsqlBoolean(NULL),
		  dsqlPlan(NULL),
		  dsqlOrder(NULL),
		  dsqlRows(NULL),
		  dsqlCursorName(pool),
		  dsqlReturning(NULL),
		  dsqlRse(NULL),
		  dsqlContext(NULL),
		  statement(NULL),
		  subStatement(NULL),
		  stream(0),
		  marks(0)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual EraseNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual EraseNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static void pass1Erase(thread_db* tdbb, CompilerScratch* csb, EraseNode* node);
	const StmtNode* erase(thread_db* tdbb, jrd_req* request, WhichTrigger whichTrig) const;

public:
	NestConst<RelationSourceNode> dsqlRelation;
	NestConst<BoolExprNode> dsqlBoolean;
	NestConst<PlanNode> dsqlPlan;
	NestConst<ValueListNode> dsqlOrder;
	NestConst<RowsClause> dsqlRows;
	MetaName dsqlCursorName;
	NestConst<ReturningClause> dsqlReturning;
	NestConst<RseNode> dsqlRse;
	dsql_ctx* dsqlContext;
	NestConst<StmtNode> statement;
	NestConst<StmtNode> subStatement;
	NestConst<ForNode> forNode;			// parent implicit cursor, if present
	StreamType stream;
	unsigned marks;						// see StmtNode::IUD_MARK_xxx
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ErrorHandlerNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ErrorHandlerNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ErrorHandlerNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

public:
	NestConst<StmtNode> action;
	ExceptionArray conditions;
};


class ExecProcedureNode final : public TypedNode<StmtNode, StmtNode::TYPE_EXEC_PROCEDURE>
{
public:
	explicit ExecProcedureNode(MemoryPool& pool,
				const QualifiedName& aDsqlName = QualifiedName(),
				ValueListNode* aInputs = NULL, ValueListNode* aOutputs = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_EXEC_PROCEDURE>(pool),
		  dsqlName(pool, aDsqlName),
		  dsqlProcedure(NULL),
		  inputSources(aInputs),
		  inputTargets(NULL),
		  inputMessage(NULL),
		  outputSources(aOutputs),
		  outputTargets(NULL),
		  outputMessage(NULL),
		  procedure(NULL)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ExecProcedureNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ExecProcedureNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ExecProcedureNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	ValueListNode* explodeOutputs(DsqlCompilerScratch* dsqlScratch, const dsql_prc* procedure);
	void executeProcedure(thread_db* tdbb, jrd_req* request) const;

public:
	QualifiedName dsqlName;
	dsql_prc* dsqlProcedure;
	NestConst<ValueListNode> inputSources;
	NestConst<ValueListNode> inputTargets;
	NestConst<MessageNode> inputMessage;
	NestConst<ValueListNode> outputSources;
	NestConst<ValueListNode> outputTargets;
	NestConst<MessageNode> outputMessage;
	NestConst<jrd_prc> procedure;
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ExecStatementNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ExecStatementNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static void genOptionalExpr(DsqlCompilerScratch* dsqlScratch, const UCHAR code, ValueExprNode* node);

	void getString(thread_db* tdbb, jrd_req* request, const ValueExprNode* node,
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual IfNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual IfNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual IfNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual InAutonomousTransactionNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual InAutonomousTransactionNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual InAutonomousTransactionNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual InitVariableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual InitVariableNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual InitVariableNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual InitVariableNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
		  returns(pool),
		  localDeclList(NULL),
		  body(NULL)
	{
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ExecBlockNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

private:
	static void revertParametersOrder(Firebird::Array<dsql_par*>& parameters);

public:
	Firebird::Array<NestConst<ParameterClause> > parameters;
	Firebird::Array<NestConst<ParameterClause> > returns;
	NestConst<CompoundStmtNode> localDeclList;
	NestConst<StmtNode> body;
};


class ExceptionNode final : public TypedNode<StmtNode, StmtNode::TYPE_EXCEPTION>
{
public:
	ExceptionNode(MemoryPool& pool, const MetaName& name,
				ValueExprNode* aMessageExpr = NULL, ValueListNode* aParameters = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_EXCEPTION>(pool),
		  messageExpr(aMessageExpr),
		  parameters(aParameters)
	{
		exception = FB_NEW_POOL(pool) ExceptionItem(pool);
		exception->type = ExceptionItem::XCP_CODE;
		exception->name = name.c_str();
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ExceptionNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ExceptionNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ForNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual StmtNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual StmtNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

	bool isWriteLockMode(jrd_req* request) const;
	void setWriteLockMode(jrd_req* request) const;

	// Used by UPDATE and DELETE sub-statements of MERGE
	void checkRecordUpdated(thread_db* tdbb, jrd_req* request, record_param* rpb) const;
	void setRecordUpdated(thread_db* tdbb, jrd_req* request, record_param* rpb) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual HandlerNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual HandlerNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual HandlerNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual LabelNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual LabelNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual LabelNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual LineColumnNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

private:
	NestConst<StmtNode> statement;
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual LoopNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual LoopNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual LoopNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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
		Nullable<OverrideClause> overrideClause;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

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
public:
	explicit MessageNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_MESSAGE>(pool),
		  itemsUsedInSubroutines(pool)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	void setup(thread_db* tdbb, CompilerScratch* csb, USHORT message, USHORT count);

	virtual USHORT setupDesc(thread_db* tdbb, CompilerScratch* csb, USHORT index,
		dsc* desc, ItemInfo* itemInfo);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual MessageNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual MessageNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual MessageNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual MessageNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

public:
	Firebird::SortedArray<USHORT> itemsUsedInSubroutines;
	NestConst<Format> format;
	ULONG impureFlags = 0;
	USHORT messageNumber = 0;
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	StmtNode* internalDsqlPass(DsqlCompilerScratch* dsqlScratch, bool updateOrInsert);
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ModifyNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ModifyNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static void pass1Modify(thread_db* tdbb, CompilerScratch* csb, ModifyNode* node);
	const StmtNode* modify(thread_db* tdbb, jrd_req* request, WhichTrigger whichTrig) const;

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
	Nullable<USHORT> dsqlReturningLocalTableNumber;
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

	OuterMapNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		return this;
	}

	OuterMapNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		return this;
	}

	const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const override;
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual PostEventNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual PostEventNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual PostEventNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ReceiveNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual ReceiveNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual ReceiveNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	StmtNode* internalDsqlPass(DsqlCompilerScratch* dsqlScratch, bool updateOrInsert, bool& needSavePoint);
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual StoreNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual StoreNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

private:
	static bool pass1Store(thread_db* tdbb, CompilerScratch* csb, StoreNode* node);
	void makeDefaults(thread_db* tdbb, CompilerScratch* csb);
	const StmtNode* store(thread_db* tdbb, jrd_req* request, WhichTrigger whichTrig) const;

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
	Nullable<USHORT> dsqlReturningLocalTableNumber;
	Nullable<OverrideClause> overrideClause;
};


class SelectNode final : public TypedNode<StmtNode, StmtNode::TYPE_SELECT>
{
public:
	explicit SelectNode(MemoryPool& pool)
		: TypedNode<StmtNode, StmtNode::TYPE_SELECT>(pool),
		  dsqlExpr(NULL),
		  dsqlRse(NULL),
		  statements(pool),
		  dsqlForUpdate(false),
		  dsqlWithLock(false)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual SelectNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual SelectNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual SelectNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

public:
	NestConst<SelectExprNode> dsqlExpr;
	NestConst<RseNode> dsqlRse;
	Firebird::Array<NestConst<StmtNode> > statements;
	bool dsqlForUpdate;
	bool dsqlWithLock;
};


// This is only for GPRE's cmp_set_generator().
class SetGeneratorNode final : public TypedNode<StmtNode, StmtNode::TYPE_SET_GENERATOR>
{
public:
	SetGeneratorNode(MemoryPool& pool, const MetaName& name, ValueExprNode* aValue = NULL)
		: TypedNode<StmtNode, StmtNode::TYPE_SET_GENERATOR>(pool),
		  generator(pool, name), value(aValue)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;

	// DSQL support is implemented in CreateAlterSequenceNode.
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch)
	{
		fb_assert(false);
	}

	virtual SetGeneratorNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual SetGeneratorNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StallNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual StallNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual StallNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;
};


class SuspendNode : public TypedNode<StmtNode, StmtNode::TYPE_SUSPEND>
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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual SuspendNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual SuspendNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual SuspendNode* pass2(thread_db* tdbb, CompilerScratch* csb);
	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual ReturnNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

public:
	NestConst<ValueExprNode> value;
};


class SavepointEncloseNode final : public TypedNode<StmtNode, StmtNode::TYPE_SAVEPOINT>
{
public:
	explicit SavepointEncloseNode(MemoryPool& pool, StmtNode* stmt)
		: TypedNode<StmtNode, StmtNode::TYPE_SAVEPOINT>(pool),
		  statement(stmt)
	{
	}

public:
	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	static StmtNode* make(MemoryPool& pool, DsqlCompilerScratch* dsqlScratch, StmtNode* node);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual SavepointEncloseNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

	virtual SavepointEncloseNode* pass1(thread_db* tdbb, CompilerScratch* csb);
	virtual SavepointEncloseNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const;

public:
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
	virtual SessionManagementWrapperNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		Node::dsqlPass(dsqlScratch);

		// Save and reset the statement type, as SessionManagementNode sets it to TYPE_SESSION_MANAGEMENT but
		// we are a DML statement.
		DsqlStatement::Type statementType = dsqlScratch->getStatement()->getType();
		wrapped->dsqlPass(dsqlScratch);
		dsqlScratch->getStatement()->setType(statementType);

		return this;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		DsqlOnlyStmtNode::internalPrint(printer);

		NODE_PRINT(printer, wrapped);
		NODE_PRINT(printer, text);

		return "SessionManagementWrapperNode";
	}

	virtual void genBlr(DsqlCompilerScratch* dsqlScratch)
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


class SetTransactionNode : public TransactionNode
{
public:
	struct RestrictionOption : Firebird::PermanentStorage
	{
		RestrictionOption(MemoryPool& p, Firebird::ObjectsArray<MetaName>* aTables,
					unsigned aLockMode)
			: PermanentStorage(p),
			  tables(aTables),
			  lockMode(aLockMode)
		{
		}

		Firebird::ObjectsArray<MetaName>* tables;
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

	static const unsigned LOCK_MODE_SHARED 		= 0x1;
	static const unsigned LOCK_MODE_PROTECTED	= 0x2;
	static const unsigned LOCK_MODE_READ		= 0x4;
	static const unsigned LOCK_MODE_WRITE		= 0x8;

public:
	explicit SetTransactionNode(MemoryPool& pool)
		: TransactionNode(pool),
		  reserveList(pool),
		  tpb(pool)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		TransactionNode::internalPrint(printer);

		NODE_PRINT(printer, readOnly);
		NODE_PRINT(printer, wait);
		NODE_PRINT(printer, isoLevel);
		NODE_PRINT(printer, noAutoUndo);
		NODE_PRINT(printer, ignoreLimbo);
		NODE_PRINT(printer, restartRequests);
		NODE_PRINT(printer, autoCommit);
		NODE_PRINT(printer, lockTimeout);
		//// FIXME-PRINT: NODE_PRINT(printer, reserveList);
		NODE_PRINT(printer, tpb);

		return "SetTransactionNode";
	}

	virtual SetTransactionNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const;

private:
	void genTableLock(DsqlCompilerScratch* dsqlScratch, const RestrictionOption& tblLock,
		USHORT lockLevel);

public:
	Firebird::Array<RestrictionOption*> reserveList;
	Firebird::UCharBuffer tpb;
	Nullable<CommitNumber> atSnapshotNumber;
	Nullable<unsigned> isoLevel;
	Nullable<USHORT> lockTimeout;
	Nullable<bool> readOnly;
	Nullable<bool> wait;
	Nullable<bool> noAutoUndo;
	Nullable<bool> ignoreLimbo;
	Nullable<bool> restartRequests;
	Nullable<bool> autoCommit;
};


class CommitRollbackNode : public TransactionNode
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
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual CommitRollbackNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const;

private:
	const Command command;
	const bool retain;
};


class UserSavepointNode : public TransactionNode
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
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual UserSavepointNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const;

public:
	const Command command;
	const MetaName name;
};


class SessionResetNode : public SessionManagementNode
{
public:
	explicit SessionResetNode(MemoryPool& pool)
		: SessionManagementNode(pool)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		return "SessionResetNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;
};


class SetRoleNode : public SessionManagementNode
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
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, trusted);
		NODE_PRINT(printer, roleName);

		return "SetRoleNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

public:
	bool trusted;
	MetaName roleName;
};


class SetSessionNode : public SessionManagementNode
{
public:
	enum Type : UCHAR
	{
		TYPE_IDLE_TIMEOUT,
		TYPE_STMT_TIMEOUT
	};

	SetSessionNode(MemoryPool& pool, Type aType, ULONG aVal, UCHAR blr_timepart);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

private:
	Type m_type;
	ULONG m_value;
};


class SetDebugOptionNode : public SessionManagementNode
{
public:
	SetDebugOptionNode(MemoryPool& pool, MetaName* aName, ExprNode* aValue);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, name);
		NODE_PRINT(printer, value);

		return "SetDebugOptionNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

private:
	MetaName name;
	ExprNode* value;
};


class SetDecFloatRoundNode : public SessionManagementNode
{
public:
	SetDecFloatRoundNode(MemoryPool& pool, MetaName* name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, rndMode);

		return "SetDecFloatRoundNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

public:
	USHORT rndMode;
};


class SetDecFloatTrapsNode : public SessionManagementNode
{
public:
	SetDecFloatTrapsNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  traps(0u)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, traps);

		return "SetDecFloatTrapsNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

	void trap(MetaName* name);

public:
	USHORT traps;
};


class SetBindNode : public SessionManagementNode
{
public:
	SetBindNode(MemoryPool& pool)
		: SessionManagementNode(pool),
		  from(nullptr),
		  to(nullptr)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, from);
		NODE_PRINT(printer, to);

		return "SetBindNode";
	}

	virtual SessionManagementNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

public:
	dsql_fld* from;
	dsql_fld* to;
};


class SetTimeZoneNode : public SessionManagementNode
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
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		SessionManagementNode::internalPrint(printer);

		NODE_PRINT(printer, str);
		NODE_PRINT(printer, local);

		return "SetTimeZoneNode";
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const;

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

	const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const override;

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

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);

public:
	NestConst<RelationSourceNode> relation;
	Firebird::Array<NestConst<FieldNode>> fields;
	NestConst<ValueListNode> values;
	Firebird::Array<NestConst<FieldNode>> matching;
	NestConst<PlanNode> plan;
	NestConst<ValueListNode> order;
	NestConst<RowsClause> rows;
	NestConst<ReturningClause> returning;
	Nullable<OverrideClause> overrideClause;
	NestConst<StoreNode> storeNode;
	NestConst<ModifyNode> modifyNode;
	Firebird::Array<NestConst<AssignmentNode>> varAssignments;
};


} // namespace

#endif // DSQL_STMT_NODES_H
