/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		event_proto.h
 *	DESCRIPTION:	Prototype Header file for event.cpp
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_EVENT_PROTO_H
#define JRD_EVENT_PROTO_H

#include "../common/classes/init.h"
#include "../common/classes/semaphore.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/RefCounted.h"
#include "../common/ThreadData.h"
#include "../jrd/event.h"
#include "../common/isc_s_proto.h"
#include "../common/config/config.h"


namespace Jrd {

class Attachment;

class EventManager final : public Firebird::GlobalStorage, public Firebird::IpcObject
{
	const int PID;

public:
	EventManager(const Firebird::string& id, const Firebird::Config* conf);
	~EventManager();

	static void init(Attachment*);

	void deleteSession(SLONG);

	SLONG queEvents(SLONG, USHORT, const UCHAR*, Firebird::IEventCallback*);
	void cancelEvents(SLONG);
	void postEvent(USHORT, const TEXT*, USHORT);
	void deliverEvents();

	bool initialize(Firebird::SharedMemoryBase*, bool);
	void mutexBug(int osErrorCode, const char* text);
	void exceptionHandler(const Firebird::Exception& ex, ThreadFinishSync<EventManager*>::ThreadRoutine* routine);

private:
	void acquire_shmem();
	frb* alloc_global(UCHAR type, ULONG length, bool recurse);
	void create_process();
	SLONG create_session();
	void delete_event(evnt*);
	void delete_process(SLONG);
	void delete_request(evt_req*);
	void delete_session(SLONG);
	void deliver();
	void deliver_request(evt_req*);
	void exit_handler(void *);
	evnt* find_event(USHORT, const TEXT*);
	void free_global(frb*);
	req_int* historical_interest(ses*, SLONG);
	void insert_tail(srq*, srq*);
	evnt* make_event(USHORT, const TEXT*);
	bool post_process(prb*);
	void probe_processes();
	void release_shmem();
	void remove_que(srq*);
	bool request_completed(evt_req*);
	void watcher_thread();
	void init_shared_file();

	static void watcher_thread(EventManager* eventMgr)
	{
		eventMgr->watcher_thread();
	}

	static void mutex_bugcheck(const TEXT*, int);
	static void punt(const TEXT*);

	prb* m_process;
	SLONG m_processOffset;

	const Firebird::string& m_dbId;
	const Firebird::Config* const m_config;
	Firebird::AutoPtr<Firebird::SharedMemory<evh> > m_sharedMemory;

	Firebird::Semaphore m_startupSemaphore;
	ThreadFinishSync<EventManager*> m_cleanupSync;

	bool m_sharedFileCreated;
	bool m_exiting;
};

} // namespace

#endif // JRD_EVENT_PROTO_H
