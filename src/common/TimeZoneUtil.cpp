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
#include "../common/TimeZones.h"
#include "../common/StatusHolder.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/timestamp.h"
#include "../common/classes/GenericMap.h"
#include "../common/config/config.h"
#include "../common/os/path_utils.h"
#include "../common/os/os_utils.h"
#include "unicode/ucal.h"

using namespace Firebird;

namespace
{
	class TimeZoneDesc
	{
	public:
		TimeZoneDesc(MemoryPool& pool)
			: asciiName(pool),
			  unicodeName(pool)
		{
		}

	public:
		void setName(const char* name)
		{
			asciiName = name;

			for (const auto c : asciiName)
				unicodeName.push(c);

			unicodeName.push(0);
		}

		const char* getAsciiName() const
		{
			return asciiName.c_str();
		}

		const UChar* getUnicodeName() const
		{
			return unicodeName.begin();
		}

	private:
		string asciiName;
		Array<UChar> unicodeName;
	};
}

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
	class TimeZoneDataPath
	{
	public:
		TimeZoneDataPath(MemoryPool& pool)
			: path(pool)
		{
			PathName temp;

			// Could not call fb_utils::getPrefix here.
			if (FB_TZDATADIR[0])
				temp = FB_TZDATADIR;
			else
				PathUtils::concatPath(temp, Config::getRootDirectory(), "tzdata");

			const static char* const ICU_TIMEZONE_FILES_DIR = "ICU_TIMEZONE_FILES_DIR";

			// Do not update ICU_TIMEZONE_FILES_DIR if it's already set.
			fb_utils::setenv(ICU_TIMEZONE_FILES_DIR, temp.c_str(), false);
			fb_utils::readenv(ICU_TIMEZONE_FILES_DIR, path);
		}

		const PathName& get()
		{
			return path;
		}

	private:
		PathName path;
	};

	class TimeZoneStartup
	{
	public:
		TimeZoneStartup(MemoryPool& pool)
			: timeZoneList(pool),
			  nameIdMap(pool)
		{
			if (!initFromFile())
			{
				for (USHORT i = 0; i < FB_NELEM(BUILTIN_TIME_ZONE_LIST); ++i)
				{
					auto& timeZone = timeZoneList.add();
					timeZone.setName(BUILTIN_TIME_ZONE_LIST[i]);
				}
			}

			unsigned id = 0;

			for (const auto& timeZone : timeZoneList)
			{
				string name = timeZone.getAsciiName();
				name.upper();
				nameIdMap.put(name, id++);
			}
		}

		const ObjectsArray<TimeZoneDesc>& getTimeZoneList()
		{
			return timeZoneList;
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

	private:
		bool initFromFile()
		{
			PathName idsPath;
			PathUtils::concatPath(idsPath, TimeZoneUtil::getTzDataPath(), "ids.dat");
			const int fileHandle = os_utils::open(idsPath.c_str(), O_RDONLY | O_BINARY, 0);

			if (fileHandle == -1)
				return false;

			struct STAT stat;
			if (os_utils::fstat(fileHandle, &stat) != 0)
			{
				close(fileHandle);
				return false;
			}

			Array<UCHAR> buffer(stat.st_size);
			buffer.resize(stat.st_size);
			const long bytesRead = read(fileHandle, buffer.begin(), buffer.getCount());

			close(fileHandle);

			if (bytesRead != buffer.getCount())
				return false;

			const UCHAR* p = buffer.begin();
			const UCHAR* end = buffer.end();

			if (buffer.getCount() > 10)
			{
				// file signature
				if (memcmp(p, "FBTZ", 5) == 0)
				{
					p += 5;
					if (isc_portable_integer(p, 2) == 1)	// file version: must be 1
					{
						string databaseVersion;

						for (p += 2; p < end && *p; ++p)
							databaseVersion += *p;

						++p;

						if (end - p >= 2)
						{
							unsigned count = isc_portable_integer(p, 2);

							// Our main criteria to choose the file or the builtin data is the count
							// of entries. TZ database version is the second, as new version could
							// have the same entries as before.

							if (count < FB_NELEM(BUILTIN_TIME_ZONE_LIST))
							{
								gds__log("tzdata ids.dat file is older than builtin time zone list.");
								return false;
							}
							else if (count == FB_NELEM(BUILTIN_TIME_ZONE_LIST) &&
								databaseVersion <= BUILTIN_TIME_ZONE_VERSION)
							{
								return false;
							}

							for (p += 2; p < end; ++p)
							{
								auto& timeZone = timeZoneList.add();
								string name;

								for (; p < end && *p; ++p)
									name += *p;

								timeZone.setName(name.c_str());

								if (p >= end)
								{
									timeZoneList.clear();
									break;
								}
							}

							if (timeZoneList.getCount() == count)
								return true;
						}
					}
				}
			}

			gds__log("tzdata ids.dat file is corrupted.");

			timeZoneList.clear();

			return false;
		}

	private:
		ObjectsArray<TimeZoneDesc> timeZoneList;
		GenericMap<Pair<Left<string, USHORT> > > nameIdMap;
	};
}	// namespace

