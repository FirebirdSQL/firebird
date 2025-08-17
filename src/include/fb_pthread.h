/*
 *	PROGRAM:	Firebird RDBMS posix definitions
 *	MODULE:		fb_pthread.h
 *	DESCRIPTION:	Defines appropriate macros before including pthread.h
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2012 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef INCLUDE_FB_PTHREAD_H
#define INCLUDE_FB_PTHREAD_H

#include "firebird.h"

#if defined(LINUX) && (!defined(__USE_GNU))
#define __USE_GNU 1	// required on this OS to have required for us stuff declared
#endif // LINUX		// should be defined before include <pthread.h> - AP 2009

#include <pthread.h>

#ifndef HAVE_PTHREAD_MUTEX_TIMEDLOCK

#include <time.h>
#include <errno.h>

inline int pthread_mutex_timedlock_fallback(pthread_mutex_t* mutex, const timespec* timeout)
{
	timespec current_time, sleep_time;
	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = 10000000; // 10ms sleep between attempts

	do
	{
		int result = pthread_mutex_trylock(mutex);

		if (result == 0)
			return 0; // Successfully acquired lock

		if (result != EBUSY)
			return result; // Some other error

		clock_gettime(CLOCK_REALTIME, &current_time);

		if (current_time.tv_sec > timeout->tv_sec ||
			(current_time.tv_sec == timeout->tv_sec &&
			current_time.tv_nsec >= timeout->tv_nsec))
		{
			return ETIMEDOUT;
		}

		nanosleep(&sleep_time, nullptr);
	} while(true);
}

#endif	// !HAVE_PTHREAD_MUTEX_TIMEDLOCK

#endif // INCLUDE_FB_PTHREAD_H
