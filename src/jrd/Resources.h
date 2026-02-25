/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		Resources.h
 *	DESCRIPTION:	Resource used by request / transaction
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2023 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 */

#ifndef JRD_RESOURCES_H
#define JRD_RESOURCES_H

#include "fb_blk.h"
#include "../jrd/CacheVector.h"
#include "../common/sha2/sha2.h"
#include <functional>

namespace Jrd {

class RelationPermanent;
class RoutinePermanent;
class CharSetContainer;
class jrd_rel;
class jrd_prc;
class Function;
class DbTriggersHeader;
class DbTriggers;
class CharSetVers;
class IndexPermanent;
class IndexVersion;
class Constant;

namespace Cached
{
	// DB objects stored in cache vector
	typedef CacheElement<jrd_rel, RelationPermanent> Relation;
	typedef CacheElement<jrd_prc, RoutinePermanent> Procedure;
	typedef CacheElement<CharSetVers, CharSetContainer> CharSet;
	typedef CacheElement<Function, RoutinePermanent> Function;
	typedef CacheElement<DbTriggers, DbTriggersHeader> Triggers;
	typedef CacheElement<IndexVersion, IndexPermanent> Index;
	typedef CacheElement<Constant, RoutinePermanent> Constant;
}

class Resources;

// Set of objects cached per particular MDC version

union VersionedPartPtr
{
	jrd_rel* relation;
	jrd_prc* procedure;
	Function* function;
	CharSetVers* charset;
	DbTriggers* triggers;
	IndexVersion* index;
	Constant* constant;
};

class VersionedObjects : public pool_alloc_rpt<VersionedPartPtr>,
	public Firebird::RefCounted
{

public:
	VersionedObjects(FB_SIZE_T cnt) :
		version(0),
		capacity(cnt)
	{ }

	void clear()
	{
		memset(data, 0, sizeof(data[0]) * capacity);
	}

	template <class C>
	void put(FB_SIZE_T n, C* obj)
	{
		fb_assert(n < capacity);
		fb_assert(!object<C>(n));
		object<C>(n) = obj;
	}

	template <class C>
	C* get(FB_SIZE_T n) const
	{
		fb_assert(n < capacity);
		return object<C>(n);
	}

	FB_SIZE_T getCapacity()
	{
		return capacity;
	}

	MdcVersion version;		// version when filled

private:
	FB_SIZE_T capacity;
	VersionedPartPtr data[1];

	template <class C> C*& object(FB_SIZE_T n);
	template <class C> C* object(FB_SIZE_T n) const;
};

// specialization
template <> inline Function*& VersionedObjects::object<Function>(FB_SIZE_T n) { return data[n].function; }
template <> inline jrd_prc*& VersionedObjects::object<jrd_prc>(FB_SIZE_T n) { return data[n].procedure; }
template <> inline jrd_rel*& VersionedObjects::object<jrd_rel>(FB_SIZE_T n) { return data[n].relation; }
template <> inline CharSetVers*& VersionedObjects::object<CharSetVers>(FB_SIZE_T n) { return data[n].charset; }
template <> inline DbTriggers*& VersionedObjects::object<DbTriggers>(FB_SIZE_T n) { return data[n].triggers; }
template <> inline IndexVersion*& VersionedObjects::object<IndexVersion>(FB_SIZE_T n) { return data[n].index; }
template <> inline Constant*& VersionedObjects::object<Constant>(FB_SIZE_T n) { return data[n].constant; }

template <> inline Function* VersionedObjects::object<Function>(FB_SIZE_T n) const { return data[n].function; }
template <> inline jrd_prc* VersionedObjects::object<jrd_prc>(FB_SIZE_T n) const { return data[n].procedure; }
template <> inline jrd_rel* VersionedObjects::object<jrd_rel>(FB_SIZE_T n) const { return data[n].relation; }
template <> inline CharSetVers* VersionedObjects::object<CharSetVers>(FB_SIZE_T n) const { return data[n].charset; }
template <> inline DbTriggers* VersionedObjects::object<DbTriggers>(FB_SIZE_T n) const { return data[n].triggers; }
template <> inline IndexVersion* VersionedObjects::object<IndexVersion>(FB_SIZE_T n) const { return data[n].index; }
template <> inline Constant* VersionedObjects::object<Constant>(FB_SIZE_T n) const { return data[n].constant; }


template <class OBJ, class PERM>
class CachedResource
{
public:
	CachedResource(CacheElement<OBJ, PERM>* elem, FB_SIZE_T version)
		: cacheElement(elem), versionOffset(version)
	{ }

	CachedResource()
		: cacheElement(nullptr)
	{ }

	OBJ* operator()(const VersionedObjects* runTime) const
	{
		return cacheElement ? runTime->get<OBJ>(versionOffset) : nullptr;
	}

	OBJ* operator()(const VersionedObjects& runTime) const
	{
		return cacheElement ? runTime.get<OBJ>(versionOffset) : nullptr;
	}

	OBJ* operator()(thread_db* tdbb) const
	{
		return cacheElement ? cacheElement->getVersioned(tdbb, 0) : nullptr;
	}

	CacheElement<OBJ, PERM>* operator()() const
	{
		return cacheElement;
	}

	FB_SIZE_T getOffset() const
	{
		return versionOffset;
	}

	void clear()
	{
		cacheElement = nullptr;
	}

