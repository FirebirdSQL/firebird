#ifndef COMMON_FIXTURES
#define COMMON_FIXTURES
#include "boost/test/unit_test.hpp"

#include <filesystem>
#include <random>

namespace TestsUtils
{

namespace fs = std::filesystem;

inline std::string generateRandomString(std::size_t length)
{
	std::random_device rd;
	std::mt19937 generator(rd());

	std::uniform_int_distribution<> distribution(0, 9);

	std::string randomString;
	for (std::size_t i = 0; i < length; ++i)
	{
		randomString += '0' + distribution(generator);
	}

	return randomString;
}

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
