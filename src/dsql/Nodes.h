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

#ifndef DSQL_NODES_H
#define DSQL_NODES_H

#include "../jrd/jrd.h"
#include "../dsql/DsqlCompilerScratch.h"
#include "../dsql/Visitors.h"
#include "../common/classes/array.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/TriState.h"
#include <functional>
#include <initializer_list>
#include <type_traits>

namespace Jrd {

class AggregateSort;
class CompilerScratch;
class SubQuery;
class Cursor;
class Node;
class NodePrinter;
class ExprNode;
class NodeRefsHolder;
class Optimizer;
class OptimizerRetrieval;
class RecordSource;
class RseNode;
class SlidingWindow;
class TypeClause;
class ValueExprNode;
class SortNode;


// Must be less then MAX_SSHORT. Not used for static arrays.
inline constexpr unsigned MAX_CONJUNCTS = 32000;

// New: MAX_STREAMS should be a multiple of BITS_PER_LONG (32 and hard to believe it will change)

inline constexpr StreamType INVALID_STREAM = ~StreamType(0);
inline constexpr StreamType MAX_STREAMS = 4096;

inline constexpr StreamType STREAM_MAP_LENGTH = MAX_STREAMS + 2;

// New formula is simply MAX_STREAMS / BITS_PER_LONG
inline constexpr int OPT_STREAM_BITS = MAX_STREAMS / BITS_PER_LONG; // 128 with 4096 streams

typedef Firebird::HalfStaticArray<StreamType, OPT_STATIC_STREAMS> StreamList;
typedef Firebird::SortedArray<StreamType> SortedStreamList;

typedef Firebird::Array<NestConst<ValueExprNode> > NestValueArray;


class Printable
{
public:
	virtual ~Printable()
	{
	}

public:
	void print(NodePrinter& printer) const;

	virtual Firebird::string internalPrint(NodePrinter& printer) const = 0;
};


template <typename T>
class RegisterNode
{
public:
	explicit RegisterNode(std::initializer_list<UCHAR> blrList)
	{
		for (const auto blr : blrList)
			PAR_register(blr, T::parse);
	}
};

template <typename T>
class RegisterBoolNode
{
public:
	explicit RegisterBoolNode(std::initializer_list<UCHAR> blrList)
	{
		for (const auto blr : blrList)
			PAR_register(blr, T::parse);
	}
};


class Node : public Printable
{
public:
	explicit Node(MemoryPool& pool)
		: line(0),
		  column(0)
	{
	}

	virtual ~Node()
	{
	}

	// Compile a parsed statement into something more interesting.
	template <typename T>
	static T* doDsqlPass(DsqlCompilerScratch* dsqlScratch, NestConst<T>& node)
	{
		if (!node)
			return NULL;

		return node->dsqlPass(dsqlScratch);
	}

	// Compile a parsed statement into something more interesting and assign it to target.
	template <typename T1, typename T2>
	static void doDsqlPass(DsqlCompilerScratch* dsqlScratch, NestConst<T1>& target, NestConst<T2>& node)
	{
		if (!node)
			target = NULL;
		else
			target = node->dsqlPass(dsqlScratch);
	}

	// Changes dsqlScratch->isPsql() value, calls doDsqlPass and restore dsqlScratch->isPsql().
	template <typename T>
	static T* doDsqlPass(DsqlCompilerScratch* dsqlScratch, NestConst<T>& node, bool psql)
	{
		PsqlChanger changer(dsqlScratch, psql);
		return doDsqlPass(dsqlScratch, node);
	}

	// Changes dsqlScratch->isPsql() value, calls doDsqlPass and restore dsqlScratch->isPsql().
	template <typename T1, typename T2>
	static void doDsqlPass(DsqlCompilerScratch* dsqlScratch, NestConst<T1>& target, NestConst<T2>& node, bool psql)
	{
		PsqlChanger changer(dsqlScratch, psql);
		doDsqlPass(dsqlScratch, target, node);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override = 0;

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
	}

	virtual Node* dsqlPass(DsqlCompilerScratch* /*dsqlScratch*/)
	{
		return this;
	}

public:
	ULONG line;
	ULONG column;
};


class DdlNode : public Node
{
public:
	explicit DdlNode(MemoryPool& pool)
		: Node(pool)
	{
	}

	static void protectSystemSchema(const MetaName& name, ObjectType objType)
	{
		if (name == SYSTEM_SCHEMA)
		{
			Firebird::status_exception::raise(
				Firebird::Arg::Gds(isc_dyn_cannot_mod_obj_sys_schema) <<
				getObjectName(objType));
		}
	}

	static bool deleteSecurityClass(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& secClass);

	static void storePrivileges(thread_db* tdbb, jrd_tra* transaction,
		const QualifiedName& name, int type, const char* privileges);

	static void deletePrivilegesByRelName(thread_db* tdbb, jrd_tra* transaction,
		const QualifiedName& name, int type);

public:
	// Check permission on DDL operation. Return true if everything is OK.
	// Raise an exception for bad permission.
	// If returns false permissions will be check in old style at vio level as well as while direct RDB$ tables modify.
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction) = 0;

	// Set the scratch's transaction when executing a node. Fact of accessing the scratch during
	// execution is a hack.
	void executeDdl(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction, bool trusted = false)
	{
		// dsqlScratch should be NULL with CREATE DATABASE.
		if (dsqlScratch)
			dsqlScratch->setTransaction(transaction);

		if (!trusted)
			checkPermission(tdbb, transaction);
		execute(tdbb, dsqlScratch, transaction);
	}

	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		dsqlScratch->getDsqlStatement()->setType(DsqlStatement::TYPE_DDL);
		return this;
	}

