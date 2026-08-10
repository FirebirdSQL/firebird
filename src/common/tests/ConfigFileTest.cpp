#include "boost/test/unit_test.hpp"
#include "../common/config/config_file.h"

#include "CommonFixtures.h"

#include <fstream>

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonClassesSuite)
BOOST_AUTO_TEST_SUITE(ConfigFileTests)


BOOST_FIXTURE_TEST_CASE(IncludeInUserSessionBug, TestsUtils::TempPathFixture)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	std::string text = "(\nservices\n{\ninclude ";
	text += tempPathFX.string();
	text += "\n}";

	std::ofstream confidentialFile(tempPathFX);
	confidentialFile << "Secret data";
	confidentialFile.close();

	BOOST_CHECK_NO_THROW(ConfigFile file({}, text.data(), 0)); // Allow include, no exception
	BOOST_CHECK_THROW(ConfigFile file({}, text.data(), ConfigFile::DENY_INCLUDE), Firebird::Exception);
}


BOOST_AUTO_TEST_SUITE_END() // AutoPtrFunctionalTests
BOOST_AUTO_TEST_SUITE_END()	// CommonClassesSuite
