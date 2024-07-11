#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/sqz.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(CompressorSuite)


BOOST_AUTO_TEST_SUITE(CompressorTests)

BOOST_AUTO_TEST_CASE(PackAndUnpackTest)
{
	auto& pool = *getDefaultMemoryPool();

	const UCHAR data[] = "111111111123456777777";
	const auto dataLength = sizeof(data) - 1;
	const Compressor dcc(pool, false, false, dataLength, data);

	const auto packedLength = dcc.getPackedLength();
	Array<UCHAR> packBuffer;
	dcc.pack(data, packBuffer.getBuffer(packedLength, false));

	Array<UCHAR> unpackBuffer;
	unpackBuffer.getBuffer(Compressor::getUnpackedLength(packBuffer.getCount(), packBuffer.begin()), false);
	BOOST_TEST(unpackBuffer.getCount() == dataLength);

	BOOST_TEST(dcc.unpack(packBuffer.getCount(), packBuffer.begin(),
		unpackBuffer.getCount(), unpackBuffer.begin()) == unpackBuffer.end());

	BOOST_TEST(memcmp(data, unpackBuffer.begin(), dataLength) == 0);
}

BOOST_AUTO_TEST_SUITE_END()	// CompressorTests


BOOST_AUTO_TEST_SUITE_END()	// CompressorSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
