/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			MasterImplementation.cpp
 *	DESCRIPTION:	Main firebird interface, used to get other interfaces.
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "firebird/Interface.h"

#include <string.h>

#include "../yvalve/MasterImplementation.h"
#include "../common/classes/init.h"
#include "../common/MsgMetadata.h"
#include "../common/StatusHolder.h"
#include "../yvalve/PluginManager.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/fb_pair.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/semaphore.h"
#include "../common/isc_proto.h"
#include "../common/ThreadStart.h"
#include "../common/utils_proto.h"
#include "../common/dllinst.h"
#include "ibase.h"
#include "../yvalve/utl_proto.h"

using namespace Firebird;

namespace Why {

//
// getStatus()
//

class UserStatus FB_FINAL : public Firebird::DisposeIface<Firebird::BaseStatus<UserStatus> >
{
public:
	// IStatus implementation
	void dispose()
	{
		delete this;
	}
};

IStatus* MasterImplementation::getStatus()
{
	return FB_NEW UserStatus;
}


//
// getDispatcher()
//

IProvider* MasterImplementation::getDispatcher()
{
	IProvider* dispatcher = FB_NEW Dispatcher;
	dispatcher->addRef();
	return dispatcher;
}

//
// getDtc()
//

Static<Dtc> MasterImplementation::dtc;

Dtc* MasterImplementation::getDtc()
{
	return &dtc;
}

//
// getPluginManager()
//

IPluginManager* MasterImplementation::getPluginManager()
{
	static Static<PluginManager> manager;

	return &manager;
}

IMetadataBuilder* MasterImplementation::getMetadataBuilder(CheckStatusWrapper* status, unsigned fieldCount)
{
	try
	{
		IMetadataBuilder* bld = FB_NEW MetadataBuilder(fieldCount);
		bld->addRef();
		return bld;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}

/***
IDebug* MasterImplementation::getDebug()
{
#ifdef DEV_BUILD
	return getImpDebug();
#else
	return NULL;
#endif
}
***/

int MasterImplementation::serverMode(int mode)
{
	static int currentMode = -1;
	if (mode >= 0)
		currentMode = mode;
	return currentMode;
}

} // namespace Why

//
// timer
//

namespace Why {

Thread::Handle timerThreadHandle = 0;

FB_BOOLEAN MasterImplementation::getProcessExiting()
{
#ifdef WIN_NT
	if (timerThreadHandle && WaitForSingleObject(timerThreadHandle, 0) != WAIT_TIMEOUT)
		return true;
#endif

	return false;
}


namespace {

// Protects timerQueue array
GlobalPtr<Mutex> timerAccess;
// Protects from races during module unload process
// Should be taken before timerAccess
GlobalPtr<Mutex> timerPause;

GlobalPtr<Semaphore> timerWakeup;
GlobalPtr<Semaphore> timerCleanup;
// Should use atomic flag for thread stop to provide correct membar
AtomicCounter stopTimerThread(0);

struct TimerEntry
{
	ISC_UINT64 fireTime;
	ITimer* timer;

	static const ISC_UINT64 generate(const TimerEntry& item) { return item.fireTime; }
	static THREAD_ENTRY_DECLARE timeThread(THREAD_ENTRY_PARAM);

	static void init()
	{
		Thread::start(timeThread, 0, THREAD_high, &timerThreadHandle);
	}

