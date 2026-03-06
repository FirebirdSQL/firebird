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

#include "../common/classes/alloc.h"
#include "../jrd/GarbageCollector.h"
#include "../jrd/tra.h"

using namespace Firebird::Jrd;
using namespace Firebird;

namespace Firebird::Jrd {


void GarbageCollector::RelationData::clear()
{
	m_pages.clear();
}


TraNumber GarbageCollector::RelationData::findPage(const ULONG pageno, const TraNumber tranid)
{
	PageTranMap::Accessor pages(&m_pages);
	if (!pages.locate(pageno))
		return MAX_TRA_NUMBER;

	// hvlad: this routine could be guarded by shared sync - therefore comparison
	// and assignment below should be atomic operation. But we don't require
	// exact precision here.
	if (pages.current().tranid > tranid)
		pages.current().tranid = tranid;

	return pages.current().tranid;
}

TraNumber GarbageCollector::RelationData::addPage(const ULONG pageno, const TraNumber tranid)
{
	TraNumber findTran = findPage(pageno, tranid);
	if (findTran != MAX_TRA_NUMBER)
		return findTran;

	m_pages.add(PageTran(pageno, tranid));
	return tranid;
}


void GarbageCollector::RelationData::swept(const TraNumber oldest_snapshot, PageBitmap** bm)
{
	PageTranMap::Accessor pages(&m_pages);

	bool next = pages.getFirst();
	while (next)
	{
		if (pages.current().tranid < oldest_snapshot)
		{
			if (bm)
			{
				PBM_SET(&m_pool, bm, pages.current().pageno);
			}
			next = pages.fastRemove();
		}
		else
			next = pages.getNext();
	}
}


GarbageCollector::~GarbageCollector()
{
	SyncLockGuard exGuard(&m_sync, SYNC_EXCLUSIVE, "GarbageCollector::~GarbageCollector");

	for (FB_SIZE_T pos = 0; pos < m_relations.getCount(); pos++)
	{
		RelationData* relData = m_relations[pos];

		Sync sync(&relData->m_sync, "GarbageCollector::~GarbageCollector");
		sync.lock(SYNC_EXCLUSIVE);

		m_relations[pos] = NULL;
		sync.unlock();
		delete relData;
	}

	m_relations.clear();
}


TraNumber GarbageCollector::addPage(const USHORT relID, const ULONG pageno, const TraNumber tranid)
{
	Sync syncGC(&m_sync, "GarbageCollector::addPage");
	RelationData* relData = getRelData(syncGC, relID, true);

	SyncLockGuard syncData(&relData->m_sync, SYNC_SHARED, "GarbageCollector::addPage");
	TraNumber minTraID = relData->findPage(pageno, tranid);
	if (minTraID != MAX_TRA_NUMBER)
		return minTraID;

	syncData.unlock();
	syncData.lock(SYNC_EXCLUSIVE, "GarbageCollector::addPage");
	syncGC.unlock();

	return relData->addPage(pageno, tranid);
}


PageBitmap* GarbageCollector::getPages(const TraNumber oldest_snapshot, USHORT &relID)
{
	SyncLockGuard shGuard(&m_sync, SYNC_SHARED, "GarbageCollector::getPages");

	if (m_relations.isEmpty())
	{
		m_nextRelID = 0;
		return NULL;
	}

	FB_SIZE_T pos;
	if (!m_relations.find(m_nextRelID, pos) && (pos == m_relations.getCount()))
		pos = 0;

	for (; pos < m_relations.getCount(); pos++)
	{
		RelationData* relData = m_relations[pos];
		SyncLockGuard syncData(&relData->m_sync, SYNC_EXCLUSIVE, "GarbageCollector::getPages");

		PageBitmap* bm = NULL;
		relData->swept(oldest_snapshot, &bm);

		if (bm)
		{
			relID = relData->getRelID();
			m_nextRelID = relID + 1;
			return bm;
		}
	}

	m_nextRelID = 0;
	return NULL;
}


void GarbageCollector::removeRelation(const USHORT relID)
{
	Sync syncGC(&m_sync, "GarbageCollector::removeRelation");
	syncGC.lock(SYNC_EXCLUSIVE);

	FB_SIZE_T pos;
	if (!m_relations.find(relID, pos))
		return;

	RelationData* relData = m_relations[pos];
	Sync syncData(&relData->m_sync, "GarbageCollector::removeRelation");
	syncData.lock(SYNC_EXCLUSIVE);

	m_relations.remove(pos);
	syncGC.unlock();

	syncData.unlock();
	delete relData;
}


void GarbageCollector::sweptRelation(const TraNumber oldest_snapshot, const USHORT relID)
{
	Sync syncGC(&m_sync, "GarbageCollector::sweptRelation");

	RelationData* relData = getRelData(syncGC, relID, false);
	if (relData)
	{
		SyncLockGuard syncData(&relData->m_sync, SYNC_EXCLUSIVE, "GarbageCollector::sweptRelation");

		syncGC.unlock();
		relData->swept(oldest_snapshot);
	}
}


GarbageCollector::RelationData* GarbageCollector::getRelData(Sync &sync, const USHORT relID,
	bool allowCreate)
{
	FB_SIZE_T pos;

	sync.lock(SYNC_SHARED);
	if (!m_relations.find(relID, pos))
	{
		if (!allowCreate)
			return NULL;

		sync.unlock();
		sync.lock(SYNC_EXCLUSIVE);
		if (!m_relations.find(relID, pos))
		{
			m_relations.insert(pos, FB_NEW_POOL(m_pool) RelationData(m_pool, relID));
		}
		sync.downgrade(SYNC_SHARED);
	}

	return m_relations[pos];
}


} // namespace Jrd
