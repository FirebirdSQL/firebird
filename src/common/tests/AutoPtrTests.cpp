#include "boost/test/unit_test.hpp"
#include "../common/classes/auto.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonClassesSuite)
BOOST_AUTO_TEST_SUITE(AutoPtrFunctionalTests)

static int aliveCounter = 0;

class TestRefCounter
{
public:
	TestRefCounter()
	{
		++aliveCounter;
	}
	~TestRefCounter()
	{
		--aliveCounter;
	}
};

AutoPtr<TestRefCounter> makePtr()
{
	return AutoPtr<TestRefCounter>(new TestRefCounter());
}

BOOST_AUTO_TEST_CASE(StringAssignmentTest)
{
	AutoPtr<TestRefCounter> ptr;

	ptr = makePtr();

	ptr = makePtr();

	ptr.reset();

	BOOST_TEST(aliveCounter == 0);
}


BOOST_AUTO_TEST_SUITE_END() // AutoPtrFunctionalTests
BOOST_AUTO_TEST_SUITE_END()	// CommonClassesSuite