public:
	enum DdlTriggerWhen { DTW_BEFORE, DTW_AFTER };

	static void executeDdlTrigger(thread_db* tdbb, jrd_tra* transaction,
		DdlTriggerWhen when, int action, const QualifiedName& objectName,
		const QualifiedName& oldNewObjectName, const Firebird::string& sqlText);

	// Update RDB$FIELDS received by reference.
	static void updateRdbFields(const Jrd::TypeClause* type,
		SSHORT& fieldType,
		SSHORT& fieldLength,
		SSHORT& fieldSubTypeNull, SSHORT& fieldSubType,
		SSHORT& fieldScaleNull, SSHORT& fieldScale,
		SSHORT& characterSetIdNull, SSHORT& characterSetId,
		SSHORT& characterLengthNull, SSHORT& characterLength,
		SSHORT& fieldPrecisionNull, SSHORT& fieldPrecision,
		SSHORT& collationIdNull, SSHORT& collationId,
		SSHORT& segmentLengthNull, SSHORT& segmentLength);

protected:
	typedef Firebird::Pair<Firebird::Left<MetaName, bid> > MetaNameBidPair;
	typedef Firebird::GenericMap<MetaNameBidPair> MetaNameBidMap;

	// Return exception code based on combination of create and alter clauses.
	static ISC_STATUS createAlterCode(bool create, bool alter, ISC_STATUS createCode,
		ISC_STATUS alterCode, ISC_STATUS createOrAlterCode)
	{
		if (create && alter)
			return createOrAlterCode;

		if (create)
			return createCode;

		if (alter)
			return alterCode;

		fb_assert(false);
		return 0;
	}

	void executeDdlTrigger(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		DdlTriggerWhen when, int action, const QualifiedName& objectName,
		const QualifiedName& oldNewObjectName);
	void storeGlobalField(thread_db* tdbb, jrd_tra* transaction, QualifiedName& name,
		const TypeClause* field,
		const Firebird::string& computedSource = "",
		const BlrDebugWriter::BlrData& computedValue = BlrDebugWriter::BlrData());

public:
	// Prefix DDL exceptions. To be implemented in each command.
	// Attention: do not store temp strings in Arg::StatusVector,
	// when needed keep them permanently in command's node.
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector) = 0;

	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) = 0;

	virtual bool mustBeReplicated() const
	{
		return true;
	}

	virtual bool disallowedInReadOnlyDatabase() const
	{
		return true;
	}
};


class TransactionNode : public Node
{
public:
	explicit TransactionNode(MemoryPool& pool)
		: Node(pool)
	{
	}

public:
	TransactionNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		Node::dsqlPass(dsqlScratch);
		return this;
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** transaction) const = 0;
};


class SessionManagementNode : public Node
{
public:
	explicit SessionManagementNode(MemoryPool& pool)
		: Node(pool)
	{
	}

public:
	SessionManagementNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		Node::dsqlPass(dsqlScratch);

		dsqlScratch->getDsqlStatement()->setType(DsqlStatement::TYPE_SESSION_MANAGEMENT);

		return this;
	}

	virtual void execute(thread_db* tdbb, DsqlRequest* request, jrd_tra** traHandle) const = 0;
};


class DmlNode : public Node
{
public:
	// DML node kinds
	enum Kind
	{
		KIND_STATEMENT,
		KIND_VALUE,
		KIND_BOOLEAN,
		KIND_REC_SOURCE,
		KIND_LIST
	};

	explicit DmlNode(MemoryPool& pool)
		: Node(pool)
	{
	}

	// Merge missing values, computed values, validation expressions, and views into a parsed request.
	template <typename T> static void doPass1(thread_db* tdbb, CompilerScratch* csb, T** node)
	{
		if (!*node)
			return;

		*node = (*node)->pass1(tdbb, csb);
	}

public:
	virtual Kind getKind() = 0;
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch) = 0;
	virtual DmlNode* pass1(thread_db* tdbb, CompilerScratch* csb) = 0;
	virtual DmlNode* pass2(thread_db* tdbb, CompilerScratch* csb) = 0;
	virtual DmlNode* copy(thread_db* tdbb, NodeCopier& copier) const = 0;
};


template <typename T, typename T::Type typeConst>
class TypedNode : public T
{
public:
	explicit TypedNode(MemoryPool& pool)
		: T(typeConst, pool)
	{
	}

public:
	typename T::Type getType() const override
	{
		return typeConst;
	}

public:
	const static typename T::Type TYPE = typeConst;
};


template <typename To, typename From> static To* nodeAs(From* fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE ? static_cast<To*>(fromNode) : NULL;
}

template <typename To, typename From> static To* nodeAs(NestConst<From>& fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE ? static_cast<To*>(fromNode.getObject()) : NULL;
}

template <typename To, typename From> static const To* nodeAs(const From* fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE ? static_cast<const To*>(fromNode) : NULL;
}

template <typename To, typename From> static const To* nodeAs(const NestConst<From>& fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE ? static_cast<const To*>(fromNode.getObject()) : NULL;
}

template <typename To, typename From> static bool nodeIs(const From* fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE;
}

template <typename To, typename From> static bool nodeIs(const NestConst<From>& fromNode)
{
	return fromNode && fromNode->getType() == To::TYPE;
}


class NodeRefsHolder : public Firebird::AutoStorage
{
public:
	NodeRefsHolder()
		: refs(getPool())
	{
	}

	explicit NodeRefsHolder(MemoryPool& pool)
		: AutoStorage(pool),
		  refs(pool)
	{
	}

