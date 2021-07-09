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

#ifndef DSQL_DDL_NODES_H
#define DSQL_DDL_NODES_H

#include "../jrd/blr.h"
#include "../jrd/dyn.h"
#include "../common/msg_encode.h"
#include "../dsql/make_proto.h"
#include "../dsql/BlrDebugWriter.h"
#include "../dsql/Nodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/NodePrinter.h"
#include "../common/classes/array.h"
#include "../common/classes/ByteChunk.h"
#include "../common/classes/Nullable.h"
#include "../jrd/vio_proto.h"
#include "../dsql/errd_proto.h"

namespace Jrd {

class CompoundStmtNode;
class RelationSourceNode;
class ValueListNode;
class SecDbContext;

class BoolSourceClause : public Printable
{
public:
	explicit BoolSourceClause(MemoryPool& p)
		: value(NULL),
		  source(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, value);
		NODE_PRINT(printer, source);

		return "BoolSourceClause";
	}

public:
	NestConst<BoolExprNode> value;
	Firebird::string source;
};


class ValueSourceClause : public Printable
{
public:
	explicit ValueSourceClause(MemoryPool& p)
		: value(NULL),
		  source(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, value);
		NODE_PRINT(printer, source);

		return "ValueSourceClause";
	}

public:
	NestConst<ValueExprNode> value;
	Firebird::string source;
};


class DbFileClause : public Printable
{
public:
	DbFileClause(MemoryPool& p, const DbFileClause& o)
		: name(p, o.name),
		  start(o.start),
		  length(o.length)
	{
	}

	explicit DbFileClause(MemoryPool& p, const Firebird::string& aName)
		: name(p, aName),
		  start(0),
		  length(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, name);
		NODE_PRINT(printer, start);
		NODE_PRINT(printer, length);

		return "DbFileClause";
	}

public:
	Firebird::string name;	// File name
	SLONG start;			// Starting page
	SLONG length;			// File length in pages
};


class ExternalClause : public Printable
{
public:
	ExternalClause(MemoryPool& p, const ExternalClause& o)
		: name(p, o.name),
		  engine(p, o.engine),
		  udfModule(p, o.udfModule)
	{
	}

	explicit ExternalClause(MemoryPool& p)
		: name(p),
		  engine(p),
		  udfModule(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, name);
		NODE_PRINT(printer, engine);
		NODE_PRINT(printer, udfModule);

		return "ExternalClause";
	}

public:
	Firebird::string name;
	Firebird::MetaName engine;
	Firebird::string udfModule;
};


class ParameterClause : public Printable
{
public:
	ParameterClause(MemoryPool& pool, dsql_fld* field, const Firebird::MetaName& aCollate,
		ValueSourceClause* aDefaultClause = NULL, ValueExprNode* aParameterExpr = NULL);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;

public:
	Firebird::MetaName name;
	NestConst<dsql_fld> type;
	NestConst<ValueSourceClause> defaultClause;
	NestConst<ValueExprNode> parameterExpr;
	Nullable<int> udfMechanism;
};


struct CollectedParameter
{
	CollectedParameter()
	{
		comment.clear();
		defaultSource.clear();
		defaultValue.clear();
	}

	bid comment;
	bid defaultSource;
	bid defaultValue;
};

typedef Firebird::GenericMap<
			Firebird::Pair<Firebird::Left<Firebird::MetaName, CollectedParameter> > >
	CollectedParameterMap;


class ExecInSecurityDb
{
public:
	virtual ~ExecInSecurityDb() { }

	void executeInSecurityDb(jrd_tra* tra);

protected:
	virtual void runInSecurityDb(SecDbContext* secDbContext) = 0;
};


template <typename CreateNode, typename DropNode, ISC_STATUS ERROR_CODE>
class RecreateNode : public DdlNode
{
public:
	RecreateNode(MemoryPool& p, CreateNode* aCreateNode)
		: DdlNode(p),
		  createNode(aCreateNode),
		  dropNode(p, createNode->name)
	{
		dropNode.silent = true;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		Node::internalPrint(printer);

		NODE_PRINT(printer, createNode);
		NODE_PRINT(printer, dropNode);

		return "RecreateNode";
	}

	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction)
	{
		return dropNode.checkPermission(tdbb, transaction) && createNode->checkPermission(tdbb, transaction);
	}

	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		// run all statements under savepoint control
		AutoSavePoint savePoint(tdbb, transaction);

		dropNode.execute(tdbb, dsqlScratch, transaction);
		createNode->execute(tdbb, dsqlScratch, transaction);

		savePoint.release();	// everything is ok
	}

	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		createNode->dsqlPass(dsqlScratch);
		dropNode.dsqlPass(dsqlScratch);
		return DdlNode::dsqlPass(dsqlScratch);
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(ERROR_CODE) << createNode->name;
	}

protected:
	NestConst<CreateNode> createNode;
	DropNode dropNode;
};


class AlterCharSetNode : public DdlNode
{
public:
	AlterCharSetNode(MemoryPool& pool, const Firebird::MetaName& aCharSet,
				const Firebird::MetaName& aDefaultCollation)
		: DdlNode(pool),
		  charSet(pool, aCharSet),
		  defaultCollation(pool, aDefaultCollation)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_charset_failed) << charSet;
	}

private:
	Firebird::MetaName charSet;
	Firebird::MetaName defaultCollation;
};


