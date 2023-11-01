#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/vector.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(VectorSuite)


BOOST_AUTO_TEST_SUITE(VectorTests)

BOOST_AUTO_TEST_CASE(Tests)
{
	Vector<int, 100> v;

	for (int i = 0; i < 100; ++i)
		v.add(i);

	for (int i = 0; i < 50; ++i)
		v.remove(0);

	bool passed = true;

	for (int i = 50; i < 100; ++i)
	{
		if (v[i - 50] != i)
			passed = false;
	}

	BOOST_TEST(passed);
}

BOOST_AUTO_TEST_SUITE_END()	// VectorTests


BOOST_AUTO_TEST_SUITE(SortedVectorTests)

BOOST_AUTO_TEST_CASE(Tests)
{
	SortedVector<int, 100> v;

	for (int i = 0; i < 100; ++i)
		v.add(99 - i);

	for (int i = 0; i < 50; ++i)
		v.remove(0);

	bool passed = true;

	for (int i = 50; i < 100; ++i)
	{
		if (v[i - 50] != i)
			passed = false;
	}

	BOOST_TEST(passed);
}

BOOST_AUTO_TEST_SUITE_END()	// SortedVectorTests


BOOST_AUTO_TEST_SUITE_END()	// VectorSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
