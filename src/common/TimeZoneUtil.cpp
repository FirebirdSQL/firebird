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

#include "firebird.h"
#include "../common/TimeZoneUtil.h"
#include "../common/StatusHolder.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/timestamp.h"
#include "../common/classes/GenericMap.h"
#include "../common/config/config.h"
#include "../common/os/path_utils.h"
#include "unicode/ucal.h"

#ifdef TZ_UPDATE
#include "../common/classes/objects_array.h"
#endif

using namespace Firebird;

//-------------------------------------

namespace
{
	struct TimeZoneDesc
	{
		const char* asciiName;
		const UChar* icuName;
	};
}	// namespace

#include "./TimeZones.h"

//-------------------------------------

static const TimeZoneDesc* getDesc(USHORT timeZone);
static inline bool isOffset(USHORT timeZone);
static USHORT makeFromOffset(int sign, unsigned tzh, unsigned tzm);
static inline SSHORT offsetZoneToDisplacement(USHORT timeZone);
static inline USHORT displacementToOffsetZone(SSHORT displacement);
static int parseNumber(const char*& p, const char* end);
static void skipSpaces(const char*& p, const char* end);

//-------------------------------------

namespace
{
	struct TimeZoneStartup
	{
		TimeZoneStartup(MemoryPool& pool)
			: nameIdMap(pool)
		{
#if defined DEV_BUILD && defined TZ_UPDATE
			tzUpdate();
#endif

			for (USHORT i = 0; i < FB_NELEM(TIME_ZONE_LIST); ++i)
			{
				string s(TIME_ZONE_LIST[i].asciiName);
				s.upper();
				nameIdMap.put(s, i);
			}
		}

		bool getId(string name, USHORT& id)
		{
			USHORT index;
			name.upper();

			if (nameIdMap.get(name, index))
			{
				id = MAX_USHORT - index;
				return true;
			}
			else
				return false;
		}

#if defined DEV_BUILD && defined TZ_UPDATE
		void tzUpdate()
		{
			SortedObjectsArray<string> currentZones, icuZones;

			for (unsigned i = 0; i < FB_NELEM(TIME_ZONE_LIST); ++i)
				currentZones.push(TIME_ZONE_LIST[i].asciiName);

			Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();
			UErrorCode icuErrorCode = U_ZERO_ERROR;

			UEnumeration* uenum = icuLib.ucalOpenTimeZones(&icuErrorCode);
			int32_t length;

			while (const UChar* str = icuLib.uenumUnext(uenum, &length, &icuErrorCode))
			{
				char buffer[256];

				for (int i = 0; i <= length; ++i)
					buffer[i] = (char) str[i];

				icuZones.push(buffer);
			}

			icuLib.uenumClose(uenum);

			for (auto const& zone : currentZones)
			{
				FB_SIZE_T pos;

				if (icuZones.find(zone, pos))
					icuZones.remove(pos);
				else
					printf("--> %s does not exist in ICU.\n", zone.c_str());
			}

			ObjectsArray<string> newZones;

			for (int i = 0; i < FB_NELEM(TIME_ZONE_LIST); ++i)
				newZones.push(TIME_ZONE_LIST[i].asciiName);

			for (auto const& zone : icuZones)
				newZones.push(zone);

			printf("// The content of this file is generated with help of macro TZ_UPDATE.\n\n");

			int index = 0;

			for (auto const& zone : newZones)
			{
				printf("static const UChar TZSTR_%d[] = {", index);

				for (int i = 0; i < zone.length(); ++i)
					printf("'%c', ", zone[i]);

				printf("'\\0'};\n");

				++index;
			}

			printf("\n");

			printf("// Do not change order of items in this array! The index corresponds to a TimeZone ID, which must be fixed!\n");
			printf("static const TimeZoneDesc TIME_ZONE_LIST[] = {");

			index = 0;

			for (auto const& zone : newZones)
			{
				printf("%s\n\t{\"%s\", TZSTR_%d}", (index == 0 ? "" : ","), zone.c_str(), index);
				++index;
			}

			printf("\n");
			printf("};\n\n");
		}
#endif	// defined DEV_BUILD && defined TZ_UPDATE

	private:
		GenericMap<Pair<Left<string, USHORT> > > nameIdMap;
	};
}	// namespace

//-------------------------------------

static const UDate MIN_ICU_TIMESTAMP = TimeZoneUtil::timeStampToIcuDate(TimeStamp::MIN_TIMESTAMP);
static const UDate MAX_ICU_TIMESTAMP = TimeZoneUtil::timeStampToIcuDate(TimeStamp::MAX_TIMESTAMP);
static const unsigned ONE_DAY = 24 * 60 - 1;	// used for offset encoding
static InitInstance<TimeZoneStartup> timeZoneStartup;