	static void cleanup();
};

typedef SortedArray<TimerEntry, InlineStorage<TimerEntry, 64>, ISC_UINT64, TimerEntry> TimerQueue;
GlobalPtr<TimerQueue> timerQueue;

InitMutex<TimerEntry> timerHolder("TimerHolder");

void TimerEntry::cleanup()
{
	{
		MutexLockGuard guard(timerAccess, FB_FUNCTION);

		stopTimerThread.setValue(1);
		timerWakeup->release();
	}
	timerCleanup->tryEnter(5);
	Thread::waitForCompletion(timerThreadHandle);

	while (timerQueue->hasData())
	{
		ITimer* timer = NULL;
		{
			MutexLockGuard guard(timerAccess, FB_FUNCTION);

			TimerEntry* e = timerQueue->end();
			timer = (--e)->timer;
			timerQueue->remove(e);
		}
		timer->release();
	}
}

ISC_UINT64 curTime()
{
	ISC_UINT64 rc = fb_utils::query_performance_counter();
	rc *= 1000000;
	rc /= fb_utils::query_performance_frequency();
	return rc;
}

TimerEntry* getTimer(ITimer* timer)
{
	fb_assert(timerAccess->locked());

	for (unsigned int i = 0; i < timerQueue->getCount(); ++i)
	{
		TimerEntry& e(timerQueue->operator[](i));
		if (e.timer == timer)
		{
			return &e;
		}
	}

	return NULL;
}

THREAD_ENTRY_DECLARE TimerEntry::timeThread(THREAD_ENTRY_PARAM)
{
#ifdef WIN_NT
	// The timer thread could unload plugins. Plugins almost always linked with
	// dispatcher (fbclient.dll) thus, when plugin unloaded it decrement usage
	// count of fbclient.dll. If application unload fbclient.dll not calling
	// fb_shutdown, then last unloaded plugin will finally unload fbclient.dll
	// and the code that is currently running, leading to AV.
	// To prevent such scenario we increment usage count of fbclient.dll and
	// will decrement it in safe way at the end of the timer thread.

	char buff[MAX_PATH];
	GetModuleFileName(hDllInst, buff, sizeof(buff));
	HMODULE hDll = LoadLibrary(buff);
#endif

	while (stopTimerThread.value() == 0
#ifdef WIN_NT
			&& Firebird::dDllUnloadTID == 0
#endif
			)
	{
		ISC_UINT64 microSeconds = 0;

		{
			MutexLockGuard pauseGuard(timerPause, FB_FUNCTION);
			MutexLockGuard guard(timerAccess, FB_FUNCTION);

			const ISC_UINT64 cur = curTime();

			if (timerQueue->getCount() > 0)
			{
				TimerEntry e(timerQueue->operator[](0));

				if (e.fireTime <= cur)
				{
					timerQueue->remove((FB_SIZE_T) 0);

					// We must leave timerAccess mutex here to avoid deadlocks
					MutexUnlockGuard ug(timerAccess, FB_FUNCTION);

					e.timer->handler();
					e.timer->release();
					continue;
				}
				else
				{
					microSeconds = e.fireTime - cur;
				}
			}
		}

		if (microSeconds)
		{
			timerWakeup->tryEnter(0, microSeconds / 1000);
		}
		else
		{
			timerWakeup->enter();
		}
	}

	timerCleanup->release();

#ifdef WIN_NT
	if (Firebird::dDllUnloadTID)
	{
		// fb_shutdown is called as result of FreeLibrary, not by application.
		// Sooner of all we are the last user of fbclient.dll, and code will be
		// physically unloaded as result of FreeLibrary() call.
		FreeLibraryAndExitThread(hDll, 0);
	}
	else
		FreeLibrary(hDll);	// It is safe to decrement usage count here
#endif

	return 0;
}

} // namespace

class TimerImplementation :
	public AutoIface<ITimerControlImpl<TimerImplementation, CheckStatusWrapper> >
{
public:
	// ITimerControl implementation
	void start(CheckStatusWrapper* status, ITimer* timer, ISC_UINT64 microSeconds)
	{
		try
		{
			MutexLockGuard guard(timerAccess, FB_FUNCTION);

			if (stopTimerThread.value() != 0)
			{
				// Ignore an attempt to start timer - anyway thread to make it fire is down

				// We must leave timerAccess mutex here to avoid deadlocks
				MutexUnlockGuard ug(timerAccess, FB_FUNCTION);

				timer->addRef();
				timer->release();
				return;
			}

			timerHolder.init();

			TimerEntry* curTimer = getTimer(timer);
			if (!curTimer)
			{
				TimerEntry newTimer;

				newTimer.timer = timer;
				newTimer.fireTime = curTime() + microSeconds;
				timerQueue->add(newTimer);
				timer->addRef();
			}
			else
			{
				curTimer->fireTime = curTime() + microSeconds;
			}

			timerWakeup->release();
		}
		catch (const Firebird::Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void stop(CheckStatusWrapper* status, ITimer* timer)
	{
		try
		{
			MutexLockGuard guard(timerAccess, FB_FUNCTION);

			TimerEntry* curTimer = getTimer(timer);
			if (curTimer)
			{
				curTimer->timer->release();
				timerQueue->remove(curTimer);
			}
		}
		catch (const Firebird::Exception& ex)
		{
			ex.stuffException(status);
		}
	}
};

ITimerControl* MasterImplementation::getTimerControl()
{
	static Static<TimerImplementation> timer;

	return &timer;
}

void shutdownTimers()
{
	stopTimerThread.setValue(1);
	timerHolder.cleanup();
}

Mutex& pauseTimer()
{
	return timerPause;
}

} // namespace Why


//
// Util (misc calls)
//

namespace Why {

	extern UtilInterface utilInterface;		// Implemented in utl.cpp

	IUtil* MasterImplementation::getUtilInterface()
	{
		return &utilInterface;
	}

} // namespace Why


//
// ConfigManager (config info access)
//

namespace Firebird {

	extern IConfigManager* iConfigManager;

} // namespace Firebird

namespace Why {

	IConfigManager* MasterImplementation::getConfigManager()
	{
		return Firebird::iConfigManager;
	}

} // namespace Why


//
// get master
//

namespace Firebird
{
	typedef IMaster* IMasterPtr;

	extern "C" IMasterPtr API_ROUTINE fb_get_master_interface()
	{
		static Static<Why::MasterImplementation> instance;
		return &instance;
	}
}
