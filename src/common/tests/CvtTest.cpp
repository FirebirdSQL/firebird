#include "boost/test/unit_test.hpp"
#include "../common/tests/CvtTestUtils.h"
#include "../jrd/cvt_proto.h"

using namespace Firebird;
using namespace Jrd;
using namespace CvtTestUtils;


BOOST_AUTO_TEST_SUITE(CVTSuite)
BOOST_AUTO_TEST_SUITE(CVTDatetimeFormat)

// Currently we cannot print our error messages because we need master interface for that
static void errFunc(const Firebird::Arg::StatusVector& v)
{
	v.raise();
}

MockCallback cb(errFunc);

BOOST_AUTO_TEST_SUITE(CVTDatetimeToFormatString)

template<typename T>
static void testCVTDatetimeToFormatString(T date, const string& format, const string& expected, Callbacks& cb)
{
	try
	{
		dsc desc;
		desc.dsc_dtype = getDSCTypeFromDateType<T>();
		desc.dsc_length = sizeof(T);
		desc.dsc_address = (UCHAR*) &date;
		desc.dsc_scale = 0;

		BOOST_TEST_INFO("FORMAT: " << "\"" << format.c_str() << "\"");

		string result = CVT_datetime_to_format_string(&desc, format, &cb);

		BOOST_TEST(result == expected, "\nRESULT: " << result.c_str() << "\nEXPECTED: " << expected.c_str());
	}
	catch(const Exception& ex)
	{
		BOOST_TEST_INFO("Exception was caught!");
		BOOST_TEST(false);
	}
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
	testCVTDatetimeToFormatString(createDate(1, 2, 1), "Q", "1", cb);
	testCVTDatetimeToFormatString(createDate(1, 3, 1), "Q", "1", cb);
	testCVTDatetimeToFormatString(createDate(1, 4, 1), "Q", "2", cb);
	testCVTDatetimeToFormatString(createDate(1, 5, 1), "Q", "2", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "Q", "2", cb);
	testCVTDatetimeToFormatString(createDate(1, 7, 1), "Q", "3", cb);
	testCVTDatetimeToFormatString(createDate(1, 8, 1), "Q", "3", cb);
	testCVTDatetimeToFormatString(createDate(1, 9, 1), "Q", "3", cb);
	testCVTDatetimeToFormatString(createDate(1, 10, 1), "Q", "4", cb);
	testCVTDatetimeToFormatString(createDate(1, 11, 1), "Q", "4", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "Q", "4", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "MON", "Jan", cb);
	testCVTDatetimeToFormatString(createDate(1, 2, 1), "MON", "Feb", cb);
	testCVTDatetimeToFormatString(createDate(1, 3, 1), "MON", "Mar", cb);
	testCVTDatetimeToFormatString(createDate(1, 4, 1), "MON", "Apr", cb);
	testCVTDatetimeToFormatString(createDate(1, 5, 1), "MON", "May", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "MON", "Jun", cb);
	testCVTDatetimeToFormatString(createDate(1, 7, 1), "MON", "Jul", cb);
	testCVTDatetimeToFormatString(createDate(1, 8, 1), "MON", "Aug", cb);
	testCVTDatetimeToFormatString(createDate(1, 9, 1), "MON", "Sep", cb);
	testCVTDatetimeToFormatString(createDate(1, 10, 1), "MON", "Oct", cb);
	testCVTDatetimeToFormatString(createDate(1, 11, 1), "MON", "Nov", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "MON", "Dec", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "MONTH", "JANUARY", cb);
	testCVTDatetimeToFormatString(createDate(1, 2, 1), "MONTH", "FEBRUARY", cb);
	testCVTDatetimeToFormatString(createDate(1, 3, 1), "MONTH", "MARCH", cb);
	testCVTDatetimeToFormatString(createDate(1, 4, 1), "MONTH", "APRIL", cb);
	testCVTDatetimeToFormatString(createDate(1, 5, 1), "MONTH", "MAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "MONTH", "JUNE", cb);
	testCVTDatetimeToFormatString(createDate(1, 7, 1), "MONTH", "JULY", cb);
	testCVTDatetimeToFormatString(createDate(1, 8, 1), "MONTH", "AUGUST", cb);
	testCVTDatetimeToFormatString(createDate(1, 9, 1), "MONTH", "SEPTEMBER", cb);
	testCVTDatetimeToFormatString(createDate(1, 10, 1), "MONTH", "OCTOBER", cb);
	testCVTDatetimeToFormatString(createDate(1, 11, 1), "MONTH", "NOVEMBER", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "MONTH", "DECEMBER", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "RM", "I", cb);
	testCVTDatetimeToFormatString(createDate(1, 2, 1), "RM", "II", cb);
	testCVTDatetimeToFormatString(createDate(1, 3, 1), "RM", "III", cb);
	testCVTDatetimeToFormatString(createDate(1, 4, 1), "RM", "IV", cb);
	testCVTDatetimeToFormatString(createDate(1, 5, 1), "RM", "V", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "RM", "VI", cb);
	testCVTDatetimeToFormatString(createDate(1, 7, 1), "RM", "VII", cb);
	testCVTDatetimeToFormatString(createDate(1, 8, 1), "RM", "VIII", cb);
	testCVTDatetimeToFormatString(createDate(1, 9, 1), "RM", "IX", cb);
	testCVTDatetimeToFormatString(createDate(1, 10, 1), "RM", "X", cb);
	testCVTDatetimeToFormatString(createDate(1, 11, 1), "RM", "XI", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "RM", "XII", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "MM:RM-MON/MONTH", "01:I-Jan/JANUARY", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 1), "MM-RM.MON;MONTH", "06-VI.Jun;JUNE", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 1), "MM,RM.MON:MONTH", "12,XII.Dec:DECEMBER", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "WW/W", "01/1", cb);
	testCVTDatetimeToFormatString(createDate(1, 6, 15), "WW-W", "24-3", cb);
	testCVTDatetimeToFormatString(createDate(1, 12, 30), "WW.W", "52.5", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "DAY", "MONDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 2), "DAY", "TUESDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 3), "DAY", "WEDNESDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 4), "DAY", "THURSDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 5), "DAY", "FRIDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 6), "DAY", "SATURDAY", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 7), "DAY", "SUNDAY", cb);

	testCVTDatetimeToFormatString(createDate(1, 1, 1), "DY", "Mon", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 2), "DY", "Tue", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 3), "DY", "Wed", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 4), "DY", "Thu", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 5), "DY", "Fri", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 6), "DY", "Sat", cb);
	testCVTDatetimeToFormatString(createDate(1, 1, 7), "DY", "Sun", cb);

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
	testCVTDatetimeToFormatString(createTime(0, 0, 0), "HH-HH12.HH24,MI/SS SSSSS", "12 A.M.-12 A.M..00,00/00 0", cb);
	testCVTDatetimeToFormatString(createTime(12, 35, 15), "HH.HH12:HH24;MI-SS/SSSSS", "12 P.M..12 P.M.:12;35-15/45315", cb);
	testCVTDatetimeToFormatString(createTime(23, 59, 59), " HH - HH12 . HH24 , MI / SS SSSSS ", " 11 P.M. - 11 P.M. . 23 , 59 / 59 86399 ", cb);

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
	testCVTDatetimeToFormatString(timestamp, "HH-HH12-HH24-MI-SS-SSSSS.FF2", "01 A.M.-01 A.M.-01-34-15-5655.25", cb);
}

