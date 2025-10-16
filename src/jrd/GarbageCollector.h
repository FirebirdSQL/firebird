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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_GARBAGE_COLLECTOR_H
#define JRD_GARBAGE_COLLECTOR_H

#include "firebird.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/SyncObject.h"
#include "../jrd/sbm.h"


namespace Jrd {

class Database;
class Savepoint;

class GarbageCollector
{
public:
	GarbageCollector(MemoryPool& p, Database* dbb)
	  : m_pool(p), m_relations(m_pool), m_nextRelID(0)
	{}

	~GarbageCollector();

	TraNumber addPage(const USHORT relID, const ULONG pageno, const TraNumber tranid);
	PageBitmap* getPages(const TraNumber oldest_snapshot, USHORT &relID);
	void removeRelation(const USHORT relID);
	void sweptRelation(const TraNumber oldest_snapshot, const USHORT relID);

private:
	struct PageTran
	{
		PageTran() :
			pageno(0),
			tranid(0)
		{}

		PageTran(const ULONG _pageno, const TraNumber _tranid) :
			pageno(_pageno),
			tranid(_tranid)
		{}

		ULONG pageno;
		TraNumber tranid;

		static const ULONG& generate(const void*, const PageTran& item)
		{
			return item.pageno;
		}
	};
	typedef Firebird::BePlusTree<PageTran, ULONG, PageTran> PageTranMap;


	class RelationData
	{
	public:
		explicit RelationData(MemoryPool& p, USHORT relID)
			: m_pool(p), m_pages(p), m_relID(relID)
		{}

		~RelationData()
		{
			clear();
		}

		TraNumber addPage(const ULONG pageno, const TraNumber tranid);
		TraNumber findPage(const ULONG pageno, const TraNumber tranid);
		void swept(const TraNumber oldest_snapshot, PageBitmap** bm = NULL);

		USHORT getRelID() const
		{
			return m_relID;
		}

		static inline const USHORT generate(const RelationData* item)
		{
			return item->m_relID;
		}

		void clear();

		Firebird::MemoryPool& m_pool;
		Firebird::SyncObject m_sync;
		PageTranMap m_pages;
		USHORT m_relID;
	};

	typedef	Firebird::SortedArray<
				RelationData*,
				Firebird::EmptyStorage<RelationData*>,
				USHORT,
				RelationData> RelGarbageArray;

	RelationData* getRelData(Firebird::Sync& sync, const USHORT relID, bool allowCreate);

	Firebird::MemoryPool& m_pool;
	Firebird::SyncObject m_sync;
	RelGarbageArray m_relations;
	USHORT m_nextRelID;
};

} // namespace Jrd

#endif	// JRD_GARBAGE_COLLECTOR_H
