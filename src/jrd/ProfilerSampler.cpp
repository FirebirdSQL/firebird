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
#include "../jrd/ProfilerSampler.h"
#include "../common/ThreadStart.h"
#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/classes/semaphore.h"
#include "../common/isc_proto.h"
#include "../common/utils_proto.h"

#ifdef WIN_NT
#include <windows.h>
#else
#include <time.h>
#endif

using namespace Jrd;
using namespace Firebird;


namespace
{
	// Upper bound of the sampler sleep, so a stop request is noticed even with large intervals.
	constexpr SINT64 MAX_SLEEP_MICROSECONDS = 100'000;

	// How long all states must be idle before the sampler sleeps until one becomes busy. Shorter idle periods,
	// as between the requests of a client, are sampled as usual, as waking up the sampler costs more.
	constexpr SINT64 IDLE_SLEEP_DELAY_MICROSECONDS = 10'000;

	constexpr unsigned MAX_SNAPSHOT_RETRIES = 1000;

	SINT64 microsecondsToTicks(SINT64 microseconds)
	{
		return MAX(microseconds * fb_utils::query_performance_frequency() / 1'000'000, 1);
	}

	class Sleeper
	{
	public:
		Sleeper()
		{
#ifdef WIN_NT
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
			timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
				TIMER_MODIFY_STATE | SYNCHRONIZE);
#endif
			if (!timer)
				timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
#endif
		}

		~Sleeper()
		{
#ifdef WIN_NT
			if (timer)
				CloseHandle(timer);
#endif
		}

		Sleeper(const Sleeper&) = delete;
		Sleeper& operator=(const Sleeper&) = delete;

	public:
		void sleep(SINT64 ticks)
		{
			if (ticks <= 0)
				return;

			const SINT64 nanoseconds = ticks * fb_utils::BILLION / fb_utils::query_performance_frequency();

#ifdef WIN_NT
			if (timer)
			{
				LARGE_INTEGER dueTime;
				dueTime.QuadPart = -MAX(nanoseconds / 100, 1);	// relative, in 100 ns units

				if (SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE))
				{
					WaitForSingleObject(timer, INFINITE);
					return;
				}
			}

			Sleep(DWORD(MAX(nanoseconds * 1000 / fb_utils::BILLION, 1)));
#else
			timespec ts;
			ts.tv_sec = time_t(nanoseconds / fb_utils::BILLION);
			ts.tv_nsec = long(nanoseconds % fb_utils::BILLION);

			nanosleep(&ts, nullptr);
#endif
		}

	private:
#ifdef WIN_NT
		HANDLE timer = nullptr;