BOOST_AUTO_TEST_CASE(CVTDatetimeToFormatStringTest_TIME_TZ)
{
	testCVTDatetimeToFormatString(createTimeTZ(15, 35, 59, 0, 900), "HH-HH12-HH24-MI-SS-SSSSS.FF1/TZH/TZM", "03 P.M.-03 P.M.-15-35-59-56159.9/+00/00", cb);
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
	testCVTDatetimeToFormatString(timestampTZ, "HH-HH12-HH24-MI-SS-SSSSS.FF2/TZH/TZM", "01 A.M.-01 A.M.-01-34-15-5655.50/+00/00", cb);

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
	testCVTDatetimeToFormatString(timestampTZ, "HHHH12HH24MISSSSSSSFF2TZHTZM", "01 A.M.01 A.M.013456551550+0000", cb);
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
	try
	{
		string varyingString = "xx";
		varyingString += date;
		*(USHORT*) varyingString.data() = varyingString.size() - sizeof(USHORT);

		dsc desc;
		desc.dsc_dtype = dtype_varying;
		desc.dsc_length = varyingString.size() + sizeof(USHORT);
		desc.dsc_address = (UCHAR*) varyingString.data();
		desc.dsc_scale = 0;

		BOOST_TEST_INFO("INPUT: " << "\"" << date.c_str() << "\"");
		BOOST_TEST_INFO("FORMAT: " << "\"" << format.c_str() << "\"");

		const ISC_TIMESTAMP_TZ result = CVT_string_to_format_datetime(&desc, format, expectedType, &cb);

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
	catch(const Exception& ex)
	{
		BOOST_TEST_INFO("Exception was caught!");
		BOOST_TEST(false);
	}
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
	testCVTStringToFormatDateTimeExpectDate("1", "YEAR", createTimeStampTZ(1, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1234", "YEAR", createTimeStampTZ(1234, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999", "YEAR", createTimeStampTZ(9999, 0, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YYYY", createTimeStampTZ(1, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1234", "YYYY", createTimeStampTZ(1234, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999", "YYYY", createTimeStampTZ(9999, 0, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YYY", createTimeStampTZ(2001, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("522", "YYY", createTimeStampTZ(2522, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("523", "YYY", createTimeStampTZ(1523, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("999", "YYY", createTimeStampTZ(1999, 0, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "YY", createTimeStampTZ(2001, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("72", "YY", createTimeStampTZ(2072, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("73", "YY", createTimeStampTZ(1973, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("99", "YY", createTimeStampTZ(1999, 0, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "Y", createTimeStampTZ(2001, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9", "Y", createTimeStampTZ(2009, 0, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "MM", createTimeStampTZ(0, 1, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("6", "MM", createTimeStampTZ(0, 6, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("12", "MM", createTimeStampTZ(0, 12, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("Jan", "MON", createTimeStampTZ(0, 1, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Feb", "MON", createTimeStampTZ(0, 2, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Mar", "MON", createTimeStampTZ(0, 3, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Apr", "MON", createTimeStampTZ(0, 4, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("May", "MON", createTimeStampTZ(0, 5, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Jun", "MON", createTimeStampTZ(0, 6, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Jul", "MON", createTimeStampTZ(0, 7, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Aug", "MON", createTimeStampTZ(0, 8, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Sep", "MON", createTimeStampTZ(0, 9, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Oct", "MON", createTimeStampTZ(0, 10, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Nov", "MON", createTimeStampTZ(0, 11, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("Dec", "MON", createTimeStampTZ(0, 12, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("January",   "MONTH", createTimeStampTZ(0, 1, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("February",  "MONTH", createTimeStampTZ(0, 2, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("March",     "MONTH", createTimeStampTZ(0, 3, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("April",     "MONTH", createTimeStampTZ(0, 4, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("May",       "MONTH", createTimeStampTZ(0, 5, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("June",      "MONTH", createTimeStampTZ(0, 6, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("July",      "MONTH", createTimeStampTZ(0, 7, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("August",    "MONTH", createTimeStampTZ(0, 8, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("September", "MONTH", createTimeStampTZ(0, 9, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("October",   "MONTH", createTimeStampTZ(0, 10, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("November",  "MONTH", createTimeStampTZ(0, 11, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("December",  "MONTH", createTimeStampTZ(0, 12, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("I",    "RM", createTimeStampTZ(0, 1, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("II",   "RM", createTimeStampTZ(0, 2, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("III",  "RM", createTimeStampTZ(0, 3, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("IV",   "RM", createTimeStampTZ(0, 4, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("V",    "RM", createTimeStampTZ(0, 5, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("VI",   "RM", createTimeStampTZ(0, 6, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("VII",  "RM", createTimeStampTZ(0, 7, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("VIII", "RM", createTimeStampTZ(0, 8, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("IX",   "RM", createTimeStampTZ(0, 9, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("X",    "RM", createTimeStampTZ(0, 10, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("XI",   "RM", createTimeStampTZ(0, 11, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("XII",  "RM", createTimeStampTZ(0, 12, 0, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1", "DD", createTimeStampTZ(0, 0, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("15", "DD", createTimeStampTZ(0, 0, 15, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("31", "DD", createTimeStampTZ(0, 0, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("2451887", "J", createTimeStampTZ(2000, 12, 8, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1721426", "J", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("5373484", "J", createTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1:1,1", "YEAR.MM.DD", createTimeStampTZ(1, 1, 1, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-8/13", "YEAR.MM.DD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("9999 12;31", "YEAR.MM.DD", createTimeStampTZ(9999, 12, 31, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("1981-Aug/13", "YEAR.MON.DD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-August/13", "YEAR.MONTH.DD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-VIII/13", "YEAR.RM.DD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectDate("25.Jan.25", "YY;MON;DD", createTimeStampTZ(2025, 1, 25, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectDate("./.1981./-8--/13--", "  YEAR. -.MM.,,-.DD//", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TIME)
{
	testCVTStringToFormatDateTimeExpectTime("12 A.M.", "HH", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 A.M.", "HH", createTimeStampTZ(0, 0, 0, 1, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 A.M.", "HH", createTimeStampTZ(0, 0, 0, 11, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 P.M.", "HH", createTimeStampTZ(0, 0, 0, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 P.M.", "HH", createTimeStampTZ(0, 0, 0, 13, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 P.M.", "HH", createTimeStampTZ(0, 0, 0, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 A.M.", "HH12", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 A.M.", "HH12", createTimeStampTZ(0, 0, 0, 1, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 A.M.", "HH12", createTimeStampTZ(0, 0, 0, 11, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("12 P.M.", "HH12", createTimeStampTZ(0, 0, 0, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("1 P.M.", "HH12", createTimeStampTZ(0, 0, 0, 13, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("11 P.M.", "HH12", createTimeStampTZ(0, 0, 0, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "HH24", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("12", "HH24", createTimeStampTZ(0, 0, 0, 12, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("23", "HH24", createTimeStampTZ(0, 0, 0, 23, 0, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "MI", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("30", "MI", createTimeStampTZ(0, 0, 0, 0, 30, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("59", "MI", createTimeStampTZ(0, 0, 0, 0, 59, 0, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "SS", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("30", "SS", createTimeStampTZ(0, 0, 0, 0, 0, 30, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("59", "SS", createTimeStampTZ(0, 0, 0, 0, 0, 59, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("0", "SSSSS", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("45315", "SSSSS", createTimeStampTZ(0, 0, 0, 12, 35, 15, 0), cb);
	testCVTStringToFormatDateTimeExpectTime("86399", "SSSSS", createTimeStampTZ(0, 0, 0, 23, 59, 59, 0), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF1", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("5", "FF1", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("9", "FF1", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 9000), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF2", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF2", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("50", "FF2", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("99", "FF2", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 9900), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF3", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 10), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF3", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("100", "FF3", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("500", "FF3", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("999", "FF3", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 9990), cb);

	testCVTStringToFormatDateTimeExpectTime("1", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 1), cb);
	testCVTStringToFormatDateTimeExpectTime("10", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 10), cb);
	testCVTStringToFormatDateTimeExpectTime("100", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 100), cb);
	testCVTStringToFormatDateTimeExpectTime("1000", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 1000), cb);
	testCVTStringToFormatDateTimeExpectTime("5000", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 5000), cb);
	testCVTStringToFormatDateTimeExpectTime("9999", "FF4", createTimeStampTZ(0, 0, 0, 0, 0, 0, 0, 9999), cb);

	testCVTStringToFormatDateTimeExpectTime("1 P.M. - 25 - 45 - 200", "HH.MI.SS.FF4", createTimeStampTZ(0, 0, 0, 13, 25, 45, 0, 200), cb);
	testCVTStringToFormatDateTimeExpectTime("15:0:15:2", "HH24.MI.SS.FF1", createTimeStampTZ(0, 0, 0, 15, 0, 15, 0, 2000), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_TZ)
{
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00  2:30", "HH24:MI TZH:TZM", createTimeStampTZ(0, 0, 0, 12, 0, 0, 150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +2:30", "HH24:MI TZH:TZM", createTimeStampTZ(0, 0, 0, 12, 0, 0, 150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 -2:30", "HH24:MI TZH:TZM", createTimeStampTZ(0, 0, 0, 12, 0, 0, -150, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +0:30", "HH24:MI TZH:TZM", createTimeStampTZ(0, 0, 0, 12, 0, 0, 30, 0), cb);
	testCVTStringToFormatDateTimeExpectTimeTZ("12:00 +0:00", "HH24:MI TZH:TZM", createTimeStampTZ(0, 0, 0, 12, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_CASE(CVTStringToFormatDateTime_SOLID_PATTERNS)
{
	testCVTStringToFormatDateTimeExpectTime("1 P.M. - 25 - 45 - 200", "HHMISSFF4", createTimeStampTZ(0, 0, 0, 13, 25, 45, 0, 200), cb);
	testCVTStringToFormatDateTimeExpectDate("1981-8/13", "YEARMMDD", createTimeStampTZ(1981, 8, 13, 0, 0, 0, 0), cb);
}

BOOST_AUTO_TEST_SUITE_END()	// FunctionalTest
BOOST_AUTO_TEST_SUITE_END()	// CVTStringToFormatDateTime

BOOST_AUTO_TEST_SUITE_END() // CVTDatetimeFormat
BOOST_AUTO_TEST_SUITE_END()	// CVTSuite
