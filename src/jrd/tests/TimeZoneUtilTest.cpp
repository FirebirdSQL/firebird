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
#include "boost/test/unit_test.hpp"
#include "../common/TimeZoneUtil.h"
#include "../common/unicode_util.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace Firebird;


// Region zones have their offset intervals cached by TimeZoneUtil. These tests compare the results of
// TimeZoneUtil with results computed directly by ICU, in access patterns that exercise the cache.
// Each test case uses its own zones, so the cache of them starts empty.

BOOST_AUTO_TEST_SUITE(TimeZoneUtilSuite)
BOOST_AUTO_TEST_SUITE(TimeZoneCacheTests)

namespace
{
	constexpr SINT64 TICKS_PER_SECOND = ISC_TIME_SECONDS_PRECISION;
	constexpr SINT64 TICKS_PER_MINUTE = 60 * TICKS_PER_SECOND;
	constexpr SINT64 TICKS_PER_HOUR = 60 * TICKS_PER_MINUTE;

	SINT64 toTicks(ISC_TIMESTAMP ts)
	{
		return NoThrowTimeStamp::timeStampToTicks(ts);
	}

	ISC_TIMESTAMP fromTicks(SINT64 ticks)
	{
		return NoThrowTimeStamp::ticksToTimeStamp(ticks);
	}

	ISC_TIMESTAMP makeTimeStamp(int year, int month, int day, int hour = 0, int minute = 0, int second = 0)
	{
		struct tm times;
		memset(&times, 0, sizeof(times));
		times.tm_year = year - 1900;
		times.tm_mon = month - 1;
		times.tm_mday = day;
		times.tm_hour = hour;
		times.tm_min = minute;
		times.tm_sec = second;

		return NoThrowTimeStamp::encode_timestamp(&times);
	}

	std::string toString(ISC_TIMESTAMP ts)
	{
		struct tm times;
		int fractions;
		NoThrowTimeStamp::decode_timestamp(ts, &times, &fractions);

		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%04d",
			times.tm_year + 1900, times.tm_mon + 1, times.tm_mday,
			times.tm_hour, times.tm_min, times.tm_sec, fractions);

