/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		CharSetContainer.h
 *	DESCRIPTION:	Container for character set and it's collations
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * Alex Peshkoff <peshkoff@mail.ru>
 */

#ifndef JRD_CHARSETCONTAINER_H
#define JRD_CHARSETCONTAINER_H

#include "../jrd/MetaName.h"
#include "../jrd/CacheVector.h"
#include "../jrd/Collation.h"
#include "../jrd/Resources.h"
#include "../jrd/met_proto.h"
#include "../common/classes/alloc.h"
#include "../jrd/lck.h"
#include "../common/sha2/sha2.h"


namespace Firebird {

class CharSet;
class CsConvert;

}


namespace Jrd {

struct SubtypeInfo;

class CharSetContainer : public Firebird::PermanentStorage
{
public:
	CharSetContainer(thread_db* tdbb, MemoryPool& p, MetaId cs_id, NoData);

	static bool destroy(thread_db* tdbb, CharSetContainer* container);
	static CharSetContainer* create(thread_db* tdbb, MetaId id);
	void releaseLock(thread_db*) { }

	Firebird::CharSet* getCharSet()
	{
		return cs;
	}

	Firebird::CsConvert lookupConverter(thread_db* tdbb, CSetId to_cs);

	static CharSetContainer* lookupCharset(thread_db* tdbb, TTypeId ttype);

	bool hasData() const
	{
		return cs != nullptr;
	}

	QualifiedName getName() const;
	MetaId getId();

	Lock* getLock()
	{
		return cs_lock;
	}

private:
	static bool lookupInternalCharSet(CSetId id, SubtypeInfo* info);

public:
	CharsetVariants names;

private:
	Firebird::CharSet* cs;
	Lock* cs_lock;
};

class CharSetVers final : public ObjectBase
{
public:
	CharSetVers(Cached::CharSet* parent)
		: perm(parent), charset_collations(perm->getPool())
	{ }

	static const char* objectFamily(void*)
	{
		return "character set";
	}

	MetaId getId()
	{
		return perm->getId();
	}

	QualifiedName getName() const
	{
		return perm->getName();
	}

	static void destroy(thread_db* tdbb, CharSetVers* csv);
	static CharSetVers* create(thread_db* tdbb, MemoryPool& p, Cached::CharSet* perm);

	static const enum lck_t LOCKTYPE = LCK_cs_rescan;

	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
	ScanResult reload(thread_db* tdbb, ObjectBase::Flag flags)
	{
		return scan(tdbb, flags);
	}

	Collation* getCollation(CollId id);
	Collation* getCollation(const QualifiedName& name);
	Cached::CharSet* getContainer() const
	{
		return perm;
	}

	static int objectType();

	bool hash(thread_db*, Firebird::sha512&)
	{
		return true;
	}

private:
	Cached::CharSet* perm;
	Firebird::HalfStaticArray<Collation*, 16> charset_collations;

public:
	decltype(perm) getPermanent() const
	{
		return perm;
	}
};

} // namespace Jrd

#endif // JRD_CHARSETCONTAINER_H

