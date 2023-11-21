#include "boost/test/unit_test.hpp"
#include "../common/tests/CvtTestUtils.h"
#include "../jrd/cvt_proto.h"

using namespace Firebird;
using namespace Jrd;
using namespace CvtTestUtils;

BOOST_AUTO_TEST_SUITE(CVTSuite)
BOOST_AUTO_TEST_SUITE(CVTDatetimeFormat)

static void errFunc(const Firebird::Arg::StatusVector& v)
{
	v.raise();
}

CVTCallback cb(errFunc);

BOOST_AUTO_TEST_SUITE(CVTDatetimeToFormatString)

template<typename T>
static void testCVTDatetimeToFormatString(T date, const string& format, const string& expected, Callbacks& cb)
{
	dsc desc;
	desc.dsc_dtype = getDSCTypeFromDateType<T>();
	desc.dsc_length = sizeof(T);
	desc.dsc_address = (UCHAR*) &date;
	desc.dsc_scale = 0;

	string result = CVT_datetime_to_format_string(&desc, format, &cb);
	BOOST_TEST(result == expected, "\nRESULT: " << result.c_str() << "\nEXPECTED: " << expected.c_str());
}

BOOST_AUTO_TEST_SUITE(FunctionalTest)

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_DATE)
{
	testCVTDatetimeToFormatString(createDate(1, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1.0001.001.01.1", cb);
	testCVTDatetimeToFormatString(createDate(1234, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1234.1234.234.34.4", cb);
	testCVTDatetimeToFormatString(createDate(9999, 1, 1), "YEAR.YYYY.YYY.YY.Y", "9999.9999.999.99.9", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1.0001.001.01.1", cb);
	testCVTDatetimeToFormatString(createDate(1234, 1, 1), "YEAR.YYYY.YYY.YY.Y", "1234.1234.234.34.4", cb);
	testCVTDatetimeToFormatString(createDate(9999, 1, 1), "YEAR.YYYY.YYY.YY.Y", "9999.9999.999.99.9", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "Q", "1", cb);
	testCVTDatetimeToFormatString(createDate(1, 4, 1), "Q", "2", cb);
	testCVTDatetimeToFormatString(createDate(1, 7, 1), "Q", "3", cb);
	testCVTDatetimeToFormatString(createDate(1, 10, 1), "Q", "4", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "MM:RM-MON/MONTH", "01:I-Jan/JANUARY", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "MM-RM.MON;MONTH", "06-VI.Jun;JUNE", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "MM,RM.MON:MONTH", "12,XII.Dec:DECEMBER", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "WW/W", "01/1", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 15), "WW-W", "24-3", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 30), "WW.W", "52.5", cb);

	testCVTDatetimeToFormatString(createDate(2023, 6, 4), "D;DAY-DY", "1;SUNDAY-Sun", cb);
	testCVTDatetimeToFormatString(createDate(2023, 6, 7), "D.DAY,DY", "4.WEDNESDAY,Wed", cb);
	testCVTDatetimeToFormatString(createDate(2023, 6, 10), "D DAY DY", "7 SATURDAY Sat", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "DDD", "001", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 12), "DDD", "012", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 15), "DDD", "166", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 31), "DDD", "365", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "J", "1721426", cb);
	testCVTDatetimeToFormatString(createDate(2000, 12, 8), "J", "2451887", cb);
	testCVTDatetimeToFormatString(createDate(9999, 12, 31), "J", "5373484", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIME)
{
	testCVTDatetimeToFormatString(createTime(0, 0, 0), "HH-HH12.HH24,MI/SS SSSSS", "12 AM-12 AM.00,00/00 0", cb);
	testCVTDatetimeToFormatString(createTime(12, 35, 15), "HH.HH12:HH24;MI-SS/SSSSS", "12 PM.12 PM:12;35-15/45315", cb);
	testCVTDatetimeToFormatString(createTime(23, 59, 59), " HH - HH12 . HH24 , MI / SS SSSSS ", " 11 PM - 11 PM . 23 , 59 / 59 86399 ", cb);

	testCVTDatetimeToFormatString(createTime(0, 0, 0, 1), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "1.10/100;1000:10000-100000,1000000-10000000 100000000", cb);
	testCVTDatetimeToFormatString(createTime(0, 0, 0, 1000), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "1.10/100;1000:10000-100000,1000000-10000000 100000000", cb);
	testCVTDatetimeToFormatString(createTime(0, 0, 0, 9999), "FF1.FF2/FF3;FF4:FF5-FF6,FF7-FF8 FF9", "9.99/999;9999:99990-999900,9999000-99990000 999900000", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIMESTAMP)
{
	ISC_TIMESTAMP timestamp = createTimeStamp(1982, 4, 21, 1, 34, 15, 2500);

	testCVTDatetimeToFormatString(timestamp, "YEAR.YYYY.YYY.YY.Y/J", "1982.1982.982.82.2/2445081", cb);
	testCVTDatetimeToFormatString(timestamp, "Q-MM-RM-MON-MONTH", "2-04-IV-Apr-APRIL", cb);
	testCVTDatetimeToFormatString(timestamp, "WW,W-D;DAY:DD DDD.DY", "16,3-4;WEDNESDAY:21 111.Wed", cb);
	testCVTDatetimeToFormatString(timestamp, "HH-HH12-HH24-MI-SS-SSSSS.FF2", "01 AM-01 AM-01-34-15-5655.25", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIME_TZ)
{
	testCVTDatetimeToFormatString(createTimeTZ(15, 35, 59, 0, 900), "HH-HH12-HH24-MI-SS-SSSSS.FF1/TZH/TZM", "03 PM-03 PM-15-35-59-56159.9/+00/00", cb);
	testCVTDatetimeToFormatString(createTimeTZ(15, 35, 59, 160), "HH24:MI-TZH:TZM", "15:35-+02:40", cb);
	testCVTDatetimeToFormatString(createTimeTZ(15, 35, 59, -160), "HH24:MI TZH:TZM", "15:35 -02:40", cb);

	testCVTDatetimeToFormatString(createTimeTZ(0, 0, 0, 160), "TZM:TZH", "+40:02", cb);
	testCVTDatetimeToFormatString(createTimeTZ(0, 0, 0, 160), "TZH MI TZM", "+02 00 +40", cb);
	testCVTDatetimeToFormatString(createTimeTZ(0, 0, 0, -160), "TZH MI TZM", "-02 00 -40", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIMESTAMP_TZ)
{
	ISC_TIMESTAMP_TZ timestampTZ = createTimeStampTZ(1982, 4, 21, 1, 34, 15, 0, 500);

	testCVTDatetimeToFormatString(timestampTZ, "YEAR.YYYY.YYY.YY.Y/J", "1982.1982.982.82.2/2445081", cb);
	testCVTDatetimeToFormatString(timestampTZ, "Q-MM-RM-MON-MONTH", "2-04-IV-Apr-APRIL", cb);
	testCVTDatetimeToFormatString(timestampTZ, "WW,W-D;DAY:DD DDD.DY", "16,3-4;WEDNESDAY:21 111.Wed", cb);
	testCVTDatetimeToFormatString(timestampTZ, "HH-HH12-HH24-MI-SS-SSSSS.FF2/TZH/TZM", "01 AM-01 AM-01-34-15-5655.50/+00/00", cb);

	testCVTDatetimeToFormatString(createTimeStampTZ(1982, 4, 21, 1, 34, 15, 70), "HH24:MI-TZH:TZM", "01:34-+01:10", cb);
	testCVTDatetimeToFormatString(createTimeStampTZ(1982, 4, 21, 1, 34, 15, -70), "HH24:MI TZH:TZM", "01:34 -01:10", cb);

	testCVTDatetimeToFormatString(createTimeStampTZ(1982, 4, 21, 0, 0, 0, 160), "TZM:TZH", "+40:02", cb);
	testCVTDatetimeToFormatString(createTimeStampTZ(1982, 4, 21, 0, 0, 0, 160), "TZH MI TZM", "+02 00 +40", cb);
	testCVTDatetimeToFormatString(createTimeStampTZ(1982, 4, 21, 0, 0, 0, -160), "TZH MI TZM", "-02 00 -40", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_SOLID_PATTERNS)
{
	ISC_TIMESTAMP_TZ timestampTZ = createTimeStampTZ(1982, 4, 21, 1, 34, 15, 0, 500);

	testCVTDatetimeToFormatString(timestampTZ, "YEARYYYYYYYYYYJ", "198219821982822445081", cb);
	testCVTDatetimeToFormatString(timestampTZ, "QMMRMMONMONTH", "204IVAprAPRIL", cb);
	testCVTDatetimeToFormatString(timestampTZ, "WWWD/DAYDDDDDDY", "1634/WEDNESDAY1111112", cb);
	testCVTDatetimeToFormatString(timestampTZ, "HHHH12HH24MISSSSSSSFF2TZHTZM", "01 AM01 AM013456551550+0000", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_RAW_TEXT)
{
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "YYYY-\"RaW TeXt\"-MON", "1981-RaW TeXt-Jul", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "YYYY-\"Raw Text with \\\"Quotes\\\"\"-MON", "1981-Raw Text with \"Quotes\"-Jul", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "YYYY-\"\\\\\\\"\\\\BS\\\\\\\"\\\\\"-YYYY", "1981-\\\"\\BS\\\"\\-1981", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "\"Test1\"-Y\"Test2\"", "Test1-1Test2", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "\"\"-Y\"Test2\"", "-1Test2", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "\"Test1\"-Y\"\"", "Test1-1", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "\"\"-Y\"\"", "-1", cb);
	testCVTDatetimeToFormatString(createDate(1981, 7, 12), "\"\"\"\"", "", cb);
}

BOOST_AUTO_TEST_SUITE_END()	// FunctionalTest
BOOST_AUTO_TEST_SUITE_END()	// CVTDatetimeToFormatString

BOOST_AUTO_TEST_SUITE(CVTStringToFormatDateTime)

static void testCVTStringToFormatDateTime(const string& date, const string& format,
	const ISC_TIMESTAMP_TZ& expected, Firebird::EXPECT_DATETIME expectedType, Callbacks& cb)
{
	string varyingString = "xx";
	varyingString += date;
	*(USHORT*) varyingString.data() = varyingString.size() - sizeof(USHORT);

	dsc desc;
	desc.dsc_dtype = dtype_varying;
	desc.dsc_length = varyingString.size() + sizeof(USHORT);
	desc.dsc_address = (UCHAR*) varyingString.data();
	desc.dsc_scale = 0;

	const ISC_TIMESTAMP_TZ result = CVT_string_to_format_datetime(&desc, format, &cb, expectedType);

	struct tm resultTimes;
	memset(&resultTimes, 0, sizeof(resultTimes));
	int resultFractions;
	NoThrowTimeStamp::decode_timestamp(result.utc_timestamp, &resultTimes, &resultFractions);
	SSHORT resultOffset;
	TimeZoneUtil::extractOffset(result, &resultOffset);

	struct tm expectedTimes;
	memset(&expectedTimes, 0, sizeof(expectedTimes));
	int expectedFractions;
	NoThrowTimeStamp::decode_timestamp(expected.utc_timestamp, &expectedTimes, &expectedFractions);
	SSHORT expectedOffset;
	TimeZoneUtil::extractOffset(expected, &expectedOffset);

	bool isEqual = !((bool) memcmp(&resultTimes, &expectedTimes, sizeof(struct tm)))
		&& resultFractions == expectedFractions && resultOffset == expectedOffset;

	BOOST_TEST(isEqual, "\nRESULT: " << DECOMPOSE_TM_STRUCT(resultTimes, resultFractions, resultOffset)
		<< "\nEXPECTED: " << DECOMPOSE_TM_STRUCT(expectedTimes, expectedFractions, expectedOffset));
}

static void testCVTStringToFormatDateTimeExpectDate(const string& date, const string& format,
	const ISC_TIMESTAMP_TZ& expected, Callbacks& cb)
{
	testCVTStringToFormatDateTime(date, format, expected, expect_sql_date, cb);
}

static void testCVTStringToFormatDateTimeExpectTime(const string& date, const string& format,
	const ISC_TIMESTAMP_TZ& expected, Callbacks& cb)
{
	testCVTStringToFormatDateTime(date, format, expected, expect_sql_time, cb);
};

static void testCVTStringToFormatDateTimeExpectTimeTZ(const string& date, const string& format,
	const ISC_TIMESTAMP_TZ& expected, Callbacks& cb)
{
	testCVTStringToFormatDateTime(date, format, expected, expect_sql_time_tz, cb);
};

BOOST_AUTO_TEST_SUITE(FunctionalTest)

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_DATE)
{
	testCVTStringToFormatDateTimeExpectDate("1", "YEAR", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1234", "YEAR", createTimeStampTZ(1234, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999", "YEAR", createTimeStampTZ(9999, 1, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YYYY", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1234", "YYYY", createTimeStampTZ(1234, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999", "YYYY", createTimeStampTZ(9999, 1, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YYY", createTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("522", "YYY", createTimeStampTZ(2522, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("999", "YYY", createTimeStampTZ(1999, 1, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YY", createTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("72", "YY", createTimeStampTZ(2072, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("99", "YY", createTimeStampTZ(1999, 1, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "Y", createTimeStampTZ(2001, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9", "Y", createTimeStampTZ(2009, 1, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "MM", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("6", "MM", createTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("12", "MM", createTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("Jan", "MON", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Jun", "MON", createTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Dec", "MON", createTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("January", "MONTH", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("June", "MONTH", createTimeStampTZ(1, 6, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("December", "MONTH", createTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("I", "RM", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("IV", "RM", createTimeStampTZ(1, 4, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("XII", "RM", createTimeStampTZ(1, 12, 1, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "DD", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("15", "DD", createTimeStampTZ(1, 1, 15, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("31", "DD", createTimeStampTZ(1, 1, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("2451887", "J", createTimeStampTZ(2000, 12, 8, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1721426", "J", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("5373484", "J", createTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1:1,1", "YEAR.MM.DD", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-8/13", "YEAR.MM.DD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999 12;31", "YEAR.MM.DD", createTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("25.Jan.25", "YY;MON;DD", createTimeStampTZ(2025, 1, 25, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("./.1981./-8--/13--", "  YEAR. -.MM.,,-.DD//", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TIME)
{
	testCVTStringToFormatDateTimeExpectTime("12 AM", "HH", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 AM", "HH", createTimeStampTZ(1, 1, 1, 1, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 AM", "HH", createTimeStampTZ(1, 1, 1, 11, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 PM", "HH", createTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 PM", "HH", createTimeStampTZ(1, 1, 1, 13, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 PM", "HH", createTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 AM", "HH12", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 AM", "HH12", createTimeStampTZ(1, 1, 1, 1, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 AM", "HH12", createTimeStampTZ(1, 1, 1, 11, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 PM", "HH12", createTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 PM", "HH12", createTimeStampTZ(1, 1, 1, 13, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 PM", "HH12", createTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "HH24", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("12", "HH24", createTimeStampTZ(1, 1, 1, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("23", "HH24", createTimeStampTZ(1, 1, 1, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "MI", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("30", "MI", createTimeStampTZ(1, 1, 1, 0, 30, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("59", "MI", createTimeStampTZ(1, 1, 1, 0, 59, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "SS", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("30", "SS", createTimeStampTZ(1, 1, 1, 0, 0, 30, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("59", "SS", createTimeStampTZ(1, 1, 1, 0, 0, 59, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "SSSSS", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("45315", "SSSSS", createTimeStampTZ(1, 1, 1, 12, 35, 15, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("86399", "SSSSS", createTimeStampTZ(1, 1, 1, 23, 59, 59, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF1", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("5", "FF1", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("9", "FF1", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9000), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF2", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF2", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("50", "FF2", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("99", "FF2", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9900), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF3", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 10), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF3", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("100", "FF3", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("500", "FF3", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("999", "FF3", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9990), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 10), cb);
	testCVTStringToFormatDateTimeExpectTime("100", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("1000", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("5000", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("9999", "FF4", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0, 9999), cb);

	testCVTStringToFormatDateTimeExpectTime("1 PM - 25 - 45 - 200", "HH.MI.SS.FF4", createTimeStampTZ(1, 1, 1, 13, 25, 45, 0, 200), cb);
	testCVTStringToFormatDateTimeExpectTime("15:0:15:2", "HH24.MI.SS.FF1", createTimeStampTZ(1, 1, 1, 15, 0, 15, 0, 2000), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TZ)
{
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00  2:30", "HH24:MI TZH:TZM", createTimeStampTZ(1, 1, 1, 12, 0, 0, 150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +2:30", "HH24:MI TZH:TZM", createTimeStampTZ(1, 1, 1, 12, 0, 0, 150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 -2:30", "HH24:MI TZH:TZM", createTimeStampTZ(1, 1, 1, 12, 0, 0, -150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +0:30", "HH24:MI TZH:TZM", createTimeStampTZ(1, 1, 1, 12, 0, 0, 30, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +0:00", "HH24:MI TZH:TZM", createTimeStampTZ(1, 1, 1, 12, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_SOLID_PATTERNS)
{
	testCVTStringToFormatDateTimeExpectTime("1 PM - 25 - 45 - 200", "HHMISSFF4", createTimeStampTZ(1, 1, 1, 13, 25, 45, 0, 200), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-8/13", "YEARMMDD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_SUITE_END()	// FunctionalTest
BOOST_AUTO_TEST_SUITE_END()	// CVTStringToFormatDateTime

BOOST_AUTO_TEST_SUITE_END() // CVTDatetimeFormat
BOOST_AUTO_TEST_SUITE_END()	// CVTSuite
