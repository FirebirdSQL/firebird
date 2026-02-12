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

#include <vector>


namespace Jrd {

	class ThreadCollect
	{
	public:
		ThreadCollect(Firebird::MemoryPool& p)
			: threads(p)
		{ }

		void join()
		{
			if (threads.empty())
				return;

			waitFor(threads);
		}

		// put thread into completion wait queue when it finished running
		void ending(Thread&& thd)
		{
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

			const Thread::Mark mark(thd);

			for (auto& n : threads)
			{
				if (mark == n.thread)
				{
					n.ending = true;
					return;
				}
			}

			threads.push_back(Thrd(std::move(thd), true));
		}

		void ending(Thread::Mark& m)
		{
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

			for (auto& n : threads)
			{
				if (m == n.thread)
				{
					n.ending = true;
					return;
				}
			}

			fb_assert(!"Marked thread should be present in threads[]");
		}

		// put thread into completion wait queue when it starts running
		void running(Thread&& thd)
		{
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

			threads.push_back(Thrd(std::move(thd), false));
		}

		void houseKeeping()
		{
			if (threads.empty())
				return;

			// join finished threads
			AllThreads finished(threads.get_allocator());
			{ // mutex scope
				Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);

				for (auto n = threads.begin(); n != threads.end(); )
				{
					if (n->ending)
					{
						finished.push_back(std::move(*n));
						n = threads.erase(n);
					}
					else
						++n;
				}
			}

			waitFor(finished);
		}

	private:
		struct Thrd
		{
			Thrd(Thread&& aThread, bool ending)
				: thread(std::move(aThread)),
				ending(ending)
			{ }

			Thrd(Thrd&& other)
			  : thread(std::move(other.thread)),
				ending(other.ending)
			{ }

			Thrd& operator=(Thrd&& other)
			{
				thread = std::move(other.thread);
				ending = other.ending;
				return *this;
			}

			Thread thread;
			bool ending;
		};

		using AllThreads = std::vector<Thrd, Firebird::PoolAllocator<Thrd>>;

		void waitFor(AllThreads& thr)
		{
			Firebird::MutexLockGuard g(threadsMutex, FB_FUNCTION);
			while (!thr.empty())
			{
				Thrd t = std::move(thr.back());
				thr.pop_back();
				{
					Firebird::MutexUnlockGuard u(threadsMutex, FB_FUNCTION);
					t.thread.waitForCompletion();
					fb_assert(t.ending);
				}
			}
		}

		AllThreads threads;
		Firebird::Mutex threadsMutex;
	};

}	// namespace Jrd


#endif	// JRD_THREADCOLLECT_H
