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

#ifndef DSQL_AGG_NODES_H
#define DSQL_AGG_NODES_H

#include "firebird/impl/blr.h"
#include "../dsql/Nodes.h"
#include "../dsql/NodePrinter.h"

namespace Jrd {


class AnyValueAggNode final : public AggNode
{
public:
	explicit AnyValueAggNode(MemoryPool& pool, ValueExprNode* aArg = nullptr);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

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

class AvgAggNode final : public AggNode
{
public:
	explicit AvgAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	const char* getCompatDialectVerb() override
	{
		return "avg";
	}

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
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
	void outputDesc(dsc* desc) const;
	ULONG tempImpure;
};

class ListAggNode final : public AggNode
{
public:
	explicit ListAggNode(MemoryPool& pool, bool aDistinct, ValueExprNode* aArg = nullptr,
			ValueExprNode* aDelimiter = nullptr, ValueListNode* aOrderClause = nullptr);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	unsigned getCapabilities() const override
	{
		return CAP_WANTS_AGG_CALLS;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		AggNode::getChildren(holder, dsql);
		holder.add(delimiter);
	}

	bool dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const override;

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	bool setParameterType(DsqlCompilerScratch* dsqlScratch,
		std::function<void (dsc*)> makeDesc, bool forceVarChar) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

private:
	NestConst<ValueExprNode> delimiter;
	NestConst<ValueListNode> dsqlOrderClause;
};

class PercentileAggNode final : public AggNode
{
public:
	enum PercentileType : UCHAR
	{
		TYPE_PERCENTILE_CONT,
		TYPE_PERCENTILE_DISC
	};

	struct PercentileImpure
	{
		SINT64 vlux_count;
		double percentile;
		double rn;
		SINT64 crn;
		SINT64 frn;
	};

	explicit PercentileAggNode(MemoryPool& pool, PercentileType aType, ValueExprNode* aArg = nullptr,
		ValueListNode* aOrderClause = nullptr);

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	unsigned getCapabilities() const override
	{
		return CAP_WANTS_AGG_CALLS;
	}

	bool dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const override;

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		AggNode::getChildren(holder, dsql);
		holder.add(valueArg);
	}

	bool dsqlInvalidReferenceFinder(InvalidReferenceFinder& visitor) override;

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	void makeSortDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;

	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	bool aggPass(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

private:
	const PercentileType type;
	NestConst<ValueExprNode> valueArg;
	NestConst<ValueListNode> dsqlOrderClause;
	ULONG percentileImpureOffset = 0;
};

class CountAggNode final : public AggNode
{
public:
	explicit CountAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

class SumAggNode final : public AggNode
{
public:
	explicit SumAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	const char* getCompatDialectVerb() override
	{
		return "sum";
	}

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
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

class MaxMinAggNode final : public AggNode
{
public:
	enum MaxMinType
	{
		TYPE_MAX,
		TYPE_MIN
	};

	explicit MaxMinAggNode(MemoryPool& pool, MaxMinType aType, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
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

public:
	const MaxMinType type;
};

class BinAggNode final : public AggNode
{
public:
    enum BinType : UCHAR
	{
        TYPE_BIN_AND,
		TYPE_BIN_OR,
		TYPE_BIN_XOR,
		TYPE_BIN_XOR_DISTINCT
	};

	explicit BinAggNode(MemoryPool& pool, BinType aType, ValueExprNode* aArg = nullptr);

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
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

public:
	const BinType type;
};

class StdDevAggNode final : public AggNode
{
public:
	enum StdDevType
	{
		TYPE_STDDEV_SAMP,
		TYPE_STDDEV_POP,
		TYPE_VAR_SAMP,
		TYPE_VAR_POP
	};

	union StdDevImpure
	{
		struct
		{
			double x, x2;
		} dbl;
		struct
		{
			Firebird::Decimal128 x, x2;
		} dec;
	};

	explicit StdDevAggNode(MemoryPool& pool, StdDevType aType, ValueExprNode* aArg = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

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

public:
	const StdDevType type;

private:
	ULONG impure2Offset;
};

class CorrAggNode final : public AggNode
{
public:
	enum CorrType
	{
		TYPE_COVAR_SAMP,
		TYPE_COVAR_POP,
		TYPE_CORR
	};

	union CorrImpure
	{
		struct
		{
			double x, x2, y, y2, xy;
		} dbl;
		struct
		{
			Firebird::Decimal128 x, x2, y, y2, xy;
		} dec;
	};

	explicit CorrAggNode(MemoryPool& pool, CorrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	bool aggPass(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

public:
	const CorrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrAggNode final : public AggNode
{
public:
	enum RegrType
	{
		TYPE_REGR_AVGX,
		TYPE_REGR_AVGY,
		TYPE_REGR_INTERCEPT,
		TYPE_REGR_R2,
		TYPE_REGR_SLOPE,
		TYPE_REGR_SXX,
		TYPE_REGR_SXY,
		TYPE_REGR_SYY
	};

	union RegrImpure
	{
		struct
		{
			double x, x2, y, y2, xy;
		} dbl;
		struct
		{
			Firebird::Decimal128 x, x2, y, y2, xy;
		} dec;
	};

	explicit RegrAggNode(MemoryPool& pool, RegrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;
	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	bool aggPass(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

public:
	const RegrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrCountAggNode final : public AggNode
{
public:
	explicit RegrCountAggNode(MemoryPool& pool,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	bool aggPass(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;

public:
	NestConst<ValueExprNode> arg2;
};

} // namespace

#endif // DSQL_AGG_NODES_H
