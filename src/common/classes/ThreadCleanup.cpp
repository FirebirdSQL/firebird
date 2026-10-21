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

#include "firebird.h"
#include "../common/classes/ThreadCleanup.h"
#include "fb_exception.h"
#include "../yvalve/gds_proto.h"

#ifdef WIN_NT
#include <windows.h>
#endif

namespace Firebird {

#ifdef WIN_NT

static void WINAPI flsCallbackThunk(PVOID value)
{
	ThreadCleanupCallback::trampoline(value);
}

ThreadCleanupCallback::ThreadCleanupCallback(MemoryPool&, Callback aCallback, void* aArg)
	: callback(aCallback),
	  arg(aArg)
{
	flsIndex = FlsAlloc(flsCallbackThunk);

	if (flsIndex == FLS_OUT_OF_INDEXES)
		system_call_failed::raise("FlsAlloc");
}

ThreadCleanupCallback::~ThreadCleanupCallback()
{
	if (flsIndex != FLS_OUT_OF_INDEXES)
	{
		FlsFree(flsIndex);
		flsIndex = FLS_OUT_OF_INDEXES;
	}
}

void ThreadCleanupCallback::enable()
{
	// Any non-NULL value causes the OS to invoke trampoline for this thread at thread exit
	// (or FlsFree).
	if (!FlsSetValue(flsIndex, this))
		system_call_failed::raise("FlsSetValue");
}

void ThreadCleanupCallback::disable()
{
	FlsSetValue(flsIndex, nullptr);
}

#else // !WIN_NT

ThreadCleanupCallback::ThreadCleanupCallback(MemoryPool&, Callback aCallback, void* aArg)
	: callback(aCallback),
	  arg(aArg)
{
	int err = pthread_key_create(&key, &ThreadCleanupCallback::trampoline);

	if (err)
		system_call_failed::raise("pthread_key_create", err);
}

ThreadCleanupCallback::~ThreadCleanupCallback()
{
	int err = pthread_key_delete(key);

	if (err)
		gds__log("pthread_key_delete failed with error %d", err);
}

void ThreadCleanupCallback::enable()
{
	// Any non-NULL value causes the OS to invoke trampoline for this thread at thread exit.
	int err = pthread_setspecific(key, this);

	if (err)
		system_call_failed::raise("pthread_setspecific", err);
}

void ThreadCleanupCallback::disable()
{
	pthread_setspecific(key, nullptr);
}

#endif // WIN_NT

void ThreadCleanupCallback::trampoline(void* value)
{
	if (value)
		static_cast<ThreadCleanupCallback*>(value)->callback(static_cast<ThreadCleanupCallback*>(value)->arg);
}

} // namespace Firebird
