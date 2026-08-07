#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/json/JsonUtils.h"
#include "JsonTestUtils.h"

using namespace Firebird;


class LocaleChangeHolder
{
public:
	LocaleChangeHolder(const char* newLocale)
	{
		m_prevLocale = std::setlocale(LC_NUMERIC, nullptr);
		BOOST_ASSERT(m_prevLocale != nullptr);

		const char* loc = std::setlocale(LC_NUMERIC, newLocale);
		BOOST_ASSERT(loc != nullptr);
	}

	~LocaleChangeHolder()
	{
		const char* loc = std::setlocale(LC_NUMERIC, m_prevLocale);
		BOOST_ASSERT(loc != nullptr);
	}

private:
	char* m_prevLocale;
};

BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(ConvertTests)

BOOST_AUTO_TEST_SUITE(FractionalTest)

constexpr double NUMBER_TO_CONVERT = 3.1415926535'8979323846'2643383279'5028841971'6939937510;


BOOST_AUTO_TEST_CASE(ruUtf8LocaleTest)
{
	SKIP_TEST; // Missing locale
	LocaleChangeHolder ruEncoding("ru_RU.UTF-8");

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, NUMBER_TO_CONVERT).find('.') != std::string_view::npos);
}

BOOST_AUTO_TEST_CASE(enUtf8LocaleTest)
{
	LocaleChangeHolder ruEncoding("en_US.UTF-8");

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, NUMBER_TO_CONVERT).find('.') != std::string_view::npos);
}

BOOST_AUTO_TEST_CASE(russianLocaleTest)
{
	SKIP_TEST // Missing locale
	LocaleChangeHolder ruEncoding("russian");

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, NUMBER_TO_CONVERT).find('.') != std::string_view::npos);
}

BOOST_AUTO_TEST_SUITE_END()	// FractionalTest


BOOST_AUTO_TEST_SUITE(TypesTest)

BOOST_AUTO_TEST_CASE(floatTest)
{
	const float floatNumber = 3.14151f;
	std::string_view expectedString = "3.14151";

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, floatNumber).substr(0, expectedString.length()) == "3.14151");
}

BOOST_AUTO_TEST_SUITE_END()	// FractionalTest

BOOST_AUTO_TEST_CASE(doubleTest)
{
	const double doubleNumber = 3.1415926;

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, doubleNumber) == "3.1415926");
}

BOOST_AUTO_TEST_CASE(intTest)
{
	const int32_t intNumber = 2147483646;

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, intNumber) == "2147483646");
}


BOOST_AUTO_TEST_CASE(NegativeintTest)
{
	const int32_t intNumber = -2147483646;

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, intNumber) == "-2147483646");
}
BOOST_AUTO_TEST_CASE(bigintTest)
{
	const uint64_t bigintNumber = 9223372037;

	FBJSON::NumberConvertBuffer buffer;
	BOOST_TEST(FBJSON::convertNumberToString(buffer, bigintNumber) == "9223372037");
}

BOOST_AUTO_TEST_SUITE_END()	// ConvertTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
