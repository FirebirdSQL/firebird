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

#ifndef CLASSES_SPINLOCK_H
#define CLASSES_SPINLOCK_H

#include <atomic>
#include <thread>

namespace Firebird {


// Spinlock implementation that can be used in shared memory.
// Based in example found in https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
// Compatible with std::lock_guard, std::scoped_lock and std::unique_lock.
class SpinLock
{
	std::atomic_flag atomicFlag = ATOMIC_FLAG_INIT;

public:
	void lock() noexcept
	{
		while (atomicFlag.test_and_set(std::memory_order_acquire))
		{
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			// Since C++20, locks can be acquired only after notification in the unlock,
			// avoiding any unnecessary spinning.
			// Note that even though wait guarantees it returns only after the value has
			// changed, the lock is acquired after the next condition check.
			atomicFlag.wait(true, std::memory_order_relaxed);
#else
			std::this_thread::yield();
#endif
		}
	}

	bool try_lock() noexcept
	{
		return !atomicFlag.test_and_set(std::memory_order_acquire);
	}

	void unlock() noexcept
	{
		atomicFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		atomicFlag.notify_one();
#endif
	}
};


} // namespace Firebird

#endif // CLASSES_SPINLOCK_H
