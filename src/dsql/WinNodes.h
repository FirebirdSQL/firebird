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
 *  Copyright (c) 2010 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_WIN_NODES_H
#define DSQL_WIN_NODES_H

#include "firebird/impl/blr.h"
#include "../dsql/Nodes.h"
#include "../dsql/NodePrinter.h"

namespace Jrd {


// DENSE_RANK function.
class DenseRankWinNode final : public WinFuncNode
{
public:
	explicit DenseRankWinNode(MemoryPool& pool);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		// nothing
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

// RANK function.
class RankWinNode final : public WinFuncNode
{
public:
	explicit RankWinNode(MemoryPool& pool);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		// nothing
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

private:
	ULONG tempImpure;
};

// PERCENT_RANK function.
class PercentRankWinNode final : public WinFuncNode
{
public:
	explicit PercentRankWinNode(MemoryPool& pool);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS | CAP_WANTS_WIN_PASS_CALL;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		// nothing
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

private:
	ULONG tempImpure;
};

// CUME_DIST function.
class CumeDistWinNode final : public WinFuncNode
{
public:
	explicit CumeDistWinNode(MemoryPool& pool);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		// nothing
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

// ROW_NUMBER function.
class RowNumberWinNode final : public WinFuncNode
{
public:
	explicit RowNumberWinNode(MemoryPool& pool);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		// nothing
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

// FIRST_VALUE function.
class FirstValueWinNode final : public WinFuncNode
{
public:
	explicit FirstValueWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;
};

// LAST_VALUE function.
class LastValueWinNode final : public WinFuncNode
{
public:
	explicit LastValueWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;
};

// NTH_VALUE function.
class NthValueWinNode final : public WinFuncNode
{
public:
	enum
	{
		FROM_FIRST = 0,
		FROM_LAST
	};

public:
	explicit NthValueWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL,
		ValueExprNode* aRow = NULL, ValueExprNode* aFrom = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		WinFuncNode::getChildren(holder, dsql);
		holder.add(row);
		holder.add(from);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

private:
	NestConst<ValueExprNode> row;
	NestConst<ValueExprNode> from;
};

// LAG/LEAD function.
class LagLeadWinNode : public WinFuncNode
{
public:
	explicit LagLeadWinNode(MemoryPool& pool, const AggInfo& aAggInfo, int aDirection,
		ValueExprNode* aArg = NULL, ValueExprNode* aRows = NULL, ValueExprNode* aOutExpr = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		WinFuncNode::getChildren(holder, dsql);
		holder.add(rows);
		holder.add(outExpr);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override = 0;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

protected:
	const int direction;
	NestConst<ValueExprNode> rows;
	NestConst<ValueExprNode> outExpr;
};

// LAG function.
class LagWinNode final : public LagLeadWinNode
{
public:
	explicit LagWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL, ValueExprNode* aRows = NULL,
		ValueExprNode* aOutExpr = NULL);

	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		LagLeadWinNode::internalPrint(printer);
		return "LagWinNode";
	}

	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

// LEAD function.
class LeadWinNode final : public LagLeadWinNode
{
public:
	explicit LeadWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL, ValueExprNode* aRows = NULL,
		ValueExprNode* aOutExpr = NULL);

	Firebird::string internalPrint(NodePrinter& printer) const override
	{
		LagLeadWinNode::internalPrint(printer);
		return "LeadWinNode";
	}

	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

// NTILE function.
class NTileWinNode final : public WinFuncNode
{
public:
	explicit NTileWinNode(MemoryPool& pool, ValueExprNode* aArg = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_SUPPORTS_WINDOW_FRAME | CAP_WANTS_WIN_PASS_CALL;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;

	dsc* winPass(thread_db* tdbb, Request* request, SlidingWindow* window) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

private:
	struct ThisImpure
	{
		SINT64 buckets;
	};

	ULONG thisImpureOffset;
};


} // namespace

#endif // DSQL_WIN_NODES_H
