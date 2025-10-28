/*
 *	PROGRAM:	Engine Code
 *	MODULE:		CacheVector.cpp
 *	DESCRIPTION:	Vector used in shared metadata cache.
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2021 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"

#include "../jrd/CacheVector.h"
#include "../jrd/jrd.h"
#include "../jrd/Database.h"
#include "../jrd/tra.h"
#include "../jrd/met.h"
#include "../jrd/tpc_proto.h"

using namespace Jrd;
using namespace Firebird;

// class TransactionNumber

TraNumber TransactionNumber::current(thread_db* tdbb)
{
	jrd_tra* tra = tdbb->getTransaction();
	return tra ? tra->tra_number : 0;
}

TraNumber TransactionNumber::oldestActive(thread_db* tdbb)
{
	return tdbb->getDatabase()->dbb_oldest_active;
}

TraNumber TransactionNumber::next(thread_db* tdbb)
{
	return tdbb->getDatabase()->dbb_next_transaction + 1;
}

bool TransactionNumber::isNotActive(thread_db* tdbb, TraNumber traNumber)
{
	auto state = TPC_cache_state(tdbb, traNumber);
	return (state == tra_committed) || (state == tra_dead);
}

ULONG* TransactionNumber::getFlags(thread_db* tdbb)
{
	jrd_tra* tra = tdbb->getTransaction();

	// try to recover missing transaction - sooner of all scan() will use system transaction
	static ULONG pseudoFlag = 0u;
	return tra ? &tra->tra_flags : &pseudoFlag;
}


// class VersionSupport

MdcVersion VersionSupport::next(thread_db* tdbb)
{
	return MetadataCache::get(tdbb)->nextVersion();
}


// class ElementBase

[[noreturn]] void ElementBase::busyError(thread_db* tdbb, MetaId id, const char* family)
{
	fatal_exception::raiseFmt("%s id=%d busy in another thread - operation failed\n",
		family, id);
}

[[noreturn]] void ElementBase::newVersionBusy(thread_db* tdbb, MetaId id, const char* family, TraNumber traNum)
{
	Firebird::fatal_exception::raiseFmt("newVersion: %s %d is used by transaction %d\n",
		family, id, traNum);
}

[[noreturn]] void ElementBase::newVersionScan(thread_db* tdbb, MetaId id, const char* family)
{
	Firebird::fatal_exception::raiseFmt("newVersion: %s %d is scanned by us\n",
		family, id);
}

void ElementBase::commitErase(thread_db* tdbb)
{
	MetadataCache::get(tdbb)->objectCleanup(TransactionNumber::current(tdbb), this);
}

ElementBase::~ElementBase()
{ }

int ElementBase::blockingAst(void* ast_object)
{
	ElementBase* const cacheElement = static_cast<ElementBase*>(ast_object);

	try
	{
		fb_assert(cacheElement->lock);
		if (cacheElement->lock)
		{
			Database* const dbb = cacheElement->lock->lck_dbb;

			AsyncContextHolder tdbb(dbb, FB_FUNCTION);

			LCK_downgrade(tdbb, cacheElement->lock);
			bool erase = cacheElement->lock->lck_id < LCK_SR;
			if (!erase)
				LCK_release(tdbb, cacheElement->lock);

			cacheElement->reset(tdbb, erase);
		}
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

Lock* ElementBase::makeLock(thread_db* tdbb, MemoryPool& p, SINT64 key, enum lck_t locktype)
{
	Lock* lck = FB_NEW_RPT(p, 0) Lock(tdbb, sizeof(SLONG), locktype, this, blockingAst);
	lck->setKey(key);
	return lck;
}

void ElementBase::pingLock(thread_db* tdbb, ObjectBase::Flag flags, MetaId id, const char* family)
{
	if (!LCK_lock(tdbb, lock, flags & CacheFlag::ERASED ? LCK_EX : LCK_PW, LCK_WAIT))
	{
		Firebird::fatal_exception::raiseFmt("Unable to obtain WRITE rescan lock for %s %d",
			family, id);
	}

	LCK_convert(tdbb, lock, LCK_PR, LCK_WAIT);	// never fails
}

void ElementBase::setLock(thread_db* tdbb, MetaId id, const char* family)
{
	fb_assert(lock);
	if (lock)
	{
		if (!LCK_lock(tdbb, lock, LCK_PR, LCK_WAIT))
		{
			Firebird::fatal_exception::raiseFmt("Unable to obtain READ rescan lock for %s %d",
				family, id);
		}
	}
}

void ElementBase::releaseLock(thread_db* tdbb)
{
	fb_assert(lock);
	LCK_release(tdbb, lock);

#ifdef DEV_BUILD
	// avoid calling releaseLock() twice
	lock = nullptr;
#endif
}


// Class ConsistencyCheck

bool ConsistencyCheck::commitNumber(thread_db* tdbb)
{
	if (!tdbb->getAttachment())
		return true;

	if (!tdbb->getAttachment()->isRWGbak())
		return true;

	return false;
}