class CommentOnNode : public DdlNode
{
public:
	CommentOnNode(MemoryPool& pool, int aObjType,
				const Firebird::QualifiedName& aObjName, const Firebird::MetaName& aSubName,
				const Firebird::string aText)
		: DdlNode(pool),
		  objType(aObjType),
		  objName(pool, aObjName),
		  subName(pool, aSubName),
		  text(pool, aText),
		  str(pool)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		str = objName.toString();

		if (subName.hasData())
			str.append(".").append(subName.c_str());

		statusVector << Firebird::Arg::Gds(isc_dsql_comment_on_failed) << str;
	}

private:
	int objType;
	Firebird::QualifiedName objName;
	Firebird::MetaName subName;
	Firebird::string text, str;
};


class CreateAlterFunctionNode : public DdlNode
{
public:
	CreateAlterFunctionNode(MemoryPool& pool, const Firebird::MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  create(true),
		  alter(false),
		  external(NULL),
		  deterministic(false),
		  parameters(pool),
		  returnType(NULL),
		  localDeclList(NULL),
		  source(pool),
		  body(NULL),
		  compiled(false),
		  invalid(false),
		  package(pool),
		  packageOwner(pool),
		  privateScope(false),
		  preserveDefaults(false),
		  udfReturnPos(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_func_failed, isc_dsql_alter_func_failed,
					isc_dsql_create_alter_func_failed)) <<
				name;
	}

private:
	bool isUdf() const
	{
		return external && external->udfModule.hasData();
	}

	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		bool secondPass, bool runTriggers);

	void storeArgument(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		unsigned pos, bool returnArg, ParameterClause* parameter,
		const CollectedParameter* collectedParameter);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);
	void collectParameters(thread_db* tdbb, jrd_tra* transaction, CollectedParameterMap& items);

public:
	Firebird::MetaName name;
	bool create;
	bool alter;
	NestConst<ExternalClause> external;
	bool deterministic;
	Firebird::Array<NestConst<ParameterClause> > parameters;
	NestConst<ParameterClause> returnType;
	NestConst<CompoundStmtNode> localDeclList;
	Firebird::string source;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
	Firebird::MetaName package;
	Firebird::string packageOwner;
	bool privateScope;
	bool preserveDefaults;
	SLONG udfReturnPos;
};


class AlterExternalFunctionNode : public DdlNode
{
public:
	AlterExternalFunctionNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  clauses(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_func_failed) << name;
	}

public:
	Firebird::MetaName name;
	ExternalClause clauses;
};


class DropFunctionNode : public DdlNode
{
public:
	DropFunctionNode(MemoryPool& pool, const Firebird::MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false),
		  package(pool)
	{
	}

public:
	static void dropArguments(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& functionName, const Firebird::MetaName& packageName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_func_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool silent;
	Firebird::MetaName package;
};


typedef RecreateNode<CreateAlterFunctionNode, DropFunctionNode, isc_dsql_recreate_func_failed>
	RecreateFunctionNode;


class CreateAlterProcedureNode : public DdlNode
{
public:
	CreateAlterProcedureNode(MemoryPool& pool, const Firebird::MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  create(true),
		  alter(false),
		  external(NULL),
		  parameters(pool),
		  returns(pool),
		  source(pool),
		  localDeclList(NULL),
		  body(NULL),
		  compiled(false),
		  invalid(false),
		  package(pool),
		  packageOwner(pool),
		  privateScope(false),
		  preserveDefaults(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_proc_failed, isc_dsql_alter_proc_failed,
					isc_dsql_create_alter_proc_failed)) <<
				name;
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		bool secondPass, bool runTriggers);
	void storeParameter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		USHORT parameterType, unsigned pos, ParameterClause* parameter,
		const CollectedParameter* collectedParameter);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);
	void collectParameters(thread_db* tdbb, jrd_tra* transaction, CollectedParameterMap& items);

public:
	Firebird::MetaName name;
	bool create;
	bool alter;
	NestConst<ExternalClause> external;
	Firebird::Array<NestConst<ParameterClause> > parameters;
	Firebird::Array<NestConst<ParameterClause> > returns;
	Firebird::string source;
	NestConst<CompoundStmtNode> localDeclList;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
	Firebird::MetaName package;
	Firebird::string packageOwner;
	bool privateScope;
	bool preserveDefaults;
};


class DropProcedureNode : public DdlNode
{
public:
	DropProcedureNode(MemoryPool& pool, const Firebird::MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false),
		  package(pool)
	{
	}

public:
	static void dropParameters(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& procedureName, const Firebird::MetaName& packageName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_proc_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool silent;
	Firebird::MetaName package;
};


typedef RecreateNode<CreateAlterProcedureNode, DropProcedureNode, isc_dsql_recreate_proc_failed>
	RecreateProcedureNode;


class TriggerDefinition
{
public:
	explicit TriggerDefinition(MemoryPool& p)
		: name(p),
		  relationName(p),
		  external(NULL),
		  source(p),
		  systemFlag(fb_sysflag_user),
		  fkTrigger(false)
	{
	}

	void store(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool modify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual ~TriggerDefinition()
	{
	}

protected:
	virtual void preModify(thread_db* /*tdbb*/, DsqlCompilerScratch* /*dsqlScratch*/,
		jrd_tra* /*transaction*/)
	{
	}

	virtual void postModify(thread_db* /*tdbb*/, DsqlCompilerScratch* /*dsqlScratch*/,
		jrd_tra* /*transaction*/)
	{
	}

public:
	Firebird::MetaName name;
	Firebird::MetaName relationName;
	Nullable<FB_UINT64> type;
	Nullable<bool> active;
	Nullable<int> position;
	NestConst<ExternalClause> external;
	Firebird::string source;
	Firebird::ByteChunk blrData;
	Firebird::ByteChunk debugData;
	USHORT systemFlag;
	bool fkTrigger;
};


class CreateAlterTriggerNode : public DdlNode, public TriggerDefinition
{
public:
	CreateAlterTriggerNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  TriggerDefinition(p),
		  create(true),
		  alter(false),
		  localDeclList(NULL),
		  body(NULL),
		  compiled(false),
		  invalid(false)
	{
		name = aName;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_trigger_failed, isc_dsql_alter_trigger_failed,
					isc_dsql_create_alter_trigger_failed)) <<
				name;
	}