//-------------------------------------

const char TimeZoneUtil::GMT_FALLBACK[5] = "GMT*";
InitInstance<PathName> TimeZoneUtil::tzDataPath;

void TimeZoneUtil::initTimeZoneEnv()
{
	PathName path;

	// Could not call fb_utils::getPrefix here.
	if (FB_TZDATADIR[0])
		path = FB_TZDATADIR;
	else
		PathUtils::concatPath(path, Config::getRootDirectory(), "tzdata");

	const static char* const ICU_TIMEZONE_FILES_DIR = "ICU_TIMEZONE_FILES_DIR";

	// Do not update ICU_TIMEZONE_FILES_DIR if it's already set.
	fb_utils::setenv(ICU_TIMEZONE_FILES_DIR, path.c_str(), false);
	fb_utils::readenv(ICU_TIMEZONE_FILES_DIR, tzDataPath());
}

const PathName& TimeZoneUtil::getTzDataPath()
{
	return tzDataPath();
}

// Return the current user's time zone.
USHORT TimeZoneUtil::getSystemTimeZone()
{
	static volatile bool cachedError = false;
	static volatile USHORT cachedTimeZoneId = TimeZoneUtil::GMT_ZONE;
	static volatile int32_t cachedTimeZoneNameLen = -1;
	static UChar cachedTimeZoneName[TimeZoneUtil::MAX_SIZE];
	static GlobalPtr<RWLock> lock;

	if (cachedError)
		return cachedTimeZoneId;

	// ASF: The code below in this function is prepared to detect changes in OS time zone or config setting, but
	// the called functions are not. So cache and return directly the previously detected time zone.
	if (cachedTimeZoneNameLen != -1)
		return cachedTimeZoneId;

	UErrorCode icuErrorCode = U_ZERO_ERROR;
	Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();

	UChar buffer[TimeZoneUtil::MAX_SIZE];
	int32_t len;
	const char* configDefault = Config::getDefaultTimeZone();

	if (configDefault && configDefault[0])
	{
		UChar* dst = buffer;

		for (const char* src = configDefault; src - configDefault < TimeZoneUtil::MAX_SIZE && *src; ++src, ++dst)
			*dst = *src;

		*dst = 0;
		len = dst - buffer;
	}
	else
		len = icuLib.ucalGetDefaultTimeZone(buffer, FB_NELEM(buffer), &icuErrorCode);

	ReadLockGuard readGuard(lock, "TimeZoneUtil::getSystemTimeZone");

	if (!U_FAILURE(icuErrorCode) &&
		cachedTimeZoneNameLen != -1 &&
		len == cachedTimeZoneNameLen &&
		memcmp(buffer, cachedTimeZoneName, len * sizeof(USHORT)) == 0)
	{
		return cachedTimeZoneId;
	}

	readGuard.release();
	WriteLockGuard writeGuard(lock, "TimeZoneUtil::getSystemTimeZone");

	string bufferStrAscii;

	if (!U_FAILURE(icuErrorCode))
	{
		bool error;
		string bufferStrUnicode(reinterpret_cast<const char*>(buffer), len * sizeof(USHORT));
		bufferStrAscii = IntlUtil::convertUtf16ToAscii(bufferStrUnicode, &error);
		USHORT id;

		if (timeZoneStartup().getId(bufferStrAscii, id))
		{
			memcpy(cachedTimeZoneName, buffer, len * sizeof(USHORT));
			cachedTimeZoneId = id;
			cachedTimeZoneNameLen = len;
			return cachedTimeZoneId;
		}
	}
	else
		icuErrorCode = U_ZERO_ERROR;

	gds__log("ICU error (%d) retrieving the system time zone (%s). Falling back to displacement.",
		int(icuErrorCode), bufferStrAscii.c_str());

	UCalendar* icuCalendar = icuLib.ucalOpen(NULL, -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

	if (!icuCalendar)
	{
		gds__log("ICU's ucal_open error opening the default callendar.");
		cachedError = true;
		return cachedTimeZoneId;	// GMT
	}

	int32_t displacement = (icuLib.ucalGet(icuCalendar, UCAL_ZONE_OFFSET, &icuErrorCode) +
		icuLib.ucalGet(icuCalendar, UCAL_DST_OFFSET, &icuErrorCode)) / U_MILLIS_PER_MINUTE;

	icuLib.ucalClose(icuCalendar);

	if (!U_FAILURE(icuErrorCode))
	{
		int sign = displacement < 0 ? -1 : 1;
		unsigned tzh = (unsigned) abs(int(displacement / 60));
		unsigned tzm = (unsigned) abs(int(displacement % 60));
		cachedTimeZoneId = makeFromOffset(sign, tzh, tzm);
	}
	else
		gds__log("Cannot retrieve the system time zone: %d.", int(icuErrorCode));

	cachedError = true;

	return cachedTimeZoneId;
}

void TimeZoneUtil::getDatabaseVersion(Firebird::string& str)
{
	Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();
	UErrorCode icuErrorCode = U_ZERO_ERROR;

	const char* version = icuLib.ucalGetTZDataVersion(&icuErrorCode);

	if (U_FAILURE(icuErrorCode))
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_getTZDataVersion.");

	str = version;
}

void TimeZoneUtil::iterateRegions(std::function<void (USHORT, const char*)> func)
{
	for (USHORT i = 0; i < FB_NELEM(TIME_ZONE_LIST); ++i)
		func(MAX_USHORT - i, TIME_ZONE_LIST[i].asciiName);
}

// Parses a time zone, offset- or region-based.
USHORT TimeZoneUtil::parse(const char* str, unsigned strLen)
{
	const char* end = str + strLen;
	const char* p = str;

	skipSpaces(p, end);

	int sign = 1;
	bool signPresent = false;

	if (p < end && (*p == '-' || *p == '+'))
	{
		signPresent = true;
		sign = *p == '-' ? -1 : 1;
		++p;
		skipSpaces(p, end);
	}

	if (p < end && (signPresent || (*p >= '0' && *p <= '9')))
	{
		int tzh = parseNumber(p, end);
		int tzm = 0;

		skipSpaces(p, end);

		if (p < end && *p == ':')
		{
			++p;
			skipSpaces(p, end);
			tzm = (unsigned) parseNumber(p, end);
			skipSpaces(p, end);
		}

		if (p != end)
			status_exception::raise(Arg::Gds(isc_invalid_timezone_offset) << string(str, strLen));

		return makeFromOffset(sign, tzh, tzm);
	}
	else
		return parseRegion(p, str + strLen - p);
}

// Parses a time zone id from a region string.
USHORT TimeZoneUtil::parseRegion(const char* str, unsigned strLen)
{
	const char* end = str + strLen;

	skipSpaces(str, end);

	const char* start = str;

	while (str < end &&
		((*str >= 'a' && *str <= 'z') ||
		 (*str >= 'A' && *str <= 'Z') ||
		 *str == '_' ||
		 *str == '/' ||
		 (str != start && *str >= '0' && *str <= '9') ||
		 (str != start && *str == '+') ||
		 (str != start && *str == '-')))
	{
		++str;
	}

	unsigned len = str - start;

	skipSpaces(str, end);

	if (str == end)
	{
		string s(start, len);
		USHORT id;

		if (timeZoneStartup().getId(s, id))
			return id;
	}

	status_exception::raise(Arg::Gds(isc_invalid_timezone_region) << string(start, len));
	return 0;
}

// Format a time zone to string, as offset or region.
unsigned TimeZoneUtil::format(char* buffer, size_t bufferSize, USHORT timeZone, bool fallback, SLONG offset)
{
	char* p = buffer;

	if (fallback)
	{
		if (offset == NO_OFFSET)
			p += fb_utils::snprintf(p, bufferSize - (p - buffer), "%s", GMT_FALLBACK);
		else
		{
			if (offset != 0)
				*p++ = offset < 0 ? '-' : '+';

			if (offset < 0)
				offset = -offset;

			int minutes = offset % 60;
			offset /= 60;
			p += fb_utils::snprintf(p, bufferSize - (p - buffer), "%02d:%02d", offset, minutes);
		}
	}
	else if (isOffset(timeZone))
	{
		SSHORT displacement = offsetZoneToDisplacement(timeZone);

		*p++ = displacement < 0 ? '-' : '+';

		if (displacement < 0)
			displacement = -displacement;

		p += fb_utils::snprintf(p, bufferSize - 1, "%2.2d:%2.2d", displacement / 60, displacement % 60);
	}
	else
	{
		strncpy(buffer, getDesc(timeZone)->asciiName, bufferSize);

		p += strlen(buffer);
	}

	return p - buffer;
}

// Returns if the offsets are valid.
bool TimeZoneUtil::isValidOffset(int sign, unsigned tzh, unsigned tzm)
{
	fb_assert(sign >= -1 && sign <= 1);
	return tzm <= 59 && (tzh < 14 || (tzh == 14 && tzm == 0));
}

// Extracts the offsets from a offset- or region-based datetime with time zone.
void TimeZoneUtil::extractOffset(const ISC_TIMESTAMP_TZ& timeStampTz, int* sign, unsigned* tzh, unsigned* tzm)
{
	SSHORT displacement;
	extractOffset(timeStampTz, &displacement);

	*sign = displacement < 0 ? -1 : 1;
	displacement = displacement < 0 ? -displacement : displacement;

	*tzh = displacement / 60;
	*tzm = displacement % 60;
}

// Extracts the offset (+- minutes) from a offset- or region-based datetime with time zone.
void TimeZoneUtil::extractOffset(const ISC_TIMESTAMP_TZ& timeStampTz, SSHORT* offset)
{
	SSHORT displacement;

	if (timeStampTz.time_zone == GMT_ZONE)
		displacement = 0;
	else if (isOffset(timeStampTz.time_zone))
		displacement = offsetZoneToDisplacement(timeStampTz.time_zone);
	else
	{
		UErrorCode icuErrorCode = U_ZERO_ERROR;

		Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();

		UCalendar* icuCalendar = icuLib.ucalOpen(
			getDesc(timeStampTz.time_zone)->icuName, -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

		if (!icuCalendar)
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_open.");

		icuLib.ucalSetMillis(icuCalendar, timeStampToIcuDate(timeStampTz.utc_timestamp), &icuErrorCode);

		if (U_FAILURE(icuErrorCode))
		{
			icuLib.ucalClose(icuCalendar);
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_setMillis.");
		}

		displacement = (icuLib.ucalGet(icuCalendar, UCAL_ZONE_OFFSET, &icuErrorCode) +
			icuLib.ucalGet(icuCalendar, UCAL_DST_OFFSET, &icuErrorCode)) / U_MILLIS_PER_MINUTE;

		if (U_FAILURE(icuErrorCode))
		{
			icuLib.ucalClose(icuCalendar);
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_get.");
		}

		icuLib.ucalClose(icuCalendar);
	}

	*offset = displacement;
}

// Converts a time-tz to a time in a given zone.
ISC_TIME TimeZoneUtil::timeTzToTime(const ISC_TIME_TZ& timeTz, USHORT toTimeZone, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tempTimeStampTz;
	tempTimeStampTz.utc_timestamp.timestamp_date = cb->getLocalDate();
	tempTimeStampTz.utc_timestamp.timestamp_time = 0;
	tempTimeStampTz.time_zone = cb->getSessionTimeZone();
	TimeZoneUtil::localTimeStampToUtc(tempTimeStampTz);

	tempTimeStampTz.utc_timestamp.timestamp_time = timeTz.utc_time;
	tempTimeStampTz.time_zone = timeTz.time_zone;

	return timeStampTzToTimeStamp(tempTimeStampTz, toTimeZone).timestamp_time;
}

// Converts a timestamp-tz to a timestamp in a given zone.
ISC_TIMESTAMP TimeZoneUtil::timeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, USHORT toTimeZone)
{
	ISC_TIMESTAMP_TZ tempTimeStampTz = timeStampTz;
	tempTimeStampTz.time_zone = toTimeZone;

	struct tm times;
	int fractions;
	decodeTimeStamp(tempTimeStampTz, false, TimeZoneUtil::NO_OFFSET, &times, &fractions);

	return TimeStamp::encode_timestamp(&times, fractions);
}

// Converts a time from local to UTC.
void TimeZoneUtil::localTimeToUtc(ISC_TIME& time, Callbacks* cb)
{
	ISC_TIME_TZ timeTz;
	timeTz.utc_time = time;
	timeTz.time_zone = cb->getSessionTimeZone();
	localTimeToUtc(timeTz, cb);

	time = timeTz.utc_time;
}

// Converts a time from local to UTC.
void TimeZoneUtil::localTimeToUtc(ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tempTimeStampTz;
	tempTimeStampTz.utc_timestamp.timestamp_date = cb->getCurrentGmtTimeStamp().timestamp_date;
	tempTimeStampTz.utc_timestamp.timestamp_time = timeTz.utc_time;
	tempTimeStampTz.time_zone = timeTz.time_zone;
	localTimeStampToUtc(tempTimeStampTz);

	timeTz.utc_time = tempTimeStampTz.utc_timestamp.timestamp_time;
}

// Converts a timestamp from its local datetime fields to UTC.
void TimeZoneUtil::localTimeStampToUtc(ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tempTimeStampTz;
	tempTimeStampTz.utc_timestamp.timestamp_date = timeStamp.timestamp_date;
	tempTimeStampTz.utc_timestamp.timestamp_time = timeStamp.timestamp_time;
	tempTimeStampTz.time_zone = cb->getSessionTimeZone();

	localTimeStampToUtc(tempTimeStampTz);

	timeStamp.timestamp_date = tempTimeStampTz.utc_timestamp.timestamp_date;
	timeStamp.timestamp_time = tempTimeStampTz.utc_timestamp.timestamp_time;
}

// Converts a timestamp from its local datetime fields to UTC.
void TimeZoneUtil::localTimeStampToUtc(ISC_TIMESTAMP_TZ& timeStampTz)
{
	int displacement;

	if (timeStampTz.time_zone == GMT_ZONE)
		return;
	else if (isOffset(timeStampTz.time_zone))
		displacement = offsetZoneToDisplacement(timeStampTz.time_zone);
	else
	{
		tm times;
		TimeStamp::decode_timestamp(*(ISC_TIMESTAMP*) &timeStampTz, &times, nullptr);

		UErrorCode icuErrorCode = U_ZERO_ERROR;

		Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();

		UCalendar* icuCalendar = icuLib.ucalOpen(
			getDesc(timeStampTz.time_zone)->icuName, -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

		if (!icuCalendar)
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_open.");

		icuLib.ucalSetAttribute(icuCalendar, UCAL_REPEATED_WALL_TIME, UCAL_WALLTIME_FIRST);
		icuLib.ucalSetAttribute(icuCalendar, UCAL_SKIPPED_WALL_TIME, UCAL_WALLTIME_FIRST);

		icuLib.ucalSetDateTime(icuCalendar, 1900 + times.tm_year, times.tm_mon, times.tm_mday,
			times.tm_hour, times.tm_min, times.tm_sec, &icuErrorCode);

		if (U_FAILURE(icuErrorCode))
		{
			icuLib.ucalClose(icuCalendar);
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_setDateTime.");
		}

		displacement = (icuLib.ucalGet(icuCalendar, UCAL_ZONE_OFFSET, &icuErrorCode) +
			icuLib.ucalGet(icuCalendar, UCAL_DST_OFFSET, &icuErrorCode)) / U_MILLIS_PER_MINUTE;

		if (U_FAILURE(icuErrorCode))
		{
			icuLib.ucalClose(icuCalendar);
			status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_get.");
		}

		icuLib.ucalClose(icuCalendar);
	}

	const auto  ticks = TimeStamp::timeStampToTicks(timeStampTz.utc_timestamp) -
		(displacement * 60 * ISC_TIME_SECONDS_PRECISION);

	timeStampTz.utc_timestamp = TimeStamp::ticksToTimeStamp(ticks);
}

bool TimeZoneUtil::decodeTime(const ISC_TIME_TZ& timeTz, bool gmtFallback, SLONG gmtOffset, Callbacks* cb,
	struct tm* times, int* fractions)
{
	bool tzLookup = true;
	ISC_TIMESTAMP_TZ timeStampTz;

	try
	{
#ifdef DEV_BUILD
		if (gmtFallback && getenv("MISSING_ICU_EMULATION"))
			(Arg::Gds(isc_random) << "Emulating missing ICU").raise();
#endif
		timeStampTz = cvtTimeTzToTimeStampTz(timeTz, cb);
	}
	catch (const Exception&)
	{
		if (gmtFallback)
		{
			tzLookup = false;
			timeStampTz.time_zone = displacementToOffsetZone(gmtOffset == TimeZoneUtil::NO_OFFSET ? 0 : gmtOffset);
			timeStampTz.utc_timestamp = cb->getCurrentGmtTimeStamp();
			timeStampTz.utc_timestamp.timestamp_time = timeTz.utc_time;
		}
		else
			throw;
	}

	decodeTimeStamp(timeStampTz, gmtFallback, gmtOffset, times, fractions);
	return tzLookup;
}

bool TimeZoneUtil::decodeTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, bool gmtFallback, SLONG gmtOffset,
	struct tm* times, int* fractions)
{
	bool icuFail = false;
	int displacement;

	if (timeStampTz.time_zone == GMT_ZONE)
		displacement = 0;
	else if (isOffset(timeStampTz.time_zone))
		displacement = offsetZoneToDisplacement(timeStampTz.time_zone);
	else
	{
		UErrorCode icuErrorCode = U_ZERO_ERROR;

		try
		{
#ifdef DEV_BUILD
			if (gmtFallback && getenv("MISSING_ICU_EMULATION"))
				(Arg::Gds(isc_random) << "Emulating missing ICU").raise();
#endif
			Jrd::UnicodeUtil::ConversionICU& icuLib = Jrd::UnicodeUtil::getConversionICU();

			UCalendar* icuCalendar = icuLib.ucalOpen(
				getDesc(timeStampTz.time_zone)->icuName, -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

			if (!icuCalendar)
				status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_open.");

			icuLib.ucalSetMillis(icuCalendar, timeStampToIcuDate(timeStampTz.utc_timestamp), &icuErrorCode);

			if (U_FAILURE(icuErrorCode))
			{
				icuLib.ucalClose(icuCalendar);
				status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_setMillis.");
			}

			displacement = (icuLib.ucalGet(icuCalendar, UCAL_ZONE_OFFSET, &icuErrorCode) +
				icuLib.ucalGet(icuCalendar, UCAL_DST_OFFSET, &icuErrorCode)) / U_MILLIS_PER_MINUTE;

			if (U_FAILURE(icuErrorCode))
			{
				icuLib.ucalClose(icuCalendar);
				status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_get.");
			}

			icuLib.ucalClose(icuCalendar);
		}
		catch (const Exception&)
		{
			if (!gmtFallback)
				throw;

			icuFail = true;
			displacement = gmtOffset == TimeZoneUtil::NO_OFFSET ? 0 : gmtOffset;
		}
	}

	const auto ticks = TimeStamp::timeStampToTicks(timeStampTz.utc_timestamp) +
		(displacement * 60 * ISC_TIME_SECONDS_PRECISION);

	TimeStamp::decode_timestamp(TimeStamp::ticksToTimeStamp(ticks), times, fractions);

	return !icuFail;
}

ISC_TIMESTAMP_TZ TimeZoneUtil::getCurrentSystemTimeStamp()
{
	TimeStamp now = TimeStamp::getCurrentTimeStamp();

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp = now.value();
	tsTz.time_zone = getSystemTimeZone();
	localTimeStampToUtc(tsTz);

	return tsTz;
}

ISC_TIMESTAMP_TZ TimeZoneUtil::getCurrentGmtTimeStamp()
{
	NoThrowTimeStamp now;

	// ASF: This comment is copied from NoThrowTimeStamp::getCurrentTimeStamp.
	// NS: We round generated timestamps to whole millisecond.
	// Not many applications can deal with fractional milliseconds properly and
	// we do not use high resolution timers either so actual time granularity
	// is going to to be somewhere in range between 1 ms (like on UNIX/Risc)
	// and 53 ms (such as Win9X)

	int milliseconds;

#ifdef WIN_NT
	SYSTEMTIME stUtc;
	GetSystemTime(&stUtc);
	milliseconds = stUtc.wMilliseconds;
#else
	time_t seconds; // UTC time

#ifdef HAVE_GETTIMEOFDAY
	struct timeval tp;
	GETTIMEOFDAY(&tp);
	seconds = tp.tv_sec;
	milliseconds = tp.tv_usec / 1000;
#else
	struct timeb time_buffer;
	ftime(&time_buffer);
	seconds = time_buffer.time;
	milliseconds = time_buffer.millitm;
#endif
#endif // WIN_NT

	const int fractions = milliseconds * ISC_TIME_SECONDS_PRECISION / 1000;

#ifdef WIN_NT
	// Manually convert SYSTEMTIME to "struct tm" used below

	struct tm times, *ptimes = &times;

	times.tm_sec = stUtc.wSecond;			// seconds after the minute - [0,59]
	times.tm_min = stUtc.wMinute;			// minutes after the hour - [0,59]
	times.tm_hour = stUtc.wHour;			// hours since midnight - [0,23]
	times.tm_mday = stUtc.wDay;				// day of the month - [1,31]
	times.tm_mon = stUtc.wMonth - 1;		// months since January - [0,11]
	times.tm_year = stUtc.wYear - 1900;		// years since 1900
	times.tm_wday = stUtc.wDayOfWeek;		// days since Sunday - [0,6]

	// --- no used for encoding below
	times.tm_yday = 0;						// days since January 1 - [0,365]
	times.tm_isdst = -1;					// daylight savings time flag
#else
#ifdef HAVE_GMTIME_R
	struct tm times, *ptimes = &times;
	if (!gmtime_r(&seconds, &times))
		system_call_failed::raise("gmtime_r");
#else
	struct tm *ptimes = gmtime(&seconds);
	if (!ptimes)
		system_call_failed::raise("gmtime");
#endif
#endif // WIN_NT

	now.encode(ptimes, fractions);

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp = now.value();
	tsTz.time_zone = GMT_ZONE;

	return tsTz;
}

void TimeZoneUtil::validateGmtTimeStamp(NoThrowTimeStamp& ts)
{
	if (ts.isEmpty())
		ts.value() = getCurrentGmtTimeStamp().utc_timestamp;
}

// Converts a time to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::cvtTimeToTimeStampTz(const ISC_TIME& time, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITHOUT TIME ZONE => TIMESTAMP WITH TIME ZONE

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = cb->getLocalDate();
	tsTz.utc_timestamp.timestamp_time = time;
	tsTz.time_zone = cb->getSessionTimeZone();
	localTimeStampToUtc(tsTz);

	return tsTz;
}

// Converts a time to time-tz.
ISC_TIME_TZ TimeZoneUtil::cvtTimeToTimeTz(const ISC_TIME& time, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tsTz = cvtTimeToTimeStampTz(time, cb);

	ISC_TIME_TZ timeTz;
	timeTz.utc_time = tsTz.utc_timestamp.timestamp_time;
	timeTz.time_zone = tsTz.time_zone;

	return timeTz;
}

// Converts a time-tz to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::cvtTimeTzToTimeStampTz(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	// SQL: Copy date fields from CURRENT_DATE and time and time zone fields from the source.

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = cb->getLocalDate();
	tsTz.utc_timestamp.timestamp_time = timeTzToTime(timeTz, cb->getSessionTimeZone(), cb);
	tsTz.time_zone = cb->getSessionTimeZone();
	localTimeStampToUtc(tsTz);
	tsTz.time_zone = timeTz.time_zone;

	return tsTz;
}

