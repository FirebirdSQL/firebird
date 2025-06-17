#include "boost/test/unit_test.hpp"
#include "boost/test/data/test_case.hpp"
#include "../common/classes/QualifiedMetaString.h"
#include <numeric>
#include <tuple>

using namespace Firebird;
using std::make_tuple;


BOOST_AUTO_TEST_SUITE(QualifiedMetaStringSuite)
BOOST_AUTO_TEST_SUITE(QualifiedMetaStringTests)


const auto parseSchemaObjectListNoSepTestData = boost::unit_test::data::make({
	make_tuple("Object1", "\"OBJECT1\""),
	make_tuple(" Object2 ", "\"OBJECT2\""),
	make_tuple(" Object3  Object4 ", "\"OBJECT3\", \"OBJECT4\""),
	make_tuple(" \"Object5 \"  \" Object6 \" ", "\"Object5\", \" Object6\""),
	make_tuple(" Object7  \" Object8 \" ", "\"OBJECT7\", \" Object8\""),
	make_tuple(" Object9    Object10  ", "\"OBJECT9\", \"OBJECT10\""),
	make_tuple("Schema1 . Obj1 Schema2.Obj2 Obj3  ", "\"SCHEMA1\".\"OBJ1\", \"SCHEMA2\".\"OBJ2\", \"OBJ3\""),
});

BOOST_DATA_TEST_CASE(ParseSchemaObjectListNoSepTest, parseSchemaObjectListNoSepTestData, input, formattedList)
{
	const auto parseAndFormatList = [](const char* input) -> std::string
	{
		ObjectsArray<QualifiedMetaString> list;
		QualifiedMetaString::parseSchemaObjectListNoSep(input, list);

		if (list.hasData())
		{
			return std::accumulate(
				std::next(list.begin()),
				list.end(),
				std::string(list[0].toQuotedString().c_str()),
				[](const auto& a, const auto& b) {
					return a + ", " + (b.object.hasData() ? b.toQuotedString().c_str() : "\" \"");
				}
			);
		}
		else
			return {};
	};

	BOOST_TEST(parseAndFormatList(input) == formattedList);
}


const auto parseSchemaObjectTestData = boost::unit_test::data::make({
	make_tuple("Object", "", "OBJECT"),
	make_tuple("Schema$_0.{Object}", "SCHEMA$_0", "{OBJECT}"),
	make_tuple("Schema.Object", "SCHEMA", "OBJECT"),
	make_tuple("\"A\"\"name\"", "", "A\"name"),
	make_tuple("Schema.\"Name\"", "SCHEMA", "Name"),
	make_tuple("\"Schema\".Name", "Schema", "NAME"),
	make_tuple("\"Sche ma\".\"Na me\"", "Sche ma", "Na me"),
	make_tuple(" x . y ", "X", "Y"),
	make_tuple("  \" x \"   .  \" y \"   ", " x ", " y "),
	make_tuple("  \"x\"   .  \"y\"   ", "x", "y"),
	make_tuple(" \"Sch\"\"ma\" . \"Obj\"\"ect\" ", "Sch\"ma", "Obj\"ect"),
});

BOOST_DATA_TEST_CASE(ParseSchemaObjectTest, parseSchemaObjectTestData, input, expectedSchema, expectedObject)
{
	const auto name = QualifiedMetaString::parseSchemaObject(input);
	BOOST_TEST(name.schema == MetaString(expectedSchema));
	BOOST_TEST(name.object == MetaString(expectedObject));
}


const auto parseSchemaObjectErrorTestData = boost::unit_test::data::make({
	"1Object",
	"_Object",
	"$Object",
	"\"\"name\"",
	"Sche ma.Na me",
	"Sch.ema.\"Na.me\"",
	"a.b.c",
	"a\"b\".c\"d\"",
	"",
	" ",
	".",
	" . ",
	"\"\"",
	"\" \"",
	"\"\".\"\"",
	"\" \".\" \"",
	"\" \" . \" \"",
});

BOOST_DATA_TEST_CASE(ParseSchemaObjectErrorTest, parseSchemaObjectErrorTestData, input)
{
	BOOST_CHECK_THROW(QualifiedMetaString::parseSchemaObject(input), status_exception);
}


BOOST_AUTO_TEST_SUITE_END()	// QualifiedMetaStringTests
BOOST_AUTO_TEST_SUITE_END()	// QualifiedMetaStringSuite
