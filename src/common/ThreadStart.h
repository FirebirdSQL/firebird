/*
 *	PROGRAM:	JRD access method
 *	MODULE:		thd.h
 *	DESCRIPTION:	Thread support definitions
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
 *
 * 2002.10.28 Sean Leyne - Completed removal of obsolete "DGUX" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * Alex Peshkov
 */

#ifndef JRD_THREADSTART_H
#define JRD_THREADSTART_H

#include "../common/ThreadData.h"
#include "../common/classes/semaphore.h"

#ifdef WIN_NT
#include <windows.h>
#endif


// Thread priorities (may be ignored)

const int THREAD_high			= 1;
const int THREAD_medium_high	= 2;
const int THREAD_medium			= 3;
const int THREAD_medium_low		= 4;
const int THREAD_low			= 5;
const int THREAD_critical		= 6;


// Thread startup

// BRS 01/07/2004
// Hack due to a bug in mingw.
// The definition inside the thdd class should be replaced with the following one.
typedef THREAD_ENTRY_DECLARE ThreadEntryPoint(THREAD_ENTRY_PARAM);

#if defined(WIN_NT)
typedef DWORD ThreadId;
#elif defined(LINUX) && !defined(ANDROID) && !defined(LSB_BUILD)
#define USE_LWP_AS_THREAD_ID
typedef int ThreadId;
#elif defined(USE_POSIX_THREADS)
typedef pthread_t ThreadId;
#else
error - unknown ThreadId type
#endif

class Thread
{
public:
#ifdef WIN_NT
	typedef DWORD InternalId;
	typedef HANDLE Handle;
#endif
#ifdef USE_POSIX_THREADS
	typedef pthread_t Handle;
	typedef pthread_t InternalId;
#endif

	static Thread start(ThreadEntryPoint* routine, void* arg, int priority_arg, Handle* p_handle = NULL);
	static void waitForCompletion(Handle& handle);
	static void kill(Handle& handle);

	static ThreadId getId();

	static void sleep(unsigned milliseconds);
	static void yield();

	bool isCurrent();

	Thread()
	{
		memset(&internalId, 0, sizeof(internalId));
	}

private:
	Thread(InternalId iid)
		: internalId(iid)
	{ }

	InternalId internalId;
};

inline ThreadId getThreadId()
{
	return Thread::getId();
}


#ifndef USE_POSIX_THREADS
#define USE_FINI_SEM
#endif

template <typename TA, void (*cleanup) (TA) = nullptr>
class ThreadFinishSync
{
public:
	typedef void ThreadRoutine(TA);

	ThreadFinishSync(Firebird::MemoryPool& pool, ThreadRoutine* routine, int priority_arg = THREAD_medium)
		:
#ifdef USE_FINI_SEM
		  fini(pool),
#else
		  threadHandle(0),
#endif
		  threadRoutine(routine),
		  threadPriority(priority_arg),
		  closing(false)
	{ }

	void run(TA arg)
	{
		threadArg = arg;

		Thread::start(internalRun, this, threadPriority
#ifndef USE_FINI_SEM
					, &threadHandle
#endif
			);
	}

	bool tryWait()
	{
		if (closing)
		{
			waitForCompletion();
			return true;
		}
		return false;
	}

	void waitForCompletion()
	{
#ifdef USE_FINI_SEM
		fini.enter();
#else
		if (threadHandle)
		{
			Thread::waitForCompletion(threadHandle);
			threadHandle = 0;
		}
#endif
	}

private:
#ifdef USE_FINI_SEM
	Firebird::Semaphore fini;
#else
	Thread::Handle threadHandle;
#endif

	TA threadArg;
	ThreadRoutine* threadRoutine;
	int threadPriority;
	bool closing;

	static THREAD_ENTRY_DECLARE internalRun(THREAD_ENTRY_PARAM arg)
	{
		((ThreadFinishSync*) arg)->internalRun();
		return 0;
	}

	void internalRun()
	{
		try
		{
			threadRoutine(threadArg);
		}
		catch (const Firebird::Exception& ex)
		{
			threadArg->exceptionHandler(ex, threadRoutine);
		}

#ifdef USE_FINI_SEM
		try
		{
			fini.release();
		}
		catch (const Firebird::Exception& ex)
		{
			threadArg->exceptionHandler(ex, threadRoutine);
		}
#endif

		if (cleanup)
			cleanup(threadArg);
		closing = true;
	}
};

#endif // JRD_THREADSTART_H