	virtual void preModify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		if (alter)
		{
			executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE,
				DDL_TRIGGER_ALTER_TRIGGER, name, NULL);
		}
	}

	virtual void postModify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		if (alter)
		{
			executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER,
				DDL_TRIGGER_ALTER_TRIGGER, name, NULL);
		}
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);

	static inline bool hasOldContext(const unsigned value)
	{
		const unsigned val1 = ((value + 1) >> 1) & 3;
		const unsigned val2 = ((value + 1) >> 3) & 3;
		const unsigned val3 = ((value + 1) >> 5) & 3;
		return (val1 && val1 != 1) || (val2 && val2 != 1) || (val3 && val3 != 1);
	}

	static inline bool hasNewContext(const unsigned value)
	{
		const unsigned val1 = ((value + 1) >> 1) & 3;
		const unsigned val2 = ((value + 1) >> 3) & 3;
		const unsigned val3 = ((value + 1) >> 5) & 3;
		return (val1 && val1 != 3) || (val2 && val2 != 3) || (val3 && val3 != 3);
	}

public:
	bool create;
	bool alter;
	NestConst<CompoundStmtNode> localDeclList;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
};


class DropTriggerNode : public DdlNode
{
public:
	DropTriggerNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  silent(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_trigger_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterTriggerNode, DropTriggerNode, isc_dsql_recreate_trigger_failed>
	RecreateTriggerNode;


class CreateCollationNode : public DdlNode
{
public:
	CreateCollationNode(MemoryPool& p, const Firebird::MetaName& aName,
				const Firebird::MetaName& aForCharSet)
		: DdlNode(p),
		  name(p, aName),
		  forCharSet(p, aForCharSet),
		  fromName(p),
		  fromExternal(p),
		  specificAttributes(p),
		  attributesOn(0),
		  attributesOff(0),
		  forCharSetId(0),
		  fromCollationId(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	void setAttribute(USHORT attribute)
	{
		if ((attributesOn | attributesOff) & attribute)
		{
			// msg: 222: "Invalid collation attributes"
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) << Arg::PrivateDyn(222));
		}

		attributesOn |= attribute;
	}

	void unsetAttribute(USHORT attribute)
	{
		if ((attributesOn | attributesOff) & attribute)
		{
			// msg: 222: "Invalid collation attributes"
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) << Arg::PrivateDyn(222));
		}

		attributesOff |= attribute;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_collation_failed) << name;
	}

public:
	Firebird::MetaName name;
	Firebird::MetaName forCharSet;
	Firebird::MetaName fromName;
	Firebird::string fromExternal;
	Firebird::UCharBuffer specificAttributes;

private:
	USHORT attributesOn;
	USHORT attributesOff;
	USHORT forCharSetId;
	USHORT fromCollationId;
};


class DropCollationNode : public DdlNode
{
public:
	DropCollationNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_collation_failed) << name;
	}

public:
	Firebird::MetaName name;
};


class CreateDomainNode : public DdlNode
{
public:
	CreateDomainNode(MemoryPool& p, ParameterClause* aNameType)
		: DdlNode(p),
		  nameType(aNameType),
		  notNull(false),
		  check(NULL)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_domain_failed) << nameType->name;
	}

public:
	NestConst<ParameterClause> nameType;
	bool notNull;
	NestConst<BoolSourceClause> check;
};


class AlterDomainNode : public DdlNode
{
public:
	AlterDomainNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  dropConstraint(false),
		  dropDefault(false),
		  setConstraint(NULL),
		  setDefault(NULL),
		  renameTo(p)
	{
	}

public:
	static void checkUpdate(const dyn_fld& origFld, const dyn_fld& newFld);
	static ULONG checkUpdateNumericType(const dyn_fld& origFld, const dyn_fld& newFld);
	static void getDomainType(thread_db* tdbb, jrd_tra* transaction, dyn_fld& dynFld);
	static void modifyLocalFieldIndex(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& relationName, const Firebird::MetaName& fieldName,
		const Firebird::MetaName& newFieldName);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_domain_failed) << name;
	}

private:
	void rename(thread_db* tdbb, jrd_tra* transaction, SSHORT dimensions);

public:
	Firebird::MetaName name;
	bool dropConstraint;
	bool dropDefault;
	NestConst<BoolSourceClause> setConstraint;
	NestConst<ValueSourceClause> setDefault;
	Firebird::MetaName renameTo;
	Firebird::AutoPtr<dsql_fld> type;
	Nullable<bool> notNullFlag;	// true = NOT NULL / false = NULL
};


class DropDomainNode : public DdlNode
{
public:
	DropDomainNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

	static bool deleteDimensionRecords(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_domain_failed) << name;
	}

private:
	void check(thread_db* tdbb, jrd_tra* transaction);

public:
	Firebird::MetaName name;
};