// Converts a time-tz to timestamp.
ISC_TIMESTAMP TimeZoneUtil::cvtTimeTzToTimeStamp(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITH TIME ZONE => TIMESTAMP WITHOUT TIME ZONE

	ISC_TIMESTAMP_TZ tsTz = cvtTimeTzToTimeStampTz(timeTz, cb);

	return timeStampTzToTimeStamp(tsTz, cb->getSessionTimeZone());
}

// Converts a time-tz to time.
ISC_TIME TimeZoneUtil::cvtTimeTzToTime(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	return timeTzToTime(timeTz, cb->getSessionTimeZone(), cb);
}

// Converts a timestamp-tz to timestamp.
ISC_TIMESTAMP TimeZoneUtil::cvtTimeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, Callbacks* cb)
{
	return timeStampTzToTimeStamp(timeStampTz, cb->getSessionTimeZone());
}

// Converts a timestamp-tz to time-tz.
ISC_TIME_TZ TimeZoneUtil::cvtTimeStampTzToTimeTz(const ISC_TIMESTAMP_TZ& timeStampTz)
{
	ISC_TIME_TZ timeTz;
	timeTz.utc_time = timeStampTz.utc_timestamp.timestamp_time;
	timeTz.time_zone = timeStampTz.time_zone;

	return timeTz;
}

