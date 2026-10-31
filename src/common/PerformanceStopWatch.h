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

// This class is a wrapper to fb_utils::query_performance_counter() that lets the caller exclude
// the time spent by the caller's own bookkeeping from the measured intervals.
//
// The bookkeeping done for an event is bracketed by queryTicks() and endOverhead(). The bracketed
// time is accumulated and subtracted from every interval that encloses it, so a parent (a request,
// a record source) doesn't get charged with the cost of measuring its children.
// Each clock read samples the time somewhere in the middle of its own execution, so the part of
// the reads that falls outside the brackets is accounted with the cost of one read (queryCost).
// That cost depends on the CPU frequency, which changes while running (a core that was idle runs
// the first reads slower), so it's measured again every CALIBRATION_INTERVAL_MS, inside a bracket.
// Overheads are kept in 1/OVERHEAD_SCALE ticks, as a read may cost less than a tick (on Windows).
//
// This class is not thread-safe.
class PerformanceStopWatch
{
public:
	PerformanceStopWatch()
		: calibrationIntervalTicks(fb_utils::query_performance_frequency() * CALIBRATION_INTERVAL_MS / 1000),
		  queryCost(measureQueryCost()),
		  nextCalibrationTicks(fb_utils::query_performance_counter() + calibrationIntervalTicks)
	{
	}

	PerformanceStopWatch(const PerformanceStopWatch&) = delete;
	PerformanceStopWatch& operator=(const PerformanceStopWatch&) = delete;

public:
	SINT64 queryTicks() const
	{
		return fb_utils::query_performance_counter();
	}

	// Starts a measured interval without preceding bookkeeping. The cost of this read is still
	// overhead for the enclosing intervals.
	SINT64 beginInterval()
	{
		accumulatedOverhead += queryCost;
		return fb_utils::query_performance_counter();
	}

	// Ends bookkeeping started at overheadStartTicks (a value returned by queryTicks).
	// Returns the ticks that start the next measured interval.
	SINT64 endOverhead(SINT64 overheadStartTicks)
	{
		SINT64 currentTicks = fb_utils::query_performance_counter();

		if (currentTicks >= nextCalibrationTicks)
		{
			queryCost = measureQueryCost();
			currentTicks = fb_utils::query_performance_counter();
			nextCalibrationTicks = currentTicks + calibrationIntervalTicks;
		}

		accumulatedOverhead += (currentTicks - overheadStartTicks) * OVERHEAD_SCALE + queryCost;

		return currentTicks;
	}

	SINT64 getAccumulatedOverhead() const
	{
		return accumulatedOverhead;
	}

	// Returns the ticks between previousTicks and currentTicks, excluding the bookkeeping done in
	// between and the parts of the two clock reads inside the interval.
	SINT64 getElapsedTicksAndAdjustOverhead(SINT64 currentTicks, SINT64 previousTicks,
		SINT64 previousAccumulatedOverhead) const
	{
		const SINT64 overheadTicks = (accumulatedOverhead - previousAccumulatedOverhead + queryCost) / OVERHEAD_SCALE;
		return MAX(currentTicks - previousTicks - overheadTicks, 0);
	}

private:
	// The average cost of a read in batches of consecutive reads. The difference between just two
	// consecutive reads underestimates it, as the CPU overlaps their execution. The minimum of the
	// batches is used, so preemptions and cold caches don't inflate it.
	static SINT64 measureQueryCost()
	{
		constexpr unsigned BATCHES = 8;
		constexpr unsigned READS_PER_BATCH = 256;

		SINT64 minTicks = 0;

		for (unsigned batch = 0; batch < BATCHES; ++batch)
		{
			const SINT64 startTicks = fb_utils::query_performance_counter();

			for (unsigned i = 0; i < READS_PER_BATCH; ++i)
				fb_utils::query_performance_counter();

			const SINT64 ticks = fb_utils::query_performance_counter() - startTicks;

			if (batch == 0 || ticks < minTicks)
				minTicks = ticks;
		}

		return MAX(minTicks * OVERHEAD_SCALE / (READS_PER_BATCH + 1), 0);
	}

private:
	static constexpr SINT64 OVERHEAD_SCALE = 256;
	static constexpr SINT64 CALIBRATION_INTERVAL_MS = 100;

	const SINT64 calibrationIntervalTicks;
	SINT64 queryCost;		// in 1/OVERHEAD_SCALE ticks
	SINT64 nextCalibrationTicks;
	SINT64 accumulatedOverhead = 0;	// in 1/OVERHEAD_SCALE ticks
};

}	// namespace Firebird


#endif	// COMMON_PERFORMANCE_STOP_WATCH_H
