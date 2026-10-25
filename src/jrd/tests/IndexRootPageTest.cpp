#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/ods.h"

using namespace Firebird;
using namespace Jrd;
using namespace Ods;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(IndexRootPageSuite)


BOOST_AUTO_TEST_SUITE(BackSlotTests)

// Index root page image with room for 16 slots
union RootPageImage
{
	index_root_page page;
	UCHAR bytes[sizeof(index_root_page) + 15 * sizeof(index_root_page::irt_repeat)];
};

// Zero-filled image, so every test case starts from a clean page
static index_root_page* makeRootPage(unsigned count)
{
	static RootPageImage image;

	memset(&image, 0, sizeof(image));
	image.page.irt_relation = 1;
	image.page.irt_count = count;
	return &image.page;
}

BOOST_AUTO_TEST_CASE(BackSlotInRange)
{
	auto root = makeRootPage(4);

	root->irt_rpt[1].irt_backslot = 2;
	BOOST_TEST(root->getBackSlot(&root->irt_rpt[1]) == &root->irt_rpt[2]);

	root->irt_rpt[1].irt_backslot = 3;		// last valid slot
	BOOST_TEST(root->getBackSlot(&root->irt_rpt[1]) == &root->irt_rpt[3]);
}

BOOST_AUTO_TEST_CASE(BackSlotOutOfRange)
{
	auto root = makeRootPage(4);

	root->irt_rpt[0].irt_backslot = 4;		// == irt_count
	BOOST_TEST(root->getBackSlot(&root->irt_rpt[0]) == nullptr);

	root->irt_rpt[0].irt_backslot = 0xFFFF;
	BOOST_TEST(root->getBackSlot(&root->irt_rpt[0]) == nullptr);

	root->irt_count = 0;		// no slot may resolve anymore
	BOOST_TEST(root->getBackSlot(&root->irt_rpt[0]) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()	// BackSlotTests


BOOST_AUTO_TEST_SUITE_END()	// IndexRootPageSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