		return buffer;
	}

	// Deterministic pseudo-random numbers.
	class Random
	{
	public:
		explicit Random(uint64_t aState)
			: state(aState)
		{
		}

		SINT64 next(SINT64 lo, SINT64 hi)
		{
			state = state * 6364136223846793005ULL + 1442695040888963407ULL;
			return lo + SINT64((state >> 11) % uint64_t(hi - lo));
		}

	private:
		uint64_t state;
	};

	// Computes results directly with ICU, as TimeZoneUtil did without the cache.
	class IcuReference
	{
	public:
		explicit IcuReference(const char* aZoneName)
			: zoneName(aZoneName),
			  zone(TimeZoneUtil::parseRegion(aZoneName, strlen(aZoneName))),
			  icuLib(UnicodeUtil::getConversionICU())
		{
			UChar unicodeName[TimeZoneUtil::MAX_SIZE];
			const auto len = strlen(aZoneName);

			for (size_t i = 0; i <= len; ++i)
				unicodeName[i] = aZoneName[i];

			UErrorCode icuErrorCode = U_ZERO_ERROR;
			calendar = icuLib.ucalOpen(unicodeName, -1, nullptr, UCAL_GREGORIAN, &icuErrorCode);
			BOOST_REQUIRE(calendar && U_SUCCESS(icuErrorCode));

			icuLib.ucalSetAttribute(calendar, UCAL_REPEATED_WALL_TIME, UCAL_WALLTIME_FIRST);
			icuLib.ucalSetAttribute(calendar, UCAL_SKIPPED_WALL_TIME, UCAL_WALLTIME_FIRST);
		}

		~IcuReference()
		{
			icuLib.ucalClose(calendar);
		}

		IcuReference(const IcuReference&) = delete;
		IcuReference& operator=(const IcuReference&) = delete;

	public:
		// Displacement in minutes of an UTC instant.
		SSHORT utcDisplacement(ISC_TIMESTAMP utc)
		{
			UErrorCode icuErrorCode = U_ZERO_ERROR;
			icuLib.ucalSetMillis(calendar, TimeZoneUtil::timeStampToIcuDate(utc), &icuErrorCode);
			return getDisplacement(icuErrorCode);
		}

		ISC_TIMESTAMP localToUtc(ISC_TIMESTAMP local)
		{
			struct tm times;
			NoThrowTimeStamp::decode_timestamp(local, &times, nullptr);

			UErrorCode icuErrorCode = U_ZERO_ERROR;
			icuLib.ucalSetDateTime(calendar, 1900 + times.tm_year, times.tm_mon, times.tm_mday,
				times.tm_hour, times.tm_min, times.tm_sec, &icuErrorCode);

			return fromTicks(toTicks(local) - getDisplacement(icuErrorCode) * TICKS_PER_MINUTE);
		}

		// UTC instants where the zone offset changes, between two years.
		std::vector<ISC_TIMESTAMP> getTransitions(int fromYear, int toYear)
		{
			ISC_TIMESTAMP_TZ from, to;
			from.utc_timestamp = makeTimeStamp(fromYear, 1, 1);
			from.time_zone = TimeZoneUtil::GMT_ZONE;
			to.utc_timestamp = makeTimeStamp(toYear, 1, 1);
			to.time_zone = TimeZoneUtil::GMT_ZONE;

			std::vector<ISC_TIMESTAMP> transitions;
			TimeZoneRuleIterator iterator(zone, from, to);
			bool first = true;

			while (iterator.next())
			{
				// The first rule starts before fromYear.
				if (!first)
					transitions.push_back(iterator.startTimestamp.utc_timestamp);

				first = false;
			}

			return transitions;
		}

	private:
		SSHORT getDisplacement(UErrorCode& icuErrorCode)
		{
			const SSHORT displacement = (icuLib.ucalGet(calendar, UCAL_ZONE_OFFSET, &icuErrorCode) +
				icuLib.ucalGet(calendar, UCAL_DST_OFFSET, &icuErrorCode)) / U_MILLIS_PER_MINUTE;
			BOOST_REQUIRE(U_SUCCESS(icuErrorCode));
			return displacement;
		}

	public:
		const char* const zoneName;
		const USHORT zone;

	private:
		UnicodeUtil::ConversionICU& icuLib;
		UCalendar* calendar = nullptr;
	};

	// UTC -> local, through extractOffset and decodeTimeStamp.
	bool checkUtc(IcuReference& ref, ISC_TIMESTAMP utc)
	{
		const SSHORT expected = ref.utcDisplacement(utc);

		ISC_TIMESTAMP_TZ tsTz;
		tsTz.utc_timestamp = utc;
		tsTz.time_zone = ref.zone;

		SSHORT offset;
		TimeZoneUtil::extractOffset(tsTz, &offset);

		struct tm times;
		int fractions;
		TimeZoneUtil::decodeTimeStamp(tsTz, false, TimeZoneUtil::NO_OFFSET, &times, &fractions);
		const auto local = NoThrowTimeStamp::encode_timestamp(&times, fractions);

		const bool ok = offset == expected &&
			toTicks(local) == toTicks(utc) + expected * TICKS_PER_MINUTE;

		// Avoid BOOST_TEST overhead in the many successful checks.
		if (!ok)
		{
			BOOST_ERROR(ref.zoneName << " UTC " << toString(utc) <<
				": offset " << offset << ", expected " << expected << ", local " << toString(local));
		}

		return ok;
	}

	// Local -> UTC, through localTimeStampToUtc.
	bool checkLocal(IcuReference& ref, ISC_TIMESTAMP local)
	{
		const auto expected = ref.localToUtc(local);

		ISC_TIMESTAMP_TZ tsTz;
		tsTz.utc_timestamp = local;
		tsTz.time_zone = ref.zone;
		TimeZoneUtil::localTimeStampToUtc(tsTz);

		const bool ok = toTicks(tsTz.utc_timestamp) == toTicks(expected);

		if (!ok)
		{
			BOOST_ERROR(ref.zoneName << " local " << toString(local) <<
				": UTC " << toString(tsTz.utc_timestamp) << ", expected " << toString(expected));
		}

		return ok;
	}

	// UTC instants at and around each transition.
	void checkUtcAroundTransitions(IcuReference& ref, const std::vector<ISC_TIMESTAMP>& transitions)
	{
		static const SINT64 DELTAS[] = {
			-TICKS_PER_HOUR, -TICKS_PER_SECOND, -1, 0, 1, TICKS_PER_SECOND, TICKS_PER_HOUR
		};

		for (const auto transition : transitions)
		{
			for (const auto delta : DELTAS)
				checkUtc(ref, fromTicks(toTicks(transition) + delta));
		}
	}

	// Wall times around each transition, including the ones in gaps, overlaps and in the cache margin
	// (2 days) after the interval start. They are dense only near the transition.
	void checkLocalAroundTransitions(IcuReference& ref, const std::vector<ISC_TIMESTAMP>& transitions)
	{
		for (const auto transition : transitions)
		{
			const SINT64 transitionTicks = toTicks(transition);

			// Wall times where the offset changes. Transitions are in whole seconds and, before 1970,
			// a tick less is still the transition ICU millisecond.
			const SSHORT previousDisplacement = ref.utcDisplacement(fromTicks(transitionTicks - TICKS_PER_SECOND));
			const SINT64 before = transitionTicks + previousDisplacement * TICKS_PER_MINUTE;
			const SINT64 after = transitionTicks + ref.utcDisplacement(transition) * TICKS_PER_MINUTE;

			for (const auto wall : {before, after})
			{
				for (SINT64 delta = -2 * TICKS_PER_SECOND; delta <= 2 * TICKS_PER_SECOND; delta += TICKS_PER_SECOND / 2)
					checkLocal(ref, fromTicks(wall + delta));
			}

			for (SINT64 delta = -3 * TICKS_PER_HOUR; delta <= 3 * TICKS_PER_HOUR; delta += 15 * TICKS_PER_MINUTE)
				checkLocal(ref, fromTicks(before + delta));

			for (SINT64 delta = -54 * TICKS_PER_HOUR; delta <= 54 * TICKS_PER_HOUR; delta += 2 * TICKS_PER_HOUR)
				checkLocal(ref, fromTicks(before + delta));
		}
	}
}	// namespace


