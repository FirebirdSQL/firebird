#include "boost/test/unit_test.hpp"
#include "boost/test/data/test_case.hpp"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include <numeric>
#include <string>
#include <tuple>

using namespace Firebird;
using std::make_tuple;


BOOST_AUTO_TEST_SUITE(MetaStringSuite)
BOOST_AUTO_TEST_SUITE(MetaStringTests)


const auto parseTestData = boost::unit_test::data::make({
	make_tuple("Object1", "\"OBJECT1\""),
	make_tuple(" Object2 ", "\"OBJECT2\""),
	make_tuple(" Object3 , Object4 ", "\"OBJECT3\", \"OBJECT4\""),
	make_tuple(" \"Object5 \" , \" Object6 \" ", "\"Object5\", \" Object6\""),
	make_tuple(" Object7 , \" Object8 \" ", "\"OBJECT7\", \" Object8\""),
	make_tuple(" Object9 ,   Object10  ", "\"OBJECT9\", \"OBJECT10\""),
});

BOOST_DATA_TEST_CASE(ParseTest, parseTestData, input, formattedList)
{
	const auto parseAndFormatList = [](const char* input) -> std::string
	{
		ObjectsArray<MetaString> list;
		MetaString::parseList(input, list);

		if (list.hasData())
		{
			return std::accumulate(
				std::next(list.begin()),
				list.end(),
				std::string(list[0].toQuotedString().c_str()),
				[](const auto& a, const auto& b) {
					return a + ", " + (b.hasData() ? b.toQuotedString().c_str() : "\" \"");
				}
			);
		}
		else
			return {};
	};

	BOOST_TEST(parseAndFormatList(input) == formattedList);
}


const auto parseErrorTestData = boost::unit_test::data::make({
	"1Object",
	"_Object",
	"$Object",
	"\"\"name\"",
	"Na me",
	"Na.me",
	"Name,"
	"Name, "
	","
	"Name,,Name",
});

BOOST_DATA_TEST_CASE(ParseErrorTest, parseErrorTestData, input)
{
	ObjectsArray<MetaString> array;
	BOOST_CHECK_THROW(MetaString::parseList(input, array), status_exception);
}


BOOST_AUTO_TEST_SUITE_END()	// MetaStringTests
BOOST_AUTO_TEST_SUITE_END()	// MetaStringSuite
