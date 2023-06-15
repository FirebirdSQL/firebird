#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/dsc.h"
#include "../jrd/cvt_proto.h"
#include "../common/TimeZoneUtil.h"
#include "../common/TimeZones.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(CVTSuite)
BOOST_AUTO_TEST_SUITE(CVTDatetimeFormat)

template<typename T>
constexpr int sign(T value)
{
	return (T(0) < value) - (value < T(0));
}

struct tm InitTMStruct(int year, int month, int day)
{
	struct tm times;
	memset(&times, 0, sizeof(struct tm));

	times.tm_year = year - 1900;
	times.tm_mon = month - 1;
	times.tm_mday = day;
	mktime(&times);

	return times;
}

ISC_DATE CreateDate(int year, int month, int day)
{
	struct tm times = InitTMStruct(year, month, day);
	return NoThrowTimeStamp::encode_date(&times);
}

ISC_TIME CreateTime(int hours, int minutes, int seconds, int fractions = 0)
{
	return NoThrowTimeStamp::encode_time(hours, minutes, seconds, fractions);
}

ISC_TIMESTAMP CreateTimeStamp(int year, int month, int day, int hours, int minutes, int seconds, int fractions = 0)
{
	struct tm times = InitTMStruct(year, month, day);
	times.tm_hour = hours;
	times.tm_min = minutes;
	times.tm_sec = seconds;

	return NoThrowTimeStamp::encode_timestamp(&times, fractions);
}

ISC_TIME_TZ CreateTimeTZ(int hours, int minutes, int seconds, int offsetInMinutes, int fractions = 0)
{
	ISC_TIME_TZ timeTZ;
	timeTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(offsetInMinutes), abs(offsetInMinutes / 60),
		abs(offsetInMinutes % 60));
	timeTZ.utc_time = CreateTime(hours, minutes, seconds, fractions);

	return timeTZ;
}

ISC_TIMESTAMP_TZ CreateTimeStampTZ(int year, int month, int day, int hours, int minutes, int seconds,
	int offsetInMinutes, int fractions = 0)
{
	ISC_TIMESTAMP_TZ timestampTZ;
	timestampTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(offsetInMinutes), abs(offsetInMinutes / 60),
		abs(offsetInMinutes % 60));
	timestampTZ.utc_timestamp = CreateTimeStamp(year, month, day, hours, minutes, seconds, fractions);

	return timestampTZ;
}

void ErrFunc(const Firebird::Arg::StatusVector& v)
{
	v.raise();
}

class CVTCallback : public Firebird::Callbacks
{
public:
	explicit CVTCallback(ErrorFunction aErr) : Callbacks(aErr)
	{
	}

public:
	bool transliterate(const dsc* from, dsc* to, CHARSET_ID&) override { return true; }
	CHARSET_ID getChid(const dsc* d) override { return 0; }
	Jrd::CharSet* getToCharset(CHARSET_ID charset2) override { return nullptr; }
	void validateData(Jrd::CharSet* toCharset, SLONG length, const UCHAR* q) override { }
	ULONG validateLength(Jrd::CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
		const USHORT size) override { return 0; }
	SLONG getLocalDate() override { return 0; }
	ISC_TIMESTAMP getCurrentGmtTimeStamp() override { ISC_TIMESTAMP ts; return ts; }
	USHORT getSessionTimeZone() override { return 0; }
	void isVersion4(bool& v4) override { }
};

CVTCallback cb(ErrFunc);

BOOST_AUTO_TEST_SUITE(CVTDatetimeToFormatString)

template<typename T>
UCHAR GetDSCTypeFromDateType() { return 0; }

template<>
UCHAR GetDSCTypeFromDateType<ISC_DATE>() { return dtype_sql_date; }

template<>
UCHAR GetDSCTypeFromDateType<ISC_TIME>() { return dtype_sql_time; }

template<>
UCHAR GetDSCTypeFromDateType<ISC_TIMESTAMP>() { return dtype_timestamp; }

