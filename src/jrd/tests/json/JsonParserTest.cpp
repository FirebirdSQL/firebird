#include "boost/test/unit_test.hpp"
#include "../TestContext.h"

#include "../jrd/json/classes/JsonTypes.h"

#include <utility>

using namespace FBJSON;


BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(ParseTests)


BOOST_AUTO_TEST_SUITE(StringParseTests)

BOOST_AUTO_TEST_CASE(TestSimpleQuotedString)
{
	// Simple, unicode, unicode braek, slash, uni-uni, uni-break-uni
	TextPos dummy = 0;
	StringParser parser;
	parser.parseQuotedString({"Hello \\\" world\""}, dummy, true, true, nullptr);
	BOOST_TEST(parser.hasError() == false);

	dummy = 0;
	parser.parseQuotedString({"Hello \\\" world"}, dummy, true, false, nullptr);
	BOOST_TEST(parser.hasError() == false);

	SmallString output;

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\\" world"}, dummy, true, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello \\\" world");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\n world world"}, dummy, true, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello \\n world world");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\n world world"}, dummy, false, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello \n world world");

	dummy = 0;
	output.clear();

	static_assert(char(0x0020) == ' ', "Wrong test");
	parser.parseQuotedString({"Hello \\u0020 world world"}, dummy, false, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello   world world");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\u0020 world world"}, dummy, true, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello   world world");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\u0020"}, dummy, true, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello  ");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\ud83d\\ude80 world world"}, dummy, false, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello \\ud83d\\ude80 world world");

	dummy = 0;
	output.clear();
	parser.parseQuotedString({"Hello \\ud83d\\ude80 world\\\" world \\n world \\u0020je\\\""}, dummy, false, false, &output);
	BOOST_TEST(parser.hasError() == false);
	BOOST_TEST(output.data() == "Hello \\ud83d\\ude80 world\" world \n world  je\"");
}

SmallString removeSlashes(const SmallString& source)
{
	SmallString out = source;
	ULONG pos = 0;
	while (true)
	{
		pos = out.find("\\", pos);
		if (pos == SmallString::npos)
			break;

		auto secondPart = out.substr(pos + 1);
		out.resize(pos);
		out += secondPart;
	}

	return out;
}

void parse(const SmallString& source, ULONG posToInsert, std::string_view insertion, std::string_view replacement,
	SmallString& targetUnescaped, SmallString& targetEscaped, SmallString& outUnescaped, SmallString& outEscaped)
{
	targetEscaped = source;
	for (ULONG i = 0; i < insertion.size(); ++i)
		targetEscaped[posToInsert + i] = insertion[i];

	std::string_view testView(targetEscaped.data(), targetEscaped.length());

	// target = removeSlashes(source);
	targetUnescaped = targetEscaped.substr(0, posToInsert);
	targetUnescaped += replacement;
	targetUnescaped += targetEscaped.substr(posToInsert + insertion.length());


	TextPos dummy = 0;
	StringParser parser;

	outEscaped.clear();
	parser.parseQuotedString({testView}, dummy, true, false, &outEscaped);
	if (parser.hasError())
		outEscaped = "<error>";

	dummy = 0;
	outUnescaped.clear();
	parser.parseQuotedString({testView}, dummy, false, false, &outUnescaped);
	if (parser.hasError())
		outUnescaped = "<error>";
}