	template <typename T> void add(const NestConst<T>& node)
	{
		static_assert(std::is_base_of<ExprNode, T>::value, "T must be derived from ExprNode");

		static_assert(
			std::is_convertible<
				decltype(const_cast<T*>(node.getObject())->pass1(
					(thread_db*) nullptr, (CompilerScratch*) nullptr)),
				decltype(const_cast<T*>(node.getObject()))
			>::value,
			"pass1 problem");

		static_assert(
			std::is_convertible<
				decltype(const_cast<T*>(node.getObject())->pass2(
					(thread_db*) nullptr, (CompilerScratch*) nullptr)),
				decltype(const_cast<T*>(node.getObject()))
			>::value,
			"pass2 problem");

		static_assert(
			std::is_convertible<
				decltype(const_cast<T*>(node.getObject())->dsqlFieldRemapper(*(FieldRemapper*) nullptr)),
				decltype(const_cast<T*>(node.getObject()))
			>::value,
			"dsqlFieldRemapper problem");

		T** ptr = const_cast<T**> (node.getAddress());
		fb_assert(ptr);

		refs.add(reinterpret_cast<ExprNode**>(ptr));
	}

public:
	Firebird::HalfStaticArray<ExprNode**, 8> refs;
};


class ExprNode : public DmlNode
{
public:
	enum Type
	{
		// Value types
		TYPE_AGGREGATE,
		TYPE_ALIAS,
		TYPE_ARITHMETIC,
		TYPE_ARRAY,
		TYPE_AT,
		TYPE_BOOL_AS_VALUE,
		TYPE_CAST,
		TYPE_COALESCE,
		TYPE_COLLATE,
		TYPE_CONCATENATE,
		TYPE_CURRENT_DATE,
		TYPE_CURRENT_TIME,
		TYPE_CURRENT_TIMESTAMP,
		TYPE_CURRENT_ROLE,
		TYPE_CURRENT_SCHEMA,
		TYPE_CURRENT_USER,
		TYPE_DERIVED_EXPR,
		TYPE_DECODE,
		TYPE_DEFAULT,
		TYPE_DERIVED_FIELD,
		TYPE_DOMAIN_VALIDATION,
		TYPE_EXTRACT,
		TYPE_FIELD,
		TYPE_GEN_ID,
		TYPE_INTERNAL_INFO,
		TYPE_LITERAL,
		TYPE_LOCAL_TIME,
		TYPE_LOCAL_TIMESTAMP,
		TYPE_MAP,
		TYPE_NEGATE,
		TYPE_NULL,
		TYPE_ORDER,
		TYPE_OVER,
		TYPE_PARAMETER,
		TYPE_RECORD_KEY,
		TYPE_SCALAR,
		TYPE_STMT_EXPR,
		TYPE_STR_CASE,
		TYPE_STR_LEN,
		TYPE_SUBQUERY,
		TYPE_SUBSTRING,
		TYPE_SUBSTRING_SIMILAR,
		TYPE_SYSFUNC_CALL,
		TYPE_TRIM,
		TYPE_UDF_CALL,
		TYPE_VALUE_IF,
		TYPE_VARIABLE,
		TYPE_WINDOW_CLAUSE,
		TYPE_WINDOW_CLAUSE_FRAME,
		TYPE_WINDOW_CLAUSE_FRAME_EXTENT,
		TYPE_REFERENCE,

		// Bool types
		TYPE_BINARY_BOOL,
		TYPE_COMPARATIVE_BOOL,
		TYPE_MISSING_BOOL,
		TYPE_NOT_BOOL,
		TYPE_RSE_BOOL,
		TYPE_IN_LIST_BOOL,

		// RecordSource types
		TYPE_AGGREGATE_SOURCE,
		TYPE_LOCAL_TABLE,
		TYPE_PROCEDURE,
		TYPE_RELATION,
		TYPE_RSE,
		TYPE_SELECT_EXPR,
		TYPE_UNION,
		TYPE_WINDOW,
		TYPE_TABLE_VALUE_FUNCTION,

		// List types
		TYPE_REC_SOURCE_LIST,
		TYPE_VALUE_LIST
	};

	// Generic flags.
	static constexpr USHORT FLAG_INVARIANT	= 0x01;	// Node is recognized as being invariant.
	static constexpr USHORT FLAG_PATTERN_MATCHER_CACHE	= 0x02;

	// Boolean flags.
	static constexpr USHORT FLAG_DEOPTIMIZE	= 0x04;	// Boolean which requires deoptimization.
	static constexpr USHORT FLAG_RESIDUAL	= 0x08;	// Boolean which must remain residual.
	static constexpr USHORT FLAG_ANSI_NOT	= 0x10;	// ANY/ALL predicate is prefixed with a NOT one.

	// Value flags.
	static constexpr USHORT FLAG_DOUBLE		= 0x20;
	static constexpr USHORT FLAG_DATE		= 0x40;
	static constexpr USHORT FLAG_DECFLOAT	= 0x80;
	static constexpr USHORT FLAG_INT128		= 0x100;

	explicit ExprNode(Type aType, MemoryPool& pool)
		: DmlNode(pool),
		  impureOffset(0),
		  nodFlags(0)
	{
	}

	virtual const char* getCompatDialectVerb()
	{
		return NULL;
	}

	// Allocate and assign impure space for various nodes.
	template <typename T> static void doPass2(thread_db* tdbb, CompilerScratch* csb, T** node)
	{
		if (!*node)
			return;

		*node = (*node)->pass2(tdbb, csb);
	}

	virtual Type getType() const = 0;

	Firebird::string internalPrint(NodePrinter& printer) const override = 0;

