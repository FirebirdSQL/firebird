#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/dsc.h"
#include "../jrd/align.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(CommonTests)
BOOST_AUTO_TEST_SUITE(DscSuite)


BOOST_AUTO_TEST_SUITE(TrivialDscTests)

BOOST_AUTO_TEST_CASE(dsc_should_contain_same_values_after_copy_assignment)
{
	dsc toDsc;
	dsc fromDsc;

	// fromDsc should be prepared
	fromDsc.dsc_dtype = dtype_long;
	fromDsc.dsc_scale = 0;
	fromDsc.dsc_length = 4;
	fromDsc.dsc_sub_type = 0;
	fromDsc.dsc_flags = DSC_nullable;
	fromDsc.dsc_address = reinterpret_cast<UCHAR*>(0x04);
	fromDsc.dsc_sub_first = nullptr;
	fromDsc.dsc_next = nullptr;
	fromDsc.dsc_sub_count = 0;

	toDsc = fromDsc;

	BOOST_TEST(toDsc.dsc_dtype == fromDsc.dsc_dtype);
	BOOST_TEST(toDsc.dsc_scale == fromDsc.dsc_scale);
	BOOST_TEST(toDsc.dsc_length == fromDsc.dsc_length);
	BOOST_TEST(toDsc.dsc_sub_type == fromDsc.dsc_sub_type);
	BOOST_TEST(toDsc.dsc_flags == fromDsc.dsc_flags);
	BOOST_TEST(toDsc.dsc_address == fromDsc.dsc_address);
	BOOST_TEST(toDsc.dsc_sub_first == fromDsc.dsc_sub_first);
	BOOST_TEST(toDsc.dsc_next == fromDsc.dsc_next);
	BOOST_TEST(toDsc.dsc_sub_count == fromDsc.dsc_sub_count);
}

BOOST_AUTO_TEST_SUITE_END()	// TrivialDscTests


BOOST_AUTO_TEST_SUITE(CompositeDscTests)

BOOST_AUTO_TEST_CASE(dsc_should_create_deep_copy_on_assignment)
{
	dsc toDsc;
	dsc superFromDsc;
	auto subFromDsc1 = FB_NEW dsc;
	auto subFromDsc2 = FB_NEW dsc;

	// superFromDsc should be prepared
	superFromDsc.dsc_dtype = dtype_rowtype;
	superFromDsc.dsc_scale = 0;
	superFromDsc.dsc_length = 8;
	superFromDsc.dsc_sub_type = 0;
	superFromDsc.dsc_flags = DSC_nullable;
	superFromDsc.dsc_address = reinterpret_cast<UCHAR*>(0x04);
	superFromDsc.dsc_sub_first = subFromDsc1;
	superFromDsc.dsc_next = nullptr;
	superFromDsc.dsc_sub_count = 2;

	// subFromDsc1 should be prepared
	subFromDsc1->dsc_dtype = dtype_long;
	subFromDsc1->dsc_scale = 0;
	subFromDsc1->dsc_length = 4;
	subFromDsc1->dsc_sub_type = 0;
	subFromDsc1->dsc_flags = DSC_nullable;
	subFromDsc1->dsc_address = reinterpret_cast<UCHAR*>(0x04);
	subFromDsc1->dsc_sub_first = nullptr;
	subFromDsc1->dsc_next = subFromDsc2;
	subFromDsc1->dsc_sub_count = 0;

	// subFromDsc2 should be prepared
	subFromDsc2->dsc_dtype = dtype_long;
	subFromDsc2->dsc_scale = 0;
	subFromDsc2->dsc_length = 4;
	subFromDsc2->dsc_sub_type = 0;
	subFromDsc2->dsc_flags = DSC_nullable;
	subFromDsc2->dsc_address = reinterpret_cast<UCHAR*>(0x08);
	subFromDsc2->dsc_sub_first = nullptr;
	subFromDsc2->dsc_next = nullptr;
	subFromDsc2->dsc_sub_count = 0;

	toDsc = superFromDsc;

	BOOST_TEST(toDsc.dsc_dtype == superFromDsc.dsc_dtype);
	BOOST_TEST(toDsc.dsc_scale == superFromDsc.dsc_scale);
	BOOST_TEST(toDsc.dsc_length == superFromDsc.dsc_length);
	BOOST_TEST(toDsc.dsc_sub_type == superFromDsc.dsc_sub_type);
	BOOST_TEST(toDsc.dsc_flags == superFromDsc.dsc_flags);
	BOOST_TEST(toDsc.dsc_address == superFromDsc.dsc_address);
	BOOST_TEST(toDsc.dsc_sub_count == superFromDsc.dsc_sub_count);

	BOOST_TEST(toDsc.dsc_sub_first->dsc_dtype == subFromDsc1->dsc_dtype);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_scale == subFromDsc1->dsc_scale);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_length == subFromDsc1->dsc_length);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_sub_type == subFromDsc1->dsc_sub_type);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_flags == subFromDsc1->dsc_flags);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_address == subFromDsc1->dsc_address);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_sub_count == subFromDsc1->dsc_sub_count);

	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_dtype == subFromDsc2->dsc_dtype);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_scale == subFromDsc2->dsc_scale);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_length == subFromDsc2->dsc_length);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_sub_type == subFromDsc2->dsc_sub_type);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_flags == subFromDsc2->dsc_flags);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_address == subFromDsc2->dsc_address);
	BOOST_TEST(toDsc.dsc_sub_first->dsc_next->dsc_sub_count == subFromDsc2->dsc_sub_count);
}

BOOST_AUTO_TEST_SUITE_END()	// CompositeDscTests


BOOST_AUTO_TEST_SUITE_END()	// DscSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonTests
