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
			tempPathFX = fs::temp_directory_path() / (generateRandomString(10) + "_common_test.tmp");
		}

		~TempPathFixture()
		{
			if (fs::exists(tempPathFX))
				fs::remove(tempPathFX);
		}
	};

}

#endif
