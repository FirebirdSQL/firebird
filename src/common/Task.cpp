/*
 *	PROGRAM:	Firebird Database Engine
 *	MODULE:		Task.cpp
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

#include "../common/Task.h"

namespace Firebird {

/// class WorkerThread

THREAD_ENTRY_DECLARE WorkerThread::workerThreadRoutine(THREAD_ENTRY_PARAM arg)
{
	WorkerThread* thd = static_cast<WorkerThread*> (arg);
	return (THREAD_ENTRY_RETURN)(IPTR) thd->threadRoutine();
}

WorkerThread* WorkerThread::start(Coordinator* coordinator)
{
	AutoPtr<WorkerThread> thd = FB_NEW WorkerThread(coordinator);

	Thread::start(workerThreadRoutine, thd, THREAD_medium, &thd->m_thdHandle);

	return thd.release();
}

int WorkerThread::threadRoutine()
{
	m_state = IDLE;
	m_signalSem.release();

	while(m_state != STOPPING)
	{
		m_waitSem.enter();

		if (m_state == RUNNING && m_worker != NULL)
		{
			m_worker->work(this);
			m_worker = NULL;
		}

		if (m_state == RUNNING)
		{
			m_state = IDLE;
			m_signalSem.release();
		}

		if (m_state == STOPPING)
			break;
	}
	return 0;
}

void WorkerThread::runWorker(Worker* worker)
{
	fb_assert(m_worker == NULL);
	fb_assert(m_state == IDLE);

	m_worker = worker;
	m_state = RUNNING;
	m_waitSem.release();
}

bool WorkerThread::waitForState(STATE state, int timeout)
{
	while (m_state != state) // || m_state == old_state - consume old signals ?
	{
		if (timeout >= 0)
		{
			m_signalSem.tryEnter(0, timeout);
			break;
		}
		else
			m_signalSem.enter();
	}

	return (m_state == state);
}

void WorkerThread::shutdown(bool wait)
{
	if (m_state == SHUTDOWN)
		return;

	m_state = STOPPING;
	m_waitSem.release();

	if (wait)
	{
		Thread::waitForCompletion(m_thdHandle);
		m_state = SHUTDOWN;
	}
}



/// class Worker

bool Worker::work(WorkerThread* thd)
{
	fb_assert(m_state == READY);

	m_state = WORKING;
	m_thread = thd;
	Task::WorkItem* workItem = NULL;
	while (true)
	{
		if (m_thread && m_thread->getState() != WorkerThread::RUNNING)
			break;

		if (!m_task->getWorkItem(&workItem))
			break;

		if (!m_task->handler(*workItem))
			break;
	}

	m_thread = NULL;
	m_state = IDLE;
	return true;
}

bool Worker::waitFor(int timeout)
{
	if (m_state == IDLE)
		return true;

	if (m_thread == NULL)
		return false;

	m_thread->waitForState(WorkerThread::IDLE, timeout);
	return (m_state == IDLE);
}


/// class Coordinator

Coordinator::~Coordinator()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	for (WorkerThread** p = m_activeThreads.begin(); p < m_activeThreads.end(); p++)
		(*p)->shutdown(false);

	while (!m_activeThreads.isEmpty())
	{
		WorkerThread* thd = m_activeThreads.pop();
		{
			MutexUnlockGuard unlock(m_mutex, FB_FUNCTION);
			thd->shutdown(true);
		}
		delete thd;
	}

	while (!m_idleThreads.isEmpty())
	{
		WorkerThread* thd = m_idleThreads.pop();
		{
			MutexUnlockGuard unlock(m_mutex, FB_FUNCTION);
			thd->shutdown(true);
		}
		delete thd;
	}

	while (!m_activeWorkers.isEmpty())
	{
		Worker* w = m_activeWorkers.back();

		MutexUnlockGuard unlock(m_mutex, FB_FUNCTION);
		w->waitFor(-1);
	}

	while (!m_idleWorkers.isEmpty())
	{
		Worker* w = m_idleWorkers.pop();
		delete w;
	}
}

void Coordinator::runSync(Task* task)
{
	int cntWorkers = setupWorkers(task->getMaxWorkers());
	if (cntWorkers < 1)
		return;

	HalfStaticArray<WorkerAndThd, 8> taskWorkers(*m_pool, cntWorkers);

	Worker* syncWorker = getWorker();
	taskWorkers.push(WorkerAndThd(syncWorker, NULL));

	for (int i = 1; i < cntWorkers; i++)
	{
		WorkerThread* thd = getThread();
		if (thd)
		{
			Worker* w = getWorker();
			taskWorkers.push(WorkerAndThd(w, thd));

			w->setTask(task);
			thd->runWorker(w);
		}
	}

	// run syncronously
	syncWorker->setTask(task);
	syncWorker->work(NULL);

	// wait for all workes
	for (int i = 0; i < cntWorkers; i++)
	{
		WorkerAndThd& wt = taskWorkers[i];
		if (wt.thread)
		{
			if (!wt.worker->isIdle())
				wt.thread->waitForState(WorkerThread::IDLE, -1);

			releaseThread(wt.thread);
		}
		releaseWorker(wt.worker);
	}
}

Worker* Coordinator::getWorker()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	Worker* w = NULL;
	if (!m_idleWorkers.isEmpty())
	{
		w = m_idleWorkers.pop();
		m_activeWorkers.push(w);
	}
	return w;
}

void Coordinator::releaseWorker(Worker* w)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	FB_SIZE_T pos;
	if (m_activeWorkers.find(w, pos))
	{
		m_activeWorkers.remove(pos);
		m_idleWorkers.push(w);
	}
	fb_assert(m_idleWorkers.find(w, pos));
}

int Coordinator::setupWorkers(int count)
{
	// TODO adjust count

	for (int i = m_workers.getCount(); i < count; i++)
	{
		Worker* w = FB_NEW_POOL(*m_pool) Worker(this);
		m_workers.add(w);
		m_idleWorkers.push(w);
	}

	return count;
}

WorkerThread* Coordinator::getThread()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	WorkerThread* thd = NULL;

	if (!m_idleThreads.isEmpty())
		thd = m_idleThreads.pop();
	else
	{
		thd = WorkerThread::start(this);
		if (thd)
			thd->waitForState(WorkerThread::IDLE, -1);
	}

	if (thd)
	{
		fb_assert(thd->getState() == WorkerThread::IDLE);
		m_activeThreads.push(thd);
	}
	return thd;
}

void Coordinator::releaseThread(WorkerThread* thd)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	const WorkerThread::STATE thdState = thd->getState();
	if (thdState != WorkerThread::IDLE)
	{
		fb_assert(thdState == WorkerThread::STOPPING || thdState == WorkerThread::SHUTDOWN);
		return;
	}

	FB_SIZE_T pos;
	if (m_activeThreads.find(thd, pos))
	{
		m_activeThreads.remove(pos);
		m_idleThreads.push(thd);
	}
	else
	{
		fb_assert(false);

		if (!m_idleThreads.find(thd, pos))
			m_idleThreads.push(thd);
	}
}


} // namespace Jrd