template<>
UCHAR GetDSCTypeFromDateType<ISC_TIME_TZ>() { return dtype_sql_time_tz; }

template<>
UCHAR GetDSCTypeFromDateType<ISC_TIMESTAMP_TZ>() { return dtype_timestamp_tz; }

template<typename T>
void testCVTDatetimeToFormatString(T date, const string& format, const string& expected, Callbacks& cb)
{
	dsc desc;
	desc.dsc_dtype = GetDSCTypeFromDateType<T>();
	desc.dsc_length = sizeof(T);
	desc.dsc_address = (UCHAR*) &date;
	desc.dsc_scale = 0;

	string result = CVT_datetime_to_format_string(&desc, format, &cb);
	BOOST_TEST(result == expected, "\nRESULT: " << result.c_str() << "\nEXPECTED: " << expected.c_str());
}

BOOST_AUTO_TEST_SUITE(FunctionalTest)

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_DATE)
{
	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1.1.1.1.1", cb);
	testCVTDatetimeToFormatString(CreateDate(1234, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1234.1234.234.34.4", cb);
	testCVTDatetimeToFormatString(CreateDate(9999, 1, 1), "YEAR.YYYY.YYY.YY.Y", "9999.9999.999.99.9", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1.1.1.1.1", cb);
	testCVTDatetimeToFormatString(CreateDate(1234, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1234.1234.234.34.4", cb);
	testCVTDatetimeToFormatString(CreateDate(9999, 1, 1), "YEAR.YYYY.YYY.YY.Y", "9999.9999.999.99.9", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "Q", "1", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 4, 1), "Q", "2", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 7, 1), "Q", "3", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 10, 1), "Q", "4", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "MM:RM-MON/MONTH", "1:I-Jan/JANUARY", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 6, 1), "MM-RM.MON;MONTH", "6-VI.Jun;JUNE", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 12, 1), "MM,RM.MON:MONTH", "12,XII.Dec:DECEMBER", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "WW/W", "1/1", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 6, 15), "WW-W", "24-3", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 12, 30), "WW.W", "52.5", cb);

	testCVTDatetimeToFormatString(CreateDate(2023, 6, 4), "D;DAY-DY", "1;SUNDAY-Sun", cb);
	testCVTDatetimeToFormatString(CreateDate(2023, 6, 7), "D.DAY,DY", "4.WEDNESDAY,Wed", cb);
	testCVTDatetimeToFormatString(CreateDate(2023, 6, 10), "D DAY DY", "7 SATURDAY Sat", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "DDD", "1", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 6, 15), "DDD", "166", cb);
	testCVTDatetimeToFormatString(CreateDate(1, 12, 31), "DDD", "365", cb);

	testCVTDatetimeToFormatString(CreateDate(1, 1, 1), "J", "1721426", cb);
	testCVTDatetimeToFormatString(CreateDate(2000, 12, 8), "J", "2451887", cb);
	testCVTDatetimeToFormatString(CreateDate(9999, 12, 31), "J", "5373484", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIME)
{
	testCVTDatetimeToFormatString(CreateTime(0, 0, 0), "HH-HH12.HH24,MI/SS SSSSS", "12 AM-12 AM.0,0/0 0", cb);
	testCVTDatetimeToFormatString(CreateTime(12, 35, 15), "HH.HH12:HH24;MI-SS/SSSSS", "12 PM.12 PM:12;35-15/45315", cb);
	testCVTDatetimeToFormatString(CreateTime(23, 59, 59), " HH - HH12 . HH24 , MI / SS SSSSS ", " 11 PM - 11 PM . 23 , 59 / 59 86399 ", cb);

	testCVTDatetimeToFormatString(CreateTime(0, 0, 0, 1), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "1.10/100;1000:10000-100000,1000000-10000000 100000000", cb);
	testCVTDatetimeToFormatString(CreateTime(0, 0, 0, 1000), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "1.10/100;1000:10000-100000,1000000-10000000 100000000", cb);
	testCVTDatetimeToFormatString(CreateTime(0, 0, 0, 9999), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "9.99/999;9999:99990-999900,9999000-99990000 999900000", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIMESTAMP)
{
	ISC_TIMESTAMP timestamp = CreateTimeStamp(1982, 4, 21, 1, 34, 15, 2500);

	testCVTDatetimeToFormatString(timestamp, "YEAR.YYYY.YYY.YY.Y/J", "1982.1982.982.82.2/2445081", cb);
	testCVTDatetimeToFormatString(timestamp, "Q-MM-RM-MON-MONTH", "2-4-IV-Apr-APRIL", cb);
	testCVTDatetimeToFormatString(timestamp, "WW,W-D;DAY:DD DDD.DY", "16,3-4;WEDNESDAY:21 111.Wed", cb);
	testCVTDatetimeToFormatString(timestamp, "HH-HH12-HH24-MI-SS-SSSSS.FF2", "1 AM-1 AM-1-34-15-5655.25", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIME_TZ)
{
	testCVTDatetimeToFormatString(CreateTimeTZ(15, 35, 59, 0, 900), "HH-HH12-HH24-MI-SS-SSSSS.FF1/TZH/TZM", "3 PM-3 PM-15-35-59-56159.9/0/0", cb);
	testCVTDatetimeToFormatString(CreateTimeTZ(15, 35, 59, 160), "HH24:MI-TZH:TZM", "18:15-2:40", cb);
	testCVTDatetimeToFormatString(CreateTimeTZ(15, 35, 59, -160), "HH24:MI TZH:TZM", "12:55 -2:40", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIMESTAMP_TZ)
{
	ISC_TIMESTAMP_TZ timestampTZ = CreateTimeStampTZ(1982, 4, 21, 1, 34, 15, 0, 500);

	testCVTDatetimeToFormatString(timestampTZ, "YEAR.YYYY.YYY.YY.Y/J", "1982.1982.982.82.2/2445081", cb);
	testCVTDatetimeToFormatString(timestampTZ, "Q-MM-RM-MON-MONTH", "2-4-IV-Apr-APRIL", cb);
	testCVTDatetimeToFormatString(timestampTZ, "WW,W-D;DAY:DD DDD.DY", "16,3-4;WEDNESDAY:21 111.Wed", cb);
	testCVTDatetimeToFormatString(timestampTZ, "HH-HH12-HH24-MI-SS-SSSSS.FF2/TZH/TZM", "1 AM-1 AM-1-34-15-5655.50/0/0", cb);

	testCVTDatetimeToFormatString(CreateTimeStampTZ(1982, 4, 21, 1, 34, 15, 70), "HH24:MI-TZH:TZM", "2:44-1:10", cb);
	testCVTDatetimeToFormatString(CreateTimeStampTZ(1982, 4, 21, 1, 34, 15, -70), "HH24:MI TZH:TZM", "0:24 -1:10", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_RAW_TEXT)
{
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "YYYY-\"RaW TeXt\"-MON", "1981-RaW TeXt-Jul", cb);
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "\"Test1\"-Y\"Test2\"", "Test1-1Test2", cb);
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "\"\"-Y\"Test2\"", "-1Test2", cb);
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "\"Test1\"-Y\"\"", "Test1-1", cb);
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "\"\"-Y\"\"", "-1", cb);
	testCVTDatetimeToFormatString(CreateDate(1981, 7, 12), "\"\"\"\"", "", cb);
}

