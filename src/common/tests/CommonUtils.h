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

	// Wrapper to pass const char array as template argument
	template<std::size_t N>
	struct ConstexprString
	{
		char value[N];

		constexpr ConstexprString(const char (&str)[N])
		{
			std::copy_n(str, N, value);
		}

		constexpr operator std::string_view() const
		{
			return std::string_view(value, N - 1); // Exclude '\0'
		}
	};

	inline bool checkErrorMessage(const Firebird::status_exception& ex, const std::string_view expected)
	{
		const auto message = getErrorMessage(ex);
		BOOST_TEST_INFO(std::string("Expected exception: ") + expected.data());
		BOOST_TEST_INFO("Caught exception:   " + message); // Space for alignment
		return message == expected;
	}

	template<ConstexprString Expecter>
	inline bool checkErrorMessage(const Firebird::status_exception& ex)
	{
		return checkErrorMessage(ex, static_cast<std::string_view>(Expecter));
	}

}

#endif
