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

#ifndef COMMON_IPC_NAMED_SIGNAL_H
#define COMMON_IPC_NAMED_SIGNAL_H

#include "firebird.h"
#include "../classes/fb_string.h"
#include "../StatusArg.h"
#include "../utils_proto.h"
#include <chrono>
#ifdef WIN_NT
#include <windows.h>
#else
#include "fb_pthread.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#endif

#ifdef ANDROID
#error This is not supported in Android.
#endif

namespace Firebird {


// An event in Windows, a semaphore in POSIX.
class IpcNamedSignal
{
public:
	explicit IpcNamedSignal(const string& name);
	~IpcNamedSignal();

	IpcNamedSignal(const IpcNamedSignal&) = delete;
	IpcNamedSignal& operator=(const IpcNamedSignal&) = delete;

private:
	static string fixName(const string& name);

#ifndef WIN_NT
public:
	static void remove(const string& name)
	{
		if (sem_unlink(fixName(name).c_str()) != 0)
			fb_assert(false);
	}
#endif

public:
	void reset();
	void signal();
	bool wait(std::chrono::microseconds timeout);

private:
#ifdef WIN_NT
	HANDLE handle = INVALID_HANDLE_VALUE;
#else
	sem_t* handle = nullptr;
#endif
};


#ifdef WIN_NT


inline IpcNamedSignal::IpcNamedSignal(const string& name)
{
	TEXT objectName[BUFFER_TINY];
	strncpy(objectName, name.c_str(), sizeof(objectName));

	if (!fb_utils::private_kernel_object_name(objectName, sizeof(objectName)))
		(Arg::Gds(isc_random) << "private_kernel_object_name failed").raise();

	handle = CreateEvent(nullptr, TRUE, FALSE, objectName);

	if (!handle)
		system_error::raise("CreateEvent");

	SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
}

inline IpcNamedSignal::~IpcNamedSignal()
{
	if (handle)
		CloseHandle(handle);
}

inline string IpcNamedSignal::fixName(const string& name)
{
	return name;
}

inline void IpcNamedSignal::reset()
{
	ResetEvent(handle);
}

inline void IpcNamedSignal::signal()
{
	if (!SetEvent(handle))
		system_error::raise("SetEvent");
}

inline bool IpcNamedSignal::wait(std::chrono::microseconds timeout)
{
	const auto wait = WaitForSingleObject(handle, timeout.count() / 1'000);

	if (wait == WAIT_OBJECT_0)
		return true;
	else if (wait == WAIT_TIMEOUT)
		return false;
	else
		system_error::raise("WaitForSingleObject");
}


#else


inline IpcNamedSignal::IpcNamedSignal(const string& name)
{
	handle = sem_open(fixName(name).c_str(), O_CREAT, S_IRUSR | S_IWUSR, 0);

	if (handle == SEM_FAILED)
		system_error::raise("sem_open");
}

inline IpcNamedSignal::~IpcNamedSignal()
{
	if (handle)
		sem_close(handle);
}

inline string IpcNamedSignal::fixName(const string& name)
{
	if (name[0] == '/')
		return name;

	return "/" + name;
}

inline void IpcNamedSignal::reset()
{
	int semVal;

	while (sem_getvalue(handle, &semVal) == 0 && semVal > 0)
		sem_trywait(handle);
}

inline void IpcNamedSignal::signal()
{
	if (sem_post(handle) != 0)
		system_error::raise("sem_post");
}

inline bool IpcNamedSignal::wait(std::chrono::microseconds timeout)
{
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

	constexpr SINT64 BILLION = 1'000'000'000;
	const SINT64 nanos =
		std::chrono::seconds(timer.tv_sec).count() + timer.tv_nsec + std::chrono::nanoseconds(timeout).count();
	timer.tv_sec = nanos / BILLION;
	timer.tv_nsec = nanos % BILLION;

#ifdef HAVE_SEM_TIMEDWAIT
	const auto wait = sem_timedwait(handle, &timer);
#else
	const auto wait = sem_timedwait_fallback(handle, &timer);
#endif

	if (wait == 0)
		return true;
	else if (errno == ETIMEDOUT)
		return false;
	else
		system_error::raise("sem_timedwait");
}


#endif


} // namespace Firebird

#endif // COMMON_IPC_NAMED_SIGNAL_H
