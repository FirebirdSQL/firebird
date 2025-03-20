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
 *  Copyright (c) 2022 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../dsql/DsqlStatementCache.h"
#include "../dsql/DsqlStatements.h"
#include "../jrd/Attachment.h"
#include "../jrd/Statement.h"
#include "../jrd/lck.h"
#include "../jrd/lck_proto.h"

using namespace Firebird;
using namespace Jrd;


// Class DsqlStatementCache

DsqlStatementCache::DsqlStatementCache(MemoryPool& o, Attachment* attachment)
	: PermanentStorage(o),
	  map(o),
	  activeStatementList(o),
	  inactiveStatementList(o)
{
	const auto dbb = attachment->att_database;
	maxCacheSize = dbb->dbb_config->getMaxStatementCacheSize();
}

DsqlStatementCache::~DsqlStatementCache()
{
	shutdown(JRD_get_thread_data());
}

int DsqlStatementCache::blockingAst(void* astObject)
{
#ifdef DSQL_STATEMENT_CACHE_DEBUG
	printf("DsqlStatementCache::blockingAst()\n");
#endif

	const auto self = static_cast<DsqlStatementCache*>(astObject);

	try
	{
		const auto dbb = self->lock->lck_dbb;
		AsyncContextHolder tdbb(dbb, FB_FUNCTION, self->lock);

		self->purge(tdbb, false);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}

RefPtr<DsqlStatement> DsqlStatementCache::getStatement(thread_db* tdbb, const string& text, USHORT clientDialect,
	bool isInternalRequest)
{
	RefStrPtr key;
	buildStatementKey(tdbb, key, text, clientDialect, isInternalRequest);

	if (const auto entryPtr = map.get(key))
	{
		const auto entry = *entryPtr;
		auto dsqlStatement(entry->dsqlStatement);

		string verifyKey;
		buildVerifyKey(tdbb, verifyKey, isInternalRequest);

		FB_SIZE_T verifyPos;
		if (!entry->verifyCache.find(verifyKey, verifyPos))
		{
			dsqlStatement->getStatement()->verifyAccess(tdbb);
			entry->verifyCache.insert(verifyPos, verifyKey);
		}

		if (!entry->active)
		{
			entry->dsqlStatement->setCacheKey(key);
			// Active statement has cacheKey and will tell us when it's going to be released.
			entry->dsqlStatement->release();

			entry->active = true;

			cacheSize -= entry->size;

			activeStatementList.splice(activeStatementList.end(), inactiveStatementList, entry);
		}

#ifdef DSQL_STATEMENT_CACHE_DEBUG
		dump();
#endif

		return dsqlStatement;
	}

	return {};
}

void DsqlStatementCache::putStatement(thread_db* tdbb, const string& text, USHORT clientDialect,
	bool isInternalRequest, RefPtr<DsqlStatement> dsqlStatement)
{
	fb_assert(dsqlStatement->isDml());

	ensureLockIsCreated(tdbb);

	if (isEmpty())
	{
		ThreadStatusGuard tempStatus(tdbb);

		if (!LCK_convert(tdbb, lock, LCK_SW, LCK_NO_WAIT))
			return;
	}

	const unsigned statementSize = dsqlStatement->getSize();

	RefStrPtr key;
	buildStatementKey(tdbb, key, text, clientDialect, isInternalRequest);

	StatementEntry newStatement(getPool());
	newStatement.key = key;
	newStatement.size = statementSize;
	newStatement.dsqlStatement = std::move(dsqlStatement);
	newStatement.active = true;

	string verifyKey;
	buildVerifyKey(tdbb, verifyKey, isInternalRequest);
	newStatement.verifyCache.add(verifyKey);

	newStatement.dsqlStatement->setCacheKey(key);
	// Active statement has cacheKey and will tell us when it's going to be released.
	newStatement.dsqlStatement->release();

	activeStatementList.pushBack(std::move(newStatement));
	map.put(key, --activeStatementList.end());

#ifdef DSQL_STATEMENT_CACHE_DEBUG
	dump();
#endif
}

void DsqlStatementCache::removeStatement(thread_db* tdbb, DsqlStatement* statement)
{
	if (const auto cacheKey = statement->getCacheKey())
	{
		if (const auto entryPtr = map.get(cacheKey))
		{
			const auto entry = *entryPtr;

			entry->dsqlStatement->resetCacheKey();

			if (entry->active)
			{
				entry->dsqlStatement->addRef();
				activeStatementList.erase(entry);
			}
			else
			{
				inactiveStatementList.erase(entry);
				cacheSize -= entry->size;
			}

			map.remove(entry->key);
		}
	}
}

void DsqlStatementCache::statementGoingInactive(Firebird::RefStrPtr& key)
{
	const auto entryPtr = map.get(key);

	if (!entryPtr)
		return;

	const auto entry = *entryPtr;

	fb_assert(entry->active);
	entry->active = false;
	entry->dsqlStatement->addRef();
	entry->size = entry->dsqlStatement->getSize();	// update size

	inactiveStatementList.splice(inactiveStatementList.end(), activeStatementList, entry);

	cacheSize += entry->size;

	if (cacheSize > maxCacheSize)
		shrink();
}

void DsqlStatementCache::purge(thread_db* tdbb, bool releaseLock)
{
	if (!isEmpty())
	{
		fb_assert(lock && lock->lck_logical == LCK_SW);

		for (auto& entry : activeStatementList)
		{
			entry.dsqlStatement->addRef();
			entry.dsqlStatement->resetCacheKey();
		}

		for (auto& entry : inactiveStatementList)
			entry.dsqlStatement->resetCacheKey();

		map.clear();
		activeStatementList.clear();
		inactiveStatementList.clear();

		cacheSize = 0;
	}

	if (!lock)
		return;

	if (releaseLock)
		LCK_release(tdbb, lock);
	else
	{
		ThreadStatusGuard tempStatus(tdbb);

		const bool ret = LCK_convert(tdbb, lock, LCK_SR, LCK_NO_WAIT); // never fails
		fb_assert(ret);
	}
}

void DsqlStatementCache::purgeAllAttachments(thread_db* tdbb)
{
	purge(tdbb, false);

	fb_assert(!lock || lock->lck_logical == LCK_SR);

	Lock tempLock(tdbb, 0, LCK_dsql_statement_cache);

	if (!LCK_lock(tdbb, &tempLock, LCK_PW, LCK_WAIT))	// notify others
		status_exception::raise(tdbb->tdbb_status_vector);

	LCK_release(tdbb, &tempLock);
}

void DsqlStatementCache::buildStatementKey(thread_db* tdbb, RefStrPtr& key, const string& text, USHORT clientDialect,
	bool isInternalRequest)
{
	const auto attachment = tdbb->getAttachment();

	const auto searchPathLen = std::accumulate(
		attachment->att_schema_search_path->begin(),
		attachment->att_schema_search_path->end(),
		0u,
		[](const auto& len, const auto& pathItem) {
			return len + pathItem.length() + 1u;
		}
	);

	const SSHORT charSetId = isInternalRequest ? CS_METADATA : attachment->att_charset;
	const int debugOptions = (int) attachment->getDebugOptions().getDsqlKeepBlr();

	key = FB_NEW_POOL(getPool()) RefString(getPool());
	key->resize(1 + sizeof(charSetId) + text.length() + 1 + searchPathLen + 1);

	char* p = key->begin();
	*p++ = (clientDialect << 2) | (int(isInternalRequest) << 1) | debugOptions;
	memcpy(p, &charSetId, sizeof(charSetId));
	p += sizeof(charSetId);
	memcpy(p, text.c_str(), text.length() + 1);
	p += text.length() + 1;

	for (const auto& pathItem : *attachment->att_schema_search_path)
	{
		memcpy(p, pathItem.c_str(), pathItem.length() + 1);
		p += pathItem.length() + 1;
	}

	*p = '\0';

	fb_assert(p + 1 == key->end());
}

void DsqlStatementCache::buildVerifyKey(thread_db* tdbb, string& key, bool isInternalRequest)
{
	key.clear();

	const auto attachment = tdbb->getAttachment();

	if (isInternalRequest || !attachment->att_user)
		return;

	const auto& roles = attachment->att_user->getGrantedRoles(tdbb);

	string roleStr;

	for (const auto& role : roles)
	{
		roleStr.printf("%d,%s,", int(role.length()), role.c_str());
		key += roleStr;
	}
}

void DsqlStatementCache::shrink()
{
#ifdef DSQL_STATEMENT_CACHE_DEBUG
	printf("DsqlStatementCache::shrink() - cacheSize: %u, maxCacheSize: %u\n\n", cacheSize, maxCacheSize);
#endif

	while (cacheSize > maxCacheSize && !inactiveStatementList.isEmpty())
	{
		const auto& front = inactiveStatementList.front();
		front.dsqlStatement->resetCacheKey();
		map.remove(front.key);
		cacheSize -= front.size;
		inactiveStatementList.erase(inactiveStatementList.begin());
	}

#ifdef DSQL_STATEMENT_CACHE_DEBUG
	dump();
#endif
}

void DsqlStatementCache::ensureLockIsCreated(thread_db* tdbb)
{
	if (!lock)
	{
		lock = FB_NEW_RPT(getPool(), 0) Lock(tdbb, 0, LCK_dsql_statement_cache, this, blockingAst);
		LCK_lock(tdbb, lock, LCK_SR, LCK_WAIT);	// never fails
	}
}

#ifdef DSQL_STATEMENT_CACHE_DEBUG
void DsqlStatementCache::dump()
{
	printf("DsqlStatementCache::dump() - cacheSize: %u, maxCacheSize: %u\n\n", cacheSize, maxCacheSize);

	printf("\tactive:\n");

	for (auto& entry : activeStatementList)
		printf("\t\tsize: %u; text: %s\n", entry.size, entry.dsqlStatement->getSqlText()->c_str());

	printf("\n\tinactive:\n");

	for (auto& entry : inactiveStatementList)
		printf("\t\tsize: %u; text: %s\n", entry.size, entry.dsqlStatement->getSqlText()->c_str());

	printf("\n");
}
#endif