	virtual bool dsqlAggregateFinder(AggregateFinder& visitor)
	{
		bool ret = false;

		NodeRefsHolder holder(visitor.getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
			ret |= visitor.visit(*i);

		return ret;
	}

	virtual bool dsqlAggregate2Finder(Aggregate2Finder& visitor)
	{
		bool ret = false;

		NodeRefsHolder holder(visitor.getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
			ret |= visitor.visit(*i);

		return ret;
	}

	virtual bool dsqlFieldFinder(FieldFinder& visitor)
	{
		bool ret = false;

		NodeRefsHolder holder(visitor.getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
			ret |= visitor.visit(*i);

		return ret;
	}

	virtual bool dsqlInvalidReferenceFinder(InvalidReferenceFinder& visitor)
	{
		bool ret = false;

		NodeRefsHolder holder(visitor.dsqlScratch->getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
			ret |= visitor.visit(*i);

		return ret;
	}

	virtual bool dsqlSubSelectFinder(SubSelectFinder& visitor)
	{
		bool ret = false;

		NodeRefsHolder holder(visitor.getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
			ret |= visitor.visit(*i);

		return ret;
	}

	virtual ExprNode* dsqlFieldRemapper(FieldRemapper& visitor)
	{
		NodeRefsHolder holder(visitor.getPool());
		getChildren(holder, true);

		for (auto i : holder.refs)
		{
			if (*i)
				*i = (*i)->dsqlFieldRemapper(visitor);
		}

		return this;
	}

	template <typename T>
	static void doDsqlFieldRemapper(FieldRemapper& visitor, NestConst<T>& node)
	{
		if (node)
			node = node->dsqlFieldRemapper(visitor);
	}

	template <typename T1, typename T2>
	static void doDsqlFieldRemapper(FieldRemapper& visitor, NestConst<T1>& target, NestConst<T2> node)
	{
		target = node ? node->dsqlFieldRemapper(visitor) : NULL;
	}

	// Check if expression returns deterministic result
	virtual bool deterministic(thread_db* tdbb) const;

	// Check if expression could return NULL or expression can turn NULL into a true/false.
	virtual bool possiblyUnknown() const;

	// Check if expression is known to ignore NULLs
	virtual bool ignoreNulls(const StreamList& streams) const;

	// Verify if this node is allowed in an unmapped boolean.
	virtual bool unmappable(const MapNode* mapNode, StreamType shellStream) const;

	// Check if expression returns constant result
	virtual bool constant() const
	{
		return false;
	}

	// Return all streams referenced by the expression.
	virtual void collectStreams(SortedStreamList& streamList) const;

	bool isChildrenConstant() const;

	bool containsStream(StreamType stream, bool only = false) const
	{
		SortedStreamList nodeStreams;
		collectStreams(nodeStreams);

		return only ?
			nodeStreams.getCount() == 1 && nodeStreams[0] == stream :
			nodeStreams.exist(stream);
	}

	bool containsAnyStream(const StreamList& streams) const
	{
		SortedStreamList nodeStreams;
		collectStreams(nodeStreams);

		for (const auto stream : streams)
		{
			if (nodeStreams.exist(stream))
				return true;
		}

		return false;
	}

	bool containsAnyStream() const
	{
		SortedStreamList nodeStreams;
		collectStreams(nodeStreams);

		return nodeStreams.hasData();
	}

	virtual bool dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const;

	ExprNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		DmlNode::dsqlPass(dsqlScratch);
		return this;
	}

	// Determine if two expression trees are the same.
	virtual bool sameAs(const ExprNode* other, bool ignoreStreams) const;

	// See if node is presently computable.
	// A node is said to be computable, if all the streams involved
	// in that node are csb_active. The csb_active flag defines
	// all the streams available in the current scope of the query.
	virtual bool computable(CompilerScratch* csb, StreamType stream,
		bool allowOnlyCurrentStream, ValueExprNode* value = NULL);

	virtual void findDependentFromStreams(const CompilerScratch* csb,
		StreamType currentStream, SortedStreamList* streamList);
	ExprNode* pass1(thread_db* tdbb, CompilerScratch* csb) override;
	ExprNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	virtual ExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override = 0;

public:
	ULONG impureOffset;
	USHORT nodFlags;
};


class BoolExprNode : public ExprNode
{
public:
	BoolExprNode(Type aType, MemoryPool& pool)
		: ExprNode(aType, pool)
	{
	}

	Kind getKind() override
	{
		return KIND_BOOLEAN;
	}

	BoolExprNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		ExprNode::dsqlPass(dsqlScratch);
		return this;
	}

	BoolExprNode* dsqlFieldRemapper(FieldRemapper& visitor) override
	{
		ExprNode::dsqlFieldRemapper(visitor);
		return this;
	}

	BoolExprNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass1(tdbb, csb);
		return this;
	}

	BoolExprNode* pass2(thread_db* tdbb, CompilerScratch* csb) override final;

	virtual void pass2Boolean(thread_db* /*tdbb*/, CompilerScratch* /*csb*/, std::function<void ()> process)
	{
		process();
	}

	BoolExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override = 0;
	virtual Firebird::TriState execute(thread_db* tdbb, Request* request) const = 0;
};

class ValueExprNode : public ExprNode
{
public:
	ValueExprNode(Type aType, MemoryPool& pool)
		: ExprNode(aType, pool)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override = 0;

	Kind getKind() override
	{
		return KIND_VALUE;
	}

	const dsc& getDsqlDesc() const
	{
		return dsqlDesc;
	}

	void makeDsqlDesc(DsqlCompilerScratch* dsqlScratch)
	{
		auto settableDesc = dsqlSetableDesc();

		if (settableDesc)
			make(dsqlScratch, settableDesc);
	}

	dsc* dsqlSetableDesc()
	{
		return isSharedNode() ? nullptr : &dsqlDesc;
	}

	// Must be overridden returning true in shared nodes.
	virtual bool isSharedNode()
	{
		return false;
	}

	ValueExprNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		ExprNode::dsqlPass(dsqlScratch);
		return this;
	}

	virtual bool setParameterType(DsqlCompilerScratch* /*dsqlScratch*/,
		std::function<void (dsc*)> /*makeDesc*/, bool /*forceVarChar*/)
	{
		return false;
	}

	virtual void setParameterName(dsql_par* parameter) const = 0;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) = 0;

