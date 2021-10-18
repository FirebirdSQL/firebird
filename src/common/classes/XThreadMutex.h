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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef COMMON_X_THREAD_MUTEX_H
#define COMMON_X_THREAD_MUTEX_H

#include "../common/classes/Reasons.h"
#include "../common/classes/RefMutex.h"
#include "../common/classes/semaphore.h"

namespace Firebird
{

// Non-recursive mutex that may be unlocked by any thread
// Based on semaphore
class XThreadMutex : private Semaphore, private Reasons
{
public:
	XThreadMutex()
	{
		Semaphore::release();
#ifdef DEV_BUILD
		locked = false;
#endif
	}

	~XThreadMutex()
	{
		fb_assert(!locked);
	}

	void enter(const char* aReason)
	{
		Semaphore::enter();
		fb_assert(!locked);
#ifdef DEV_BUILD
		locked = true;
#endif
		reason(aReason);
	}

	bool tryEnter(const char* aReason)
	{
		const bool ret = Semaphore::tryEnter();
		if (ret)
		{
			fb_assert(!locked);
#ifdef DEV_BUILD
			locked = true;
#endif
			reason(aReason);
		}
		return ret;
	}

	void leave()
	{
		fb_assert(locked);
#ifdef DEV_BUILD
		locked = false;
#endif
		Semaphore::release();
	}

private:
#ifdef DEV_BUILD
	bool locked;
#endif
};

typedef RaiiLockGuard<XThreadMutex> XThreadLockGuard;
typedef EnsureUnlock<XThreadMutex, NotRefCounted> XThreadEnsureUnlock;

}

#endif // COMMON_X_THREAD_MUTEX_H
