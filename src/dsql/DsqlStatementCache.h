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

namespace Jrd {


class Attachment;
class DsqlStatement;
class Lock;
class thread_db;


class DsqlStatementCache final : public Firebird::PermanentStorage
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

		Firebird::RefStrPtr key;
		Firebird::RefPtr<DsqlStatement> dsqlStatement;
		Firebird::SortedObjectsArray<Firebird::string> verifyCache;
		unsigned size = 0;
		bool active = true;
	};

	class RefStrPtrComparator
	{
	public:
		static bool greaterThan(const Firebird::RefStrPtr& i1, const Firebird::RefStrPtr& i2)
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

	Firebird::RefPtr<DsqlStatement> getStatement(thread_db* tdbb, const Firebird::string& text,
		USHORT clientDialect, bool isInternalRequest);

	void putStatement(thread_db* tdbb, const Firebird::string& text, USHORT clientDialect, bool isInternalRequest,
		Firebird::RefPtr<DsqlStatement> dsqlStatement);

	void statementGoingInactive(Firebird::RefStrPtr& key);

	void purge(thread_db* tdbb);
	void purgeAllAttachments(thread_db* tdbb);

private:
	void buildStatementKey(thread_db* tdbb, Firebird::RefStrPtr& key, const Firebird::string& text,
		USHORT clientDialect, bool isInternalRequest);

	void buildVerifyKey(thread_db* tdbb, Firebird::string& key, bool isInternalRequest);

	void shrink();

#ifdef DSQL_STATEMENT_CACHE_DEBUG
	void dump();
#endif

private:
	Firebird::NonPooledMap<
		Firebird::RefStrPtr,
		Firebird::DoublyLinkedList<StatementEntry>::Iterator,
		RefStrPtrComparator
	> map;
	Firebird::DoublyLinkedList<StatementEntry> activeStatementList;
	Firebird::DoublyLinkedList<StatementEntry> inactiveStatementList;
	Firebird::AutoPtr<Lock> lock;
	unsigned maxCacheSize = 0;
	unsigned cacheSize = 0;
};


}	// namespace Jrd

#endif // DSQL_STATEMENT_CACHE_H