class CreateAlterExceptionNode : public DdlNode
{
public:
	CreateAlterExceptionNode(MemoryPool& p, const Firebird::MetaName& aName,
				const Firebird::string& aMessage)
		: DdlNode(p),
		  name(p, aName),
		  message(p, aMessage),
		  create(true),
		  alter(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_except_failed, isc_dsql_alter_except_failed,
					isc_dsql_create_alter_except_failed)) <<
				name;
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	Firebird::MetaName name;
	Firebird::string message;
	bool create;
	bool alter;
};


class DropExceptionNode : public DdlNode
{
public:
	DropExceptionNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  silent(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_except_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterExceptionNode, DropExceptionNode, isc_dsql_recreate_except_failed>
	RecreateExceptionNode;


class CreateAlterSequenceNode : public DdlNode
{
public:
	CreateAlterSequenceNode(MemoryPool& pool, const Firebird::MetaName& aName,
				BaseNullable<SINT64> aValue, BaseNullable<SLONG> aStep)
		: DdlNode(pool),
		  create(true),
		  alter(false),
		  legacy(false),
		  restartSpecified(false),
		  name(pool, aName),
		  value(aValue),
		  step(aStep)
	{
		// Unfortunately, line/column carry no useful information here.
		// Hence, the check was created directly in parse.y.
		/*
		if (!aValue.specified && !aStep.specified)
		{
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  // Unexpected end of command
					  Arg::Gds(isc_command_end_err2) << Arg::Num(this->line) <<
													Arg::Num(this->column));
		}
		*/
	}

	static SSHORT store(thread_db* tdbb, jrd_tra* transaction, const Firebird::MetaName& name,
		fb_sysflag sysFlag, SINT64 value, SLONG step);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		dsqlScratch->getStatement()->setType(
			legacy ? DsqlCompiledStatement::TYPE_SET_GENERATOR : DsqlCompiledStatement::TYPE_DDL);
		return this;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector);

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	bool create;
	bool alter;
	bool legacy;
	bool restartSpecified;
	const Firebird::MetaName name;
	const BaseNullable<SINT64> value;
	const BaseNullable<SLONG> step;
};


class DropSequenceNode : public DdlNode
{
public:
	DropSequenceNode(MemoryPool& pool, const Firebird::MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false)
	{
	}

	static void deleteIdentity(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_sequence_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterSequenceNode, DropSequenceNode, isc_dsql_recreate_sequence_failed>
	RecreateSequenceNode;


class RelationNode : public DdlNode
{
public:
	class FieldDefinition
	{
	public:
		explicit FieldDefinition(MemoryPool& p)
			: name(p),
			  relationName(p),
			  fieldSource(p),
			  identitySequence(p),
			  defaultSource(p),
			  baseField(p)
		{
		}

		void modify(thread_db* tdbb, jrd_tra* transaction);
		void store(thread_db* tdbb, jrd_tra* transaction);

	public:
		Firebird::MetaName name;
		Firebird::MetaName relationName;
		Firebird::MetaName fieldSource;
		Firebird::MetaName identitySequence;
		Nullable<USHORT> collationId;
		Nullable<bool> notNullFlag;	// true = NOT NULL / false = NULL
		Nullable<USHORT> position;
		Firebird::string defaultSource;
		Firebird::ByteChunk defaultValue;
		Nullable<USHORT> viewContext;
		Firebird::MetaName baseField;
	};

	struct IndexConstraintClause
	{
		explicit IndexConstraintClause(MemoryPool& p)
			: name(p),
			  descending(false)
		{
		}

		Firebird::MetaName name;
		bool descending;
	};

	struct Constraint
	{
		enum Type { TYPE_CHECK, TYPE_NOT_NULL, TYPE_PK, TYPE_UNIQUE, TYPE_FK };

		// Specialized BlrWriter for constraints.
		class BlrWriter : public Jrd::BlrDebugWriter
		{
		public:
			explicit BlrWriter(MemoryPool& p)
				: Jrd::BlrDebugWriter(p),
				  dsqlScratch(NULL)
			{
			}

			void init(DsqlCompilerScratch* aDsqlScratch)
			{
				dsqlScratch = aDsqlScratch;
				dsqlScratch->getBlrData().clear();
				dsqlScratch->getDebugData().clear();
				appendUChar(isVersion4() ? blr_version4 : blr_version5);
			}

			virtual bool isVersion4()
			{
				return dsqlScratch->isVersion4();
			}

		private:
			DsqlCompilerScratch* dsqlScratch;
		};

		explicit Constraint(MemoryPool& p)
			: type(TYPE_CHECK),	// Just something to initialize. Do not assume it.
			  columns(p),
			  index(NULL),
			  refRelation(p),
			  refColumns(p),
			  refUpdateAction(RI_RESTRICT),
			  refDeleteAction(RI_RESTRICT),
			  triggers(p),
			  blrWritersHolder(p)
		{
		}

		Constraint::Type type;
		Firebird::ObjectsArray<Firebird::MetaName> columns;
		NestConst<IndexConstraintClause> index;
		Firebird::MetaName refRelation;
		Firebird::ObjectsArray<Firebird::MetaName> refColumns;
		const char* refUpdateAction;
		const char* refDeleteAction;
		Firebird::ObjectsArray<TriggerDefinition> triggers;
		Firebird::ObjectsArray<BlrWriter> blrWritersHolder;
	};

	struct CreateDropConstraint
	{
		explicit CreateDropConstraint(MemoryPool& p)
			: name(p)
		{
		}

		Firebird::MetaName name;
		Firebird::AutoPtr<Constraint> create;
	};

	struct Clause
	{
		enum Type
		{
			TYPE_ADD_CONSTRAINT,
			TYPE_ADD_COLUMN,
			TYPE_ALTER_COL_NAME,
			TYPE_ALTER_COL_NULL,
			TYPE_ALTER_COL_POS,
			TYPE_ALTER_COL_TYPE,
			TYPE_DROP_COLUMN,
			TYPE_DROP_CONSTRAINT
		};

		explicit Clause(MemoryPool& p, Type aType)
			: type(aType)
		{
		}

		const Type type;
	};

	struct RefActionClause
	{
		static const unsigned ACTION_CASCADE		= 1;
		static const unsigned ACTION_SET_DEFAULT	= 2;
		static const unsigned ACTION_SET_NULL		= 3;
		static const unsigned ACTION_NONE			= 4;

		RefActionClause(MemoryPool& p, unsigned aUpdateAction, unsigned aDeleteAction)
			: updateAction(aUpdateAction),
			  deleteAction(aDeleteAction)
		{
		}

		unsigned updateAction;
		unsigned deleteAction;
	};

	struct AddConstraintClause : public Clause
	{
		enum ConstraintType
		{
			CTYPE_NOT_NULL,
			CTYPE_PK,
			CTYPE_FK,
			CTYPE_UNIQUE,
			CTYPE_CHECK
		};

		explicit AddConstraintClause(MemoryPool& p)
			: Clause(p, TYPE_ADD_CONSTRAINT),
			  name(p),
			  constraintType(CTYPE_NOT_NULL),
			  columns(p),
			  index(NULL),
			  refRelation(p),
			  refColumns(p),
			  refAction(NULL),
			  check(NULL)
		{
		}

		Firebird::MetaName name;
		ConstraintType constraintType;
		Firebird::ObjectsArray<Firebird::MetaName> columns;
		NestConst<IndexConstraintClause> index;
		Firebird::MetaName refRelation;
		Firebird::ObjectsArray<Firebird::MetaName> refColumns;
		NestConst<RefActionClause> refAction;
		NestConst<BoolSourceClause> check;
	};

	struct AddColumnClause : public Clause
	{
		explicit AddColumnClause(MemoryPool& p)
			: Clause(p, TYPE_ADD_COLUMN),
			  field(NULL),
			  defaultValue(NULL),
			  constraints(p),
			  collate(p),
			  computed(NULL),
			  identity(false),
			  identityStart(0)
		{
		}

		dsql_fld* field;
		NestConst<ValueSourceClause> defaultValue;
		Firebird::ObjectsArray<AddConstraintClause> constraints;
		Firebird::MetaName collate;
		NestConst<ValueSourceClause> computed;
		bool identity;
		SINT64 identityStart;
	};

	struct AlterColNameClause : public Clause
	{
		explicit AlterColNameClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_NAME),
			  fromName(p),
			  toName(p)
		{
		}

		Firebird::MetaName fromName;
		Firebird::MetaName toName;
	};

	struct AlterColNullClause : public Clause
	{
		explicit AlterColNullClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_NULL),
			  name(p),
			  notNullFlag(false)
		{
		}

		Firebird::MetaName name;
		bool notNullFlag;
	};

