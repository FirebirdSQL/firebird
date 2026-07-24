#include "boost/test/unit_test.hpp"
#include "../../TestContext.h"

#include "../jrd/json/classes/JsonDatetime.h"

#include "../common/tests/CvtTestUtils.h"
#include "../jrd/cvt_proto.h"
#include "fb_exception.h"

#include <utility>
#include <algorithm>

using namespace FBJSON;


BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(JsonTypesTests)

BOOST_AUTO_TEST_SUITE(JsonDatetimeTests)

constexpr int YEAR = 2023;

bool compareDatetime(ISC_TIMESTAMP_TZ result, ISC_TIMESTAMP_TZ expected)
{
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

	return !((bool) memcmp(&resultTimes, &expectedTimes, sizeof(struct tm)))
		&& resultFractions == expectedFractions && resultOffset == expectedOffset;
}

bool compareTimeOnly(ISC_TIMESTAMP_TZ result, ISC_TIMESTAMP_TZ expected)
{
	struct tm resultTimes;
	memset(&resultTimes, 0, sizeof(resultTimes));
	NoThrowTimeStamp::decode_timestamp(result.utc_timestamp, &resultTimes);

	struct tm expectedTimes;
	memset(&expectedTimes, 0, sizeof(expectedTimes));
	NoThrowTimeStamp::decode_timestamp(expected.utc_timestamp, &expectedTimes);

	return resultTimes.tm_hour == expectedTimes.tm_hour &&
		resultTimes.tm_min == expectedTimes.tm_min &&
		resultTimes.tm_sec == expectedTimes.tm_sec;
}

static void errFunc(const Firebird::Arg::StatusVector& v)
{
	v.raise();
}

BOOST_FIXTURE_TEST_CASE(SetFromStringTests, EngineHolder)
{
	using namespace std::string_view_literals;

	JsonDatetime test(*getDefaultMemoryPool());
	BOOST_CHECK_THROW(test.set("1443324 5123332"sv, ""), Firebird::Exception);

	CvtTestUtils::MockCallback cb(errFunc, std::bind(CvtTestUtils::mockGetLocalDate, YEAR));

	test.set("2018-01-01"sv, "", &cb);
	BOOST_TEST(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(2018, 1, 1, 0, 0, 0, 0)));
	BOOST_TEST(test.getTypeName() == Tokens::TypeName::DATE);


	test.set("01:23:45 +02:00"sv, "", &cb);
	BOOST_TEST(compareTimeOnly(test.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 0, 1, 23, 45, 120)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIME_WITH_TZ);


	test.set("01:23:45"sv, "", &cb);
	BOOST_TEST(compareTimeOnly(test.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 0, 1, 23, 45, 0)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIME_WITHOUT_TZ);


	test.set("2018-01-01 01:23:45 +02:00"sv, "", &cb);
	BOOST_TEST(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(2018, 1, 1, 1, 23, 45, 120)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIMESTAMT_WITH_TZ);


	test.set("2018-01-01 01:23:45"sv, "", &cb);
	BOOST_TEST(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(2018, 1, 1, 1, 23, 45, 0)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIMESTAMT_WITHOUT_TZ);
}


BOOST_FIXTURE_TEST_CASE(SetFromStringWithFormatTests, EngineHolder)
{
	using namespace std::string_view_literals ;

	CvtTestUtils::MockCallback cb(errFunc, std::bind(CvtTestUtils::mockGetLocalDate, YEAR));
	JsonDatetime test(*getDefaultMemoryPool());

	test.set("14"sv, "DD"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 14, 0, 0, 0, 0)));
	BOOST_TEST(test.getTypeName() == Tokens::TypeName::DATE);


	test.set("01:23 +02"sv, "HH24:MI TZH"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 0, 1, 23, 0, 120)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIME_WITH_TZ);


	test.set("01:23:00"sv, "HH24:MI:SS"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 0, 1, 23, 0, 0)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIME_WITHOUT_TZ);


	test.set("2018 04 02 01:23 +02"sv, "YYYY DD MM HH24:MI TZH"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(2018, 2, 4, 1, 23, 0, 120)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIMESTAMT_WITH_TZ);


	test.set("2018 04 02 01:23"sv, "YYYY DD MM HH24:MI"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(2018, 2, 4, 1, 23, 0, 0)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIMESTAMT_WITHOUT_TZ);


	test.set("14 5"sv, "DD TZH"sv, &cb);
	BOOST_CHECK(compareDatetime(test.getTS(),
		CvtTestUtils::createTimeStampTZ(00, 0, 14, 0, 0, 0, 300)));

	BOOST_TEST(test.getTypeName() == Tokens::TypeName::TIMESTAMT_WITH_TZ);
}