// Converts a timestamp to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::cvtTimeStampToTimeStampTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	// SQL: Copy time and time zone fields from the source.

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp = timeStamp;
	tsTz.time_zone = cb->getSessionTimeZone();

	localTimeStampToUtc(tsTz);

	return tsTz;
}

// Converts a timestamp to time-tz.
ISC_TIME_TZ TimeZoneUtil::cvtTimeStampToTimeTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITH TIME ZONE => TIME WITH TIME ZONE

	ISC_TIMESTAMP_TZ tsTz = cvtTimeStampToTimeStampTz(timeStamp, cb);

	ISC_TIME_TZ timeTz;
	timeTz.utc_time = tsTz.utc_timestamp.timestamp_time;
	timeTz.time_zone = tsTz.time_zone;

	return timeTz;
}

// Converts a date to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::cvtDateToTimeStampTz(const ISC_DATE& date, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITHOUT TIME ZONE => TIMESTAMP WITH TIME ZONE

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = date;
	tsTz.utc_timestamp.timestamp_time = 0;
	tsTz.time_zone = cb->getSessionTimeZone();
	localTimeStampToUtc(tsTz);

	return tsTz;
}

//-------------------------------------

TimeZoneRuleIterator::TimeZoneRuleIterator(USHORT aId, ISC_TIMESTAMP_TZ& aFrom, ISC_TIMESTAMP_TZ& aTo)
	: id(aId),
	  icuLib(Jrd::UnicodeUtil::getConversionICU()),
	  toTicks(TimeStamp::timeStampToTicks(aTo.utc_timestamp))
{
	UErrorCode icuErrorCode = U_ZERO_ERROR;

	icuCalendar = icuLib.ucalOpen(getDesc(id)->icuName, -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

	if (!icuCalendar)
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_open.");

	icuDate = TimeZoneUtil::timeStampToIcuDate(aFrom.utc_timestamp);

	icuLib.ucalSetMillis(icuCalendar, icuDate, &icuErrorCode);

	if (U_FAILURE(icuErrorCode))
	{
		fb_assert(false);
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_setMillis.");
	}

	UBool hasInitial = icuLib.ucalGetTimeZoneTransitionDate(icuCalendar, UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE,
		&icuDate, &icuErrorCode);

	if (U_FAILURE(icuErrorCode))
	{
		fb_assert(false);
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_getTimeZoneTransitionDate.");
	}

	if (!hasInitial)
		icuDate = MIN_ICU_TIMESTAMP;

	icuLib.ucalSetMillis(icuCalendar, icuDate, &icuErrorCode);

	if (U_FAILURE(icuErrorCode))
	{
		fb_assert(false);
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_setMillis.");
	}

	startTicks = TimeStamp::timeStampToTicks(TimeZoneUtil::icuDateToTimeStamp(icuDate));
}

TimeZoneRuleIterator::~TimeZoneRuleIterator()
{
	icuLib.ucalClose(icuCalendar);
}

bool TimeZoneRuleIterator::next()
{
	if (startTicks > toTicks)
		return false;

	UErrorCode icuErrorCode = U_ZERO_ERROR;

	startTimestamp.utc_timestamp = TimeStamp::ticksToTimeStamp(startTicks);
	startTimestamp.time_zone = TimeZoneUtil::GMT_ZONE;

	zoneOffset = icuLib.ucalGet(icuCalendar, UCAL_ZONE_OFFSET, &icuErrorCode) / U_MILLIS_PER_MINUTE;
	dstOffset = icuLib.ucalGet(icuCalendar, UCAL_DST_OFFSET, &icuErrorCode) / U_MILLIS_PER_MINUTE;

	UBool hasNext = icuLib.ucalGetTimeZoneTransitionDate(icuCalendar, UCAL_TZ_TRANSITION_NEXT,
		&icuDate, &icuErrorCode);

	if (U_FAILURE(icuErrorCode))
	{
		fb_assert(false);
		status_exception::raise(Arg::Gds(isc_random) << "Error calling ICU's ucal_getTimeZoneTransitionDate.");
	}

	if (!hasNext || icuDate > MAX_ICU_TIMESTAMP)
		icuDate = MAX_ICU_TIMESTAMP;

	icuLib.ucalSetMillis(icuCalendar, icuDate, &icuErrorCode);

	const auto endTicks = TimeStamp::timeStampToTicks(TimeZoneUtil::icuDateToTimeStamp(icuDate)) - 1;

	endTimestamp.utc_timestamp = TimeStamp::ticksToTimeStamp(endTicks +
		(icuDate == MAX_ICU_TIMESTAMP ? ISC_TIME_SECONDS_PRECISION / 1000 : 0));
	endTimestamp.time_zone = TimeZoneUtil::GMT_ZONE;

	startTicks = endTicks + 1;

	return true;
}

//-------------------------------------

static const TimeZoneDesc* getDesc(USHORT timeZone)
{
	if (MAX_USHORT - timeZone < FB_NELEM(TIME_ZONE_LIST))
		return &TIME_ZONE_LIST[MAX_USHORT - timeZone];

	status_exception::raise(Arg::Gds(isc_invalid_timezone_id) << Arg::Num(timeZone));
	return nullptr;
}

// Returns true if the time zone is offset-based or false if region-based.
static inline bool isOffset(USHORT timeZone)
{
	return timeZone <= ONE_DAY * 2;
}

// Makes a time zone id from offsets.
static USHORT makeFromOffset(int sign, unsigned tzh, unsigned tzm)
{
	if (!TimeZoneUtil::isValidOffset(sign, tzh, tzm))
	{
		string str;
		str.printf("%s%02u:%02u", (sign == -1 ? "-" : "+"), tzh, tzm);
		status_exception::raise(Arg::Gds(isc_invalid_timezone_offset) << str);
	}

	return (USHORT)displacementToOffsetZone((tzh * 60 + tzm) * sign);
}

// Gets the displacement from a offset-based time zone id.
static inline SSHORT offsetZoneToDisplacement(USHORT timeZone)
{
	fb_assert(isOffset(timeZone));

	return (SSHORT) (int(timeZone) - ONE_DAY);
}

static inline USHORT displacementToOffsetZone(SSHORT displacement)
{
	return (USHORT)(int(displacement) + ONE_DAY);
}

// Parses a integer number.
static int parseNumber(const char*& p, const char* end)
{
	const char* start = p;
	int n = 0;

	while (p < end && *p >= '0' && *p <= '9')
		n = n * 10 + *p++ - '0';

	if (p == start)
		status_exception::raise(Arg::Gds(isc_invalid_timezone_offset) << string(start, end - start));

	return n;
}

// Skip spaces and tabs.
static void skipSpaces(const char*& p, const char* end)
{
	while (p < end && (*p == ' ' || *p == '\t'))
		++p;
}