BOOST_AUTO_TEST_CASE(UtcThenLocalTest)
{
	// UTC conversions fill the cache, then local conversions use it.
	for (const auto zoneName : {"Europe/Warsaw", "America/Sao_Paulo", "Australia/Lord_Howe", "Europe/Dublin"})
	{
		IcuReference ref(zoneName);
		const auto transitions = ref.getTransitions(1850, 2040);
		BOOST_TEST_REQUIRE(transitions.size() > 10u);

		checkUtcAroundTransitions(ref, transitions);
		checkLocalAroundTransitions(ref, transitions);
	}
}

BOOST_AUTO_TEST_CASE(LocalThenUtcTest)
{
	// Local conversions start with an empty cache and fill it, then everything is checked again.
	for (const auto zoneName : {"America/New_York", "Asia/Tehran", "America/St_Johns"})
	{
		IcuReference ref(zoneName);
		const auto transitions = ref.getTransitions(1850, 2040);
		BOOST_TEST_REQUIRE(transitions.size() > 10u);

		checkLocalAroundTransitions(ref, transitions);
		checkUtcAroundTransitions(ref, transitions);
		checkLocalAroundTransitions(ref, transitions);
	}
}

BOOST_AUTO_TEST_CASE(LargeOffsetChangeTest)
{
	// Offset changes of about one day, that skip or repeat a whole date.
	for (const auto zoneName : {"Pacific/Apia", "Pacific/Kiritimati", "Pacific/Kwajalein", "America/Juneau"})
	{
		IcuReference ref(zoneName);
		const auto transitions = ref.getTransitions(1850, 2040);
		BOOST_TEST_REQUIRE(!transitions.empty());

		checkUtcAroundTransitions(ref, transitions);
		checkLocalAroundTransitions(ref, transitions);
	}

	// Samoa skipped 2011-12-30.
	IcuReference apia("Pacific/Apia");
	checkLocal(apia, makeTimeStamp(2011, 12, 30, 12));
}