BOOST_FIXTURE_TEST_CASE(RedefineDatetimeFormatTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime datetime(*getDefaultMemoryPool());
	CvtTestUtils::MockCallback cb(errFunc, std::bind(CvtTestUtils::mockGetLocalDate, YEAR));

	// Time
	datetime.set("12:10"sv, "", &cb);
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::TIME_WITHOUT_TZ);
	BOOST_CHECK(compareTimeOnly(datetime.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 0, 0, 12, 10, 0, 0)));

	// Date
	datetime.updateFormat("MM:DD"sv, &cb);
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::DATE);
	BOOST_CHECK(compareDatetime(datetime.getTS(),
		CvtTestUtils::createTimeStampTZ(0, 12, 10, 0, 0, 0, 0)));
}

BOOST_FIXTURE_TEST_CASE(InvalidInputTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime datetime(*getDefaultMemoryPool());
	BOOST_CHECK_THROW(datetime.set("14 5"sv, "DD"), Firebird::Exception);
	BOOST_CHECK_THROW(datetime.set("14 532fe4"sv, ""), Firebird::Exception);
}

void testMore(JsonDatetime dateA, JsonDatetime dateB)
{
	// A vs B
	BOOST_CHECK(dateA > dateB);
	BOOST_CHECK((dateA < dateB) == false);
	BOOST_CHECK((dateA == dateB) == false);
	BOOST_CHECK((dateA != dateB) == true);

	// A vs A
	BOOST_CHECK(dateA == dateA);
	BOOST_CHECK((dateA != dateA) == false);
}

BOOST_FIXTURE_TEST_CASE(DatetimeCompareTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime dateA(*getDefaultMemoryPool());
	JsonDatetime dateB(*getDefaultMemoryPool());

	// Date
	dateA.set("2018-02-01"sv);

	dateB.set("2018-01-01"sv);

	testMore(dateA, dateB);

	// Timestamp
	dateA.set("2018-01-01 01:25:45"sv);

	dateB.set("2018-01-01 01:23:45"sv);
	testMore(dateA, dateB);

	// Time
	dateA.set("01:25:45"sv);
	dateB.set("01:23:45"sv);
	testMore(dateA, dateB);
}

BOOST_FIXTURE_TEST_CASE(DatetimeMethodsTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime date(*getDefaultMemoryPool());

	// We need a bit string to test the move
	constexpr std::string_view sourceStr = "12:10                                                                 "sv;
	SmallString toString(*getDefaultMemoryPool());

	static_assert(sourceStr.length() > Firebird::string::INLINE_BUFFER_SIZE);
	SmallString fromString(*getDefaultMemoryPool(), sourceStr.data(), sourceStr.length());
	date.set(std::move(fromString), ""sv);

	// Should be empty after the move
	BOOST_TEST(fromString.empty());

	// Move from datetime
	toString = date.extractString();
	BOOST_TEST((std::string_view)toString == sourceStr);

	// Should be empty after the move
	toString = date.extractString();
	BOOST_TEST(toString.empty());
}

BOOST_FIXTURE_TEST_CASE(ZeroTimeTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime datetime(*getDefaultMemoryPool());

	// Time
	datetime.set("00:00"sv, "");
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::TIME_WITHOUT_TZ);

	datetime.set("00:00 +2:00"sv, "");
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::TIME_WITH_TZ);

	datetime.set("13.01.2023 00:00"sv, "");
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::TIMESTAMT_WITHOUT_TZ);

	datetime.set("13.01.2023 00:00 +2:00"sv, "");
	BOOST_TEST(datetime.getTypeName() == Tokens::TypeName::TIMESTAMT_WITH_TZ);
}

