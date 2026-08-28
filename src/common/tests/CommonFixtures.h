#ifndef COMMON_FIXTURES
#define COMMON_FIXTURES
#include "boost/test/unit_test.hpp"

#include "CommonUtils.h"

#include <filesystem>

namespace TestsUtils
{

namespace fs = std::filesystem;

struct TempPathFixture
{
	fs::path tempPathFX;

	TempPathFixture()
	{
		auto tempDir = fs::temp_directory_path();
		// Resolve symlink (/var on macos)
		tempDir = fs::canonical(tempDir);

		tempPathFX = tempDir / (generateRandomString(10) + "_common_test.tmp");
	}

	~TempPathFixture()
	{
		if (fs::exists(tempPathFX))
			fs::remove(tempPathFX);
	}
};

}

#endif
