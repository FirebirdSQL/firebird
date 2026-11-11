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

#ifndef JRD_PROFILER_SAMPLER_H
#define JRD_PROFILER_SAMPLER_H

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/locks.h"
#include "../common/classes/RefCounted.h"
#include <atomic>

namespace Jrd {


// Where an attachment is executing, published by the attachment thread (the only writer) and read
// by the sampler thread. The writer pays a few plain stores per event, protected by a seqlock.
// The sampler charges the wall time elapsed since its previous sample to every frame it sees.
class ProfilerSampleState final : public Firebird::RefCounted, public Firebird::GlobalStorage
{
public:
	enum class FrameKind : UCHAR
	{
		REQUEST,	// a = line, b = column of the current PSQL statement (0 = none)
		OPEN,		// a = record source id, b = cursor id
		FETCH		// a = record source id, b = cursor id
	};

	struct Frame
	{
		SINT64 statementId;
		SINT64 requestId;
		ULONG a;
		ULONG b;
		FrameKind kind;
	};

	enum class KeyKind : UCHAR
	{
		REQUEST,	// whole request, a = b = 0
		LINE,		// a = line, b = column
		OPEN,		// a = record source id, b = cursor id
		FETCH		// a = record source id, b = cursor id
	};

	struct Key
	{
		SINT64 statementId;
		SINT64 requestId;
		ULONG a;
		ULONG b;
		KeyKind kind;

		bool operator==(const Key& o) const noexcept
		{
			return requestId == o.requestId && statementId == o.statementId &&
				a == o.a && b == o.b && kind == o.kind;
		}

		bool operator<(const Key& o) const noexcept
		{
			if (requestId != o.requestId)
				return requestId < o.requestId;
			if (statementId != o.statementId)
				return statementId < o.statementId;
			if (kind != o.kind)
				return kind < o.kind;
			if (a != o.a)
				return a < o.a;
			return b < o.b;
		}

		bool operator>(const Key& o) const noexcept
		{
			return o < *this;
		}
	};

	using SampleMap = Firebird::NonPooledMap<Key, FB_UINT64>;

	static constexpr unsigned MAX_DEPTH = 256;

public:
	ProfilerSampleState();

	ProfilerSampleState(const ProfilerSampleState&) = delete;
	ProfilerSampleState& operator=(const ProfilerSampleState&) = delete;

public:
	// Writer side (attachment thread).

	// Returns the depth before the push, to be passed to popTo.
	unsigned push(const Frame& frame) noexcept
	{
		const unsigned oldDepth = depth.load(std::memory_order_relaxed);

		beginWrite();

		if (oldDepth < MAX_DEPTH)
			frames[oldDepth] = frame;

		depth.store(oldDepth + 1, std::memory_order_relaxed);

		endWrite();

		if (oldDepth == 0)
			wakeSampler();

		return oldDepth;
	}

	void popTo(unsigned newDepth) noexcept
	{
		beginWrite();
		depth.store(newDepth, std::memory_order_relaxed);
		endWrite();
	}

	void setLine(unsigned frameIndex, ULONG line, ULONG column) noexcept
	{
		if (frameIndex < MAX_DEPTH)
		{
			beginWrite();
			frames[frameIndex].a = line;
			frames[frameIndex].b = column;
			endWrite();
		}
	}

	void setActive(bool value) noexcept
	{
		active.store(value, std::memory_order_relaxed);

		if (value)
			wakeSampler();
	}

	// Each session may have its own interval. A new interval is used from the next sample on.
	void setInterval(ULONG microseconds);

	// Moves the accumulated samples to the caller.
	void takeSamples(SampleMap& target);

	// Returns false when takeSamples would return nothing that involves frames popped before this call,
	// so the caller can skip it.
	bool mayHaveSamples() const noexcept
	{
		// Pairs with the fence in sample(): either this load sees samplesPending set, or the snapshot of
		// the sampler sees the frames already popped.
		std::atomic_thread_fence(std::memory_order_seq_cst);
		return samplesPending.load(std::memory_order_relaxed);
	}

	// Discards the accumulated samples.
	void clearSamples();

	// Sampler side.

	// Takes one sample if it's due. Returns the next due time.
	SINT64 sample(SINT64 now);

	// There is nothing to sample: the session is paused or finished, or nothing is executing.
	bool isIdle() const noexcept
	{
		return !active.load(std::memory_order_relaxed) || depth.load(std::memory_order_relaxed) == 0;
	}

	// Starts sampling again after the sampler slept, without charging the time slept.
	void restart(SINT64 now) noexcept;

	// Set by the sampler before it sleeps because all states are idle. A state that becomes busy wakes it.
	static inline std::atomic<bool> samplerSleeping = false;

private:
	void beginWrite() noexcept
	{
		seq.store(seq.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_release);
	}

	void endWrite() noexcept
	{
		seq.store(seq.load(std::memory_order_relaxed) + 1, std::memory_order_release);
	}

	static void wakeSampler() noexcept
	{
		// Pairs with the fence of the sampler before it sleeps: either it sees this state busy,
		// or this sees it sleeping.
		std::atomic_thread_fence(std::memory_order_seq_cst);

		if (samplerSleeping.load(std::memory_order_relaxed))
			wakeSleepingSampler();
	}

	static void wakeSleepingSampler() noexcept;

	bool snapshot(Frame* target, unsigned& count) const noexcept;

	SINT64 nextInterval() noexcept;

private:
	std::atomic<unsigned> seq = 0;
	std::atomic<unsigned> depth = 0;
	std::atomic<bool> active = false;
	std::atomic<SINT64> intervalTicks = 0;
	Frame frames[MAX_DEPTH];

	// Owned by the sampler thread.
	SINT64 nextSampleTicks = 0;
	SINT64 lastSampleTicks = 0;
	ULONG jitterState = 0x9E3779B9;	// xorshift32 state of nextInterval(), any non-zero seed

	Firebird::Mutex samplesMutex;
	SampleMap samples;
	std::atomic<bool> samplesPending = false;	// set before a snapshot, cleared when samples are taken
};


// Process-wide thread that samples all registered states.
class ProfilerSampler final
{
public:
	static void add(ProfilerSampleState* state);
	static void remove(ProfilerSampleState* state);

	// Stops the thread at engine shutdown. States added after it are not sampled.
	static void shutdown();
};


}	// namespace Jrd

#endif	// JRD_PROFILER_SAMPLER_H
