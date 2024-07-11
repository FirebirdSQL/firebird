#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/Aligner.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(AlignerSuite)


BOOST_AUTO_TEST_CASE(AlignerTest)
{
	UCHAR buffer[sizeof(int)] = {1, 2, 3, 4};
	Aligner<int> aligner(buffer, sizeof(buffer));
	auto ptr = (const UCHAR*)(const int*) aligner;

#ifdef RISC_ALIGNMENT
	BOOST_TEST(((uintptr_t) ptr) % sizeof(int) == 0);
#endif

	BOOST_TEST(ptr[0] == buffer[0]);
	BOOST_TEST(ptr[1] == buffer[1]);
	BOOST_TEST(ptr[2] == buffer[2]);
	BOOST_TEST(ptr[3] == buffer[3]);
}


BOOST_AUTO_TEST_CASE(OutAlignerTest)
{
	UCHAR buffer[sizeof(int)] = {0};

	{	// scope
		OutAligner<int> aligner(buffer, sizeof(buffer));
		auto ptr = (UCHAR*)(int*) aligner;

#ifdef RISC_ALIGNMENT
		BOOST_TEST(((uintptr_t) ptr) % sizeof(int) == 0);
#endif

		ptr[0] = 1;
		ptr[1] = 2;
		ptr[2] = 3;
		ptr[3] = 4;
	}

	BOOST_TEST(buffer[0] == 1);
	BOOST_TEST(buffer[1] == 2);
	BOOST_TEST(buffer[2] == 3);
	BOOST_TEST(buffer[3] == 4);
}


BOOST_AUTO_TEST_CASE(BiAlignerTest)
{
	UCHAR buffer[sizeof(int)] = {1, 2, 3, 4};

	{	// scope
		BiAligner<int> aligner(buffer, sizeof(buffer));
		auto ptr = (UCHAR*)(int*) aligner;

#ifdef RISC_ALIGNMENT
		BOOST_TEST(((uintptr_t) ptr) % sizeof(int) == 0);
#endif

		BOOST_TEST(ptr[0] == buffer[0]);
		BOOST_TEST(ptr[1] == buffer[1]);
		BOOST_TEST(ptr[2] == buffer[2]);
		BOOST_TEST(ptr[3] == buffer[3]);

		++ptr[0];
		++ptr[1];
		++ptr[2];
		++ptr[3];
	}

	BOOST_TEST(buffer[0] == 2);
	BOOST_TEST(buffer[1] == 3);
	BOOST_TEST(buffer[2] == 4);
	BOOST_TEST(buffer[3] == 5);
}


BOOST_AUTO_TEST_SUITE_END()	// AlignerSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