//-------------------------------------

static const UDate MIN_ICU_TIMESTAMP = TimeZoneUtil::timeStampToIcuDate(TimeStamp::MIN_TIMESTAMP);
static const UDate MAX_ICU_TIMESTAMP = TimeZoneUtil::timeStampToIcuDate(TimeStamp::MAX_TIMESTAMP);
static const unsigned ONE_DAY = 24 * 60 - 1;	// used for offset encoding
static InitInstance<TimeZoneDataPath> timeZoneDataPath;
static InitInstance<TimeZoneStartup> timeZoneStartup;

//-------------------------------------


const ISC_DATE TimeZoneUtil::TIME_TZ_BASE_DATE = 58849;	// 2020-01-01
const char TimeZoneUtil::GMT_FALLBACK[5] = "GMT*";

void TimeZoneUtil::initTimeZoneEnv()
{
	timeZoneDataPath();
}

const PathName& TimeZoneUtil::getTzDataPath()
{
	return timeZoneDataPath().get();
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
	for (USHORT i = 0; i < timeZoneStartup().getTimeZoneList().getCount(); ++i)
		func(MAX_USHORT - i, timeZoneStartup().getTimeZoneList()[i].getAsciiName());
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
		strncpy(buffer, getDesc(timeZone)->getAsciiName(), bufferSize);

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
			getDesc(timeStampTz.time_zone)->getUnicodeName(), -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

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

// Converts a time from local to UTC.
void TimeZoneUtil::localTimeToUtc(ISC_TIME& time, ISC_USHORT timeZone)
{
	ISC_TIME_TZ timeTz;
	timeTz.utc_time = time;
	timeTz.time_zone = timeZone;
	localTimeToUtc(timeTz);

	time = timeTz.utc_time;
}

// Converts a time from local to UTC.
void TimeZoneUtil::localTimeToUtc(ISC_TIME_TZ& timeTz)
{
	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = TIME_TZ_BASE_DATE;
	tsTz.utc_timestamp.timestamp_time = timeTz.utc_time;
	tsTz.time_zone = timeTz.time_zone;
	localTimeStampToUtc(tsTz);

	timeTz.utc_time = tsTz.utc_timestamp.timestamp_time;
}

// Converts a timestamp from its local datetime fields to UTC.
void TimeZoneUtil::localTimeStampToUtc(ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = timeStamp.timestamp_date;
	tsTz.utc_timestamp.timestamp_time = timeStamp.timestamp_time;
	tsTz.time_zone = cb->getSessionTimeZone();

	localTimeStampToUtc(tsTz);

	timeStamp.timestamp_date = tsTz.utc_timestamp.timestamp_date;
	timeStamp.timestamp_time = tsTz.utc_timestamp.timestamp_time;
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
			getDesc(timeStampTz.time_zone)->getUnicodeName(), -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

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

bool TimeZoneUtil::decodeTime(const ISC_TIME_TZ& timeTz, bool gmtFallback, SLONG gmtOffset,
	struct tm* times, int* fractions)
{
	ISC_TIMESTAMP_TZ timeStampTz;
	timeStampTz.utc_timestamp.timestamp_date = TIME_TZ_BASE_DATE;
	timeStampTz.utc_timestamp.timestamp_time = timeTz.utc_time;
	timeStampTz.time_zone = timeTz.time_zone;

	return decodeTimeStamp(timeStampTz, gmtFallback, gmtOffset, times, fractions);
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
				getDesc(timeStampTz.time_zone)->getUnicodeName(), -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

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
			displacement = gmtOffset == NO_OFFSET ? 0 : gmtOffset;
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

// Converts a time-tz to a time.
ISC_TIME TimeZoneUtil::timeTzToTime(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp.timestamp_date = TIME_TZ_BASE_DATE;
	tsTz.utc_timestamp.timestamp_time = timeTz.utc_time;
	tsTz.time_zone = timeTz.time_zone;

	struct tm times;
	int fractions;
	decodeTimeStamp(tsTz, false, NO_OFFSET, &times, &fractions);

	tsTz.utc_timestamp.timestamp_date = cb->getLocalDate();
	tsTz.utc_timestamp.timestamp_time =
		TimeStamp::encode_time(times.tm_hour, times.tm_min, times.tm_sec, fractions);

	localTimeStampToUtc(tsTz);

	return timeStampTzToTimeStamp(tsTz, cb->getSessionTimeZone()).timestamp_time;
}

// Converts a timestamp-tz to a timestamp in a given zone.
ISC_TIMESTAMP TimeZoneUtil::timeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, USHORT toTimeZone)
{
	ISC_TIMESTAMP_TZ tsTz = timeStampTz;
	tsTz.time_zone = toTimeZone;

	struct tm times;
	int fractions;
	decodeTimeStamp(tsTz, false, NO_OFFSET, &times, &fractions);

	return TimeStamp::encode_timestamp(&times, fractions);
}

// Converts a time to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::timeToTimeStampTz(const ISC_TIME& time, Callbacks* cb)
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
ISC_TIME_TZ TimeZoneUtil::timeToTimeTz(const ISC_TIME& time, Callbacks* cb)
{
	ISC_TIMESTAMP_TZ tsTz;
	tsTz.time_zone = cb->getSessionTimeZone();
	tsTz.utc_timestamp.timestamp_date = TIME_TZ_BASE_DATE;
	tsTz.utc_timestamp.timestamp_time = time;

	localTimeStampToUtc(tsTz);

	ISC_TIME_TZ timeTz;
	timeTz.time_zone = tsTz.time_zone;
	timeTz.utc_time = tsTz.utc_timestamp.timestamp_time;

	return timeTz;
}

// Converts a time-tz to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::timeTzToTimeStampTz(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	// SQL: Copy date fields from CURRENT_DATE and time and time zone fields from the source.

	struct tm localTimes;
	TimeStamp::decode_date(cb->getLocalDate(), &localTimes);

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.time_zone = timeTz.time_zone;
	tsTz.utc_timestamp.timestamp_date = TIME_TZ_BASE_DATE;
	tsTz.utc_timestamp.timestamp_time = timeTz.utc_time;

	struct tm times;
	int fractions;
	decodeTimeStamp(tsTz, false, NO_OFFSET, &times, &fractions);

	times.tm_mday = localTimes.tm_mday;
	times.tm_mon = localTimes.tm_mon;
	times.tm_year = localTimes.tm_year;

	tsTz.utc_timestamp = TimeStamp::encode_timestamp(&times, fractions);
	localTimeStampToUtc(tsTz);

	return tsTz;
}

// Converts a time-tz to timestamp.
ISC_TIMESTAMP TimeZoneUtil::timeTzToTimeStamp(const ISC_TIME_TZ& timeTz, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITH TIME ZONE => TIMESTAMP WITHOUT TIME ZONE

	ISC_TIMESTAMP_TZ tsTz = timeTzToTimeStampTz(timeTz, cb);

	return timeStampTzToTimeStamp(tsTz, cb->getSessionTimeZone());
}

// Converts a timestamp-tz to timestamp.
ISC_TIMESTAMP TimeZoneUtil::timeStampTzToTimeStamp(const ISC_TIMESTAMP_TZ& timeStampTz, Callbacks* cb)
{
	return timeStampTzToTimeStamp(timeStampTz, cb->getSessionTimeZone());
}

// Converts a timestamp-tz to time-tz.
ISC_TIME_TZ TimeZoneUtil::timeStampTzToTimeTz(const ISC_TIMESTAMP_TZ& timeStampTz)
{
	struct tm times;
	int fractions;
	decodeTimeStamp(timeStampTz, false, NO_OFFSET, &times, &fractions);

	ISC_TIME_TZ timeTz;
	timeTz.utc_time = TimeStamp::encode_time(times.tm_hour, times.tm_min, times.tm_sec, fractions);
	timeTz.time_zone = timeStampTz.time_zone;
	localTimeToUtc(timeTz);

	return timeTz;
}

// Converts a timestamp to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::timeStampToTimeStampTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	// SQL: Copy time and time zone fields from the source.

	ISC_TIMESTAMP_TZ tsTz;
	tsTz.utc_timestamp = timeStamp;
	tsTz.time_zone = cb->getSessionTimeZone();

	localTimeStampToUtc(tsTz);

	return tsTz;
}

// Converts a timestamp to time-tz.
ISC_TIME_TZ TimeZoneUtil::timeStampToTimeTz(const ISC_TIMESTAMP& timeStamp, Callbacks* cb)
{
	// SQL: source => TIMESTAMP WITH TIME ZONE => TIME WITH TIME ZONE

	return timeStampTzToTimeTz(timeStampToTimeStampTz(timeStamp, cb));
}

// Converts a date to timestamp-tz.
ISC_TIMESTAMP_TZ TimeZoneUtil::dateToTimeStampTz(const ISC_DATE& date, Callbacks* cb)
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

	icuCalendar = icuLib.ucalOpen(getDesc(id)->getUnicodeName(), -1, NULL, UCAL_GREGORIAN, &icuErrorCode);

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
	if (MAX_USHORT - timeZone < timeZoneStartup().getTimeZoneList().getCount())
		return &timeZoneStartup().getTimeZoneList()[MAX_USHORT - timeZone];

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
