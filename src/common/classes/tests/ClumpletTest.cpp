#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/ClumpletReader.h"
#include "../common/classes/ClumpletWriter.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(ClumpletSuite)


BOOST_AUTO_TEST_CASE(ClumpletWriteAndReadInfoResponseTest)
{
	ClumpletWriter clumplet(ClumpletWriter::Kind::InfoResponse, MAX_DPB_SIZE);
	clumplet.insertInt(isc_info_sql_stmt_type, isc_info_sql_stmt_select);
	clumplet.insertInt(isc_info_sql_batch_fetch, 0);
	clumplet.insertTag(isc_info_end);

	clumplet.rewind();
	BOOST_TEST(!clumplet.isEof());
	BOOST_TEST(clumplet.getClumpTag() == isc_info_sql_stmt_type);
	BOOST_TEST(clumplet.getInt() == isc_info_sql_stmt_select);

	clumplet.moveNext();
	BOOST_TEST(!clumplet.isEof());
	BOOST_TEST(clumplet.getClumpTag() == isc_info_sql_batch_fetch);
	BOOST_TEST(clumplet.getInt() == 0);

	clumplet.moveNext();
	BOOST_TEST(!clumplet.isEof());
	BOOST_TEST(clumplet.getClumpTag() == isc_info_end);

	clumplet.moveNext();
	BOOST_TEST(clumplet.isEof());
}


BOOST_AUTO_TEST_CASE(ClumpletWriteAndSkipInfoResponseTest)
{
	ClumpletWriter clumplet(ClumpletWriter::Kind::InfoResponse, MAX_DPB_SIZE);
	clumplet.insertInt(isc_info_sql_stmt_type, isc_info_sql_stmt_select);
	clumplet.insertInt(isc_info_sql_batch_fetch, 0);
	clumplet.insertTag(isc_info_end);

	clumplet.rewind();
	BOOST_TEST(!clumplet.isEof());

	clumplet.moveNext();
	BOOST_TEST(!clumplet.isEof());

	clumplet.moveNext();
	BOOST_TEST(!clumplet.isEof());

	clumplet.moveNext();
	BOOST_TEST(clumplet.isEof());
}


BOOST_AUTO_TEST_SUITE_END()	// ClumpletSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
