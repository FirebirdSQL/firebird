/*
 *	PROGRAM:	JRD threading support
 *	MODULE:		ThreadCollect.h
 *	DESCRIPTION:	Threads' group completion handling
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2018, 2022 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef JRD_THREADCOLLECT_H
#define JRD_THREADCOLLECT_H

#include "../common/ThreadStart.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/locks.h"


namespace Jrd {

	class ThreadCollect
	{
	public:
		ThreadCollect(MemoryPool& p)
			: threads(p)
		{ }

		void join()
		{
			if (!threads.hasData())
				return;

			waitFor(threads);
		}

		void ending(Thread::Handle& h)
		{
			// put thread into completion wait queue when it finished running
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

			for (unsigned n = 0; n < threads.getCount(); ++n)
			{
				if (threads[n].hndl == h)
				{
					threads[n].ending = true;
					return;
				}
			}

			Thrd t = {h, true};
			threads.add(t);
		}

		void running(Thread::Handle& h)
		{
			// put thread into completion wait queue when it starts running
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

			Thrd t = {h, false};
			threads.add(t);
		}

		void houseKeeping()
		{
			if (!threads.hasData())
				return;

			// join finished threads
			AllThreads t;
			{ // mutex scope
				Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

				for (unsigned n = 0; n < threads.getCount(); )
				{
					if (threads[n].ending)
					{
						t.add(threads[n]);
						threads.remove(n);
					}
					else
						++n;
				}
			}

			waitFor(t);
		}

	private:
		struct Thrd
		{
			Thread::Handle hndl;
			bool ending;
		};
		typedef Firebird::HalfStaticArray<Thrd, 4> AllThreads;

		void waitFor(AllThreads& thr)
		{
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);
			while (thr.hasData())
			{
				FB_SIZE_T n = thr.getCount() - 1;
				Thrd& t = thr[n];
				{
					Firebird::MutexUnlockGuard u(threadsMutex, FB_FUNCTION);
					Thread::waitForCompletion(t.hndl);
					fb_assert(t.ending);
				}
				thr.remove(n);
			}
		}

		AllThreads threads;
		Firebird::Mutex threadsMutex;
	};

}	// namespace Jrd


#endif	// JRD_THREADCOLLECT_H
