/*
 *	PROGRAM:		Firebird CONSTANTS implementation.
 *	MODULE:			Constant.h
 *	DESCRIPTION:	Routine to cache and reload constants
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


#ifndef JRD_CONSTANT_H
#define JRD_CONSTANT_H

#include "firebird.h"
#include "../jrd/CacheVector.h"
#include "../jrd/Resources.h"
#include "../jrd/obj.h"
#include "../jrd/val.h"
#include "../jrd/lck.h"

namespace Jrd
{
class DsqlCompilerScratch;
class dsql_fld;

class ConstantPermanent : public Firebird::PermanentStorage
{
public:
	explicit ConstantPermanent(thread_db* tdbb, MemoryPool& p, MetaId metaId, NoData)
		: PermanentStorage(p),
			id(metaId),
			name(p)
	{ }

	explicit ConstantPermanent(MemoryPool& p)
		: PermanentStorage(p),
			id(~0),
			name(p)
	{ }

	MetaId getId() const
	{
		return id;
	}

	static bool destroy(thread_db* tdbb, ConstantPermanent* routine)
	{
		return false;
	}

	void releaseLock(thread_db*) { }

	const QualifiedName& getName() const noexcept { return name; }
	void setName(const QualifiedName& value) { name = value; }

	bool hasData() const { return name.hasData(); }

public:
	MetaId id;							// routine ID
	QualifiedName name;					// routine name
};

class Constant final : public Firebird::PermanentStorage, public ObjectBase
{
public:
	static Constant* lookup(thread_db* tdbb, MetaId id);
	static Constant* lookup(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag flags);

	// lock requeued by CacheElement
	static const enum lck_t LOCKTYPE = LCK_constant_rescan;

private:
	explicit Constant(Cached::Constant* perm)
		: Firebird::PermanentStorage(perm->getPool()),
		  cachedConstant(perm)
	{ }

public:
	explicit Constant(MemoryPool& p)
		: Firebird::PermanentStorage(p)
	{ }

	static bool destroy(thread_db* tdbb, Constant* routine)
	{
		return false;
	}

	static Constant* create(thread_db* tdbb, MemoryPool& pool, Cached::Constant* perm);
	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
	void checkReload(thread_db* tdbb) const;

	static const char* objectFamily(void*)
	{
		return "constant";
	}

public:
	const QualifiedName& getName() const noexcept { return getPermanent()->getName(); }
	MetaId getId() const noexcept { return getPermanent()->getId(); }

	int getObjectType() const noexcept
	{
		return objectType();
	}

	SLONG getSclType() const noexcept
	{
		return obj_package_constant;
	}

	ScanResult reload(thread_db* tdbb, ObjectBase::Flag fl);

	static int objectType();

	bool hash(thread_db* tdbb, Firebird::sha512& digest);

public:
	inline const dsc& getValue() const
	{
		return m_value.vlu_desc;
	}

	static void drop(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& name);
	static void dropAllFromPackage(thread_db* tdbb, Jrd::jrd_tra* transaction, const QualifiedName& parent, bool privateFlag);
	static dsc getDesc(thread_db* tdbb, Jrd::jrd_tra* transaction, const QualifiedName& name);

	static void genConstantBlr(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
		ValueExprNode* constExpr, dsql_fld* type, const MetaName& schema);

private:
	void makeValue(thread_db* tdbb, Attachment* attachment, bid blob_id);

	virtual ~Constant()
	{
		delete m_value.vlu_string;
	}

public:
	Cached::Constant* cachedConstant;		// entry in the cache

	Cached::Constant* getPermanent() const noexcept
	{
		return cachedConstant;
	}

private:
	impure_value m_value{};

	// Make sure the constant value will be read in at less at reload state
	bool m_callReload = true;
};

} // namespace Jrd

#endif // JRD_CONSTANT_H