	struct AlterColPosClause : public Clause
	{
		explicit AlterColPosClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_POS),
			  name(p),
			  newPos(0)
		{
		}

		Firebird::MetaName name;
		SSHORT newPos;
	};

	struct AlterColTypeClause : public Clause
	{
		explicit AlterColTypeClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_TYPE),
			  field(NULL),
			  defaultValue(NULL),
			  dropDefault(false),
			  identityRestart(false),
			  computed(NULL)
		{
		}

		dsql_fld* field;
		NestConst<ValueSourceClause> defaultValue;
		bool dropDefault;
		bool identityRestart;
		Nullable<SINT64> identityRestartValue;
		NestConst<ValueSourceClause> computed;
	};

	struct DropColumnClause : public Clause
	{
		explicit DropColumnClause(MemoryPool& p)
			: Clause(p, TYPE_DROP_COLUMN),
			  name(p),
			  cascade(false)
		{
		}

		Firebird::MetaName name;
		bool cascade;
	};

	struct DropConstraintClause : public Clause
	{
		explicit DropConstraintClause(MemoryPool& p)
			: Clause(p, TYPE_DROP_CONSTRAINT),
			  name(p)
		{
		}

		Firebird::MetaName name;
	};

	RelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode);

	static void deleteLocalField(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& relationName, const Firebird::MetaName& fieldName);

protected:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		DdlNode::internalPrint(printer);

		//// FIXME-PRINT: NODE_PRINT(printer, dsqlNode);
		NODE_PRINT(printer, name);
		//// FIXME-PRINT: NODE_PRINT(printer, clauses);

		return "RelationNode";
	}

	void defineField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AddColumnClause* clause, SSHORT position,
		const Firebird::ObjectsArray<Firebird::MetaName>* pkcols);
	bool defineDefault(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
		ValueSourceClause* clause, Firebird::string& source, BlrDebugWriter::BlrData& value);
	void makeConstraint(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AddConstraintClause* clause, Firebird::ObjectsArray<CreateDropConstraint>& constraints,
		bool* notNull = NULL);
	void defineConstraint(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		Firebird::MetaName& constraintName, Constraint& constraint);
	void defineCheckConstraint(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		BoolSourceClause* clause);
	void defineCheckConstraintTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		BoolSourceClause* clause, FB_UINT64 triggerType);
	void defineSetDefaultTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		bool onUpdate);
	void defineSetNullTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		bool onUpdate);
	void defineDeleteCascadeTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint);
	void defineUpdateCascadeTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint);
	void generateUnnamedTriggerBeginning(Constraint& constraint, bool onUpdate,
		BlrDebugWriter& blrWriter);
	void stuffDefaultBlr(const Firebird::ByteChunk& defaultBlr, BlrDebugWriter& blrWriter);
	void stuffMatchingBlr(Constraint& constraint, BlrDebugWriter& blrWriter);
	void stuffTriggerFiringCondition(const Constraint& constraint, BlrDebugWriter& blrWriter);

