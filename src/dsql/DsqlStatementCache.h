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

#ifndef DSQL_STATEMENT_CACHE_H
#define DSQL_STATEMENT_CACHE_H

///#define DSQL_STATEMENT_CACHE_DEBUG 1

#include "../common/classes/alloc.h"
#include "../common/classes/DoublyLinkedList.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/RefCounted.h"

namespace Firebird::Jrd {


class Attachment;
class DsqlStatement;
class Lock;
class thread_db;


class DsqlStatementCache final : public PermanentStorage
{
private:
	struct StatementEntry
	{
		explicit StatementEntry(MemoryPool& p)
			: verifyCache(p)
		{
		}

		StatementEntry(MemoryPool& p, StatementEntry&& o)
			: key(std::move(o.key)),
			  dsqlStatement(std::move(o.dsqlStatement)),
			  verifyCache(p, std::move(o.verifyCache)),
			  size(o.size),
			  active(o.active)
		{
		}

		StatementEntry(const StatementEntry&) = delete;
		StatementEntry& operator=(const StatementEntry&) = delete;

		RefStrPtr key;
		RefPtr<DsqlStatement> dsqlStatement;
		SortedObjectsArray<string> verifyCache;
		unsigned size = 0;
		bool active = true;
	};

	class RefStrPtrComparator
	{
	public:
		static bool greaterThan(const RefStrPtr& i1, const RefStrPtr& i2)
		{
			return *i1 > *i2;
		}
	};

public:
	explicit DsqlStatementCache(MemoryPool& o, Attachment* attachment);
	~DsqlStatementCache();

	DsqlStatementCache(const DsqlStatementCache&) = delete;
	DsqlStatementCache& operator=(const DsqlStatementCache&) = delete;

private:
	static int blockingAst(void* astObject);

public:
	bool isActive() const
	{
		return maxCacheSize > 0;
	}

	bool isEmpty() const
	{
		return activeStatementList.isEmpty() && inactiveStatementList.isEmpty();
	}

	RefPtr<DsqlStatement> getStatement(thread_db* tdbb, const string& text,
		USHORT clientDialect, bool isInternalRequest);

	void putStatement(thread_db* tdbb, const string& text, USHORT clientDialect, bool isInternalRequest,
		RefPtr<DsqlStatement> dsqlStatement);

	void removeStatement(thread_db* tdbb, DsqlStatement* statement);
	void statementGoingInactive(RefStrPtr& key);

	void purge(thread_db* tdbb, bool releaseLock);
	void purgeAllAttachments(thread_db* tdbb);

	void shutdown(thread_db* tdbb)
	{
		purge(tdbb, true);
	}

private:
	void buildStatementKey(thread_db* tdbb, RefStrPtr& key, const string& text,
		USHORT clientDialect, bool isInternalRequest);

	void buildVerifyKey(thread_db* tdbb, string& key, bool isInternalRequest);
	void shrink();
	void ensureLockIsCreated(thread_db* tdbb);

#ifdef DSQL_STATEMENT_CACHE_DEBUG
	void dump();
#endif

private:
	NonPooledMap<
		RefStrPtr,
		DoublyLinkedList<StatementEntry>::Iterator,
		RefStrPtrComparator
	> map;
	DoublyLinkedList<StatementEntry> activeStatementList;
	DoublyLinkedList<StatementEntry> inactiveStatementList;
	AutoPtr<Lock> lock;
	unsigned maxCacheSize = 0;
	unsigned cacheSize = 0;
};


}	// namespace Firebird::Jrd

#endif // DSQL_STATEMENT_CACHE_H