BOOST_AUTO_TEST_CASE(BigStringParse)
{
	ULONG size = 515;
	SmallString source;
	source.resize(size);
	for (ULONG i = 0; i < size; ++i)
	{
		source[i] = 'a' + (i % ('z' - 'a'));
	}

	SmallString targetUnescaped, targetEscaped, outUnescaped, outEscaped;

	parse(source, 256, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetEscaped.data());

	parse(source, 255, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetEscaped.data());

	parse(source, 254, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetEscaped.data());

	parse(source, 253, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetEscaped.data());


	// Simple unicode is always converted to ascii
	parse(source, 250, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 251, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 252, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 253, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 254, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 255, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 256, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 257, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());

	parse(source, 258, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(outUnescaped.ToStringView() == targetUnescaped.data());
	BOOST_TEST(outEscaped.ToStringView() == targetUnescaped.data());
}


BOOST_AUTO_TEST_CASE(TestUnsafeStringParse)
{
	SmallString output;

	StringParser::parseStringUnsafe({"Hello \\\" world"}, output);
	BOOST_TEST(output.ToStringView() == "Hello \\\" world");

	StringParser::parseStringUnsafe({"Hello \\n world world"}, output);
	BOOST_TEST(output.ToStringView() == "Hello \\n world world");

	static_assert(char(0x0020) == ' ', "Wrong test");
	StringParser::parseStringUnsafe({"Hello \\u0020 world world"}, output);
	BOOST_TEST(output.ToStringView() == "Hello   world world");

	StringParser::parseStringUnsafe({"Hello \\u0020 world world"}, output);
	BOOST_TEST(output.ToStringView() == "Hello   world world");

	StringParser::parseStringUnsafe({"Hello \\u0020"}, output);
	BOOST_TEST(output.ToStringView() == "Hello  ");

	StringParser::parseStringUnsafe({"Hello \\ud83d\\ude80 world world"}, output);
	BOOST_TEST(output.ToStringView() == "Hello \\ud83d\\ude80 world world");
}

BOOST_AUTO_TEST_CASE(TestUnsafeStringOverflowBug)
{
	SmallString source = "Hello \\u0020 \\n";
	source[source.length() - 3] = '\0'; // Before \\n
	std::string_view view(source.data());

	//Overflow check
	SmallString output;
	StringParser::parseStringUnsafe({view}, output);
	BOOST_TEST(output.data() == "Hello  ");
}

BOOST_AUTO_TEST_CASE(TwoEscapesAtChunkBorderTest)
{
	const ULONG tailLengths[] = {300, 12, 8, 4, 2, 1, 0};

	for (const ULONG tailLength : tailLengths)
	{
		for (ULONG secondEscapePos = 240; secondEscapePos < 258; ++secondEscapePos)
		{
			SmallString source = "ab\\n";
			source.append(secondEscapePos - source.length(), 'x');
			source += "\\u0041"; // 'A'
			source.append(tailLength, 'y');

			SmallString expected = "ab\n";
			expected.append(secondEscapePos - 4, 'x');
			expected += "A";
			expected.append(tailLength, 'y');

			TextPos pos = 0;
			SmallString output;
			StringParser parser;
			parser.parseQuotedString({source.ToStringView()}, pos, false, false, &output);

			BOOST_TEST_INFO("Position of the second escape: " << secondEscapePos << ", tail: " << tailLength);
			BOOST_TEST(!parser.hasError());
			BOOST_TEST_INFO("Position of the second escape: " << secondEscapePos << ", tail: " << tailLength);
			BOOST_TEST((output == expected));
			BOOST_TEST_INFO("Position of the second escape: " << secondEscapePos << ", tail: " << tailLength);
			BOOST_TEST(pos == TextPos(source.length()));
		}
	}
}
BOOST_AUTO_TEST_CASE(BackslashAtTheEndTest)
{
	const char buffer[] = "abc\\n";
	const std::string_view view(buffer, 4); // abc + backslash, 'n' is outside of the view

	TextPos pos = 0;
	SmallString output;
	StringParser parser;
	BOOST_CHECK_NO_THROW(parser.parseQuotedString({view}, pos, false, false, &output));
	BOOST_TEST(parser.hasError());
	BOOST_TEST(pos <= view.length());
}

BOOST_AUTO_TEST_CASE(NonAsciiByteTest)
{
	{
		TextPos pos = 0;
		SmallString output;
		StringParser parser;
		parser.parseQuotedString({"\\u00\xC3\xA9 tail"}, pos, false, false, &output);
		BOOST_TEST(parser.hasError());
	}

	{
		TextPos pos = 0;
		SmallString output;
		StringParser parser;
		parser.parseQuotedString({"\\\xC3\xA9 tail"}, pos, false, false, &output);
		BOOST_TEST(parser.hasError());
	}

}

BOOST_AUTO_TEST_SUITE_END()	// StringParseTests


BOOST_AUTO_TEST_SUITE(NumberParseTests)

struct NumberParseResult
{
	ParsedNumber number;
	bool hasError = false;
};

static NumberParseResult parseNumberStrict(const std::string_view text)
{
	TextPos pos = 0;
	StringParser parser;

	NumberParseResult result;
	result.number = parser.parseNumber({text}, pos, true);
	result.hasError = parser.hasError();
	return result;
}

BOOST_AUTO_TEST_CASE(IntegerTest)
{
	{
		const auto result = parseNumberStrict("0");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(!result.number.isDouble);
		BOOST_TEST(result.number.value == 0);
	}

	{
		const auto result = parseNumberStrict("  42  ");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(!result.number.isDouble);
		BOOST_TEST(result.number.value == 42);
	}

	{
		const auto result = parseNumberStrict("-42");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.value == -42);
	}

	{
		const auto result = parseNumberStrict("9223372036854775807");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.value == MAX_SINT64);
	}

	{
		const auto result = parseNumberStrict("-9223372036854775808");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.value == MIN_SINT64);
	}

	BOOST_TEST(parseNumberStrict("9223372036854775808").hasError);
	BOOST_TEST(parseNumberStrict("-9223372036854775809").hasError);
	BOOST_TEST(parseNumberStrict("12abc").hasError);
	BOOST_TEST(parseNumberStrict("").hasError);
}

