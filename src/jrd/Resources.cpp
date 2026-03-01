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

	int gotHash = 0;
	gotHash += charSets.transfer(tdbb, to, internal, digest);
	gotHash += relations.transfer(tdbb, to, internal, digest);
	gotHash += procedures.transfer(tdbb, to, internal, digest);
	gotHash += functions.transfer(tdbb, to, internal, digest);
	gotHash += triggers.transfer(tdbb, to, internal, digest);
	gotHash += indices.transfer(tdbb, to, internal, digest);
	gotHash += constants.transfer(tdbb, to, internal, digest);

	if (hasHash)
	{
		if (gotHash != 6)
			outdated();

		HashValue newValue;
		digest.getHash(newValue);
		if (memcmp(newValue, hashValue, sizeof(HashValue)))
			outdated();
	}
	else if (gotHash == 6)
	{
		digest.getHash(hashValue);
		hasHash = true;
	}
}

[[noreturn]] void Resources::outdated()
{
	ERR_post(Arg::Gds(isc_random) << "Statement format outdated, need to be reprepared");
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

void Format::hash(Firebird::sha512& digest) const
{
	// Here is supposed that in fmt_desc (i.e. Firebird::Array) all elements are located
	// one after another starting with begin() position.
	// If that became wrong this function to be modified.

	digest.process(fmt_desc.getCount() * sizeof(dsc), fmt_desc.begin());
}