BOOST_AUTO_TEST_SUITE_END()	// FunctionalTest
BOOST_AUTO_TEST_SUITE_END()	// CVTDatetimeToFormatString

BOOST_AUTO_TEST_SUITE(CVTStringToFormatDateTime)

#define DECOMPOSE_TM_STRUCT(times, fractions, timezone) "Year:" << times.tm_year + 1900 \
	<< " Month:" << times.tm_mon \
	<< " Day:" << times.tm_mday \
	<< " Hour:" << times.tm_hour \
	<< " Min:" << times.tm_min \
	<< " Sec:" << times.tm_sec \
	<< " Fract: " << fractions \
	<< " IsDST:" << times.tm_isdst \
	<< " WDay:" << times.tm_wday \
	<< " YDay:" << times.tm_yday \
	<< " TZ:" << timezone \

void TestCVTStringToFormatDateTime(const string& date, const string& format, const ISC_TIMESTAMP_TZ& expected, Callbacks& cb)
{
	string varyingString = "xx";
	varyingString += date;
	*(USHORT*) varyingString.data() = varyingString.size() - sizeof(USHORT);

	dsc desc;
	desc.dsc_dtype = dtype_varying;
	desc.dsc_length = varyingString.size() + sizeof(USHORT);
	desc.dsc_address = (UCHAR*) varyingString.data();
	desc.dsc_scale = 0;

	const ISC_TIMESTAMP_TZ result = CVT_string_to_format_datetime(&desc, format, &cb);

	struct tm resultTimes;
	memset(&resultTimes, 0, sizeof(resultTimes));
	int resultFractions;
	NoThrowTimeStamp::decode_timestamp(result.utc_timestamp, &resultTimes, &resultFractions);

	struct tm expectedTimes;
	memset(&expectedTimes, 0, sizeof(expectedTimes));
	int expectedFractions;
	NoThrowTimeStamp::decode_timestamp(expected.utc_timestamp, &expectedTimes, &expectedFractions);

	bool isEqual = !((bool) memcmp(&resultTimes, &expectedTimes, sizeof(struct tm))) && resultFractions == expectedFractions;

	BOOST_TEST(isEqual, "\nRESULT: " << DECOMPOSE_TM_STRUCT(resultTimes, resultFractions, result.time_zone)
		<< "\nEXPECTED: " << DECOMPOSE_TM_STRUCT(expectedTimes, expectedFractions, expected.time_zone));
}