BOOST_AUTO_TEST_CASE(LargeOverlapMarginTest)
{
	// Alaska repeated almost a whole day (offset from +14:58 to -9:01) in 1867.
	// With only the interval after the transition cached, wall times in the overlap must not use it,
	// as UCAL_WALLTIME_FIRST chooses the interval before the transition.
	IcuReference ref("America/Sitka");
	const auto transitions = ref.getTransitions(1850, 1870);
	BOOST_TEST_REQUIRE(transitions.size() == 1u);

	const SINT64 transitionTicks = toTicks(transitions[0]);
	checkUtc(ref, fromTicks(transitionTicks + 24 * TICKS_PER_HOUR));

	const SINT64 overlapStart = transitionTicks + ref.utcDisplacement(transitions[0]) * TICKS_PER_MINUTE;
	const SINT64 overlapEnd = transitionTicks + ref.utcDisplacement(fromTicks(transitionTicks - TICKS_PER_SECOND)) * TICKS_PER_MINUTE;
	BOOST_TEST_REQUIRE(overlapEnd - overlapStart > 23 * TICKS_PER_HOUR);

	for (SINT64 wall = overlapStart; wall < overlapEnd + 3 * TICKS_PER_HOUR; wall += 5 * TICKS_PER_MINUTE)
		checkLocal(ref, fromTicks(wall));
}

BOOST_AUTO_TEST_CASE(DstGapAndOverlapTest)
{
	IcuReference ref("Europe/Berlin");

	// Fill the cache with the intervals before and after the transitions.
	for (const auto& utc : {makeTimeStamp(2026, 3, 1), makeTimeStamp(2026, 6, 1), makeTimeStamp(2026, 12, 1)})
		checkUtc(ref, utc);

	// UCAL_WALLTIME_FIRST uses the offset before the transition for skipped and repeated wall times.
	const auto toUtc = [&](ISC_TIMESTAMP local) {
		checkLocal(ref, local);

		ISC_TIMESTAMP_TZ tsTz;
		tsTz.utc_timestamp = local;
		tsTz.time_zone = ref.zone;
		TimeZoneUtil::localTimeStampToUtc(tsTz);

		return toString(tsTz.utc_timestamp);
	};

	BOOST_TEST(toUtc(makeTimeStamp(2026, 3, 29, 1, 59, 59)) == "2026-03-29 00:59:59.0000");
	BOOST_TEST(toUtc(makeTimeStamp(2026, 3, 29, 2, 30)) == "2026-03-29 01:30:00.0000");
	BOOST_TEST(toUtc(makeTimeStamp(2026, 3, 29, 3, 0)) == "2026-03-29 01:00:00.0000");
	BOOST_TEST(toUtc(makeTimeStamp(2026, 10, 25, 1, 59, 59)) == "2026-10-24 23:59:59.0000");
	BOOST_TEST(toUtc(makeTimeStamp(2026, 10, 25, 2, 30)) == "2026-10-25 00:30:00.0000");
	BOOST_TEST(toUtc(makeTimeStamp(2026, 10, 25, 3, 0)) == "2026-10-25 02:00:00.0000");
}

BOOST_AUTO_TEST_CASE(RandomAccessTest)
{
	// Random order inserts intervals out of order and evicts the recent intervals,
	// then the same sequence is repeated to be answered from the cached intervals.
	for (const auto zoneName : {"Europe/Lisbon", "America/Santiago"})
	{
		IcuReference ref(zoneName);
		const SINT64 lo = toTicks(makeTimeStamp(1800, 1, 1));
		const SINT64 hi = toTicks(makeTimeStamp(2200, 1, 1));

		for (int pass = 0; pass < 2; ++pass)
		{
			Random random(12345);

			for (int i = 0; i < 20000; ++i)
			{
				const auto ts = fromTicks(random.next(lo, hi));

				if (i % 2 == 0)
					checkUtc(ref, ts);
				else
					checkLocal(ref, ts);
			}
		}
	}
}

