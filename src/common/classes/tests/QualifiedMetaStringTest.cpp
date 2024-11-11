#include "boost/test/unit_test.hpp"
#include "boost/test/data/test_case.hpp"
#include "../common/classes/QualifiedMetaString.h"

using namespace Firebird;
using std::make_tuple;


BOOST_AUTO_TEST_SUITE(QualifiedMetaStringSuite)
BOOST_AUTO_TEST_SUITE(QualifiedMetaStringTests)


const auto parseTestData = boost::unit_test::data::make({
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

BOOST_DATA_TEST_CASE(ParseTest, parseTestData, input, expectedSchema, expectedObject)
{
	const auto name = QualifiedMetaString::parseSchemaObject(input);
	BOOST_TEST(name.schema == MetaString(expectedSchema));
	BOOST_TEST(name.object == MetaString(expectedObject));
}


const auto parseErrorTestData = boost::unit_test::data::make({
	"1Object",
	"_Object",
	"$Object",
	"\"\"name\"",
	"Sche ma.Na me",
	"Sch.ema.\"Na.me\"",
	"a.b.c",
	"a\"b\".c\"d\"",
});

BOOST_DATA_TEST_CASE(ParseErrorTest, parseErrorTestData, input)
{
	BOOST_CHECK_THROW(QualifiedMetaString::parseSchemaObject(input), status_exception);
}



BOOST_AUTO_TEST_SUITE_END()	// QualifiedMetaStringTests
BOOST_AUTO_TEST_SUITE_END()	// QualifiedMetaStringSuite
