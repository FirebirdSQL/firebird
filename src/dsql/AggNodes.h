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

#include "../jrd/blr.h"
#include "../dsql/Nodes.h"
#include "../dsql/NodePrinter.h"

namespace Jrd {


class AvgAggNode : public AggNode
{
public:
	explicit AvgAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual const char* getCompatDialectVerb()
	{
		return "avg";
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

private:
	ULONG tempImpure;
};

class ListAggNode : public AggNode
{
public:
	explicit ListAggNode(MemoryPool& pool, bool aDistinct, ValueExprNode* aArg = NULL,
		ValueExprNode* aDelimiter = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(delimiter);
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual bool setParameterType(DsqlCompilerScratch* dsqlScratch,
		const dsc* desc, ValueExprNode* node, bool forceVarChar);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void checkOrderedWindowCapable() const
	{
		Firebird::status_exception::raise(
			Firebird::Arg::Gds(isc_wish_list) <<
			Firebird::Arg::Gds(isc_random) <<
				"LIST is not supported in ordered windows");
	}

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

private:
	NestConst<ValueExprNode> delimiter;
};

class CountAggNode : public AggNode
{
public:
	explicit CountAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;
};

class SumAggNode : public AggNode
{
public:
	explicit SumAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual const char* getCompatDialectVerb()
	{
		return "sum";
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;
};

class MaxMinAggNode : public AggNode
{
public:
	enum MaxMinType
	{
		TYPE_MAX,
		TYPE_MIN
	};

	explicit MaxMinAggNode(MemoryPool& pool, MaxMinType aType, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const MaxMinType type;
};

class StdDevAggNode : public AggNode
{
public:
	enum StdDevType
	{
		TYPE_STDDEV_SAMP,
		TYPE_STDDEV_POP,
		TYPE_VAR_SAMP,
		TYPE_VAR_POP
	};

	struct StdDevImpure
	{
		double x, x2;
	};

	explicit StdDevAggNode(MemoryPool& pool, StdDevType aType, ValueExprNode* aArg = NULL);

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const StdDevType type;

private:
	ULONG impure2Offset;
};

class CorrAggNode : public AggNode
{
public:
	enum CorrType
	{
		TYPE_COVAR_SAMP,
		TYPE_COVAR_POP,
		TYPE_CORR
	};

	struct CorrImpure
	{
		double x, x2, y, y2, xy;
	};

	explicit CorrAggNode(MemoryPool& pool, CorrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual bool aggPass(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const CorrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrAggNode : public AggNode
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

	struct RegrImpure
	{
		double x, x2, y, y2, xy;
	};

	explicit RegrAggNode(MemoryPool& pool, RegrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual bool aggPass(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const RegrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrCountAggNode : public AggNode
{
public:
	explicit RegrCountAggNode(MemoryPool& pool,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual bool aggPass(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	NestConst<ValueExprNode> arg2;
};

} // namespace

#endif // DSQL_AGG_NODES_H