BOOST_AUTO_TEST_CASE(AlternatingIntervalsTest)
{
	// More intervals than the recent ones alternating, as TIME WITH TIME ZONE conversions do with
	// TIME_TZ_BASE_DATE and the current date.
	IcuReference ref("America/Chicago");

	const ISC_TIMESTAMP dates[] = {
		makeTimeStamp(2020, 1, 1, 12),
		makeTimeStamp(2026, 7, 15, 12),
		makeTimeStamp(1990, 4, 1, 12),
		makeTimeStamp(2026, 12, 15, 12)
	};

	for (unsigned count = 2; count <= std::size(dates); ++count)
	{
		for (int i = 0; i < 100; ++i)
		{
			const auto& date = dates[i % count];
			const auto ts = fromTicks(toTicks(date) + (i % 7) * TICKS_PER_HOUR);

			checkUtc(ref, ts);
			checkLocal(ref, ts);
		}
	}
}

BOOST_AUTO_TEST_CASE(UnboundedIntervalsTest)
{
	// Intervals before the first and after the last transition, and zones without transitions.
	for (const auto zoneName : {"Asia/Tokyo", "Etc/GMT+5", "Europe/Moscow"})
	{
		IcuReference ref(zoneName);

		for (const auto& ts : {
			makeTimeStamp(1, 1, 3), makeTimeStamp(1, 6, 1), makeTimeStamp(1582, 10, 10),
			makeTimeStamp(1800, 1, 1), makeTimeStamp(1850, 1, 1),
			makeTimeStamp(2500, 1, 1), makeTimeStamp(9999, 6, 1), makeTimeStamp(9999, 12, 29)})
		{
			checkUtc(ref, ts);
			checkLocal(ref, ts);
		}
	}
}

BOOST_AUTO_TEST_CASE(ConcurrentAccessTest)
{
	// Many threads fill and read the cache of the same zone.
	IcuReference ref("Europe/Paris");

	constexpr unsigned THREADS = 8;
	constexpr unsigned COUNT = 20000;

	const SINT64 lo = toTicks(makeTimeStamp(1850, 1, 1));
	const SINT64 hi = toTicks(makeTimeStamp(2100, 1, 1));

	struct Sample
	{
		ISC_TIMESTAMP ts;
		SSHORT utcDisplacement;
		ISC_TIMESTAMP localToUtc;
	};

	// Expected results are computed before as BOOST_TEST is not thread-safe.
	std::vector<Sample> samples;
	Random random(54321);

	for (unsigned i = 0; i < COUNT; ++i)
	{
		const auto ts = fromTicks(random.next(lo, hi));
		samples.push_back({ts, ref.utcDisplacement(ts), ref.localToUtc(ts)});
	}

	std::atomic<unsigned> errors = 0;
	std::vector<std::thread> threads;

	for (unsigned t = 0; t < THREADS; ++t)
	{
		threads.emplace_back([&, t] {
			try
			{
				for (unsigned i = 0; i < COUNT; ++i)
				{
					// Each thread goes in a different order.
					const auto& sample = samples[(i * (2 * t + 1) + t * 997) % COUNT];

					ISC_TIMESTAMP_TZ tsTz;
					tsTz.utc_timestamp = sample.ts;
					tsTz.time_zone = ref.zone;

					SSHORT offset;
					TimeZoneUtil::extractOffset(tsTz, &offset);

					if (offset != sample.utcDisplacement)
						++errors;

					TimeZoneUtil::localTimeStampToUtc(tsTz);

					if (toTicks(tsTz.utc_timestamp) != toTicks(sample.localToUtc))
						++errors;
				}
			}
			catch (...)
			{
				++errors;
			}
		});
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_TEST(errors.load() == 0u);
}

BOOST_AUTO_TEST_SUITE_END()	// TimeZoneCacheTests
BOOST_AUTO_TEST_SUITE_END()	// TimeZoneUtilSuite
