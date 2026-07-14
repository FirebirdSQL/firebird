#include "boost/test/unit_test.hpp"
#include "../../jrd/blb.h"

#include "TestContext.h"

#include <utility>


BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(JrdClassesSuite)


BOOST_AUTO_TEST_SUITE(BlobTests)


static constexpr UCHAR STREAM_BLOB_BPB[] = {
	isc_bpb_version1,
	isc_bpb_type, 1, isc_bpb_type_stream,
};

std::string getDefaultString(std::string_view header = "", int pageNumber = 0, std::optional<char> dum = std::nullopt)
{
	std::string output;
	output += header;

	// Bigger page size - level 1
	// Bigger 2 pages - level 2
	auto tdbb = JRD_get_thread_data();
	auto size = tdbb->getDatabase()->dbb_page_size;

	for (int i = 0 ; i < pageNumber; i++)
	{
		std::string dummy;
		dummy.resize(size, dum.value_or('0' + i));
		output += dummy;
	}

	return output;
}

Jrd::blb* makeBlob(Jrd::bid& id, std::string_view testData = "")
{
	auto tdbb = JRD_get_thread_data();

	Jrd::blb* blob = Jrd::blb::create2(tdbb, tdbb->getTransaction(), &id, sizeof(STREAM_BLOB_BPB), STREAM_BLOB_BPB);
	BOOST_REQUIRE(blob != nullptr);

	blob->BLB_put_data(tdbb, (const UCHAR*)testData.data(), testData.length());

	return blob;
}


std::string readBlob(Jrd::bid id)
{
	auto tdbb = JRD_get_thread_data();

	auto blob = Jrd::blb::open(tdbb, tdbb->getTransaction(), &id);

	std::string buffer;
	buffer.resize(blob->blb_length, '\0');
	const ULONG readLength = blob->BLB_get_data(JRD_get_thread_data(), (UCHAR*)buffer.data(), blob->blb_length, true);

	return buffer;
}

BOOST_AUTO_TEST_SUITE(BlobModificationTests)

enum class ModifyFunction
{
	WRITE,
	PUT_DATA,
	PUT_SEGMENT,
};

void replaceInBlob(Jrd::thread_db* tdbb, Jrd::blb*& blob, const ULONG pos, const std::string_view replacement,
	const ModifyFunction modifyFunction)
{
	switch (modifyFunction)
	{
		case ModifyFunction::WRITE:
			blob->write(tdbb, pos, replacement.data(), replacement.length());
			break;
		case ModifyFunction::PUT_SEGMENT:
			if (replacement.length() < MAX_USHORT)
			{
				blob->BLB_lseek(0, pos);
				blob->BLB_put_segment(tdbb, (const UCHAR*)replacement.data(), replacement.length());
				break;
			}
			[[fallthrough]];
		case ModifyFunction::PUT_DATA:
			blob->BLB_lseek(0, pos);
			blob->BLB_put_data(tdbb, (const UCHAR*)replacement.data(), replacement.length());
			break;
	}

	blob->BLB_close(tdbb);
	blob = nullptr;
}

std::string replaceInContent(std::string sourceData, const ULONG posToInplace, const std::string_view contentToInplace)
{
	auto sourceLength = sourceData.length();

	const auto replacementEnd = posToInplace + contentToInplace.length();
	if (replacementEnd < sourceLength)
	{
		for (ULONG i = 0; i < contentToInplace.length(); ++i)
		{
			sourceData[posToInplace + i] = contentToInplace[i];
		}
	}
	else
	{
		sourceData.resize(posToInplace);
		sourceData += contentToInplace;
	}
	return sourceData;
}

static constexpr std::array functions = {ModifyFunction::PUT_DATA, ModifyFunction::PUT_SEGMENT, ModifyFunction::WRITE};
static void putFunctionInfo(const ModifyFunction func)
{
	switch (func)
	{
		case ModifyFunction::WRITE:
			BOOST_TEST_INFO("Modify function WRITE");
			break;
		case ModifyFunction::PUT_SEGMENT:
			BOOST_TEST_INFO("Modify function PUT_SEGMENT");
			break;
		case ModifyFunction::PUT_DATA:
			BOOST_TEST_INFO("Modify function PUT_DATA");
			break;
	}
}