BOOST_AUTO_TEST_SUITE(FunctionalTest)

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_DATE)
{
	TestCVTStringToFormatDateTime("1", "YEAR", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1234", "YEAR", CreateTimeStampTZ(1234, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("9999", "YEAR", CreateTimeStampTZ(9999, 1, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "YYYY", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1234", "YYYY", CreateTimeStampTZ(1234, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("9999", "YYYY", CreateTimeStampTZ(9999, 1, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "YYY", CreateTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("450", "YYY", CreateTimeStampTZ(2450, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("451", "YYY", CreateTimeStampTZ(1451, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("999", "YYY", CreateTimeStampTZ(1999, 1, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "YY", CreateTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("45", "YY", CreateTimeStampTZ(2045, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("46", "YY", CreateTimeStampTZ(1946, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("99", "YY", CreateTimeStampTZ(1999, 1, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "Y", CreateTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("9", "Y", CreateTimeStampTZ(2009, 1, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "MM", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("6", "MM", CreateTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("12", "MM", CreateTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("Jan", "MON", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("Jun", "MON", CreateTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("Dec", "MON", CreateTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("January", "MONTH", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("June", "MONTH", CreateTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("December", "MONTH", CreateTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("I", "RM", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("IV", "RM", CreateTimeStampTZ(1, 4, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("XII", "RM", CreateTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1", "DD", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("15", "DD", CreateTimeStampTZ(1, 1, 15, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("31", "DD", CreateTimeStampTZ(1, 1, 31, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("2451887", "J", CreateTimeStampTZ(2000, 12, 8, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1721426", "J", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("5373484", "J", CreateTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("1:1,1", "YEAR.MM.DD", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1981-8/13", "YEAR.MM.DD", CreateTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("9999 12;31", "YEAR.MM.DD", CreateTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("25.Jan.25", "YY;MON;DD", CreateTimeStampTZ(2025, 1, 25, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("./.1981./-8--/13--", "  YEAR. -.MM.,,-.DD//", CreateTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TIME)
{
	TestCVTStringToFormatDateTime("12 AM", "HH", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1 AM", "HH", CreateTimeStampTZ(1, 1, 1, 1, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("11 AM", "HH", CreateTimeStampTZ(1, 1, 1, 11, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("12 PM", "HH", CreateTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1 PM", "HH", CreateTimeStampTZ(1, 1, 1, 13, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("11 PM", "HH", CreateTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("12 AM", "HH12", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1 AM", "HH12", CreateTimeStampTZ(1, 1, 1, 1, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("11 AM", "HH12", CreateTimeStampTZ(1, 1, 1, 11, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("12 PM", "HH12", CreateTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("1 PM", "HH12", CreateTimeStampTZ(1, 1, 1, 13, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("11 PM", "HH12", CreateTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("0", "HH24", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("12", "HH24", CreateTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("23", "HH24", CreateTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	TestCVTStringToFormatDateTime("0", "MI", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("30", "MI", CreateTimeStampTZ(1, 1, 1, 0, 30, 0, 0), cb);
	TestCVTStringToFormatDateTime("59", "MI", CreateTimeStampTZ(1, 1, 1, 0, 59, 0, 0), cb);

	TestCVTStringToFormatDateTime("0", "SS", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("30", "SS", CreateTimeStampTZ(1, 1, 1, 0, 0, 30, 0), cb);
	TestCVTStringToFormatDateTime("59", "SS", CreateTimeStampTZ(1, 1, 1, 0, 0, 59, 0), cb);

	TestCVTStringToFormatDateTime("0", "SSSSS", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	TestCVTStringToFormatDateTime("45315", "SSSSS", CreateTimeStampTZ(1, 1, 1, 12, 35, 15, 0), cb);
	TestCVTStringToFormatDateTime("86399", "SSSSS", CreateTimeStampTZ(1, 1, 1, 23, 59, 59, 0), cb);

	TestCVTStringToFormatDateTime("1", "FF1", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	TestCVTStringToFormatDateTime("5", "FF1", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	TestCVTStringToFormatDateTime("9", "FF1", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9000), cb);

	TestCVTStringToFormatDateTime("1", "FF2", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	TestCVTStringToFormatDateTime("10", "FF2", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	TestCVTStringToFormatDateTime("50", "FF2", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	TestCVTStringToFormatDateTime("99", "FF2", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9900), cb);

	TestCVTStringToFormatDateTime("1", "FF3", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 10), cb);
	TestCVTStringToFormatDateTime("10", "FF3", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	TestCVTStringToFormatDateTime("100", "FF3", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	TestCVTStringToFormatDateTime("500", "FF3", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	TestCVTStringToFormatDateTime("999", "FF3", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9990), cb);

	TestCVTStringToFormatDateTime("1", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1), cb);
	TestCVTStringToFormatDateTime("10", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 10), cb);
	TestCVTStringToFormatDateTime("100", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	TestCVTStringToFormatDateTime("1000", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	TestCVTStringToFormatDateTime("5000", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	TestCVTStringToFormatDateTime("9999", "FF4", CreateTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9999), cb);

	TestCVTStringToFormatDateTime("1 PM - 25 - 45 - 200", "HH.MI.SS.FF4", CreateTimeStampTZ(1, 1, 1, 13, 25, 45, 0, 200), cb);
	TestCVTStringToFormatDateTime("15:0:15:2", "HH24.MI.SS.FF1", CreateTimeStampTZ(1, 1, 1, 15, 0, 15, 0, 2000), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TZ)
{
	TestCVTStringToFormatDateTime("12:00 2:30", "HH24:MI TZH:TZM", CreateTimeStampTZ(1, 1, 1, 12, 0, 0, 150, 0), cb);
	TestCVTStringToFormatDateTime("12:00 -2:30", "HH24:MI TZH:TZM", CreateTimeStampTZ(1, 1, 1, 12, 0, 0, -150, 0), cb);
}

BOOST_AUTO_TEST_SUITE_END()	// FunctionalTest
BOOST_AUTO_TEST_SUITE_END()	// CVTStringToFormatDateTime

BOOST_AUTO_TEST_SUITE_END() // CVTDatetimeFormat
BOOST_AUTO_TEST_SUITE_END()	// CVTSuite
