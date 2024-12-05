#ifndef CVT_TEST_UTILS_H
#define CVT_TEST_UTILS_H

#include "firebird.h"
#include "../common/dsc.h"
#include "../common/TimeZoneUtil.h"
#include "../common/TimeZones.h"

using Firebird::NoThrowTimeStamp;
using Firebird::TimeZoneUtil;

namespace CvtTestUtils {

#define DECOMPOSE_TM_STRUCT(times, fractions, timezone) "Year:" << times.tm_year + 1900 << \
	" Month:" << times.tm_mon << \
	" Day:" << times.tm_mday << \
	" Hour:" << times.tm_hour << \
	" Min:" << times.tm_min << \
	" Sec:" << times.tm_sec << \
	" Fract: " << fractions << \
	" IsDST:" << times.tm_isdst << \
	" WDay:" << times.tm_wday << \
	" YDay:" << times.tm_yday << \
	" TZ Offset:" << timezone \

template<typename T>
static constexpr int sign(T value)
{
	return (value >= T(0)) ? 1 : -1;
}

static ISC_DATE mockGetLocalDate(int year = 2023)
{
	struct tm time;
	memset(&time, 0, sizeof(time));
	time.tm_year = year - 1900;
	time.tm_mon = 0;
	time.tm_mday = 1;
	return NoThrowTimeStamp::encode_date(&time);
}


// Pass 0 to year, month and day to use CurrentTimeStamp for them
static struct tm initTMStruct(int year, int month, int day)
{
	struct tm currentTime;
	NoThrowTimeStamp::decode_date(mockGetLocalDate(), &currentTime);

	struct tm times;
	memset(&times, 0, sizeof(struct tm));

	times.tm_year = year > 0 ? year - 1900 : currentTime.tm_year;
	times.tm_mon = month > 0 ? month - 1 : currentTime.tm_mon;
	times.tm_mday = day > 0 ? day : currentTime.tm_mday;
	mktime(&times);

	return times;
}

// Pass 0 to year, month and day to use CurrentTimeStamp for them
static ISC_DATE createDate(int year, int month, int day)
{
	struct tm times = initTMStruct(year, month, day);
	return NoThrowTimeStamp::encode_date(&times);
}

static ISC_TIME createTime(int hours, int minutes, int seconds, int fractions = 0)
{
	return NoThrowTimeStamp::encode_time(hours, minutes, seconds, fractions);
}

// Pass 0 to year, month and day to use CurrentTimeStamp for them
static ISC_TIMESTAMP createTimeStamp(int year, int month, int day, int hours, int minutes, int seconds, int fractions = 0)
{
	struct tm times = initTMStruct(year, month, day);
	times.tm_hour = hours;
	times.tm_min = minutes;
	times.tm_sec = seconds;

	return NoThrowTimeStamp::encode_timestamp(&times, fractions);
}

// Pass 0 to year, month and day to use CurrentTimeStamp for them
static ISC_TIMESTAMP_TZ createTimeStampTZ(int year, int month, int day, int hours, int minutes, int seconds,
	int offsetInMinutes, int fractions = 0)
{
	ISC_TIMESTAMP_TZ timestampTZ;
	timestampTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(offsetInMinutes), abs(offsetInMinutes / 60),
		abs(offsetInMinutes % 60));
	timestampTZ.utc_timestamp = createTimeStamp(year, month, day, hours, minutes, seconds, fractions);

	TimeZoneUtil::localTimeStampToUtc(timestampTZ);

	return timestampTZ;
}

static ISC_TIME_TZ createTimeTZ(int hours, int minutes, int seconds, int offsetInMinutes, int fractions = 0)
{
	// Day is 2 because we need to handle 00:00 with negative timezone offset, and anyway date is not used in TIME WITH TIME ZONE
	ISC_TIMESTAMP_TZ timestampTz = createTimeStampTZ(1, 1, 2, hours, minutes, seconds, offsetInMinutes, fractions);
	return { timestampTz.utc_timestamp.timestamp_time, timestampTz.time_zone };
}


class MockCallback : public Firebird::Callbacks
{
public:
	explicit MockCallback(ErrorFunction aErr, std::function<SLONG()> mockGetLocalDateFunc)
		: Callbacks(aErr), m_mockGetLocalDateFunc(mockGetLocalDateFunc)
	{}

public:
	bool transliterate(const dsc* from, dsc* to, CHARSET_ID&) override { return true; }
	CHARSET_ID getChid(const dsc* d) override { return 0; }
	Firebird::CharSet* getToCharset(CHARSET_ID charset2) override { return nullptr; }
	void validateData(Firebird::CharSet* toCharset, SLONG length, const UCHAR* q) override { }
	ULONG validateLength(Firebird::CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
		const USHORT size) override { return 0; }

	SLONG getLocalDate() override
	{
		return m_mockGetLocalDateFunc();
	}

	ISC_TIMESTAMP getCurrentGmtTimeStamp() override { return {0, 0}; }
	USHORT getSessionTimeZone() override { return 1439; } // 1439 is ONE_DAY, so we have no offset
	void isVersion4(bool& v4) override { }

private:
	std::function<SLONG()> m_mockGetLocalDateFunc;
};


template<typename T>
static UCHAR getDSCTypeFromDateType() { return 0; }

template<>
UCHAR getDSCTypeFromDateType<ISC_DATE>() { return dtype_sql_date; }

template<>
UCHAR getDSCTypeFromDateType<ISC_TIME>() { return dtype_sql_time; }

template<>
UCHAR getDSCTypeFromDateType<ISC_TIMESTAMP>() { return dtype_timestamp; }

template<>
UCHAR getDSCTypeFromDateType<ISC_TIME_TZ>() { return dtype_sql_time_tz; }

template<>
UCHAR getDSCTypeFromDateType<ISC_TIMESTAMP_TZ>() { return dtype_timestamp_tz; }

} // namespace CvtTestUtils

#endif // CVT_TEST_UTILS_H