BOOST_FIXTURE_TEST_CASE(Level0WriteTest, EngineHolder)
{
	Jrd::bid id;

	for (auto type : functions)
	{
		const std::string testData = "Hello World, BLB_get_data, level=0";

		// Full rewrite
		auto blob = makeBlob(id, testData);
		std::string buffer;
		buffer.resize(blob->blb_length, '*');
		replaceInBlob(tdbb, blob, 0, buffer, type);
		BOOST_TEST(readBlob(id) == buffer);

		// Middle write
		blob = makeBlob(id, testData);
		replaceInBlob(tdbb, blob, 12, " __write_,", type);
		BOOST_TEST(readBlob(id) == replaceInContent(testData, 12, " __write_,"));

		// Ending is out of range - add to end
		blob = makeBlob(id, testData);
		replaceInBlob(tdbb, blob, 27, testData, type);
		BOOST_TEST(readBlob(id) == replaceInContent(testData, 27, testData));

		// Beginning is out of range
		blob = makeBlob(id, testData);
		BOOST_CHECK_THROW(blob->write(tdbb, 40, (const void*)testData.data(), testData.length()), Firebird::Exception);
	}
}

// Run log test manual: gen/Debug/firebird/tests/libEngine14_test --run_test=EngineSuite/JrdClassesSuite/BlobTests/BlobModificationTests
BOOST_FIXTURE_TEST_CASE(Level1WriteTest, EngineHolder, * boost::unit_test::disabled())
{
	Jrd::bid id;
	Jrd::blb* blob = nullptr;

	std::string result;
	std::string expected;

	const std::string_view testData = "Hello World, BLB_get_data, level=1 | ";
	std::string defaultData = getDefaultString(testData, 1);
	for (auto type : functions)
	{
		putFunctionInfo(type);

		// Full rewrite
		auto blob = makeBlob(id, defaultData);

		replaceInBlob(tdbb, blob, 0, "new data", type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, 0, "new data");
		BOOST_TEST(result == expected);
	}

	std::string replacement;
	replacement.resize(200, '*');

	for (auto type : functions)
	{
		// Middle to end write
		blob = makeBlob(id, defaultData);
		replaceInBlob(tdbb, blob, blob->blb_length - 200, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, defaultData.length() - 200, replacement);
		BOOST_TEST(result.length() == expected.length());
		BOOST_REQUIRE(result.substr(0, 400) == expected.substr(0, 400));
		BOOST_REQUIRE(result.substr(result.length() - 300) == expected.substr(expected.length() - 300));
		BOOST_TEST(result == expected);
	}

	for (auto type : functions)
	{
		putFunctionInfo(type);

		// Middle
		replacement.resize(200, '*');
		blob = makeBlob(id, defaultData);
		replaceInBlob(tdbb, blob, blob->blb_length - 4000, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, defaultData.length() - 4000, replacement);
		BOOST_REQUIRE(result.length() == expected.length());
		BOOST_REQUIRE(result.substr(result.length() - 4000, 300) == expected.substr(expected.length() - 4000, 300));
		BOOST_TEST(result == expected);
	}

	replacement.clear();
	for (auto type : functions)
	{
		putFunctionInfo(type);

		// Ending is out of range - add to end
		blob = makeBlob(id, defaultData);
		replacement.resize(blob->blb_length, '@');

		const auto insertPos = blob->blb_length - 1000;
		replaceInBlob(tdbb, blob, insertPos, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, insertPos, replacement);
		BOOST_REQUIRE(result.length() == expected.length());
		BOOST_REQUIRE(result.substr(0, 400) == expected.substr(0, 400));
		BOOST_REQUIRE(result.substr(result.length() - 300) == expected.substr(expected.length() - 300));
		BOOST_TEST(result == expected);
	}

	defaultData = getDefaultString(testData, 8);
	replacement = getDefaultString(testData, 3, '*');
	for (auto type : functions)
	{
		putFunctionInfo(type);

		// Big
		blob = makeBlob(id, defaultData);

		const auto insertPos = blob->blb_length / 2;
		replaceInBlob(tdbb, blob, insertPos, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, insertPos, replacement);
		BOOST_REQUIRE(result.length() == expected.length());

		std::string_view resultView(result);
		std::string_view expectedView(expected);
		for (FB_SIZE_T i = 0; i < expected.length(); i += 1000)
		{
			auto left = std::min<int>(1000, expected.length() - i);
			BOOST_TEST(resultView.substr(i, left) == expected.substr(i, left));
		}
	}
}

