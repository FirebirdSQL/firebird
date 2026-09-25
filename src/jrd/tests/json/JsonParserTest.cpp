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
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetEscaped.data());

	parse(source, 255, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetEscaped.data());

	parse(source, 254, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetEscaped.data());

	parse(source, 253, "\\n", "\n", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetEscaped.data());


	// Simple unicode is always converted to ascii
	parse(source, 250, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 251, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 252, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 253, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 254, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 255, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 256, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 257, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());

	parse(source, 258, "\\u0020", " ", targetUnescaped, targetEscaped, outUnescaped, outEscaped);
	BOOST_TEST(std::string_view(outUnescaped) == targetUnescaped.data());
	BOOST_TEST(std::string_view(outEscaped) == targetUnescaped.data());
}


BOOST_AUTO_TEST_CASE(TestUnsafeStringParse)
{
	SmallString output;

	StringParser::parseStringUnsafe({"Hello \\\" world"}, output);
	BOOST_TEST(output.data() == "Hello \\\" world");

	StringParser::parseStringUnsafe({"Hello \\n world world"}, output);
	BOOST_TEST(output.data() == "Hello \\n world world");

	static_assert(char(0x0020) == ' ', "Wrong test");
	StringParser::parseStringUnsafe({"Hello \\u0020 world world"}, output);
	BOOST_TEST(output.data() == "Hello   world world");

	StringParser::parseStringUnsafe({"Hello \\u0020 world world"}, output);
	BOOST_TEST(output.data() == "Hello   world world");

	StringParser::parseStringUnsafe({"Hello \\u0020"}, output);
	BOOST_TEST(output.data() == "Hello  ");

	StringParser::parseStringUnsafe({"Hello \\ud83d\\ude80 world world"}, output);
	BOOST_TEST(output.data() == "Hello \\ud83d\\ude80 world world");
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
BOOST_AUTO_TEST_SUITE_END()	// StringParseTests

BOOST_AUTO_TEST_SUITE_END()	// ParseTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
