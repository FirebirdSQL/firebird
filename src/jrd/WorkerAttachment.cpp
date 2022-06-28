/*
 *	PROGRAM:	Firebird Database Engine
 *	MODULE:		WorkerAttachment.cpp
 *	DESCRIPTION:	Parallel task execution support
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2019 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "../jrd/WorkerAttachment.h"

#include "../common/config/config.h"
#include "../common/isc_proto.h"
#include "../common/utils_proto.h"
#include "../common/StatusArg.h"
#include "../common/classes/ClumpletWriter.h"
#include "../jrd/jrd.h"
#include "../jrd/ini_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/status.h"


using namespace Firebird;

namespace Jrd {


const unsigned WORKER_IDLE_TIMEOUT = 60;	// 1 minute

/// class WorkerStableAttachment

WorkerStableAttachment::WorkerStableAttachment(FbStatusVector* status, Jrd::Attachment* attachment) :
	SysStableAttachment(attachment)
{
	UserId user;
	user.setUserName("<Worker>");
	// user.usr_flags = USR_owner; // need owner privs ??

	attachment->att_user = FB_NEW_POOL(*attachment->att_pool) UserId(*attachment->att_pool, user);
	attachment->setStable(this);

	BackgroundContextHolder tdbb(attachment->att_database, attachment, status, FB_FUNCTION);

	LCK_init(tdbb, LCK_OWNER_attachment);
	INI_init(tdbb);
	INI_init2(tdbb);
	PAG_header(tdbb, true);
	PAG_attachment_id(tdbb);
	TRA_init(attachment);
	Monitoring::publishAttachment(tdbb);

	initDone();
}

WorkerStableAttachment::~WorkerStableAttachment()
{
	fini();
}

WorkerStableAttachment* WorkerStableAttachment::create(FbStatusVector* status, Jrd::Database* dbb)
{
	Attachment* attachment = NULL;
	try
	{
		attachment = Attachment::create(dbb, NULL);
		attachment->att_filename = dbb->dbb_filename;
		attachment->att_flags |= ATT_worker;

		WorkerStableAttachment* sAtt = FB_NEW WorkerStableAttachment(status, attachment);
		return sAtt;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	if (attachment)
		Attachment::destroy(attachment);

	return NULL;
}

void WorkerStableAttachment::doOnIdleTimer(Firebird::TimerImpl* timer)
{
	WorkerAttachment::detachIdle(this);
}

void WorkerStableAttachment::fini()
{
	Attachment* attachment = NULL;
	{
		AttSyncLockGuard guard(*getSync(), FB_FUNCTION);

		attachment = getHandle();
		if (!attachment)
			return;

		Database* dbb = attachment->att_database;

		FbLocalStatus status_vector;
		BackgroundContextHolder tdbb(dbb, attachment, &status_vector, FB_FUNCTION);

		Monitoring::cleanupAttachment(tdbb);
		attachment->releaseLocks(tdbb);
		LCK_fini(tdbb, LCK_OWNER_attachment);

		attachment->releaseRelations(tdbb);
	}

	destroy(attachment);
}

/// class WorkerAttachment

GlobalPtr<Mutex> WorkerAttachment::m_mapMutex;
GlobalPtr<WorkerAttachment::MapDbIdToWorkAtts> WorkerAttachment::m_map;
bool WorkerAttachment::m_shutdown = false;

WorkerAttachment::WorkerAttachment() :
	m_idleAtts(*getDefaultMemoryPool()),
	m_activeAtts(*getDefaultMemoryPool()),
	m_cntUserAtts(0)
{
}

void WorkerAttachment::incUserAtts(const PathName& dbname)
{
	if (Config::getServerMode() == MODE_SUPER)
		return;

	WorkerAttachment* item = getByName(dbname);
	if (item)
	{
		MutexLockGuard guard(item->m_mutex, FB_FUNCTION);
		item->m_cntUserAtts++;
	}
}

void WorkerAttachment::decUserAtts(const PathName& dbname)
{
	if (Config::getServerMode() == MODE_SUPER)
		return;

	WorkerAttachment* item = getByName(dbname);
	if (item)
	{
		bool tryClear = false;
		{
			MutexLockGuard guard(item->m_mutex, FB_FUNCTION);
			item->m_cntUserAtts--;
			tryClear = (item->m_cntUserAtts == 0 && item->m_activeAtts.isEmpty());
		}

		if (tryClear)
			item->clear(true);
	}
}

WorkerAttachment* WorkerAttachment::getByName(const PathName& dbname)
{
	if (m_shutdown)
		return NULL;

	WorkerAttachment* ret = NULL;
	MutexLockGuard guard(m_mapMutex, FB_FUNCTION);

	if (m_shutdown)
		return NULL;

	if (!m_map->get(dbname, ret))
	{
		ret = new WorkerAttachment();
		m_map->put(dbname, ret);
	}

	return ret;
}

void WorkerAttachment::shutdown()
{
	if (m_shutdown)
		return;

	MutexLockGuard guard(m_mapMutex, FB_FUNCTION);

	if (m_shutdown)
		return;

	m_shutdown = true;

	MapDbIdToWorkAtts::Accessor acc(&m_map);
	if (!acc.getFirst())
		return;

	do
	{
		WorkerAttachment* item = acc.current()->second;
		item->clear(false);
		delete item;
	} 
	while (acc.getNext());

	m_map->clear();
}


void WorkerAttachment::shutdownDbb(Database* dbb)
{
	if (Config::getServerMode() != MODE_SUPER)
		return;

	MutexLockGuard guard(m_mapMutex, FB_FUNCTION);

	WorkerAttachment* item = NULL;
	if (!m_map->get(dbb->dbb_filename, item))
		return;

	item->clear(false);
}

StableAttachmentPart* WorkerAttachment::getAttachment(FbStatusVector* status, Database* dbb)
{
	// There should be no locked attachment.
#ifdef _DEBUG
	thread_db* tdbb = JRD_get_thread_data();
	if (tdbb)
	{
		Attachment* att = tdbb->getAttachment();
		if (att)
		{
			const StableAttachmentPart::Sync* sync = att->getStable()->getSync();
			fb_assert(!sync || !sync->locked());
		}
	}
#endif

	Arg::Gds(isc_shutdown).copyTo(status);

	WorkerAttachment* item = getByName(dbb->dbb_filename);
	if (!item)
		return NULL;

	MutexLockGuard guard(item->m_mutex, FB_FUNCTION);

	if (m_shutdown)
		return NULL;

	FB_SIZE_T maxWorkers = Config::getMaxParallelWorkers();
	if (maxWorkers <= 0)
		maxWorkers = MAX_ULONG;

	StableAttachmentPart* sAtt = NULL;
	while (!item->m_idleAtts.isEmpty())
	{
		if (m_shutdown)
			return NULL;

		sAtt = item->m_idleAtts.pop();
		if (sAtt->getHandle())
			break;

		// idle worker attachment was unexpectedly deleted, clean up and try next one
		MutexUnlockGuard unlock(item->m_mutex, FB_FUNCTION);

		FbLocalStatus local;
		doDetach(&local, sAtt);
		sAtt = NULL;
	}

	if (!sAtt)
	{
		if (item->m_activeAtts.getCount() >= maxWorkers)
		{
			(Arg::Gds(isc_random) << Arg::Str("No enough free worker attachments")).copyTo(status);
			return NULL;
		}

		MutexUnlockGuard unlock(item->m_mutex, FB_FUNCTION);
		status->init();
		sAtt = doAttach(status, dbb);
		if (!sAtt)
		{
			// log error ?
			if (!m_shutdown)
				iscLogStatus("Failed to create worker attachment\n", status);

			return NULL;
		}
	}

	Attachment* att = NULL;
	{
		MutexUnlockGuard unlock(item->m_mutex, FB_FUNCTION);
		AttSyncLockGuard guard(*sAtt->getSync(), FB_FUNCTION);

		att = sAtt->getHandle();
		fb_assert(!att || (att->att_flags & ATT_worker));

		if (att)
		{
			att->att_use_count++;
			att->setupIdleTimer(true);
		}
	}

	if (att)
		item->m_activeAtts.add(sAtt);

	return sAtt;
}

void WorkerAttachment::releaseAttachment(FbStatusVector* status, StableAttachmentPart* sAtt)
{
	status->init();
	WorkerAttachment* item = NULL;
	{
		AttSyncLockGuard attGuard(*sAtt->getSync(), FB_FUNCTION);

		Attachment* att = sAtt->getHandle();
		if (!att)
			return;

		att->att_use_count--;
		att->setupIdleTimer(false);

		item = getByName(att->att_database->dbb_filename);
	}

	const bool detach = (m_shutdown || (item == NULL));
	bool tryClear = false;

	if (item)
	{
		MutexLockGuard guard(item->m_mutex, FB_FUNCTION);

		FB_SIZE_T pos;
		if (item->m_activeAtts.find(sAtt, pos))
			item->m_activeAtts.remove(pos);

		if (!m_shutdown)
		{
			item->m_idleAtts.push(sAtt);
			tryClear = (item->m_cntUserAtts == 0 && item->m_activeAtts.isEmpty());
		}
	}

	if (detach)
		doDetach(status, sAtt);

	if (tryClear && (Config::getServerMode() != MODE_SUPER))
		item->clear(true);
}

void WorkerAttachment::clear(bool checkRefs)
{
	HalfStaticArray<Jrd::StableAttachmentPart*, 8> toDetach(*getDefaultMemoryPool());

	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		if (checkRefs && (m_cntUserAtts != 0 || !m_activeAtts.isEmpty()))
			return;

		toDetach.assign(m_idleAtts);

		m_idleAtts.clear();
		m_activeAtts.clear(); // should be released by regular JRD shutdown
	}

	FbLocalStatus status;
	while (!toDetach.isEmpty())
	{
		StableAttachmentPart* sAtt = toDetach.pop();

		doDetach(&status, sAtt);
	}
}

bool WorkerAttachment::detachIdle(StableAttachmentPart* sAtt)
{
	WorkerAttachment* item = NULL;
	{	// scope
		AttSyncLockGuard attGuard(sAtt->getSync(), FB_FUNCTION);

		Attachment* att = sAtt->getHandle();
		if (!att || att->att_use_count > 0)
			return false;

		item = getByName(att->att_database->dbb_filename);
	}

	if (item)
	{
		MutexLockGuard guard(item->m_mutex, FB_FUNCTION);

		FB_SIZE_T pos;
		if (item->m_idleAtts.find(sAtt, pos))
			item->m_idleAtts.remove(pos);
		else
			return false;
	}

	FbLocalStatus status;
	doDetach(&status, sAtt);

	return true;
}

StableAttachmentPart* WorkerAttachment::doAttach(FbStatusVector* status, Database* dbb)
{
	StableAttachmentPart* sAtt = NULL;

	if (Config::getServerMode() == MODE_SUPER)
		sAtt = WorkerStableAttachment::create(status, dbb);
	else
	{
		ClumpletWriter dpb(ClumpletReader::Tagged, MAX_DPB_SIZE, isc_dpb_version1);
		dpb.insertString(isc_dpb_trusted_auth, DBA_USER_NAME);
		dpb.insertInt(isc_dpb_worker_attach, 1);

		AutoPlugin<JProvider> jInstance(JProvider::getInstance());

		//jInstance->setDbCryptCallback(&status, tdbb->getAttachment()->att_crypt_callback);

		JAttachment* jAtt = jInstance->attachDatabase(status, dbb->dbb_filename.c_str(),
			dpb.getBufferLength(), dpb.getBuffer());

		if (!(status->getState() & IStatus::STATE_ERRORS))
			sAtt = jAtt->getStable();
	}

	if (sAtt)
	{
		sAtt->addRef(); // !!
		sAtt->getHandle()->setIdleTimeout(WORKER_IDLE_TIMEOUT);
	}

	return sAtt;
}

void WorkerAttachment::doDetach(FbStatusVector* status, StableAttachmentPart* sAtt)
{
	status->init();

	// if (att->att_flags & ATT_system)
	if (Config::getServerMode() == MODE_SUPER)
	{
		WorkerStableAttachment* wrk = reinterpret_cast<WorkerStableAttachment*>(sAtt);
		wrk->fini();
	}
	else
	{
		JAttachment* jAtt = sAtt->getInterface();
		jAtt->detach(status);
	}
	sAtt->release(); // !!
}

} // namespace Jrd
