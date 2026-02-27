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
#include "../jrd/Routine.h"
#include "../jrd/obj.h"
#include "../jrd/val.h"
#include "../jrd/lck.h"

namespace Jrd
{
class DsqlCompilerScratch;
class dsql_fld;

class Constant final : public Routine
{
public:
	static Constant* lookup(thread_db* tdbb, MetaId id);
	static Constant* lookup(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag flags);

public:
	static const enum lck_t LOCKTYPE = LCK_constant_rescan;

private:
	explicit Constant(Cached::Constant* perm)
		: Routine(perm->getPool()),
		  cachedConstant(perm)
	{
		flReload = true;
	}

public:
	explicit Constant(MemoryPool& p)
		: Routine(p)
	{
		flReload = true;
	}

	static Constant* create(thread_db* tdbb, MemoryPool& pool, Cached::Constant* perm);
	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
	void checkReload(thread_db* tdbb) const override;

	static const char* objectFamily(void*)
	{
		return "constant";
	}

public:
	int getObjectType() const noexcept final
	{
		return objectType();
	}

	SLONG getSclType() const noexcept final
	{
		return obj_package_constant;
	}

	ScanResult reload(thread_db* tdbb, ObjectBase::Flag fl);

	static int objectType();

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

	virtual ~Constant() override
	{
		delete m_value.vlu_string;
	}

public:
	Cached::Constant* cachedConstant;		// entry in the cache

	Cached::Constant* getPermanent() const noexcept override
	{
		return cachedConstant;
	}

private:
	impure_value m_value{};
};

} // namespace Jrd

#endif // JRD_CONSTANT_H