	ValueExprNode* dsqlFieldRemapper(FieldRemapper& visitor) override
	{
		ExprNode::dsqlFieldRemapper(visitor);
		return this;
	}

	ValueExprNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass1(tdbb, csb);
		return this;
	}

	ValueExprNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass2(tdbb, csb);
		return this;
	}

	// Compute descriptor for value expression.
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) = 0;

	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override = 0;
	virtual dsc* execute(thread_db* tdbb, Request* request) const = 0;

public:
	SCHAR nodScale = 0;

protected:
	dsc dsqlDesc;
};

template <typename T, typename ValueExprNode::Type typeConst>
class DsqlNode : public TypedNode<ValueExprNode, typeConst>
{
public:
	DsqlNode(MemoryPool& pool)
		: TypedNode<ValueExprNode, typeConst>(pool)
	{
	}

public:
	void setParameterName(dsql_par* /*parameter*/) const override
	{
		fb_assert(false);
	}

	void genBlr(DsqlCompilerScratch* /*dsqlScratch*/) override
	{
		fb_assert(false);
	}

	void make(DsqlCompilerScratch* /*dsqlScratch*/, dsc* /*desc*/) override
	{
		fb_assert(false);
	}

	void getDesc(thread_db* /*tdbb*/, CompilerScratch* /*csb*/, dsc* /*desc*/) override
	{
		fb_assert(false);
	}

	ValueExprNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		fb_assert(false);
		return NULL;
	}

	ValueExprNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		fb_assert(false);
		return NULL;
	}

	ValueExprNode* copy(thread_db* /*tdbb*/, NodeCopier& /*copier*/) const override
	{
		fb_assert(false);
		return NULL;
	}

	dsc* execute(thread_db* /*tdbb*/, Request* /*request*/) const override
	{
		fb_assert(false);
		return NULL;
	}
};

class AggNode : public TypedNode<ValueExprNode, ExprNode::TYPE_AGGREGATE>
{
public:
	// Capabilities
	// works in a window frame
	static constexpr unsigned CAP_SUPPORTS_WINDOW_FRAME	= 0x01;
	// respects window frame boundaries
	static constexpr unsigned CAP_RESPECTS_WINDOW_FRAME	= 0x02 | CAP_SUPPORTS_WINDOW_FRAME;
	// wants aggPass/aggExecute calls in a window
	static constexpr unsigned CAP_WANTS_AGG_CALLS		= 0x04;
	// wants winPass call in a window
	static constexpr unsigned CAP_WANTS_WIN_PASS_CALL	= 0x08;

protected:
	struct AggInfo
	{
		AggInfo(const char* aName, UCHAR aBlr, UCHAR aDistinctBlr)
			: name(aName),
			  blr(aBlr),
			  distinctBlr(aDistinctBlr)
		{
		}

		const char* const name;
		const UCHAR blr;
		const UCHAR distinctBlr;
	};

	// Base factory to create instance of subclasses.
	class Factory : public AggInfo
	{
	public:
		explicit Factory(const char* aName)
			: AggInfo(aName, 0, 0)
		{
			next = factories;
			factories = this;
		}

		virtual AggNode* newInstance(MemoryPool& pool) const = 0;

	public:
		const Factory* next;
	};

public:
	// Concrete implementations for the factory.

	template <typename T>
	class RegisterFactory0 : public Factory
	{
	public:
		explicit RegisterFactory0(const char* aName)
			: Factory(aName)
		{
		}

		AggNode* newInstance(MemoryPool& pool) const
		{
			return FB_NEW_POOL(pool) T(pool);
		}
	};

	template <typename T, typename Type>
	class RegisterFactory1 : public Factory
	{
	public:
		explicit RegisterFactory1(const char* aName, Type aType)
			: Factory(aName),
			  type(aType)
		{
		}

		AggNode* newInstance(MemoryPool& pool) const
		{
			return FB_NEW_POOL(pool) T(pool, type);
		}

	public:
		const Type type;
	};

	template <typename T>
	class Register : public AggInfo
	{
	public:
		explicit Register(const char* aName, UCHAR blr, UCHAR blrDistinct)
			: AggInfo(aName, blr, blrDistinct),
			  registerNode({blr, blrDistinct})
		{
		}

		explicit Register(const char* aName, UCHAR blr)
			: AggInfo(aName, blr, blr),
			  registerNode({blr})
		{
		}

	private:
		RegisterNode<T> registerNode;
	};

public:
	explicit AggNode(MemoryPool& pool, const AggInfo& aAggInfo, bool aDistinct, bool aDialect1,
		ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		ValueExprNode::getChildren(holder, dsql);
		holder.add(arg);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override = 0;

	bool dsqlAggregateFinder(AggregateFinder& visitor) override;
	bool dsqlAggregate2Finder(Aggregate2Finder& visitor) override;
	bool dsqlInvalidReferenceFinder(InvalidReferenceFinder& visitor) override;
	bool dsqlSubSelectFinder(SubSelectFinder& visitor) override;
	ValueExprNode* dsqlFieldRemapper(FieldRemapper& visitor) override;

	bool dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const override;
	void setParameterName(dsql_par* parameter) const override;
	void genBlr(DsqlCompilerScratch* dsqlScratch) override;

	AggNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		ValueExprNode::pass1(tdbb, csb);
		return this;
	}

	AggNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;

	bool possiblyUnknown() const override
	{
		return true;
	}

	bool ignoreNulls(const StreamList& /*streams*/) const override
	{
		return false;
	}

	void collectStreams(SortedStreamList& /*streamList*/) const override
	{
		// ASF: Although in v2.5 the visitor happens normally for the node childs, nod_count has
		// been set to 0 in CMP_pass2, so that doesn't happens.
		return;
	}

	bool unmappable(const MapNode* /*mapNode*/, StreamType /*shellStream*/) const override
	{
		return false;
	}

