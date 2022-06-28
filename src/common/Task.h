/*
 *	PROGRAM:	Firebird Database Engine
 *	MODULE:		Task.h
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

#ifndef COMMON_TASK_H
#define COMMON_TASK_H

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/locks.h"
#include "../common/classes/semaphore.h"
#include "../common/ThreadStart.h"

namespace Firebird
{

class Worker;
class Coordinator;
class WorkerThread;

// Task (probably big one), contains parameters, could break whole task by 
// smaller items (WorkItem), handle items, track common running state, track 
// results and error happens.
class Task
{
public:
	Task() {};
	virtual ~Task() {};

	// task item to handle
	class WorkItem
	{
	public:
		WorkItem(Task* task) :
		  m_task(task)
		{}

		virtual ~WorkItem() {}

		Task*	m_task;
	};

	// task item handler
	virtual bool handler(WorkItem&) = 0;
	virtual bool getWorkItem(WorkItem**) = 0;
	virtual bool getResult(IStatus* status) = 0;

	// evaluate task complexity and recommend number of parallel workers
	virtual int getMaxWorkers() { return 1; }
};

// Worker: handle work items, optionally uses separate thread
class Worker
{
public:
	Worker(Coordinator* coordinator) :
	  m_coordinator(coordinator),
	  m_thread(NULL),
	  m_task(NULL),
	  m_state(IDLE)
	{
	}

	virtual ~Worker() {}

	void setTask(Task* task)
	{
		m_task = task;
		m_state = READY;
	}

	bool work(WorkerThread* thd);

	bool isIdle() const	{ return m_state == IDLE; };
	bool waitFor(int timeout = -1);

protected:
	enum STATE {IDLE, READY, WORKING};

	Coordinator* const m_coordinator; // set in constructor, not changed
	WorkerThread* m_thread;
	Task* m_task;
	STATE m_state;
};

// Accept Task(s) to handle, creates and assigns Workers to work on task(s),
// bind Workers to Threads, synchronize task completion and get results.
class Coordinator
{
public:
	Coordinator(MemoryPool* pool) :
		m_pool(pool),
		m_workers(*m_pool),
		m_idleWorkers(*m_pool),
		m_activeWorkers(*m_pool),
		m_idleThreads(*m_pool),
		m_activeThreads(*m_pool)
	{}

	~Coordinator();

	void runSync(Task*);

private:
	struct WorkerAndThd
	{
		WorkerAndThd() :
			worker(NULL),
			thread(NULL)
		{}

		WorkerAndThd(Worker* w, WorkerThread* t) :
			worker(w),
			thread(t)
		{}

		Worker* worker;
		WorkerThread* thread;
	};

	// determine how many workers needed, allocate max possible number
	// of workers, make it all idle, return number of allocated workers
	int setupWorkers(int count);
	Worker* getWorker();
	void releaseWorker(Worker*);

	WorkerThread* getThread();
	void releaseThread(WorkerThread*);

	MemoryPool* m_pool;
	Mutex m_mutex;
	HalfStaticArray<Worker*, 8> m_workers;
	HalfStaticArray<Worker*, 8> m_idleWorkers;
	HalfStaticArray<Worker*, 8> m_activeWorkers;
	// todo: move to thread pool
	HalfStaticArray<WorkerThread*, 8> m_idleThreads;
	HalfStaticArray<WorkerThread*, 8> m_activeThreads;
};


class WorkerThread
{
public:
	enum STATE {STARTING, IDLE, RUNNING, STOPPING, SHUTDOWN};


	~WorkerThread()
	{
		shutdown(true);

#ifdef WIN_NT
		if (m_thdHandle != INVALID_HANDLE_VALUE)
			CloseHandle(m_thdHandle);
#endif
	}

	static WorkerThread* start(Coordinator*);

	void runWorker(Worker*);
	bool waitForState(STATE state, int timeout);
	void shutdown(bool wait);

	STATE getState() const { return m_state; }

private:
	WorkerThread(Coordinator* coordinator) :
		m_coordinator(coordinator),
		m_worker(NULL),
		m_state(STARTING)
	{}

	static THREAD_ENTRY_DECLARE workerThreadRoutine(THREAD_ENTRY_PARAM);
	int threadRoutine();

	Coordinator* const m_coordinator;
	Worker* m_worker;
	Semaphore m_waitSem;		// idle thread waits on this semaphore to start work or go out
	Semaphore m_signalSem;	// semaphore is released when thread going idle
	STATE m_state;
	Thread::Handle m_thdHandle;
};

} // namespace Jrd

#endif // COMMON_TASK_H
