#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/objects_array.h"
#include "../common/dsc.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(ObjectsArraySuite)


BOOST_AUTO_TEST_SUITE(ObjectsArrayTests)

BOOST_AUTO_TEST_CASE(IteratorTest)
{
	ObjectsArray<dsc> objArray(*getDefaultMemoryPool());

	objArray.grow(10);

	auto count = 0;
	for (auto it = objArray.begin(); it < objArray.end(); ++it)
	{
		it->dsc_length = count++;
	}

	int expected = 0;
	for (auto it = objArray.begin(); it < objArray.end(); ++it)
	{
		BOOST_TEST(it->dsc_length == expected);
		expected++;
	}

	expected = 0;
	for (auto it = objArray.begin(); it < objArray.end(); it += 2)
	{
		BOOST_TEST(it->dsc_length == expected);
		expected += 2;
	}

	expected = 0;
	for (auto it = objArray.begin(); it < objArray.end(); it += 3)
	{
		BOOST_TEST(it->dsc_length == expected);

		expected += 3;
	}
}

BOOST_AUTO_TEST_SUITE_END()	// ObjectsArrayTests

BOOST_AUTO_TEST_SUITE_END()	// ObjectsArraySuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
