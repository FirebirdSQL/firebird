/*
 *	PROGRAM:	Common Access Method
 *	MODULE:		init.cpp
 *	DESCRIPTION:	InstanceControl - class to control global ctors/dtors
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
 *  Copyright (c) 2008 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "init.h"
#include "alloc.h"
#include "auto.h"
#include "../common/SimpleStatusVector.h"
#include "../common/dllinst.h"
#include "../common/os/os_utils.h"

#ifdef WIN_NT
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

// Setting this define helps (with AV at exit time) detect globals
// with destructors, declared not using InstanceControl.
// The reason for AV is that process memory pool (from where globals should allocate memory)
// is destroyed in atexit(), before destructors are called. Therefore each delete
// operator in destructor will cause AV.
#undef DEBUG_INIT

static bool dontCleanup = false;

namespace
{
#ifdef DEV_BUILD
	void cleanError(const Firebird::Exception* e)
	{
		if (e)
		{
			// This is done to be able to look at status in debugger
			Firebird::StaticStatusVector status;
			e->stuffException(status);
		}

		// we do not have big choice in error reporting when running global destructors
		abort();
	}
#else
	void cleanError(const Firebird::Exception*) { }
#endif

	// This helps initialize globals, needed before regular ctors run
	int initDone = 0;

#ifdef HAVE_PTHREAD_ATFORK
	void child(void)
	{
		// turn off dtors execution in forked process
		initDone = 2;
	}
#endif

	void allClean()
	{
		if (initDone != 1)
		{
			return;
		}
		initDone = 2;

#ifdef WIN_NT
		if (Firebird::bDllProcessExiting)
			dontCleanup = true;
#endif
		if (dontCleanup)
			return;

#ifdef DEBUG_GDS_ALLOC
		Firebird::AutoPtr<FILE> file;

		{	// scope
			char name[PATH_MAX];

			if (os_utils::getCurrentModulePath(name, sizeof(name)))
				strncat(name, ".memdebug.log", sizeof(name) - 1);
			else
				strcpy(name, "memdebug.log");

			file = os_utils::fopen(name, "w+t");
		}
#endif	// DEBUG_GDS_ALLOC

		Firebird::InstanceControl::destructors();

		if (dontCleanup)
			return;

		try
		{
			Firebird::StaticMutex::release();
		}
		catch (...)
		{
			cleanError(NULL);
		}

		try
		{
#ifdef DEBUG_GDS_ALLOC
			// In Debug mode - this will report all memory leaks
			if (file)
			{
				getDefaultMemoryPool()->print_contents(file,
					Firebird::MemoryPool::PRINT_USED_ONLY | Firebird::MemoryPool::PRINT_RECURSIVE);
				file = NULL;
			}
#endif
			Firebird::MemoryPool::cleanupDefaultPool();
		}
		catch (...)
		{
			cleanError(NULL);
		}
	}

#ifndef DEBUG_INIT

	// This instance ensures dtors run when program exits
	Firebird::Cleanup global(allClean);

#endif //DEBUG_INIT

	void init()
	{
		// This initialization code can't use mutex to protect itself
		// cause among other it's preparing mutexes to work. But as long
		// as only one thread is used to initialize globals (which is a case
		// for all known in year 2009 systems), it's safe to use it if we
		// have at least one global (not static in function) GlobalPtr<>
		// variable. When later "static GlobalPtr<>" variables in functions
		// are constructed (which may happen in parallel in different threads),
		// races are prevented by StaticMutex::mutex.

		if (initDone != 0)
		{
			return;
		}

		Firebird::Mutex::initMutexes();
		Firebird::MemoryPool::initDefaultPool();
		Firebird::StaticMutex::create();

#ifdef DEBUG_INIT
		atexit(allClean);
#endif //DEBUG_INIT

		initDone = 1;
#ifdef HAVE_PTHREAD_ATFORK
		int ret = pthread_atfork(NULL, NULL, child);
		(void) ret;
#endif

		Firebird::MemoryPool::contextPoolInit();
	}

	Firebird::InstanceControl::InstanceList* instanceList = 0;
	FPTR_VOID gdsCleanup = 0;
	FPTR_VOID gdsShutdown = 0;
}


namespace Firebird
{
	InstanceControl::InstanceControl()
	{
		// Initialize required subsystems, including static mutex
		init();
	}

	InstanceControl::InstanceList::InstanceList(DtorPriority p)
		: priority(p)
	{
		MutexLockGuard guard(*StaticMutex::mutex, "InstanceControl::InstanceList::InstanceList");
		next = instanceList;
		prev = nullptr;
		if (instanceList)
			instanceList->prev = this;
		instanceList = this;
	}

	InstanceControl::InstanceList::~InstanceList()
	{
		fb_assert(next == nullptr);
		fb_assert(prev == nullptr);
	}

	void InstanceControl::InstanceList::remove()
	{
		MutexLockGuard guard(*StaticMutex::mutex, FB_FUNCTION);
		unlist();
	}

	void InstanceControl::InstanceList::unlist()
	{
		if (instanceList == this)
			instanceList = next;

		if (next)
			next->prev = this->prev;

		if (prev)
			prev->next = this->next;

		prev = nullptr;
		next = nullptr;
	}

	void InstanceControl::destructors()
	{
		// Call fb_shutdown() if needed
		if (gdsShutdown)
		{
			try
			{
				gdsShutdown();
			}
			catch (const Firebird::Exception& e)
			{
				cleanError(&e);
			}
		}

		// Call gds__cleanup() if present
		if (gdsCleanup)
		{
			try
			{
				gdsCleanup();
			}
			catch (const Firebird::Exception& e)
			{
				cleanError(&e);
			}
		}

		InstanceControl::InstanceList::destructors();
	}


	void InstanceControl::InstanceList::destructors()
	{
		// Destroy global objects
		DtorPriority currentPriority = STARTING_PRIORITY, nextPriority = currentPriority;

		do
		{
			currentPriority = nextPriority;

			for (InstanceList* i = instanceList; i && !dontCleanup; i = i->next)
			{
				if (i->priority == currentPriority)
				{
					try
					{
						i->dtor();
					}
					catch (const Firebird::Exception& e)
					{
						cleanError(&e);
					}
				}
				else if (i->priority > currentPriority)
				{
					if (nextPriority == currentPriority || i->priority < nextPriority)
					{
						nextPriority = i->priority;
					}
				}
			}
		} while (nextPriority != currentPriority);


		while (instanceList)
		{
			InstanceList* item = instanceList;
			item->unlist();
			delete item;
		}
	}

	void InstanceControl::registerGdsCleanup(FPTR_VOID cleanup)
	{
		fb_assert(!gdsCleanup || !cleanup || gdsCleanup == cleanup);
		gdsCleanup = cleanup;
	}

	void InstanceControl::registerShutdown(FPTR_VOID shutdown)
	{
		fb_assert(!gdsShutdown || !shutdown);
		gdsShutdown = shutdown;
	}

	void InstanceControl::cancelCleanup()
	{
		dontCleanup = true;
	}

	namespace StaticMutex
	{
		Firebird::Mutex* mutex = NULL;

		void create()
		{
			static char place[sizeof(Firebird::Mutex) + FB_ALIGNMENT];
			mutex = new((void*) FB_ALIGN(place, FB_ALIGNMENT)) Firebird::Mutex;
		}

		void release()
		{
			mutex->~Mutex();
		}
	}
}