#endif
	};

	class SamplerThread
	{
	public:
		explicit SamplerThread(MemoryPool& pool)
			: states(pool)
		{
		}

		~SamplerThread()
		{
			// States are removed by their ProfilerManager, but one left registered at shutdown
			// must not keep the thread running with the members already destroyed.
			fb_assert(states.isEmpty());

			stopThread();

			for (const auto state : states)
				state->release();
		}

		SamplerThread(const SamplerThread&) = delete;
		SamplerThread& operator=(const SamplerThread&) = delete;

	public:
		void wake() noexcept
		{
			try
			{
				wakeSemaphore.release();
			}
			catch (const Exception&)
			{
				// The sampler wakes up anyway after MAX_SLEEP_MICROSECONDS.
			}
		}

		void add(ProfilerSampleState* state)
		{
			MutexLockGuard controlGuard(controlMutex, FB_FUNCTION);

			if (shutdownDone)
				return;

			{	// scope
				MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);
				state->addRef();
				states.add(state);
			}

			if (!thread.isValid())
			{
				stopRequested = false;
				Thread::start(threadRoutine, this, THREAD_high, &thread);
			}
		}

		void remove(ProfilerSampleState* state)
		{
			MutexLockGuard controlGuard(controlMutex, FB_FUNCTION);
			bool wasLast;

			{	// scope
				MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);

				FB_SIZE_T pos;
				if (!states.find(state, pos))
					return;

				states.remove(pos);
				state->release();

				wasLast = states.isEmpty();
			}

			if (wasLast)
				stopThread();
		}

		void shutdown()
		{
			MutexLockGuard controlGuard(controlMutex, FB_FUNCTION);

			shutdownDone = true;
			stopThread();

			// Attachments not yet shut down remove their states later, finding them already gone.
			MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);

			for (const auto state : states)
				state->release();

			states.clear();
		}

	private:
		void stopThread()
		{
			if (thread.isValid())
			{
				stopRequested = true;
				wakeSemaphore.release();
				thread.waitForCompletion();
				thread.detach(false);
			}
		}

		static THREAD_ENTRY_DECLARE threadRoutine(THREAD_ENTRY_PARAM arg)
		{
			static_cast<SamplerThread*>(arg)->run();
			return 0;
		}

		void run()
		{
			Sleeper sleeper;
			const SINT64 maxSleepTicks = microsecondsToTicks(MAX_SLEEP_MICROSECONDS);
			const SINT64 idleSleepDelayTicks = microsecondsToTicks(IDLE_SLEEP_DELAY_MICROSECONDS);
			SINT64 idleSinceTicks = 0;	// 0 = some state is busy
			bool errorLogged = false;

			while (!stopRequested.load(std::memory_order_relaxed))
			{
				const SINT64 now = fb_utils::query_performance_counter();
				SINT64 nextSampleTicks = now + maxSleepTicks;
				bool allIdle = true;

				// The thread is restarted only when all states are removed, so an error loses the samples
				// of this pass, but doesn't stop the sampling. It's logged once until a pass succeeds.
				try
				{
					MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);

					for (const auto state : states)
					{
						nextSampleTicks = MIN(nextSampleTicks, state->sample(now));
						allIdle = allIdle && state->isIdle();
					}

					errorLogged = false;
				}
				catch (const Exception& ex)
				{
					allIdle = false;

					if (!errorLogged)
					{
						iscLogException("Error in profiler sampler thread", ex);
						errorLogged = true;
					}
				}

				if (!allIdle)
					idleSinceTicks = 0;
				else if (!idleSinceTicks)
					idleSinceTicks = now;
				else if (now - idleSinceTicks >= idleSleepDelayTicks)
				{
					sleepWhileIdle();
					idleSinceTicks = 0;
					continue;
				}

				sleeper.sleep(nextSampleTicks - fb_utils::query_performance_counter());
			}
		}

		// Sleeps until a state becomes busy, so idle attachments don't wake the sampler every interval.
		void sleepWhileIdle()
		{
			ProfilerSampleState::samplerSleeping.store(true, std::memory_order_relaxed);

			// Pairs with the fence in ProfilerSampleState::wakeSampler().
			std::atomic_thread_fence(std::memory_order_seq_cst);

			bool allIdle = true;

			{	// scope
				MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);

				for (const auto state : states)
				{
					if (!state->isIdle())
					{
						allIdle = false;
						break;
					}
				}
			}

			if (allIdle && !stopRequested.load(std::memory_order_relaxed))
				wakeSemaphore.tryEnter(0, int(MAX_SLEEP_MICROSECONDS / 1000));

			ProfilerSampleState::samplerSleeping.store(false, std::memory_order_relaxed);

			const SINT64 now = fb_utils::query_performance_counter();

			MutexLockGuard statesGuard(statesMutex, FB_FUNCTION);

			for (const auto state : states)
				state->restart(now);
		}

	private:
		Mutex controlMutex;		// serializes thread start and stop
		Mutex statesMutex;		// protects states
		HalfStaticArray<ProfilerSampleState*, 8> states;
		Semaphore wakeSemaphore;	// released to end sleepWhileIdle()
		Thread thread;
		std::atomic<bool> stopRequested = false;
		bool shutdownDone = false;	// protected by controlMutex
	};

	GlobalPtr<SamplerThread> samplerThread;
}


//--------------------------------------


ProfilerSampleState::ProfilerSampleState()
	: samples(*getDefaultMemoryPool())
{
}

void ProfilerSampleState::setInterval(ULONG microseconds)
{
	intervalTicks.store(microsecondsToTicks(microseconds), std::memory_order_relaxed);
}

void ProfilerSampleState::takeSamples(SampleMap& target)
{
	MutexLockGuard guard(samplesMutex, FB_FUNCTION);
	target.takeOwnership(samples);
	samplesPending.store(false, std::memory_order_relaxed);
}

void ProfilerSampleState::clearSamples()
{
	MutexLockGuard guard(samplesMutex, FB_FUNCTION);
	samples.clear();
	samplesPending.store(false, std::memory_order_relaxed);
}

