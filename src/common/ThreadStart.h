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

#include <utility>

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
constexpr static ThreadId INVALID_ID = 0;
#elif defined(LINUX) && !defined(ANDROID) && !defined(LSB_BUILD)
#define USE_LWP_AS_THREAD_ID
typedef int ThreadId;
constexpr static ThreadId INVALID_ID = 0;
#elif defined(USE_POSIX_THREADS)
typedef pthread_t ThreadId;
constexpr static ThreadId INVALID_ID = 0;
#else
error - unknown ThreadId type
#endif

class Thread
{
public:
#ifdef WIN_NT
	typedef HANDLE Handle;
	constexpr static Handle INVALID_HANDLE = INVALID_HANDLE_VALUE;
#endif
#ifdef USE_POSIX_THREADS
	typedef pthread_t Handle;
	constexpr static Handle INVALID_HANDLE = 0;
#endif

	class Mark
	{
		friend class Thread;

	public:
		Mark(const Thread& t);

		bool operator==(const Mark&) const noexcept;

	private:
		Mark() = delete;
		Mark(const Mark&) = delete;
		Mark(const Mark&&) = delete;
		Mark& operator=(const Mark&) = delete;
		Mark& operator=(const Mark&&) = delete;

#ifdef WIN_NT
		ThreadId m_Id;
#endif
#ifdef USE_POSIX_THREADS
		Thread::Handle m_handle;
#endif
	};

	friend class Mark;

	static void start(ThreadEntryPoint* routine, void* arg, int priority_arg, Thread* pThread = nullptr);

#ifdef WIN_NT
	bool waitFor(unsigned milliseconds) const noexcept;
#endif
	void waitForCompletion();
	void kill() noexcept;

	static ThreadId getCurrentThreadId();

	Handle getHandle() const noexcept
	{
		return m_handle;
	}

	static void sleep(unsigned milliseconds);
	static void yield();

	bool isCurrent() const noexcept;

	bool isValid() const noexcept
	{
		return m_handle != INVALID_HANDLE;
	}

	// invoked from dtor - ignore syscall's error
	void detach(bool close = true) noexcept
	{
		if (isValid() && close)
		{
#ifdef WIN_NT
			CloseHandle(m_handle);
#endif
#ifdef USE_POSIX_THREADS
			pthread_detach(m_handle);
#endif
		}
		m_handle = INVALID_HANDLE;
	}

	Thread() noexcept { }

	Thread(Thread&& other) noexcept
	{
		moveFrom(std::move(other));
	}

	~Thread() noexcept
	{
		detach();
	}

	Thread& operator= (Thread&& other) noexcept
	{
		detach();
		moveFrom(std::move(other));
		return *this;
	}

private:
#ifdef WIN_NT
	Thread(Handle handle, ThreadId id) noexcept
		: m_handle(handle), m_Id(id)
	{ }
#endif

	void moveFrom(Thread&& other) noexcept
	{
		fb_assert(!isValid());

		m_handle = other.m_handle;
		other.m_handle = INVALID_HANDLE;
#ifdef WIN_NT
		m_Id = other.m_Id;
		other.m_Id = INVALID_ID;
#endif
	}

	Handle m_handle = INVALID_HANDLE;
#ifdef WIN_NT
	ThreadId m_Id = INVALID_ID;
#endif
};


inline bool Thread::Mark::operator==(const Thread::Mark& other) const noexcept
{
#ifdef WIN_NT
	return m_Id == other.m_Id;
#endif
#ifdef USE_POSIX_THREADS
	return pthread_equal(m_handle, other.m_handle);
#endif
}


inline Thread::Mark::Mark(const Thread& t)
#ifdef WIN_NT
	: m_Id(t.m_Id)
#endif
#ifdef USE_POSIX_THREADS
	: m_handle(t.m_handle)
#endif
{ }


inline ThreadId getThreadId()
{
	return Thread::getCurrentThreadId();
}


template <typename TA, void (*cleanup) (TA) = nullptr>
class ThreadFinishSync
{
public:
	typedef void ThreadRoutine(TA);

	ThreadFinishSync(Firebird::MemoryPool& pool, ThreadRoutine* routine, int priority_arg = THREAD_medium)
		: threadRoutine(routine),
		  threadPriority(priority_arg),
		  closing(false)
	{ }

	void run(TA arg)
	{
		threadArg = arg;
		Thread::start(internalRun, this, threadPriority, &thread);
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
		thread.waitForCompletion();
	}

private:
	Thread thread;
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

		if (cleanup)
			cleanup(threadArg);
		closing = true;
	}
};

#endif // JRD_THREADSTART_H
