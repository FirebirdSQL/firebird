#ifndef TEST_COMMON_UTILS
#define TEST_COMMON_UTILS

#include "firebird.h"
#include "fb_exception.h"

#include "boost/test/unit_test.hpp"

#include <string>
#include <string_view>
#include <random>

namespace TestsUtils
{
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

	// Use std::string because it works better with BOOST_TEST
	inline std::string getErrorMessage(const Firebird::status_exception& ex)
	{
		const ISC_STATUS* status = ex.value();

		std::string buffer;
		TEXT temp[BUFFER_LARGE];
		while (fb_interpret(temp, sizeof(temp), &status))
		{
			buffer += temp;
			buffer += " ";
		}

		if (!buffer.empty())
			buffer.resize(buffer.length() - 1);

		return buffer;
	}

	inline bool checkErrorMessage(const Firebird::status_exception& ex, const std::string_view expected)
	{
		const auto message = getErrorMessage(ex);
		BOOST_TEST_INFO(std::string("Expected exception: ") + expected.data());
		BOOST_TEST_INFO("Caught exception:   " + message); // Space for alignment
		return message == expected;
	}
}

#endif
