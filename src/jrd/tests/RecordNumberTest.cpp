#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/RecordNumber.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(RecordNumberSuite)


BOOST_AUTO_TEST_SUITE(RecordNumberTests)

BOOST_AUTO_TEST_CASE(IsValildTest)
{
	RecordNumber invalidRecNum1;
	BOOST_TEST(!invalidRecNum1.isValid());

	RecordNumber invalidRecNum2(invalidRecNum1);
	BOOST_TEST(!invalidRecNum2.isValid());

	RecordNumber validRecNum1(1);
	BOOST_TEST(validRecNum1.isValid());

	RecordNumber validRecNum2(validRecNum1);
	BOOST_TEST(validRecNum2.isValid());
}

BOOST_AUTO_TEST_SUITE_END()	// RecordNumberTests


BOOST_AUTO_TEST_SUITE_END()	// RecordNumberSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
