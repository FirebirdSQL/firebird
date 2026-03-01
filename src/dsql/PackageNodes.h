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
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_PACKAGE_NODES_H
#define DSQL_PACKAGE_NODES_H

#include "../dsql/DdlNodes.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../include/fb_exception.h"

namespace Jrd {

enum class PackageItemType : USHORT
{
	FUNCTION = 0,
	PROCEDURE,
	CONSTANT,
	META_SIZE
};

template<class TArray, class TType>
class ItemNames : public TArray
{
public:
	ItemNames() : TArray()
	{}

	ItemNames(Firebird::MemoryPool& pool) : TArray(pool)
	{}

	operator TArray&()
	{
		return *this;
	}

	template<PackageItemType IValue>
	void addName(const QualifiedName& newName)
	{
		checkDuplicate<IValue>(newName);
		TArray::add(TType(newName.object));
	}

	template<PackageItemType IValue>
	void checkDuplicate(const QualifiedName& newName)
	{
		if constexpr (std::is_same_v<TType, MetaName>)
		{
			if (!TArray::exist(newName.object))
				return; // The name is unique
		}
		else
		{
			// Cast
			if (!TArray::exist(TType(newName.object)))
				return; // The name is unique
		}

		static_assert(size_t(IValue) >= 0 && size_t(IValue) < size_t(PackageItemType::META_SIZE), "Invalid item type");
		static const std::array<const char*, size_t(PackageItemType::META_SIZE)> names{
			"FUNCTION",
			"PROCEDURE",
			"CONSTANT",
		};

		 // Print just the object name because the full path is present n the parent error message
		Firebird::status_exception::raise(
			Firebird::Arg::Gds(isc_no_meta_update) <<
			Firebird::Arg::Gds(isc_dyn_duplicate_package_item) <<
				Firebird::Arg::Str(names[size_t(IValue)]) << Firebird::Arg::Str(newName.object.toQuotedString()));
	}
};


class PackageItemsHolder
{
public:
	using ItemsSignatureArray = ItemNames<Firebird::SortedObjectsArray<Signature>, Signature>;

public:
	PackageItemsHolder()
	{ }

	PackageItemsHolder(Firebird::MemoryPool& pool) :
		functions(pool),
		procedures(pool),
		constants(pool)
	{ }

	void drop(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, const QualifiedName& packageAndSchema);
	void checkDefineMatch(Firebird::MemoryPool& pool, const QualifiedName& packageAndSchema, const PackageItemsHolder& newItems);
	void collectPackagedItems(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageAndSchema, bool details, bool collectConstants);
	void clear();

public:
	ItemsSignatureArray functions;
	ItemsSignatureArray procedures;
	ItemsSignatureArray constants;
};

class PackageReferenceNode final : public TypedNode<ValueExprNode, ExprNode::TYPE_REFERENCE>
{
public:
	PackageReferenceNode(Firebird::MemoryPool& pool, const QualifiedName& name,
		const UCHAR itemType = blr_pkg_ref_item_const);

	Firebird::string internalPrint(NodePrinter& printer) const final;

	ValueExprNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) final;
	static DmlNode* parse(thread_db* tdbb, Firebird::MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);
	void genBlr(DsqlCompilerScratch* dsqlScratch) final;

	void setParameterName(dsql_par* parameter) const final;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) final;

	static bool constantExists(thread_db* tdbb, Jrd::jrd_tra* transaction,
		const QualifiedName& name, bool* isPrivate = nullptr);

	// Compute descriptor for value expression.
	void getDesc(thread_db*, CompilerScratch*, dsc*) final;

	ValueExprNode* copy(thread_db*, NodeCopier&) const final;
	ValueExprNode* pass2(thread_db* tdbb, CompilerScratch* csb) final;
	dsc* execute(thread_db*, Request*) const final;

	const char* getName() const
	{
		return m_fullName.object.c_str();
	}

private:
	CachedResource<Constant, RoutinePermanent> m_constant;
	const QualifiedName m_fullName;

	const UCHAR m_itemType;
	ULONG m_impureOffset = 0;
};


class CreatePackageConstantNode final : public DdlNode
{
public:
	CreatePackageConstantNode(Firebird::MemoryPool& pool, const MetaName& name,
		dsql_fld* type = nullptr, ValueExprNode* value = nullptr, bool isPrivate = false)
		: DdlNode(pool),
		  name(pool, name),
		  m_type(type),
		  m_expr(value),
		  m_isPrivate(isPrivate)
	{ }

	Firebird::string internalPrint(NodePrinter& printer) const;
	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) final;
	void checkPermission(thread_db* tdbb, jrd_tra* transaction) final;
	void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) final;

	inline void makePublic()
	{
		m_isPrivate = false;
	}
	inline void makePrivate()
	{
		m_isPrivate = true;
	}

private:
	dsc* makeConstantValue(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, CompilerScratch*& nodeContext);
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);


protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_const_failed, isc_dsql_alter_const_failed,
					isc_dsql_create_alter_const_failed)) <<
				Firebird::Arg::Str(name.toQuotedString());
	}

public:
	QualifiedName name;
	bool create = false;
	bool alter = false;

