/*
 *	PROGRAM:		Client/Server Common Code
 *	MODULE:			locks.cpp
 *	DESCRIPTION:	Win32 Mutex support compatible with
 *					old OS versions (like Windows 95)
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
 *  Copyright (c) 2003 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef __MINGW32__
#define _WIN32_WINNT 0x0403
#endif

#include "firebird.h"

#include "../../common/classes/locks.h"
#include "../../common/ThreadStart.h"
#include <mutex>


namespace Firebird {

#if defined(WIN_NT)

void Spinlock::init()
{
	SetCriticalSectionSpinCount(&spinlock, 4000);
}

#else //posix mutex

pthread_mutexattr_t Mutex::attr;

void Mutex::initMutexes()
{
	static std::once_flag onceFlag;
	std::call_once(onceFlag, [] {
		// Throw exceptions on errors, but they will not be processed in init
		// (first constructor). Better logging facilities are required here.
		int rc = pthread_mutexattr_init(&attr);
		if (rc < 0)
			system_call_failed::raise("pthread_mutexattr_init", rc);

		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc < 0)
			system_call_failed::raise("pthread_mutexattr_settype", rc);
	});
}

#endif

} // namespace Firebird