	bool isSet() const
	{
		return cacheElement != nullptr;
	}

	operator bool() const
	{
		return isSet();
	}

	bool operator!() const
	{
		return !isSet();
	}

private:
	CacheElement<OBJ, PERM>* cacheElement;
	FB_SIZE_T versionOffset;
};

template <>
jrd_rel* CachedResource<jrd_rel, RelationPermanent>::operator()(thread_db* tdbb) const;


class Resources final
{
public:
	template <class OBJ, class PERM>
	class RscArray : public Firebird::Array<CachedResource<OBJ, PERM>>
	{
	public:
		typedef CacheElement<OBJ, PERM> StoredElement;

		RscArray(MemoryPool& p, FB_SIZE_T& pos)
			: Firebird::Array<CachedResource<OBJ, PERM>>(p),
			  versionCurrentPosition(pos)
		{ }

		bool knownResource(StoredElement* res)
		{
			FB_SIZE_T posDummy;
			return checkPresence(res, posDummy);
		}

		CachedResource<OBJ, PERM>& registerResource(StoredElement* res)
		{
			FB_SIZE_T pos;
			if (!checkPresence(res, pos))
			{
				CachedResource<OBJ, PERM> newPtr(res, versionCurrentPosition++);
				pos = this->add(newPtr);
			}

			return this->getElement(pos);
		}

		int transfer(thread_db* tdbb, VersionedObjects* to, bool internal, Firebird::sha512& digest)
		{
			for (auto& resource : *this)
			{
				auto* ver = resource()->getVersioned(tdbb, internal ? CacheFlag::NOSCAN : CacheFlag::AUTOCREATE);
				if (ver)
				{
					if (!ver->hash(tdbb, digest))
						return 0;
					to->put(resource.getOffset(), ver);
				}
				else
					Resources::outdated();
			}
			return 1;
		}

	private:
		FB_SIZE_T& versionCurrentPosition;

		bool checkPresence(StoredElement* res, FB_SIZE_T& pos)
		{
			return this->find([res](const CachedResource<OBJ, PERM>& elem) {
					auto* e = elem();
					return e == res ? 0 : std::less<StoredElement*>{}(e, res) ? -1 : 1;
				}, pos);
		}
	};

	void transfer(thread_db* tdbb, VersionedObjects* to, bool internal);
	void release(thread_db* tdbb);
	[[noreturn]] static void outdated();

#ifdef DEV_BUILD
	MemoryPool* getPool() const
	{
		return &charSets.getPool();
	}
#endif

private:
	FB_SIZE_T versionCurrentPosition;
	typedef unsigned char HashValue[SHA512_DIGEST_SIZE];
	HashValue hashValue;
	bool hasHash = false;

public:
	template <class OBJ, class PERM> const RscArray<OBJ, PERM>& objects() const;

	Resources(MemoryPool& p)
		: versionCurrentPosition(0),
		  charSets(p, versionCurrentPosition),
		  relations(p, versionCurrentPosition),
		  procedures(p, versionCurrentPosition),
		  functions(p, versionCurrentPosition),
		  triggers(p, versionCurrentPosition),
		  indices(p, versionCurrentPosition),
		  constants(p, versionCurrentPosition)
	{ }

	~Resources();

	RscArray<CharSetVers, CharSetContainer> charSets;
	RscArray<jrd_rel, RelationPermanent> relations;
	RscArray<jrd_prc, RoutinePermanent> procedures;
	RscArray<Function, RoutinePermanent> functions;
	RscArray<DbTriggers, DbTriggersHeader> triggers;
	RscArray<IndexVersion, IndexPermanent> indices;
	RscArray<Constant, RoutinePermanent> constants;

	inline FB_SIZE_T countVersionedObjects() const noexcept
	{
		return charSets.getCount() + relations.getCount() + procedures.getCount() +
				functions.getCount() + triggers.getCount() + constants.getCount(); //? + indices.getCount() ?
	}
};

// specialization
template <> inline const Resources::RscArray<jrd_rel, RelationPermanent>& Resources::objects() const { return relations; }
template <> inline const Resources::RscArray<jrd_prc, RoutinePermanent>& Resources::objects() const { return procedures; }
template <> inline const Resources::RscArray<Function, RoutinePermanent>& Resources::objects() const { return functions; }
template <> inline const Resources::RscArray<CharSetVers, CharSetContainer>& Resources::objects() const { return charSets; }
template <> inline const Resources::RscArray<DbTriggers, DbTriggersHeader>& Resources::objects() const { return triggers; }
template <> inline const Resources::RscArray<IndexVersion, IndexPermanent>& Resources::objects() const { return indices; }
template <> inline const Resources::RscArray<Constant, RoutinePermanent>& Resources::objects() const { return constants; }

namespace Rsc
{
	typedef CachedResource<jrd_rel, RelationPermanent> Rel;
	typedef CachedResource<jrd_prc, RoutinePermanent> Proc;
	typedef CachedResource<Function, RoutinePermanent> Fun;
	typedef CachedResource<CharSetVers, CharSetContainer> CSet;
	typedef CachedResource<DbTriggers, DbTriggersHeader> Trig;
	typedef CachedResource<IndexVersion, IndexPermanent> Idx;
	typedef CachedResource<Constant, RoutinePermanent> Const;
}; //namespace Rsc


} // namespace Jrd

#endif // JRD_RESOURCES_H

