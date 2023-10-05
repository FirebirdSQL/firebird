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
	return (T(0) < value) - (value < T(0));
}

static struct tm initTMStruct(int year, int month, int day)
{
	struct tm times;
	memset(&times, 0, sizeof(struct tm));

	times.tm_year = year - 1900;
	times.tm_mon = month - 1;
	times.tm_mday = day;
	mktime(&times);

	return times;
}

static ISC_DATE createDate(int year, int month, int day)
{
	struct tm times = initTMStruct(year, month, day);
	return NoThrowTimeStamp::encode_date(&times);
}

static ISC_TIME createTime(int hours, int minutes, int seconds, int fractions = 0)
{
	return NoThrowTimeStamp::encode_time(hours, minutes, seconds, fractions);
}

static ISC_TIMESTAMP createTimeStamp(int year, int month, int day, int hours, int minutes, int seconds, int fractions = 0)
{
	struct tm times = initTMStruct(year, month, day);
	times.tm_hour = hours;
	times.tm_min = minutes;
	times.tm_sec = seconds;

	return NoThrowTimeStamp::encode_timestamp(&times, fractions);
}

static ISC_TIME_TZ createTimeTZ(int hours, int minutes, int seconds, int offsetInMinutes, int fractions = 0)
{
	ISC_TIME_TZ timeTZ;
	timeTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(offsetInMinutes), abs(offsetInMinutes / 60),
		abs(offsetInMinutes % 60));
	timeTZ.utc_time = createTime(hours, minutes, seconds, fractions);

	return timeTZ;
}

static ISC_TIMESTAMP_TZ createTimeStampTZ(int year, int month, int day, int hours, int minutes, int seconds,
	int offsetInMinutes, int fractions = 0)
{
	ISC_TIMESTAMP_TZ timestampTZ;
	timestampTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(offsetInMinutes), abs(offsetInMinutes / 60),
		abs(offsetInMinutes % 60));
	timestampTZ.utc_timestamp = createTimeStamp(year, month, day, hours, minutes, seconds, fractions);

	return timestampTZ;
}

class CVTCallback : public Firebird::Callbacks
{
public:
	explicit CVTCallback(ErrorFunction aErr) : Callbacks(aErr)
	{}

public:
	bool transliterate(const dsc* from, dsc* to, CHARSET_ID&) override { return true; }
	CHARSET_ID getChid(const dsc* d) override { return 0; }
	Jrd::CharSet* getToCharset(CHARSET_ID charset2) override { return nullptr; }
	void validateData(Jrd::CharSet* toCharset, SLONG length, const UCHAR* q) override { }
	ULONG validateLength(Jrd::CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
		const USHORT size) override { return 0; }
	SLONG getLocalDate() override { return 0; }
	ISC_TIMESTAMP getCurrentGmtTimeStamp() override { ISC_TIMESTAMP ts; return ts; }
	USHORT getSessionTimeZone() override { return 1439; }
	void isVersion4(bool& v4) override { }
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
