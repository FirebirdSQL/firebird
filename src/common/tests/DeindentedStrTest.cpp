// FIXME: Review
#include "boost/test/unit_test.hpp"
#include "../common/DeindentedStr.h"

#include <string_view>

using namespace Firebird;


namespace
{
	template <unsigned N>
	constexpr bool strEqual(const DeindentedStr<N>& actual, std::string_view expected)
	{
		auto p = 0u;

		while (p < expected.length())
		{
			if (actual.text[p] != expected[p])
				return false;

			++p;
		}

		return actual.text[p] == '\0';
	}

	template <unsigned N>
	void checkDeindented(const DeindentedStr<N>& actual, std::string_view expected)
	{
		BOOST_TEST(std::string_view(actual.c_str()) == expected);
	}
}


BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(DeindentedStrSuite)


BOOST_AUTO_TEST_CASE(DeindentsMultilineString)
{
	static constexpr auto text = deindentStr(R"""(
		select
		    *
		  from rdb$database
	)""");

	static_assert(strEqual(text, "select\n    *\n  from rdb$database\n"));
	checkDeindented(text, "select\n    *\n  from rdb$database\n");
}

BOOST_AUTO_TEST_CASE(PreservesRelativeIndent)
{
	static constexpr auto text = deindentStr(R"""(
		    begin
		        if (x) then
		            y = 1;
		    end
	)""");

	static_assert(strEqual(text, "begin\n    if (x) then\n        y = 1;\nend\n"));
	checkDeindented(text, "begin\n    if (x) then\n        y = 1;\nend\n");
}

BOOST_AUTO_TEST_CASE(HandlesTabsInCommonIndent)
{
	static constexpr auto text = deindentStr("\n\t\talpha\n\t\t\tbeta\n\t\tgamma\n");

	static_assert(strEqual(text, "alpha\n\tbeta\ngamma\n"));
	checkDeindented(text, "alpha\n\tbeta\ngamma\n");
}

BOOST_AUTO_TEST_CASE(HandlesCrLfLineBreaks)
{
	static constexpr auto text = deindentStr("\r\n\tfirst\r\n\t\tsecond\r\n\tthird\r\n");

	static_assert(strEqual(text, "first\r\n\tsecond\r\nthird\r\n"));
	checkDeindented(text, "first\r\n\tsecond\r\nthird\r\n");
}

BOOST_AUTO_TEST_CASE(KeepsPartiallyIndentedLines)
{
	static constexpr auto text = deindentStr("\n    first\n  second\n    third\n");

	static_assert(strEqual(text, "first\nsecond\nthird\n"));
	checkDeindented(text, "first\nsecond\nthird\n");
}

BOOST_AUTO_TEST_CASE(TrimsLeadingAndTrailingBlankLines)
{
	static constexpr auto text = deindentStr("\n\t  \n    first\n\n    second\n   \t\n");

	static_assert(strEqual(text, "first\n\nsecond\n"));
	checkDeindented(text, "first\n\nsecond\n");
}

BOOST_AUTO_TEST_CASE(HandlesSingleLineString)
{
	static constexpr auto text = deindentStr("single line");

	static_assert(strEqual(text, "single line"));
	checkDeindented(text, "single line");
}

BOOST_AUTO_TEST_CASE(HandlesEmptyAndBlankStrings)
{
	static constexpr auto empty = deindentStr("");
	static constexpr auto blankLines = deindentStr("\n\t \n  \r\n");

	static_assert(strEqual(empty, ""));
	static_assert(strEqual(blankLines, ""));

	checkDeindented(empty, "");
	checkDeindented(blankLines, "");
}


BOOST_AUTO_TEST_SUITE_END()	// DeindentedStrSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
