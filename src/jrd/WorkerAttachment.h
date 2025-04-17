/*
 *	PROGRAM:	Firebird Database Engine
 *	MODULE:		WorkerAttachment.h
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

#ifndef JRD_WORKER_ATTACHMENT_H
#define JRD_WORKER_ATTACHMENT_H

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/condition.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/init.h"
#include "../common/classes/locks.h"
#include "../jrd/Attachment.h"
#include "../jrd/jrd.h"
#include "../jrd/status.h"


namespace Jrd
{

class WorkerAttachment;

class WorkerStableAttachment : public SysStableAttachment
{
public:
	static WorkerStableAttachment* create(FbStatusVector* status, Database* dbb, JProvider* provider,
		WorkerAttachment* workers);

	void fini();

protected:
	virtual void doOnIdleTimer(Firebird::TimerImpl* timer);

private:
	explicit WorkerStableAttachment(FbStatusVector* status, Jrd::Attachment* att, WorkerAttachment* workers);
	virtual ~WorkerStableAttachment();

	WorkerAttachment* m_workers;
};


class WorkerContextHolder : public Jrd::DatabaseContextHolder, public Jrd::Attachment::SyncGuard
{
public:
	WorkerContextHolder(thread_db* tdbb, const char* f) :
		DatabaseContextHolder(tdbb),
		Jrd::Attachment::SyncGuard(tdbb->getAttachment(), f)
	{
	}

private:
	// copying is prohibited
	WorkerContextHolder(const WorkerContextHolder&);
	WorkerContextHolder& operator=(const WorkerContextHolder&);
};


class WorkerAttachment
{
	friend class WorkerStableAttachment;

public:
	explicit WorkerAttachment();

	static Jrd::StableAttachmentPart* getAttachment(FbStatusVector* status, Jrd::Database* dbb);
	static void releaseAttachment(FbStatusVector* status, Jrd::StableAttachmentPart* sAtt);

	static bool detachIdle(Jrd::StableAttachmentPart* sAtt);

	static void incUserAtts(const Firebird::PathName& dbname);
	static void decUserAtts(const Firebird::PathName& dbname);

	static void shutdown();
	static void shutdownDbb(Jrd::Database* dbb);

private:
	static WorkerAttachment* getByName(const Firebird::PathName& dbname);
	Jrd::StableAttachmentPart* doAttach(FbStatusVector* status, Jrd::Database* dbb);
	static void doDetach(FbStatusVector* status, Jrd::StableAttachmentPart* sAtt);
	void clear(bool checkRefs);

	void incWorkers();
	void decWorkers();
	void waitForWorkers();


	typedef Firebird::GenericMap<Firebird::Pair<Firebird::Left<Firebird::PathName, WorkerAttachment*> > >
		MapDbIdToWorkAtts;

	static Firebird::GlobalPtr<Firebird::Mutex> m_mapMutex;
	static Firebird::GlobalPtr<MapDbIdToWorkAtts> m_map;
	static bool m_shutdown;

	Firebird::Mutex m_mutex;
	Firebird::HalfStaticArray<Jrd::StableAttachmentPart*, 8> m_idleAtts;
	Firebird::SortedArray<Jrd::StableAttachmentPart*,
		Firebird::InlineStorage<Jrd::StableAttachmentPart*, 8> > m_activeAtts;

	// count of regular user attachments, used with non-shared Database
	int m_cntUserAtts = 0;

	// count of internal worker attachments, used with shared Database
	int m_cntWorkers = 0;

	// used to wait for "no internal workers" condition
	Firebird::Condition m_noWorkers;
};

} // namespace Jrd

#endif // JRD_WORKER_ATTACHMENT_H