	virtual dsc* winPass(thread_db* /*tdbb*/, Request* /*request*/, SlidingWindow* /*window*/) const
	{
		return NULL;
	}

	virtual void aggInit(thread_db* tdbb, Request* request) const = 0;	// pure, but defined
	virtual void aggFinish(thread_db* tdbb, Request* request) const;
	virtual bool aggPass(thread_db* tdbb, Request* request) const;
	dsc* execute(thread_db* tdbb, Request* request) const override;

	virtual unsigned getCapabilities() const = 0;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const = 0;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const = 0;

	AggNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;

protected:
	virtual void parseArgs(thread_db* /*tdbb*/, CompilerScratch* /*csb*/, unsigned count)
	{
		fb_assert(count == 0);
	}

	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ = 0;

public:
	const AggInfo& aggInfo;
	NestConst<ValueExprNode> arg;
	const AggregateSort* asb;
	NestConst<SortNode> sort;
	bool distinct;
	bool dialect1;
	bool indexed;

private:
	static Factory* factories;
};


// Base class for window functions.
class WinFuncNode : public AggNode
{
public:
	explicit WinFuncNode(MemoryPool& pool, const AggInfo& aAggInfo, ValueExprNode* aArg = NULL);

public:
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const
	{
	}

	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const
	{
		return NULL;
	}
};


class RecordSourceNode : public ExprNode
{
public:
	static constexpr USHORT DFLAG_SINGLETON					= 0x01;
	static constexpr USHORT DFLAG_VALUE						= 0x02;
	static constexpr USHORT DFLAG_RECURSIVE					= 0x04;	// recursive member of recursive CTE
	static constexpr USHORT DFLAG_DERIVED					= 0x08;
	static constexpr USHORT DFLAG_DT_IGNORE_COLUMN_CHECK	= 0x10;
	static constexpr USHORT DFLAG_DT_CTE_USED				= 0x20;
	static constexpr USHORT DFLAG_CURSOR					= 0x40;
	static constexpr USHORT DFLAG_LATERAL					= 0x80;
	static constexpr USHORT DFLAG_PLAN_ITEM					= 0x100;
	static constexpr USHORT DFLAG_BODY_WRAPPER				= 0x200;

	RecordSourceNode(Type aType, MemoryPool& pool)
		: ExprNode(aType, pool),
		  dsqlContext(NULL),
		  stream(INVALID_STREAM),
		  dsqlFlags(0)
	{
	}

	Kind getKind() override
	{
		return KIND_REC_SOURCE;
	}

	virtual StreamType getStream() const
	{
		return stream;
	}

	void setStream(StreamType value)
	{
		stream = value;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override = 0;

	RecordSourceNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		ExprNode::dsqlPass(dsqlScratch);
		return this;
	}

	RecordSourceNode* dsqlFieldRemapper(FieldRemapper& visitor) override
	{
		ExprNode::dsqlFieldRemapper(visitor);
		return this;
	}

	RecordSourceNode* copy(thread_db* tdbb, NodeCopier& copier) const override = 0;
	RecordSourceNode* pass1(thread_db* tdbb, CompilerScratch* csb) override = 0;
	virtual void pass1Source(thread_db* tdbb, CompilerScratch* csb, RseNode* rse,
		BoolExprNode** boolean, RecordSourceNodeStack& stack) = 0;
	RecordSourceNode* pass2(thread_db* tdbb, CompilerScratch* csb) override = 0;
	virtual void pass2Rse(thread_db* tdbb, CompilerScratch* csb) = 0;
	virtual bool containsStream(StreamType checkStream) const = 0;

	void genBlr(DsqlCompilerScratch* /*dsqlScratch*/) override
	{
		fb_assert(false);
	}

	bool possiblyUnknown() const override
	{
		return true;
	}

	bool ignoreNulls(const StreamList& /*streams*/) const override
	{
		return false;
	}

	bool unmappable(const MapNode* /*mapNode*/, StreamType /*shellStream*/) const override
	{
		return false;
	}

	void collectStreams(SortedStreamList& streamList) const override
	{
		if (!streamList.exist(getStream()))
			streamList.add(getStream());
	}

	bool sameAs(const ExprNode* /*other*/, bool /*ignoreStreams*/) const override
	{
		return false;
	}

	// Identify all of the streams for which a dbkey may need to be carried through a sort.
	virtual void computeDbKeyStreams(StreamList& streamList) const = 0;

	// Identify the streams that make up an RseNode.
	virtual void computeRseStreams(StreamList& streamList) const
	{
		streamList.add(getStream());
	}

	virtual RecordSource* compile(thread_db* tdbb, Optimizer* opt, bool innerSubStream) = 0;

public:
	dsql_ctx* dsqlContext;

protected:
	StreamType stream;

public:
	USHORT dsqlFlags;
};


class ListExprNode : public ExprNode
{
public:
	ListExprNode(Type aType, MemoryPool& pool)
		: ExprNode(aType, pool)
	{
	}

	virtual Kind getKind() override
	{
		return KIND_LIST;
	}

	void genBlr(DsqlCompilerScratch* /*dsqlScratch*/) override
	{
		fb_assert(false);
	}
};

// Container for a list of value expressions.
class ValueListNode : public TypedNode<ListExprNode, ExprNode::TYPE_VALUE_LIST>
{
public:
	ValueListNode(MemoryPool& pool, unsigned count)
		: TypedNode<ListExprNode, ExprNode::TYPE_VALUE_LIST>(pool),
		  items(pool, INITIAL_CAPACITY)
	{
		items.resize(count);

		for (unsigned i = 0; i < count; ++i)
			items[i] = NULL;
	}

	ValueListNode(MemoryPool& pool, ValueExprNode* arg1)
		: TypedNode<ListExprNode, ExprNode::TYPE_VALUE_LIST>(pool),
		  items(pool, INITIAL_CAPACITY)
	{
		items.push(arg1);
	}