public:
	NestConst<RelationSourceNode> dsqlNode;
	Firebird::MetaName name;
	Firebird::Array<NestConst<Clause> > clauses;
};


class CreateRelationNode : public RelationNode
{
public:
	CreateRelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode,
				const Firebird::string* aExternalFile = NULL)
		: RelationNode(p, aDsqlNode),
		  externalFile(aExternalFile),
		  relationType(rel_persistent)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_table_failed) << name;
	}

private:
	const Firebird::ObjectsArray<Firebird::MetaName>* findPkColumns();

public:
	const Firebird::string* externalFile;
	rel_t relationType;
};


class AlterRelationNode : public RelationNode
{
public:
	AlterRelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode)
		: RelationNode(p, aDsqlNode)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_table_failed) << name;
	}

private:
	void modifyField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AlterColTypeClause* clause);
};


class DropRelationNode : public DdlNode
{
public:
	DropRelationNode(MemoryPool& p, const Firebird::MetaName& aName, bool aView = false)
		: DdlNode(p),
		  name(p, aName),
		  view(aView),
		  silent(false)
	{
	}

	static void deleteGlobalField(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& globalName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(view ? isc_dsql_drop_view_failed :
			isc_dsql_drop_table_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool view;
	bool silent;
};


typedef RecreateNode<CreateRelationNode, DropRelationNode, isc_dsql_recreate_table_failed>
	RecreateTableNode;


class CreateAlterViewNode : public RelationNode
{
public:
	CreateAlterViewNode(MemoryPool& p, RelationSourceNode* aDsqlNode, ValueListNode* aViewFields,
				SelectExprNode* aSelectExpr)
		: RelationNode(p, aDsqlNode),
		  create(true),
		  alter(false),
		  viewFields(aViewFields),
		  selectExpr(aSelectExpr),
		  source(p),
		  withCheckOption(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_view_failed, isc_dsql_alter_view_failed,
					isc_dsql_create_alter_view_failed)) <<
				name;
	}

private:
	void createCheckTrigger(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, ValueListNode* items,
		TriggerType triggerType);

public:
	bool create;
	bool alter;
	NestConst<ValueListNode> viewFields;
	NestConst<SelectExprNode> selectExpr;
	Firebird::string source;
	bool withCheckOption;
};


class RecreateViewNode :
	public RecreateNode<CreateAlterViewNode, DropRelationNode, isc_dsql_recreate_view_failed>
{
public:
	RecreateViewNode(MemoryPool& p, CreateAlterViewNode* aCreateNode)
		: RecreateNode<CreateAlterViewNode, DropRelationNode, isc_dsql_recreate_view_failed>(
				p, aCreateNode)
	{
		dropNode.view = true;
	}
};


class CreateIndexNode : public DdlNode
{
public:
	struct Definition
	{
		Definition()
			: type(0)
		{
			expressionBlr.clear();
			expressionSource.clear();
		}

		Firebird::MetaName relation;
		Firebird::ObjectsArray<Firebird::MetaName> columns;
		Nullable<bool> unique;
		Nullable<bool> descending;
		Nullable<bool> inactive;
		SSHORT type;
		bid expressionBlr;
		bid expressionSource;
		Firebird::MetaName refRelation;
		Firebird::ObjectsArray<Firebird::MetaName> refColumns;
	};

public:
	CreateIndexNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  unique(false),
		  descending(false),
		  relation(NULL),
		  columns(NULL),
		  computed(NULL)
	{
	}

public:
	static void store(thread_db* tdbb, jrd_tra* transaction, Firebird::MetaName& name,
		const Definition& definition, Firebird::MetaName* referredIndexName = NULL);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_index_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool unique;
	bool descending;
	NestConst<RelationSourceNode> relation;
	NestConst<ValueListNode> columns;
	NestConst<ValueSourceClause> computed;
};


class AlterIndexNode : public DdlNode
{
public:
	AlterIndexNode(MemoryPool& p, const Firebird::MetaName& aName, bool aActive)
		: DdlNode(p),
		  name(p, aName),
		  active(aActive)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_index_failed) << name;
	}

public:
	Firebird::MetaName name;
	bool active;
};


class SetStatisticsNode : public DdlNode
{
public:
	SetStatisticsNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		// ASF: using ALTER INDEX's code.
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_index_failed) << name;
	}

public:
	Firebird::MetaName name;
};


class DropIndexNode : public DdlNode
{
public:
	DropIndexNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

	static bool deleteSegmentRecords(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_index_failed) << name;
	}

public:
	Firebird::MetaName name;
};


class CreateFilterNode : public DdlNode
{
public:
	class NameNumber : public Printable
	{
	public:
		NameNumber(MemoryPool& p, const Firebird::MetaName& aName)
			: name(p, aName),
			  number(0)
		{
		}

		NameNumber(MemoryPool& p, SSHORT aNumber)
			: name(p),
			  number(aNumber)
		{
		}

