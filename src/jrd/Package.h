/*
 *	PROGRAM:		Firebird CONSTANTS implementation.
 *	MODULE:			Package.h
 *	DESCRIPTION:	Routines to cache and reload package items
 *
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2025 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_PACKAGE_H
#define JRD_PACKAGE_H

#include "firebird.h"
#include "../jrd/CacheVector.h"
#include "../jrd/Resources.h"
#include "../jrd/obj.h"
#include "../jrd/val.h"
#include "../jrd/lck.h"
#include "../common/classes/GenericMap.h"

namespace Jrd
{
class CompilerScratch;
class DsqlCompilerScratch;
class dsql_fld;

class ConstantValue final : public Firebird::PermanentStorage
{
public:
	QualifiedName name;
	bool isPrivate = false;

	ConstantValue(MemoryPool& pool) :
		Firebird::PermanentStorage(pool),
		name(pool)
	{ }

	~ConstantValue()
	{
		delete m_value.vlu_string;
	}

	bool hash(thread_db* tdbb, Firebird::sha512& digest) const;

	static void genConstantBlr(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
		ValueExprNode* constExpr, dsql_fld* type, const MetaName& schema);

	void setType(const dsc typeDesc)
	{
		m_type = typeDesc;
		m_blrBlobId = {};

		delete m_value.vlu_string;
		m_value = {};
	}

	void setValue(const bid blobId)
	{
		m_blrBlobId = blobId;

		delete m_value.vlu_string;
		m_value = {};
	}

	inline dsc getDesc() const noexcept
	{
		fb_assert(m_type.dsc_dtype != dtype_unknown);
		return m_type;
	}

	bid getBlobId(thread_db* tdbb);

	dsc* makeValue(thread_db* tdbb);

private:
	void executeCsbNode(thread_db* tdbb, CompilerScratch* csb);

	// Lock in case of makeing value during the execute state
	Firebird::RWLock m_makeValueLock{};

	// The constant type and constant value type may differed
	// For example, the the defiend type is int but value type is short)
	// So keep it explicit
	dsc m_type{};

	// Get the value when scanning (without MINISCAN) or after the first execution
	impure_value m_value{};

	// keep only materialized value
	bid m_blrBlobId{};
};

struct ConstantsCache
{
	using ValueId = ULONG;

	ConstantsCache(MemoryPool& pool) :
		nameMap(pool),
		values(pool)
	{ }

	Firebird::LeftPooledMap<QualifiedName, ValueId> nameMap;

	Firebird::ObjectsArray<ConstantValue> values;

	ConstantValue& add(const QualifiedName& constName, const bool isPrivate);

	void clear()
	{
		nameMap.clear();
		values.clear();
	}
};


class PackagePermanent : public Firebird::PermanentStorage
{
public:
	explicit PackagePermanent(thread_db* tdbb, MemoryPool& p, const MetaId metaId, NoData)
		: PermanentStorage(p),
			id(metaId),
			name(p)
	{ }

	explicit PackagePermanent(MemoryPool& p)
		: PermanentStorage(p),
			id(~0),
			name(p)
	{ }

	MetaId getId() const
	{
		return id;
	}

	static bool destroy(thread_db* tdbb, PackagePermanent* routine)
	{
		return false;
	}

	void releaseLock(thread_db*) { }
	void reloadAst(thread_db* tdbb, TraNumber tran, bool erase) { }

	const QualifiedName& getName() const noexcept { return name; }
	void setName(const QualifiedName& value) { name = value; }

	bool hasData() const { return name.hasData(); }

public:
	MetaId id;
	QualifiedName name;
	QualifiedName securityName;
};

class Package final : public Firebird::PermanentStorage, public ObjectBase
{
public:
	// lock requeued by CacheElement
	static const enum lck_t LOCKTYPE = LCK_package_rescan;

private:
	explicit Package(Cached::Package* perm)
		: Firebird::PermanentStorage(perm->getPool()),
			constants(perm->getPool()),
			cachedPackage(perm)
	{ }

public:
	explicit Package(MemoryPool& p)
		: Firebird::PermanentStorage(p),
		  constants(p)
	{ }

	// Methods needed by the MetaCache
	// ----------

	static bool destroy(thread_db* tdbb, Package* routine)
	{
		return false;
	}

	static Package* create(thread_db* tdbb, MemoryPool& pool, Cached::Package* perm);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);

	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	void checkReload(thread_db* tdbb);
	ScanResult reload(thread_db* tdbb, ObjectBase::Flag flags);

	static const char* objectFamily(void*)
	{
		return "package";
	}

	MetaId getId() const
	{
		return getPermanent()->id;
	}

	int getObjectType() const noexcept
	{
		return objectType();
	}

	SLONG getSclType() const noexcept
	{
		return obj_package_header;
	}

	static int objectType();

	bool hash(thread_db* tdbb, Firebird::sha512& digest);

	Cached::Package* getPermanent() const noexcept
	{
		return cachedPackage;
	}

	// ----------

	ConstantValue& addConstant(thread_db* tdbb,
		const QualifiedName& constName,
		const bool isPrivate,
		const TypeClause* type);

	ConstantValue& addConstant(thread_db* tdbb,
		const QualifiedName& constName,
		const bool isPrivate,
		const dsc type,
		const bid blrBlobId,
		const bool skipMakeValue = false);

	ConstantValue& updateConstant(thread_db* tdbb,
		const QualifiedName& constName,
		const bool isPrivate,
		const TypeClause* type);

	ConstantValue* findConstant(const QualifiedName& name);

private:
	void collectConstants(thread_db* tdbb, const bool skipMakingValue);

	virtual ~Package() = default;

private:
	ConstantsCache constants;
	Cached::Package* cachedPackage = nullptr;		// entry in the cache
	bool m_callReload = true;
};

} // namespace Jrd

#endif // JRD_PACKAGE_H