	ValueListNode(MemoryPool& pool)
		: TypedNode<ListExprNode, ExprNode::TYPE_VALUE_LIST>(pool),
		  items(pool, INITIAL_CAPACITY)
	{
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		ListExprNode::getChildren(holder, dsql);

		for (auto& item : items)
			holder.add(item);
	}

	ValueListNode* add(ValueExprNode* argn)
	{
		items.add(argn);
		return this;
	}

	ValueListNode* addFront(ValueExprNode* argn)
	{
		items.insert(0, argn);
		return this;
	}

	void ensureCapacity(unsigned count)
	{
		items.ensureCapacity(count);
	}

	void clear()
	{
		items.clear();
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;

	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);

	ValueListNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		ValueListNode* node = FB_NEW_POOL(dsqlScratch->getPool()) ValueListNode(dsqlScratch->getPool(),
			items.getCount());

		NestConst<ValueExprNode>* dst = node->items.begin();

		for (NestConst<ValueExprNode>* src = items.begin(); src != items.end(); ++src, ++dst)
			*dst = doDsqlPass(dsqlScratch, *src);

		return node;
	}

	ValueListNode* dsqlFieldRemapper(FieldRemapper& visitor) override
	{
		ExprNode::dsqlFieldRemapper(visitor);
		return this;
	}

	ValueListNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass1(tdbb, csb);
		return this;
	}

	ValueListNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass2(tdbb, csb);
		return this;
	}

	ValueListNode* copy(thread_db* tdbb, NodeCopier& copier) const override
	{
		ValueListNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) ValueListNode(*tdbb->getDefaultPool(),
			items.getCount());

		NestConst<ValueExprNode>* j = node->items.begin();

		for (const NestConst<ValueExprNode>* i = items.begin(); i != items.end(); ++i, ++j)
			*j = copier.copy(tdbb, *i);

		return node;
	}

public:
	NestValueArray items;

private:
	static constexpr unsigned INITIAL_CAPACITY = 4;
};

// Container for a list of record source expressions.
class RecSourceListNode : public TypedNode<ListExprNode, ExprNode::TYPE_REC_SOURCE_LIST>
{
public:
	RecSourceListNode(MemoryPool& pool, unsigned count);
	RecSourceListNode(MemoryPool& pool, RecordSourceNode* arg1);

	RecSourceListNode* add(RecordSourceNode* argn)
	{
		items.add(argn);
		return this;
	}

	void getChildren(NodeRefsHolder& holder, bool dsql) const override
	{
		ListExprNode::getChildren(holder, dsql);

		for (auto& item : items)
			holder.add(item);
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;

	RecSourceListNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;

	RecSourceListNode* dsqlFieldRemapper(FieldRemapper& visitor) override
	{
		ExprNode::dsqlFieldRemapper(visitor);
		return this;
	}

	RecSourceListNode* pass1(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass1(tdbb, csb);
		return this;
	}

	RecSourceListNode* pass2(thread_db* tdbb, CompilerScratch* csb) override
	{
		ExprNode::pass2(tdbb, csb);
		return this;
	}

	RecSourceListNode* copy(thread_db* tdbb, NodeCopier& copier) const override
	{
		fb_assert(false);
		return NULL;
	}

public:
	Firebird::Array<NestConst<RecordSourceNode> > items;
};


class StmtNode : public DmlNode
{
public:
	enum Type
	{
		TYPE_ASSIGNMENT,
		TYPE_BLOCK,
		TYPE_COMPOUND_STMT,
		TYPE_CONTINUE_LEAVE,
		TYPE_CURSOR_STMT,
		TYPE_DECLARE_CURSOR,
		TYPE_DECLARE_LOCAL_TABLE,
		TYPE_DECLARE_SUBFUNC,
		TYPE_DECLARE_SUBPROC,
		TYPE_DECLARE_VARIABLE,
		TYPE_ERASE,
		TYPE_ERROR_HANDLER,
		TYPE_EXCEPTION,
		TYPE_EXEC_BLOCK,
		TYPE_EXEC_PROCEDURE,
		TYPE_EXEC_STATEMENT,
		TYPE_EXIT,
		TYPE_IF,
		TYPE_IN_AUTO_TRANS,
		TYPE_INIT_VARIABLE,
		TYPE_FOR,
		TYPE_FOR_RANGE,
		TYPE_HANDLER,
		TYPE_LABEL,
		TYPE_LINE_COLUMN,
		TYPE_LOCAL_DECLARATIONS,
		TYPE_LOOP,
		TYPE_MERGE,
		TYPE_MERGE_SEND,
		TYPE_MESSAGE,
		TYPE_MODIFY,
		TYPE_OUTER_MAP,
		TYPE_POST_EVENT,
		TYPE_RECEIVE,
		TYPE_RETURN,
		TYPE_SAVEPOINT,
		TYPE_SELECT,
		TYPE_SELECT_MESSAGE,
		TYPE_SESSION_MANAGEMENT_WRAPPER,
		TYPE_SET_GENERATOR,
		TYPE_STALL,
		TYPE_STORE,
		TYPE_SUSPEND,
		TYPE_TRUNCATE_LOCAL_TABLE,
		TYPE_UPDATE_OR_INSERT,

		TYPE_EXT_INIT_PARAMETERS,
		TYPE_EXT_TRIGGER
	};

	enum WhichTrigger : UCHAR
	{
		ALL_TRIGS = 0,
		PRE_TRIG = 1,
		POST_TRIG = 2
	};

