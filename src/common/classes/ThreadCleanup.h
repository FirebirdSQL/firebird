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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_THREAD_CLEANUP_H
#define CLASSES_THREAD_CLEANUP_H

#include "firebird.h"

#ifdef WIN_NT
#include <windows.h>
#else
#include "fb_pthread.h"
#endif

namespace Firebird {

class MemoryPool;

// Registers a callback that fires once for the current thread when it exits, using
// pthread_key_create's destructor on POSIX or Fiber-Local Storage on Windows (FLS is used
// instead of DllMain's DLL_THREAD_DETACH because its callback fires on thread exit for any
// thread that touched the slot, whether this code is linked into a DLL or statically into the
// host application - unlike DLL_THREAD_DETACH, which never fires at all when there is no
// separate DLL).
class ThreadCleanupCallback
{
public:
	typedef void (*Callback)(void*);

public:
	ThreadCleanupCallback(MemoryPool&, Callback callback, void* arg = nullptr);
	~ThreadCleanupCallback();

	ThreadCleanupCallback(const ThreadCleanupCallback&) = delete;
	ThreadCleanupCallback& operator=(const ThreadCleanupCallback&) = delete;

public:
	// Arms the current thread: callback(arg) runs once when it exits.
	void enable();

	// Disarms the current thread.
	void disable();

	// Public so platform-specific callback thunks can forward to it; not for general use.
	static void trampoline(void* value);

private:
	Callback callback;
	void* arg;

#ifdef WIN_NT
	DWORD flsIndex;
#else
	pthread_key_t key;
#endif
};

} // namespace Firebird

#endif // CLASSES_THREAD_CLEANUP_H