private:
	NestConst<dsql_fld> m_type;
	NestConst<ValueExprNode> m_expr;
	MetaId m_id = 0;
	bool m_isPrivate = false;
};


class CreateAlterPackageNode : public DdlNode
{
public:
	struct Item
	{
		static Item create(CreateAlterFunctionNode* function)
		{
			Item item;
			item.type = PackageItemType::FUNCTION;
			item.function = function;
			item.dsqlScratch = nullptr;
			return item;
		}

		static Item create(CreateAlterProcedureNode* procedure)
		{
			Item item;
			item.type = PackageItemType::PROCEDURE;
			item.procedure = procedure;
			item.dsqlScratch = nullptr;
			return item;
		}

		static Item create(CreatePackageConstantNode* constant)
		{
			Item item;
			item.type = PackageItemType::CONSTANT;
			item.constant = constant;
			item.dsqlScratch = nullptr;
			return item;
		}

		PackageItemType type;

		union
		{
			CreateAlterFunctionNode* function;
			CreateAlterProcedureNode* procedure;
			CreatePackageConstantNode* constant;
		};

		DsqlCompilerScratch* dsqlScratch;
	};


	using ItemsNameArray = ItemNames<Firebird::SortedArray<MetaName>, MetaName>;

public:
	CreateAlterPackageNode(MemoryPool& pool, const QualifiedName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  create(true),
		  alter(false),
		  source(pool),
		  items(NULL),
		  functionNames(pool),
		  procedureNames(pool),
		  constantNames(pool),
		  owner(pool)
	{
	}

public:
	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void checkPermission(thread_db* tdbb, jrd_tra* transaction) override;
	void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;

protected:
	void putErrorPrefix(Firebird::Arg::StatusVector& statusVector) override
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_pack_failed, isc_dsql_alter_pack_failed,
					isc_dsql_create_alter_pack_failed)) <<
				name.toQuotedString();
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlterIndividualParameters(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	void executeItems(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	QualifiedName name;
	bool create;
	bool alter;
	bool createIfNotExistsOnly = false;
	Firebird::string source;
	Firebird::Array<Item>* items;
	ItemsNameArray functionNames;
	ItemsNameArray procedureNames;
	ItemsNameArray constantNames;
	std::optional<SqlSecurity> ssDefiner;

private:
	MetaName owner;
};


class DropPackageNode : public DdlNode
{
public:
	DropPackageNode(MemoryPool& pool, const QualifiedName& aName)
		: DdlNode(pool),
		  name(pool, aName)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void checkPermission(thread_db* tdbb, jrd_tra* transaction) override;
	void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;

	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		if (recreate)
			dsqlScratch->qualifyNewName(name);
		else
			dsqlScratch->qualifyExistingName(name, obj_package_header);

		protectSystemSchema(name.schema, obj_package_header);
		dsqlScratch->ddlSchema = name.schema;

		return DdlNode::dsqlPass(dsqlScratch);
	}

protected:
	void putErrorPrefix(Firebird::Arg::StatusVector& statusVector) override
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_pack_failed) << name.toQuotedString();
	}

public:
	QualifiedName name;
	bool silent = false;
	bool recreate = false;
};


typedef RecreateNode<CreateAlterPackageNode, DropPackageNode, isc_dsql_recreate_pack_failed>
	RecreatePackageNode;


class CreatePackageBodyNode : public DdlNode
{
public:
	CreatePackageBodyNode(MemoryPool& pool, const QualifiedName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  source(pool),
		  declaredItems(NULL),
		  items(NULL),
		  owner(pool)
	{
	}

public:
	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void checkPermission(thread_db* tdbb, jrd_tra* transaction) override;
	void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;

protected:
	void putErrorPrefix(Firebird::Arg::StatusVector& statusVector) override
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_pack_body_failed) << name.toQuotedString();
	}

public:
	QualifiedName name;
	Firebird::string source;
	Firebird::Array<CreateAlterPackageNode::Item>* declaredItems;
	Firebird::Array<CreateAlterPackageNode::Item>* items;
	bool createIfNotExistsOnly = false;

private:
	Firebird::string owner;
};


class DropPackageBodyNode : public DdlNode
{
public:
	DropPackageBodyNode(MemoryPool& pool, const QualifiedName& aName)
		: DdlNode(pool),
		  name(pool, aName)
	{
	}

public:
	Firebird::string internalPrint(NodePrinter& printer) const override;
	void checkPermission(thread_db* tdbb, jrd_tra* transaction) override;
	void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;

	DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
	{
		dsqlScratch->qualifyExistingName(name, obj_package_header);
		protectSystemSchema(name.schema, obj_package_header);
		dsqlScratch->ddlSchema = name.schema;

		return DdlNode::dsqlPass(dsqlScratch);
	}

protected:
	void putErrorPrefix(Firebird::Arg::StatusVector& statusVector) override
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_pack_body_failed) << name.toQuotedString();
	}

public:
	QualifiedName name;
	bool silent = false;	// Unused. Just to please RecreateNode template.
	bool recreate = false;
};


typedef RecreateNode<CreatePackageBodyNode, DropPackageBodyNode, isc_dsql_recreate_pack_body_failed>
	RecreatePackageBodyNode;


} // namespace

#endif // DSQL_PACKAGE_NODES_H