	public:
		virtual Firebird::string internalPrint(NodePrinter& printer) const
		{
			NODE_PRINT(printer, name);
			NODE_PRINT(printer, number);

			return "NameNumber";
		}

	public:
		Firebird::MetaName name;
		SSHORT number;
	};

public:
	CreateFilterNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  inputFilter(NULL),
		  outputFilter(NULL),
		  entryPoint(p),
		  moduleName(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_filter_failed) << name;
	}

public:
	Firebird::MetaName name;
	NestConst<NameNumber> inputFilter;
	NestConst<NameNumber> outputFilter;
	Firebird::string entryPoint;
	Firebird::string moduleName;
};


class DropFilterNode : public DdlNode
{
public:
	DropFilterNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_filter_failed) << name;
	}

public:
	Firebird::MetaName name;
};


class CreateShadowNode : public DdlNode
{
public:
	CreateShadowNode(MemoryPool& p, const SSHORT aNumber)
		: DdlNode(p),
		  number(aNumber),
		  manual(false),
		  conditional(false),
		  files(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_shadow_failed) << Firebird::Arg::Num(number);
	}

public:
	SSHORT number;
	bool manual;
	bool conditional;
	Nullable<SLONG> firstLength;
	Firebird::Array<NestConst<DbFileClause> > files;
};


class DropShadowNode : public DdlNode
{
public:
	DropShadowNode(MemoryPool& p, const SSHORT aNumber, const bool aNodelete)
		: DdlNode(p),
		  number(aNumber),
		  nodelete(aNodelete)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_shadow_failed) << Firebird::Arg::Num(number);
	}

public:
	SSHORT number;
	bool nodelete;
};


class CreateRoleNode : public DdlNode
{
public:
	CreateRoleNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_role_failed) << name;
	}

private:
	bool isItUserName(thread_db* tdbb, jrd_tra* transaction);

public:
	Firebird::MetaName name;
};


class MappingNode : public DdlNode, private ExecInSecurityDb
{
public:
	enum OP {MAP_ADD, MAP_MOD, MAP_RPL, MAP_DROP};

	MappingNode(MemoryPool& p, OP o, const Firebird::MetaName& nm)
		: DdlNode(p),
		  name(p, nm),
		  fromUtf8(p),
		  plugin(NULL),
		  db(NULL),
		  fromType(NULL),
		  from(NULL),
		  to(NULL),
		  op(o),
		  mode('#'),
		  global(false),
		  role(false)
	{
	}

	void validateAdmin();

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_mapping_failed) << name <<
			(op == MAP_ADD ? "CREATE" : op == MAP_MOD ?
			 "ALTER" : op == MAP_RPL ? "CREATE OR ALTER" : "DROP");
	}
	void runInSecurityDb(SecDbContext* secDbContext);

private:
	void addItem(Firebird::string& ddl, const char* text, char quote = '"');

public:
	Firebird::MetaName name;
	Firebird::string fromUtf8;
	Firebird::MetaName* plugin;
	Firebird::MetaName* db;
	Firebird::MetaName* fromType;
	IntlString* from;
	Firebird::MetaName* to;
	OP op;
	char mode;	// * - any source, P - plugin, M - mapping, S - any serverwide plugin
	bool global;
	bool role;
};


class DropRoleNode : public DdlNode
{
public:
	DropRoleNode(MemoryPool& p, const Firebird::MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_role_failed) << name;
	}

public:
	Firebird::MetaName name;
};


class UserNode : public DdlNode
{
public:
	explicit UserNode(MemoryPool& p)
		: DdlNode(p)
	{ }

protected:
	Firebird::MetaName upper(const Firebird::MetaName& str);
};


class CreateAlterUserNode : public UserNode
{
public:
	enum Mode {USER_ADD, USER_MOD, USER_RPL};

	CreateAlterUserNode(MemoryPool& p, Mode md, const Firebird::MetaName& aName)
		: UserNode(p),
		  properties(p),
		  name(p, upper(aName)),
		  password(NULL),
		  firstName(NULL),
		  middleName(NULL),
		  lastName(NULL),
		  plugin(NULL),
		  comment(NULL),
		  mode(md)
	{ }

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(mode == USER_ADD ?
			isc_dsql_create_user_failed : isc_dsql_alter_user_failed) << name;
	}

public:
	class Property
	{
	public:
		explicit Property(MemoryPool& p)
			: value(p)
		{ }

		Firebird::MetaName property;
		Firebird::string value;
	};

	Firebird::ObjectsArray<Property> properties;
	const Firebird::MetaName name;
	Firebird::string* password;
	Firebird::string* firstName;
	Firebird::string* middleName;
	Firebird::string* lastName;
	Firebird::MetaName* plugin;
	Firebird::string* comment;
	Nullable<bool> adminRole;
	Nullable<bool> active;
	Mode mode;

	void addProperty(Firebird::MetaName* pr, Firebird::string* val = NULL)
	{
		fb_assert(pr);

		Property& p = properties.add();
		p.property = *pr;
		if (val)
		{
			p.value = *val;
		}
	}
};


class DropUserNode : public UserNode
{
public:
	DropUserNode(MemoryPool& p, const Firebird::MetaName& aName, const Firebird::MetaName* aPlugin = NULL)
		: UserNode(p),
		  name(p, upper(aName)),
		  plugin(p)
	{
		if (aPlugin)
			plugin = *aPlugin;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_user_failed) << name;
	}

public:
	const Firebird::MetaName name;
	Firebird::MetaName plugin;
};