// Run log test manual: gen/Debug/firebird/tests/libEngine14_test --run_test=EngineSuite/JrdClassesSuite/BlobTests/BlobModificationTests
BOOST_FIXTURE_TEST_CASE(Level2WriteTest, EngineHolder, * boost::unit_test::disabled())
{
	// Takes some time

	Jrd::bid id;
	Jrd::blb* blob = nullptr;

	std::string result;
	std::string expected;

	const std::string_view testData = "Hello World, BLB_get_data, level=2 | ";
	std::string defaultData;
	std::string replacement;

	for (auto type : functions)
	{
		putFunctionInfo(type);

		blob = makeBlob(id, defaultData);

		const auto insertPos = blob->blb_length / 2;
		replaceInBlob(tdbb, blob, insertPos, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, insertPos, replacement);
		BOOST_REQUIRE(result.length() == expected.length());

		std::string_view resultView(result);
		std::string_view expectedView(expected);
		for (FB_SIZE_T i = 0; i < expected.length(); i += 1000)
		{
			auto left = std::min<int>(1000, expected.length() - i);
			BOOST_TEST_INFO("Chunk position is " + std::to_string(i));
			BOOST_TEST(resultView.substr(i, left) == expected.substr(i, left));
			// if (resultView.substr(i, left) != expected.substr(i, left))
			// 	break;
		}
	}

	defaultData = getDefaultString(testData, 5050);
	replacement = getDefaultString(testData, 150, '*');
	for (auto type : functions)
	{
		putFunctionInfo(type);

		blob = makeBlob(id, defaultData);

		const auto insertPos = 1998;
		replaceInBlob(tdbb, blob, insertPos, replacement, type);

		result = readBlob(id);
		expected = replaceInContent(defaultData, insertPos, replacement);
		BOOST_REQUIRE(result.length() == expected.length());

		std::string_view resultView(result);
		std::string_view expectedView(expected);
		for (FB_SIZE_T i = 0; i < expected.length(); i += 1000)
		{
			auto left = std::min<int>(1000, expected.length() - i);
			BOOST_TEST_INFO("Chunk position is " + std::to_string(i));
			BOOST_TEST(resultView.substr(i, left) == expected.substr(i, left));
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()	// BlobModificationTest

BOOST_AUTO_TEST_SUITE(BlobModificationTests)


BOOST_FIXTURE_TEST_CASE(Level0ReadTest, EngineHolder)
{
	Jrd::bid id;

	const std::string_view testData = "Hello World, read, level=0";
	auto blob = makeBlob(id, testData);
	blob->BLB_close(tdbb);
	blob = Jrd::blb::open(tdbb, tdbb->getTransaction(), &id);

	std::string buffer;
	buffer.resize(blob->blb_length, '\0');
	blob->read(tdbb, 0, buffer.data(), buffer.length());
	BOOST_TEST(buffer == testData);

	auto read = blob->read(tdbb, 5, buffer.data(), buffer.length());
	BOOST_TEST(read == buffer.length() - 5);

	BOOST_TEST(buffer.substr(0,  read) == testData.substr(5));

	blob->BLB_close(tdbb);
}

BOOST_FIXTURE_TEST_CASE(Level1ReadTest, EngineHolder)
{
	Jrd::bid id;

	std::string result;
	std::string expected;

	const std::string_view testData = "Hello World, read, level=1 | ";
	std::string defaultData = getDefaultString(testData, 8);

	auto blob = makeBlob(id, defaultData);
	blob->BLB_close(tdbb);
	blob = Jrd::blb::open(tdbb, tdbb->getTransaction(), &id);

	std::string buffer;
	buffer.resize(blob->blb_length, '\0');
	auto read = blob->read(tdbb, blob->blb_length / 2, buffer.data(), buffer.length());
	BOOST_TEST(buffer.substr(0, read) == defaultData.substr(blob->blb_length / 2, read));

	read = blob->read(tdbb, 0, buffer.data(), buffer.length());
	BOOST_TEST(read == buffer.length());
	BOOST_TEST(buffer == defaultData);

	blob->BLB_close(tdbb);
}

BOOST_AUTO_TEST_SUITE_END()	// BlobModificationTests

BOOST_AUTO_TEST_SUITE_END()	// BlobTests

BOOST_AUTO_TEST_SUITE_END()	// JrdClassesSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
