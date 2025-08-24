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
 *  Copyright (c) 2025 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_IPC_SHARED_SIGNAL_H
#define COMMON_IPC_SHARED_SIGNAL_H

#include "firebird.h"

#ifdef WIN_NT
#error This is not supported in Windows.
#endif

#ifndef PTHREAD_PROCESS_SHARED
#error Your system must support PTHREAD_PROCESS_SHARED to use firebird.
#endif

#include "../StatusArg.h"
#include "../common/os/mac_utils.h"
#include "../utils_proto.h"
#include <chrono>
#include "fb_pthread.h"

namespace Firebird {


class IpcSharedSignal
{
public:
	explicit IpcSharedSignal();
	~IpcSharedSignal();

	IpcSharedSignal(const IpcSharedSignal&) = delete;
	IpcSharedSignal& operator=(const IpcSharedSignal&) = delete;

public:
	void reset();
	void signal();
	bool wait(std::chrono::microseconds timeout);

private:
	pthread_mutex_t mutex[1];
	pthread_cond_t cond[1];
	std::atomic_uint8_t flag = 0;
};


inline IpcSharedSignal::IpcSharedSignal()
{
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;

	if (pthread_mutexattr_init(&mattr) != 0)
		system_error::raise("pthread_mutexattr_init");

	if (pthread_condattr_init(&cattr) != 0)
		system_error::raise("pthread_condattr_init");

	if (!isSandboxed())
	{
		if (pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED) != 0)
			system_error::raise("pthread_mutexattr_setpshared");

		if (pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED) != 0)
			system_error::raise("pthread_condattr_setpshared");
	}

	if (pthread_mutex_init(mutex, &mattr) != 0)
		system_error::raise("pthread_mutex_init");

	if (pthread_cond_init(cond, &cattr) != 0)
		system_error::raise("pthread_cond_init");

	if (pthread_mutexattr_destroy(&mattr) != 0)
		system_error::raise("pthread_mutexattr_destroy");

	if (pthread_condattr_destroy(&cattr) != 0)
		system_error::raise("pthread_condattr_destroy");
}

inline IpcSharedSignal::~IpcSharedSignal()
{
	pthread_mutex_destroy(mutex);
	pthread_cond_destroy(cond);
}

inline void IpcSharedSignal::reset()
{
	flag.store(0, std::memory_order_release);
}

inline void IpcSharedSignal::signal()
{
	if (pthread_mutex_lock(mutex) != 0)
		system_error::raise("pthread_mutex_lock");

	flag.store(1, std::memory_order_release);

	const auto ret = pthread_cond_broadcast(cond);

	if (pthread_mutex_unlock(mutex) != 0)
		system_error::raise("pthread_mutex_unlock");

	if (ret != 0)
		system_error::raise("pthread_cond_broadcast");
}

inline bool IpcSharedSignal::wait(std::chrono::microseconds timeout)
{
	if (flag.load(std::memory_order_acquire) == 1)
		return true;

	timespec timer;

#if defined(HAVE_CLOCK_GETTIME)
	clock_gettime(CLOCK_REALTIME, &timer);
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval tp;
	GETTIMEOFDAY(&tp);
	timer.tv_sec = tp.tv_sec;
	timer.tv_nsec = tp.tv_usec * 1'000;
#else
	struct timeb time_buffer;
	ftime(&time_buffer);
	timer.tv_sec = time_buffer.time;
	timer.tv_nsec = time_buffer.millitm * 1'000'000;
#endif

	bool ret = true;

	if (pthread_mutex_lock(mutex) != 0)
		system_error::raise("pthread_mutex_lock");

	do
	{
		if (flag.load(std::memory_order_acquire) == 1)
		{
			ret = true;
			break;
		}

		// The Posix pthread_cond_wait & pthread_cond_timedwait calls
		// atomically release the mutex and start a wait.
		// The mutex is reacquired before the call returns.
		const auto wait = pthread_cond_timedwait(cond, mutex, &timer);

		if (wait == ETIMEDOUT)
		{
			// The timer expired - see if the event occurred and return
			ret = flag.load(std::memory_order_acquire) == 1;
			break;
		}
	} while (true);

	if (pthread_mutex_unlock(mutex) != 0)
		system_error::raise("pthread_mutex_unlock");

	return ret;
}


} // namespace Firebird

#endif // COMMON_IPC_SHARED_SIGNAL_H
