/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			TimeZoneUtil.h
 *	DESCRIPTION:	Time zone utility functions.
 *
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
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_TIME_ZONE_UTIL_H
#define COMMON_TIME_ZONE_UTIL_H

#include <functional>
#include "../common/classes/fb_string.h"
#include "../common/classes/timestamp.h"
#include "../common/cvt.h"
#include "../common/unicode_util.h"

 // struct tm declaration
#if defined(TIME_WITH_SYS_TIME)
#include <sys/time.h>
#include <time.h>
#else
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

namespace Firebird {

class NoThrowTimeStamp;

class TimeZoneUtil
{
public:
	static const ISC_DATE TIME_TZ_BASE_DATE;
	static const char GMT_FALLBACK[5];	// "GMT*"

	static const USHORT GMT_ZONE = 65535;
	static const unsigned MAX_LEN = 32;
	static const unsigned MAX_SIZE = MAX_LEN + 1;

private:
	static InitInstance<PathName> tzDataPath;

public:
	static UDate timeStampToIcuDate(ISC_TIMESTAMP ts)
	{
		return (TimeStamp::timeStampToTicks(ts) -
			((TimeStamp::UNIX_DATE - TimeStamp::MIN_DATE) * TimeStamp::ISC_TICKS_PER_DAY)) /
			(ISC_TIME_SECONDS_PRECISION / 1000);
	}

	static ISC_TIMESTAMP icuDateToTimeStamp(UDate icuDate)
	{
		return TimeStamp::ticksToTimeStamp(icuDate * (ISC_TIME_SECONDS_PRECISION / 1000) +
			((TimeStamp::UNIX_DATE - TimeStamp::MIN_DATE) * TimeStamp::ISC_TICKS_PER_DAY));
	}

	static void initTimeZoneEnv();
	static const PathName& getTzDataPath();

	static USHORT getSystemTimeZone();

	static void getDatabaseVersion(Firebird::string& str);

	static void iterateRegions(std::function<void (USHORT, const char*)> func);

	static USHORT parse(const char* str, unsigned strLen, bool strict = true);
	static USHORT parseRegion(const char* str, unsigned strLen);

	static unsigned format(char* buffer, size_t bufferSize, USHORT timeZone,
		bool fallback = false, SLONG offset = NO_OFFSET);

	static bool isValidOffset(int sign, unsigned tzh, unsigned tzm);

	static void extractOffset(const ISC_TIMESTAMP_TZ& timeStampTz, int* sign, unsigned* tzh, unsigned* tzm);
	static void extractOffset(const ISC_TIMESTAMP_TZ& timeStampTz, SSHORT* offset);
	static void extractOffset(const ISC_TIME_TZ& timeTz, SSHORT* offset);

	static void localTimeToUtc(ISC_TIME& time, ISC_USHORT timeZone);
	static void localTimeToUtc(ISC_TIME_TZ& timeTz);

	static void localTimeStampToUtc(ISC_TIMESTAMP& timeStamp, Callbacks* cb);
	static void localTimeStampToUtc(ISC_TIMESTAMP_TZ& timeStampTz);

	static const SLONG NO_OFFSET = MAX_SLONG;

	static bool decodeTime(const ISC_TIME_TZ& timeTz, bool gmtFallback, SLONG gmtOffset,
		struct tm* times, int* fractions = NULL);
	static bool decodeTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, bool gmtFallback, SLONG gmtOffset,
		struct tm* times, int* fractions = NULL);

	static ISC_TIMESTAMP_TZ getCurrentSystemTimeStamp();
	static ISC_TIMESTAMP_TZ getCurrentGmtTimeStamp();

	static void validateGmtTimeStamp(NoThrowTimeStamp& ts);

	static ISC_TIME timeTzToTime(const ISC_TIME_TZ& timeTz, Callbacks* cb);
	static ISC_TIMESTAMP timeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, USHORT toTimeZone);

	static ISC_TIMESTAMP_TZ timeToTimeStampTz(const ISC_TIME& time, Callbacks* cb);
	static ISC_TIME_TZ timeToTimeTz(const ISC_TIME& time, Callbacks* cb);

	static ISC_TIMESTAMP_TZ timeTzToTimeStampTz(const ISC_TIME_TZ& timeTz, Callbacks* cb);
	static ISC_TIMESTAMP timeTzToTimeStamp(const ISC_TIME_TZ& timeTz, Callbacks* cb);

	static ISC_TIME_TZ timeStampTzToTimeTz(const ISC_TIMESTAMP_TZ& timeStampTz);
	static ISC_TIMESTAMP timeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, Callbacks* cb);

	static ISC_TIMESTAMP_TZ timeStampToTimeStampTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb);
	static ISC_TIME_TZ timeStampToTimeTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb);

	static ISC_TIMESTAMP_TZ dateToTimeStampTz(const ISC_DATE& date, Callbacks* cb);
};

class TimeZoneRuleIterator
{
public:
	TimeZoneRuleIterator(USHORT aId, const ISC_TIMESTAMP_TZ& aFrom, const ISC_TIMESTAMP_TZ& aTo);
	~TimeZoneRuleIterator();

public:
	bool next();

public:
	ISC_TIMESTAMP_TZ startTimestamp;
	ISC_TIMESTAMP_TZ endTimestamp;
	SSHORT zoneOffset;
	SSHORT dstOffset;

private:
	const USHORT id;
	Jrd::UnicodeUtil::ConversionICU& icuLib;
	SINT64 startTicks;
	SINT64 toTicks;
	UCalendar* icuCalendar;
	UDate icuDate;
};

}	// namespace Firebird

#endif	// COMMON_TIME_ZONE_UTIL_H