BOOST_FIXTURE_TEST_CASE(UtcTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime timeBase(*getDefaultMemoryPool());
	JsonDatetime timeTs(*getDefaultMemoryPool());
	JsonDatetime timeTsSame(*getDefaultMemoryPool());

	// Timestamp
	timeBase.set("10.10.2022 10:3:10 +00:00"sv, ""sv);
	timeTs.set("10.10.2022 10:3:10 -00:02"sv, ""sv);
	timeTsSame.set("10.10.2022 10:1:10 -00:02"sv, ""sv);
	testMore(timeTs, timeBase);

	BOOST_CHECK(timeBase == timeTsSame);

	// Time
	timeBase.set("10:3:10 +00:00"sv, ""sv);
	timeTs.set("10:3:10 -00:02"sv, ""sv);
	timeTsSame.set("10:1:10 -00:02"sv, ""sv);
	testMore(timeTs, timeBase);

	BOOST_CHECK(timeBase == timeTsSame);
}

BOOST_FIXTURE_TEST_CASE(SetFromDscTests, EngineHolder)
{
	using namespace std::string_view_literals;
	JsonDatetime textDt(*getDefaultMemoryPool());
	JsonDatetime dscTd(*getDefaultMemoryPool());

	dsc textDsc;
	UCHAR buffer[64];
	dsc outDsc;

	{
		auto sourceTime = "10:03"sv;
		textDt.set(sourceTime, ""sv);

		textDsc.makeText(sourceTime.length(), ttype_ascii, (UCHAR*)sourceTime.data());

		outDsc.makeTime();
		outDsc.dsc_address = buffer;
		CVT_move(&textDsc, &outDsc, 0);
		dscTd.set(JRD_get_thread_data(), outDsc);

		BOOST_CHECK(textDt == dscTd);
	}

	{
		auto sourceTime = "10:03 +00:00"sv;
		textDt.set(sourceTime, ""sv);

		textDsc.makeText(sourceTime.length(), ttype_ascii, (UCHAR*)sourceTime.data());

		outDsc.makeTimeTz();
		outDsc.dsc_address = buffer;
		CVT_move(&textDsc, &outDsc, 0);
		dscTd.set(JRD_get_thread_data(), outDsc);

		BOOST_CHECK(textDt == dscTd);
	}

	{
		auto sourceTime = "10.03.2011"sv;
		textDt.set(sourceTime, ""sv);

		textDsc.makeText(sourceTime.length(), ttype_ascii, (UCHAR*)sourceTime.data());

		outDsc.makeDate();
		outDsc.dsc_address = buffer;
		CVT_move(&textDsc, &outDsc, 0);
		dscTd.set(JRD_get_thread_data(), outDsc);

		BOOST_CHECK(textDt == dscTd);
	}

	{
		auto sourceTime = "10.03.2011 10:20"sv;
		textDt.set(sourceTime, ""sv);

		textDsc.makeText(sourceTime.length(), ttype_ascii, (UCHAR*)sourceTime.data());

		outDsc.makeTimestamp();
		outDsc.dsc_address = buffer;
		CVT_move(&textDsc, &outDsc, 0);
		dscTd.set(JRD_get_thread_data(), outDsc);

		BOOST_CHECK(textDt == dscTd);
	}

	{
		auto sourceTime = "10.03.2011 10:20 +00:00"sv;
		textDt.set(sourceTime, ""sv);

		textDsc.makeText(sourceTime.length(), ttype_ascii, (UCHAR*)sourceTime.data());

		outDsc.makeTimestampTz();
		outDsc.dsc_address = buffer;
		CVT_move(&textDsc, &outDsc, 0);
		dscTd.set(JRD_get_thread_data(), outDsc);

		BOOST_CHECK(textDt == dscTd);
	}
}


BOOST_AUTO_TEST_SUITE_END()	// JsonDatetimeTests

BOOST_AUTO_TEST_SUITE_END()	// JsonTypesTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