SINT64 ProfilerSampleState::sample(SINT64 now)
{
	if (now < nextSampleTicks)
		return nextSampleTicks;

	const SINT64 elapsed = lastSampleTicks ? now - lastSampleTicks : 0;

	// The depth is read again by the snapshot, but checking it here avoids setting samplesPending when
	// nothing is executing, so mayHaveSamples() remains false for requests that finish between samples.
	if (elapsed <= 0 || isIdle())
	{
		lastSampleTicks = now;
		nextSampleTicks = now + nextInterval();
		return nextSampleTicks;
	}

	Frame snapshotFrames[MAX_DEPTH];
	unsigned count;

	// Keep the lock from the snapshot until the samples are stored, so a drain done after a frame
	// is popped never misses a sample taken while the frame was present.
	MutexLockGuard guard(samplesMutex, FB_FUNCTION);

	// Set before the snapshot, so a writer that pops its frames and then finds it clear knows that
	// no sample of them is pending. See mayHaveSamples().
	samplesPending.store(true, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_seq_cst);

	if (!snapshot(snapshotFrames, count))
	{
		// The attachment is changing its frames too fast to be read now. Retry soon, keeping
		// lastSampleTicks, so the elapsed time is charged by the next sample instead of being lost.
		nextSampleTicks = now + MAX(intervalTicks.load(std::memory_order_relaxed) / 16, 1);
		return nextSampleTicks;
	}

	lastSampleTicks = now;
	nextSampleTicks = now + nextInterval();

	// The frames were popped after the depth was checked.
	if (count == 0 && samples.isEmpty())
	{
		samplesPending.store(false, std::memory_order_relaxed);
		return nextSampleTicks;
	}

	HalfStaticArray<Key, 64> keys;

	const auto addKey = [&](const Key& key)
	{
		// Recursion may show the same key more than once in a stack, but it must be charged once.
		for (const auto& existing : keys)
		{
			if (existing == key)
				return;
		}

		keys.add(key);
		*samples.getOrPut(key) += elapsed;
	};

	for (unsigned i = 0; i < count; ++i)
	{
		const auto& frame = snapshotFrames[i];

		switch (frame.kind)
		{
			case FrameKind::REQUEST:
				addKey({frame.statementId, frame.requestId, 0, 0, KeyKind::REQUEST});

				if (frame.a || frame.b)
					addKey({frame.statementId, frame.requestId, frame.a, frame.b, KeyKind::LINE});
				break;

			case FrameKind::OPEN:
				addKey({frame.statementId, frame.requestId, frame.a, frame.b, KeyKind::OPEN});
				break;

			case FrameKind::FETCH:
				addKey({frame.statementId, frame.requestId, frame.a, frame.b, KeyKind::FETCH});
				break;
		}
	}

	return nextSampleTicks;
}

void ProfilerSampleState::restart(SINT64 now) noexcept
{
	// A state that became busy is sampled after one interval. Its time before it is not charged,
	// but it's only the time to wake up the sampler.
	lastSampleTicks = now;
	nextSampleTicks = now + nextInterval();
}

void ProfilerSampleState::wakeSleepingSampler() noexcept
{
	// Only one of the writers that see the sampler sleeping releases it.
	if (samplerSleeping.exchange(false, std::memory_order_relaxed))
		samplerThread->wake();
}

bool ProfilerSampleState::snapshot(Frame* target, unsigned& count) const noexcept
{
	// Hot loops may change the frames every few nanoseconds, so a read often overlaps a write.
	for (unsigned retry = 0; retry < MAX_SNAPSHOT_RETRIES; ++retry)
	{
		const unsigned startSeq = seq.load(std::memory_order_acquire);

		if (startSeq & 1)
			continue;	// a write is in progress

		count = MIN(depth.load(std::memory_order_relaxed), MAX_DEPTH);
		memcpy(target, frames, count * sizeof(Frame));

		std::atomic_thread_fence(std::memory_order_acquire);

		if (seq.load(std::memory_order_relaxed) == startSeq)
			return true;
	}

	return false;
}

SINT64 ProfilerSampleState::nextInterval() noexcept
{
	// Randomize the interval in [0.75, 1.25) of the configured one, so periodic code doesn't alias
	// with the sampling period.
	jitterState ^= jitterState << 13;
	jitterState ^= jitterState >> 17;
	jitterState ^= jitterState << 5;

	const SINT64 interval = intervalTicks.load(std::memory_order_relaxed);

	return MAX(interval * 3 / 4 + SINT64((jitterState % 1024) * (interval / 2) / 1024), 1);
}


//--------------------------------------


void ProfilerSampler::add(ProfilerSampleState* state)
{
	samplerThread->add(state);
}

void ProfilerSampler::remove(ProfilerSampleState* state)
{
	samplerThread->remove(state);
}

void ProfilerSampler::shutdown()
{
	samplerThread->shutdown();
}
