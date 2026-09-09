/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		Resources.cpp
 *	DESCRIPTION:	Resource used by request / transaction
 *
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/Resources.h"

#include "../jrd/Relation.h"
#include "../jrd/CharSetContainer.h"
#include "../jrd/Function.h"
#include "../jrd/met.h"

using namespace Firebird;
using namespace Jrd;


void Resources::transfer(thread_db* tdbb, VersionedObjects* to, bool internal)
{
	sha512 digest;
	to->clear();

	static constexpr int expectedHashesCount = 7;

	int gotHash = 0;
	gotHash += charSets.transfer(tdbb, to, internal, digest);
	gotHash += relations.transfer(tdbb, to, internal, digest);
	gotHash += procedures.transfer(tdbb, to, internal, digest);
	gotHash += functions.transfer(tdbb, to, internal, digest);
	gotHash += triggers.transfer(tdbb, to, internal, digest);
	gotHash += indices.transfer(tdbb, to, internal, digest);
	gotHash += packages.transfer(tdbb, to, internal, digest);

	if (hasHash)
	{
		if (gotHash != expectedHashesCount)
			outdated();

		HashValue newValue;
		digest.getHash(newValue);
		if (memcmp(newValue, hashValue, sizeof(HashValue)))
			outdated();
	}
	else if (gotHash == expectedHashesCount)
	{
		digest.getHash(hashValue);
		hasHash = true;
	}
}

[[noreturn]] void Resources::outdated()
{
	ERR_post(Arg::Gds(isc_old_format));
}

Resources::~Resources()
{ }

template <>
jrd_rel* CachedResource<jrd_rel, RelationPermanent>::operator()(thread_db* tdbb) const
{
	if (!cacheElement)
		return nullptr;

	return cacheElement->getVersioned(tdbb, cacheElement->isSystem() ? CacheFlag::NOSCAN : 0);
}

void hashDescriptor(Firebird::sha512& digest, const dsc& desc)
{
	digest.process(offsetof(dsc, dsc_sub_count) + sizeof(desc.dsc_sub_count), &desc);

	for (const dsc* subDesc = desc.dsc_sub_first; subDesc; subDesc = subDesc->dsc_next)
		hashDescriptor(digest, *subDesc);
}


void Format::hash(Firebird::sha512& digest) const
{
	for (const auto& desc : fmt_desc)
		hashDescriptor(digest, desc);
}