BOOST_AUTO_TEST_CASE(NonStrictStopTest)
{
	TextPos pos = 0;
	StringParser parser;
	const ParsedNumber number = parser.parseNumber({"123 + 4"}, pos, false);

	BOOST_TEST(!parser.hasError());
	BOOST_TEST(number.value == 123);
	BOOST_TEST(pos == TextPos(3));
}

BOOST_AUTO_TEST_CASE(DoubleTest)
{
	// These values are exactly representable, so the conversion has to be exact as well
	{
		const auto result = parseNumberStrict("1.5");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.isDouble);
		BOOST_TEST(result.number.getDouble() == 1.5);
	}

	{
		const auto result = parseNumberStrict("-0.25");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.getDouble() == -0.25);
	}

	{
		const auto result = parseNumberStrict("1e3");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.isDouble);
		BOOST_TEST(result.number.getDouble() == 1000.0);
	}

	{
		const auto result = parseNumberStrict("25E-2");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.getDouble() == 0.25);
	}

	{
		const auto result = parseNumberStrict("1.5e+2");
		BOOST_TEST(!result.hasError);
		BOOST_TEST(result.number.getDouble() == 150.0);
	}

	BOOST_TEST(parseNumberStrict("1e400").hasError);
}

BOOST_AUTO_TEST_CASE(DoubleIsCorrectlyRoundedTest)
{
	const char* const values[] = {"0.07", "0.57", "123.456", "1.005", "9.995", "0.0003", "0.000001", "3.3e-7"};

	for (const char* text : values)
	{
		const auto result = parseNumberStrict(text);

		BOOST_TEST_INFO("Number: " << text);
		BOOST_TEST(!result.hasError);
		BOOST_TEST_INFO("Number: " << text);
		BOOST_TEST(result.number.getDouble() == strtod(text, nullptr));
	}
}

BOOST_AUTO_TEST_CASE(LongFractionIsNotOverflowTest)
{
	const char* const values[] = {"3.14159265358979323846", "0.1234567890123456789012", "1.00000000000000000000"};

	for (const char* text : values)
	{
		const auto result = parseNumberStrict(text);

		BOOST_TEST_INFO("Number: " << text);
		BOOST_TEST(!result.hasError);

		if (!result.hasError)
		{
			BOOST_TEST_INFO("Number: " << text);
			BOOST_TEST(result.number.getDouble() == strtod(text, nullptr));
		}
	}
}

BOOST_AUTO_TEST_CASE(BigExponentIsOverflowTest)
{
	// The exponent is accumulated in SLONG without a limit: 2^32 + 1 wraps to 1 and the value becomes 10
	BOOST_TEST(parseNumberStrict("1e4294967297").hasError);
	BOOST_TEST(parseNumberStrict("1e99999999999999999999").hasError);
}

BOOST_AUTO_TEST_CASE(StrictModeRejectionsTest)
{
	// Strict mode must not convert a text without digits (or with an unfinished exponent) to zero
	const char* const values[] = {"-", ".", "-.", "e5", "1e", "1e-", "1e+"};

	for (const char* text : values)
	{
		BOOST_TEST_INFO("Text: " << text);
		BOOST_TEST(parseNumberStrict(text).hasError);
	}
}

BOOST_AUTO_TEST_SUITE_END()	// NumberParseTests

BOOST_AUTO_TEST_SUITE_END()	// ParseTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
