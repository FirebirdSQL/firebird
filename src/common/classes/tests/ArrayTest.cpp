#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/Aligner.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(ArraySuite)


BOOST_AUTO_TEST_SUITE(ArrayTests)

BOOST_AUTO_TEST_CASE(ConstructionWithStdInitializerTest)
{
	Array<int> array(*getDefaultMemoryPool(), {1, 2, 3, 4});

	BOOST_TEST(array.getCount() == 4);
	BOOST_TEST(array[0] == 1);
	BOOST_TEST(array[3] == 4);
}

BOOST_AUTO_TEST_CASE(ClearTest)
{
	Array<int> array(*getDefaultMemoryPool(), {1, 2, 3, 4});

	BOOST_TEST(array.getCount() == 4);

	array.clear();
	BOOST_TEST(array.getCount() == 0);
}

BOOST_AUTO_TEST_CASE(IsEmptyAndHasDataTest)
{
	Array<int> array(*getDefaultMemoryPool(), {1, 2, 3, 4});

	BOOST_TEST(array.getCount() > 0);
	BOOST_TEST(!array.isEmpty());
	BOOST_TEST(array.hasData());

	array.clear();
	BOOST_TEST(array.getCount() == 0);
	BOOST_TEST(array.isEmpty());
	BOOST_TEST(!array.hasData());
}

BOOST_AUTO_TEST_CASE(CapacityAndCountTest)
{
	Array<int> array1(10);
	BOOST_TEST(array1.getCapacity() == 10);
	BOOST_TEST(array1.getCount() == 0);

	Array<int> array2(*getDefaultMemoryPool(), 11);
	BOOST_TEST(array2.getCapacity() == 11);
	BOOST_TEST(array2.getCount() == 0);
}

BOOST_AUTO_TEST_SUITE_END()	// ArrayTests


BOOST_AUTO_TEST_SUITE(SortedArrayTests)

BOOST_AUTO_TEST_CASE(IteratorTest)
{
	SortedArray<int> array;

	// Warning: push does not add elements in order!
	array.add(3);
	array.add(2);
	array.add(4);
	array.add(0);
	array.add(1);

	int expected = 0;

	for (const auto n : array)
	{
		BOOST_TEST(n == expected);
		++expected;
	}

	BOOST_TEST(expected == 5);
}

BOOST_AUTO_TEST_SUITE_END()	// SortedArrayTests


BOOST_AUTO_TEST_SUITE_END()	// ArraySuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
