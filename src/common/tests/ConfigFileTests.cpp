#include "boost/test/unit_test.hpp"
#include "../common/config/config_file.h"

#include "CommonFixtures.h"
#include "CommonUtils.h"

#include <fstream>

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonClassesSuite)
BOOST_AUTO_TEST_SUITE(ConfigFileTests)


BOOST_AUTO_TEST_CASE(InvalidIncludeInTextStreamBug)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	const std::string_view text = R"(
database
{
enabled = true
}
include /a/b/c/d/f.d
	)";


	// Should be an exception, not a segfault
	BOOST_CHECK_EXCEPTION(ConfigFile file({}, text.data(), 0), Firebird::status_exception,
	TestsUtils::checkErrorMessage<"Invalid include operator in Passed text for </a/b/c/d/f.d> File to include not found">);
}

BOOST_FIXTURE_TEST_CASE(RecursiveInclude, TestsUtils::TempPathFixture)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	const auto pathStr = tempPathFX.string();

	std::string text = R"(
database
{
enabled = true
}
include )";

	text += pathStr;

	std::ofstream out(pathStr);
	out << "include " + pathStr;
	out.close();

	Firebird::string error;
	error.printf("Invalid include operator in %s for <%s> Include depth too big", pathStr.data(), pathStr.data());

	// Should be an exception, not a segfault
	BOOST_CHECK_EXCEPTION(ConfigFile file({}, text.data(), 0), Firebird::status_exception,
	[&error](const Firebird::status_exception& ex)
	{
		return TestsUtils::checkErrorMessage(ex, error.data());
	});
}


BOOST_AUTO_TEST_SUITE_END() // AutoPtrFunctionalTests
BOOST_AUTO_TEST_SUITE_END()	// CommonClassesSuite