typedef Firebird::Pair<Firebird::NonPooled<char, ValueListNode*> > PrivilegeClause;
typedef Firebird::Pair<Firebird::NonPooled<SSHORT, Firebird::MetaName> > GranteeClause;

class GrantRevokeNode : public DdlNode, private ExecInSecurityDb
{
public:
	GrantRevokeNode(MemoryPool& p, bool aIsGrant)
		: DdlNode(p),
		  createDbJobs(p),
		  isGrant(aIsGrant),
		  privileges(p),
		  roles(p),
		  object(NULL),
		  users(p),
		  grantAdminOption(false),
		  grantor(NULL),
		  isDdl(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(isGrant ? isc_dsql_grant_failed : isc_dsql_revoke_failed);
	}
	void runInSecurityDb(SecDbContext* secDbContext);

private:
	void modifyPrivileges(thread_db* tdbb, jrd_tra* transaction, SSHORT option, const GranteeClause* user);
	void grantRevoke(thread_db* tdbb, jrd_tra* transaction, const GranteeClause* object,
		const GranteeClause* userNod, const char* privs, const Firebird::MetaName& field, int options);
	static void checkGrantorCanGrantRelation(thread_db* tdbb, jrd_tra* transaction, const char* grantor,
		const char* privilege, const Firebird::MetaName& relationName,
		const Firebird::MetaName& fieldName, bool topLevel);
	static void checkGrantorCanGrantRole(thread_db* tdbb, jrd_tra* transaction,
			const Firebird::MetaName& grantor, const Firebird::MetaName& roleName);
	static void checkGrantorCanGrantDdl(thread_db* tdbb, jrd_tra* transaction,
			const Firebird::MetaName& grantor, const char* privilege, const Firebird::MetaName& objName);
	static void checkGrantorCanGrantObject(thread_db* tdbb, jrd_tra* transaction, const char* grantor,
		const char* privilege, const Firebird::MetaName& objName, SSHORT objType);
	static void storePrivilege(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& object, const Firebird::MetaName& user,
		const Firebird::MetaName& field, const TEXT* privilege, SSHORT userType,
		SSHORT objType, int option, const Firebird::MetaName& grantor);
	static void setFieldClassName(thread_db* tdbb, jrd_tra* transaction,
		const Firebird::MetaName& relation, const Firebird::MetaName& field);

	// Diagnostics print helper.
	static const char* privilegeName(char symbol)
	{
		switch (UPPER7(symbol))
		{
			case 'A': return "ALL";
			case 'I': return "INSERT";
			case 'U': return "UPDATE";
			case 'D': return "DELETE";
			case 'S': return "SELECT";
			case 'X': return "EXECUTE";
			case 'G': return "USAGE";
			case 'M': return "ROLE";
			case 'R': return "REFERENCE";
			// ddl
			case 'C': return "CREATE";
			case 'L': return "ALTER";
			case 'O': return "DROP";
		}

		return "<Unknown>";
	}

	struct CreateDbJob
	{
		CreateDbJob(SSHORT a_userType, const Firebird::MetaName& a_user)
			: allOnAll(false), grantErased(false), badGrantor(false),
			  userType(a_userType), user(a_user)
		{ }

		bool allOnAll, grantErased, badGrantor;
		SSHORT userType;
		Firebird::MetaName user, revoker;
	};
	Firebird::Array<CreateDbJob> createDbJobs;

public:
	bool isGrant;
	Firebird::Array<PrivilegeClause> privileges;
	Firebird::Array<GranteeClause> roles;
	NestConst<GranteeClause> object;
	Firebird::Array<GranteeClause> users;
	bool grantAdminOption;
	NestConst<Firebird::MetaName> grantor;
	// ddl rights
	bool isDdl;
};


class AlterDatabaseNode : public DdlNode
{
public:
	static const unsigned CLAUSE_BEGIN_BACKUP		= 0x01;
	static const unsigned CLAUSE_END_BACKUP			= 0x02;
	static const unsigned CLAUSE_DROP_DIFFERENCE	= 0x04;
	static const unsigned CLAUSE_CRYPT				= 0x08;

	static const unsigned RDB_DATABASE_MASK =
		CLAUSE_BEGIN_BACKUP | CLAUSE_END_BACKUP | CLAUSE_DROP_DIFFERENCE;

public:
	AlterDatabaseNode(MemoryPool& p)
		: DdlNode(p),
		  create(false),
		  createLength(0),
		  linger(-1),
		  clauses(0),
		  differenceFile(p),
		  setDefaultCharSet(p),
		  setDefaultCollation(p),
		  files(p),
		  cryptPlugin(p),
		  keyName(p)
	{
	}

public:
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		dsqlScratch->getStatement()->setType(
			create ? DsqlCompiledStatement::TYPE_CREATE_DB : DsqlCompiledStatement::TYPE_DDL);
		return this;
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual bool checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_database_failed);
	}

private:
	static void changeBackupMode(thread_db* tdbb, jrd_tra* transaction, unsigned clause);
	static void defineDifference(thread_db* tdbb, jrd_tra* transaction, const Firebird::PathName& file);
	void checkClauses(thread_db* tdbb);

public:
	bool create;	// Is the node created with a CREATE DATABASE command?
	SLONG createLength, linger;
	unsigned clauses;
	Firebird::string differenceFile;
	Firebird::MetaName setDefaultCharSet;
	Firebird::MetaName setDefaultCollation;
	Firebird::Array<NestConst<DbFileClause> > files;
	Firebird::MetaName cryptPlugin;
	Firebird::MetaName keyName;
};


} // namespace

#endif // DSQL_DDL_NODES_H
