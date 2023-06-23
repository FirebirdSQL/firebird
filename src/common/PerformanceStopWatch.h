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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_PERFORMANCE_STOP_WATCH_H
#define COMMON_PERFORMANCE_STOP_WATCH_H

#include "../common/utils_proto.h"


namespace Firebird
{

// This class is a wrapper to fb_utils::query_performance_counter() with a way to let
// the caller amortize timings expended in that function calls.
// This class is not thread-safe.
class PerformanceStopWatch
{
public:
	PerformanceStopWatch() = default;
	PerformanceStopWatch(const PerformanceStopWatch&) = delete;
	PerformanceStopWatch& operator=(const PerformanceStopWatch&) = delete;

public:
	SINT64 queryTicks()
	{
		const auto initialTicks = fb_utils::query_performance_counter();

		if ((initialTicks - lastMeasuredTicks) * 1000 / fb_utils::query_performance_frequency() >
				OVERHEAD_CALC_FREQUENCY_MS)
		{
			const auto currentTicks = lastMeasuredTicks = fb_utils::query_performance_counter();
			lastOverhead = currentTicks - initialTicks;
			accumulatedOverhead += lastOverhead + lastOverhead;
			return currentTicks;
		}
		else
		{
			accumulatedOverhead += lastOverhead;
			return initialTicks;
		}
	}

	SINT64 getAccumulatedOverhead() const
	{
		return accumulatedOverhead;
	}

	SINT64 getElapsedTicksAndAdjustOverhead(SINT64 currentTicks, SINT64 previousTicks,
		SINT64 previousAccumulatedOverhead)
	{
		const SINT64 overhead = MAX(accumulatedOverhead - previousAccumulatedOverhead, 0);
		const SINT64 elapsedTicks = currentTicks - previousTicks - overhead;

		if (elapsedTicks >= 0)
			return elapsedTicks;

		accumulatedOverhead += elapsedTicks;

		return 0;
	}

public:
	static constexpr SINT64 OVERHEAD_CALC_FREQUENCY_MS = 30 * 1000;

private:
	SINT64 lastMeasuredTicks = 0;
	SINT64 lastOverhead = 0;
	SINT64 accumulatedOverhead = 0;
};

}	// namespace Firebird


#endif	// COMMON_PERFORMANCE_STOP_WATCH_H