	// Marks used by EraseNode, ModifyNode, StoreNode and ForNode
	static constexpr unsigned MARK_POSITIONED		= 0x01;	// Erase|Modify node is positioned at explicit cursor
	static constexpr unsigned MARK_MERGE			= 0x02;	// node is part of MERGE statement
	static constexpr unsigned MARK_FOR_UPDATE		= 0x04;	// implicit cursor used in UPDATE\DELETE\MERGE statement
	static constexpr unsigned MARK_AVOID_COUNTERS	= 0x08;	// do not touch record counters
	static constexpr unsigned MARK_BULK_INSERT		= 0x10; // StoreNode is used for bulk operation

	struct ExeState
	{
		ExeState(thread_db* tdbb, Request* request, jrd_tra* transaction)
			: savedTdbb(tdbb),
			  oldPool(tdbb->getDefaultPool()),
			  oldRequest(tdbb->getRequest()),
			  oldTransaction(tdbb->getTransaction())
		{
			savedTdbb->setTransaction(transaction);
			savedTdbb->setRequest(request);
		}

		~ExeState()
		{
			savedTdbb->setTransaction(oldTransaction);
			savedTdbb->setRequest(oldRequest);
		}

		thread_db* savedTdbb;
		MemoryPool* oldPool;		// Save the old pool to restore on exit.
		Request* oldRequest;		// Save the old request to restore on exit.
		jrd_tra* oldTransaction;	// Save the old transaction to restore on exit.
		const StmtNode* topNode = nullptr;
		const StmtNode* prevNode = nullptr;
		WhichTrigger whichEraseTrig = ALL_TRIGS;
		WhichTrigger whichStoTrig = ALL_TRIGS;
		WhichTrigger whichModTrig = ALL_TRIGS;
		bool errorPending = false;		// Is there an error pending to be handled?
		bool catchDisabled = false;		// Catch errors so we can unwind cleanly.
		bool exit = false;				// Exit the looper when true.
		bool forceProfileNextEvaluate = false;
	};

public:
	explicit StmtNode(Type aType, MemoryPool& pool)
		: DmlNode(pool),
		  parentStmt(NULL),
		  impureOffset(0),
		  hasLineColumn(false)
	{
	}

	virtual Type getType() const = 0;

	// Allocate and assign impure space for various nodes.
	template <typename T> static void doPass2(thread_db* tdbb, CompilerScratch* csb, T** node,
		StmtNode* parentStmt)
	{
		if (!*node)
			return;

		if (parentStmt)
			(*node)->parentStmt = parentStmt;

		*node = (*node)->pass2(tdbb, csb);
	}

	Kind getKind() override
	{
		return KIND_STATEMENT;
	}

	Firebird::string internalPrint(NodePrinter& printer) const override;

	StmtNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		DmlNode::dsqlPass(dsqlScratch);
		return this;
	}

	StmtNode* pass1(thread_db* tdbb, CompilerScratch* csb) override = 0;
	StmtNode* pass2(thread_db* tdbb, CompilerScratch* csb) override = 0;

	StmtNode* copy(thread_db* /*tdbb*/, NodeCopier& /*copier*/) const override
	{
		fb_assert(false);
		Firebird::status_exception::raise(
			Firebird::Arg::Gds(isc_cannot_copy_stmt) <<
			Firebird::Arg::Num(int(getType())));

		return NULL;
	}

	virtual bool isProfileAware() const
	{
		return true;
	}

	virtual const StmtNode* execute(thread_db* tdbb, Request* request, ExeState* exeState) const = 0;

public:
	NestConst<StmtNode> parentStmt;
	ULONG impureOffset;	// Inpure offset from request block.
	bool hasLineColumn;
};


// Used to represent nodes that don't have a specific BLR verb, i.e.,
// do not use RegisterNode.
class DsqlOnlyStmtNode : public StmtNode
{
public:
	explicit DsqlOnlyStmtNode(Type aType, MemoryPool& pool)
		: StmtNode(aType, pool)
	{
	}

public:
	DsqlOnlyStmtNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		fb_assert(false);
		return this;
	}

	DsqlOnlyStmtNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/) override
	{
		fb_assert(false);
		return this;
	}

	DsqlOnlyStmtNode* copy(thread_db* /*tdbb*/, NodeCopier& /*copier*/) const override
	{
		fb_assert(false);
		return NULL;
	}

	const StmtNode* execute(thread_db* /*tdbb*/, Request* /*request*/, ExeState* /*exeState*/) const override
	{
		fb_assert(false);
		return NULL;
	}
};


struct ScaledNumber
{
	FB_UINT64 number;
	SCHAR scale;
	bool hex;
};


class RowsClause : public Printable
{
public:
	explicit RowsClause(MemoryPool& pool)
		: length(NULL),
		  skip(NULL)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;

public:
	NestConst<ValueExprNode> length;
	NestConst<ValueExprNode> skip;
};


class GeneratorItem : public Printable
{
public:
	GeneratorItem(Firebird::MemoryPool& pool, const QualifiedName& name)
		: id(0), name(pool, name), secName(pool)
	{}

	GeneratorItem& operator=(const GeneratorItem& other)
	{
		id = other.id;
		name = other.name;
		secName = other.secName;
		return *this;
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;

public:
	SLONG id;
	QualifiedName name;
	QualifiedName secName;
};

typedef Firebird::Array<StreamType> StreamMap;

// Copy sub expressions (including subqueries).
class SubExprNodeCopier : private StreamMap, public NodeCopier
{
public:
	SubExprNodeCopier(Firebird::MemoryPool& pool, CompilerScratch* aCsb)
		: NodeCopier(pool, aCsb, getBuffer(STREAM_MAP_LENGTH))
	{
		// Initialize the map so all streams initially resolve to the original number.
		// As soon as copy creates new streams, the map is being overwritten.
		for (unsigned i = 0; i < STREAM_MAP_LENGTH; ++i)
			remap[i] = i;
	}
};


} // namespace

#endif // DSQL_NODES_H
