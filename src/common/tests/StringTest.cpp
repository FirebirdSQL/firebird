#include "boost/test/unit_test.hpp"
#include "../common/classes/fb_string.h"
#include <filesystem>
#include <string_view>

using namespace Firebird;


BOOST_AUTO_TEST_SUITE(StringSuite)
BOOST_AUTO_TEST_SUITE(StringFunctionalTests)

// Constant and utils
static constexpr char lbl[] = "0123456789";
#define validate(A, B) BOOST_TEST(std::string_view(A.c_str(), A.length()) == std::string_view(B))
#define check(A, B) BOOST_TEST(A == B)

BOOST_AUTO_TEST_CASE(StringInitializeTest)
{
	string a;
	validate(a, "");
	a = lbl;
	string b = a;
	validate(b, lbl);
	string f = "0123456789";
	validate(f, lbl);
	string g("0123456789", 5);
	validate(g, "01234");
	string h(5, '7');
	validate(h, "77777");

	string i(1, '7');
	validate(i, "7");

	string j(&lbl[3], &lbl[5]);
	validate(j, "34");
}

BOOST_AUTO_TEST_CASE(StringAssignmentTest)
{
	string a = lbl;
	string b;
	b = a;
	validate(b, lbl);
	b = lbl;
	validate(b, lbl);
	a = 'X';
	validate(a, "X");

	a = b;
	for (string::iterator x = b.begin(); x < b.end(); x++)
		*x = 'u';
	validate(a, lbl);
	validate(b, "uuuuuuuuuu");

	char y[20], *z = y;
	const string c = a;
	for (string::const_iterator x1 = c.begin(); x1 < c.end(); x1++)
		*z++ = *x1;
	*z = 0;
	b = y;
	validate(b, lbl);
}

BOOST_AUTO_TEST_CASE(AtConstTest)
{
	const string a = lbl;
	string b(1, a.at(5));
	validate(b, "5");
}

BOOST_AUTO_TEST_CASE(GetAtTest)
{
	string a = lbl;
	string b(1, a.at(5));
	validate(b, "5");
}

BOOST_AUTO_TEST_CASE(SetAtTest)
{
	string a = lbl;
	char c = a[5];  // via operator const char*
	a.at(5) = 'u';
	a.at(7) = c;
	validate(a, "01234u6589");
}

BOOST_AUTO_TEST_CASE(FatalRangeTest)
{
	try
	{
		const string a = lbl;
		char b = a.at(15);

		BOOST_TEST_FAIL("Range exception is missing");
	}
	catch(Firebird::fatal_exception& ex)
	{
		std::string_view what = ex.what();
		BOOST_TEST(what == "Firebird::string - pos out of range");
	}
	catch(...)
	{
		BOOST_TEST_FAIL("Wrong exception");
	}
}

BOOST_AUTO_TEST_CASE(ResizeTest)
{
	string a = lbl;
	a.resize(15, 'u');
	validate(a, "0123456789uuuuu");
}

BOOST_AUTO_TEST_CASE(BigBufferAssignmentTest)
{
	string a;
	string x = lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl;
	x += lbl; //120 bytes
	a = x;
	validate(a, x.c_str());

	x = lbl;
	x += lbl;
	x += lbl;
	x += lbl; //40 bytes
	a = x;
	validate(a, x.c_str());
}

BOOST_AUTO_TEST_CASE(AddConstStringTest)
{
	string a = lbl;
	const string b = a;
	a += b;
	validate(a, "01234567890123456789");

	a = lbl;
	a += lbl;
	validate(a, "01234567890123456789");

	a = lbl;
	a += 'u';
	validate(a, "0123456789u");
}

BOOST_AUTO_TEST_CASE(OperatorAddTest)
{
	string a, b, c;

	a = "uuu";
	b = lbl;
	c = a + b;
	validate(c, "uuu0123456789");

	c = a + lbl;
	validate(c, "uuu0123456789");

	c = b + 'u';
	validate(c, "0123456789u");

	c = lbl + a;
	validate(c, "0123456789uuu");

	c = 'u' + b;
	validate(c, "u0123456789");

	validate(a, "uuu");
	validate(b, lbl);
}

BOOST_AUTO_TEST_CASE(AppendTest)
{
	string a = lbl;
	const string b = a;
	a.append(b);
	validate(a, "01234567890123456789");

	a = lbl;
	a.append(lbl);
	validate(a, "01234567890123456789");

	a = lbl;
	a.append(lbl, 6);
	validate(a, "0123456789012345");

	a = lbl;
	a.append(b, 3, 2);
	validate(a, "012345678934");

	a = lbl;
	a.append(b, 3, 20);
	validate(a, "01234567893456789");

	a = lbl;
	a.append(3, 'u');
	validate(a, "0123456789uuu");

	a = lbl;
	a.append(b.begin(), b.end());
	validate(a, "01234567890123456789");

	a = lbl;
	a.append("Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	validate(a, "0123456789Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	a = lbl;
	validate(a, lbl);

	string c = lbl;
	c += lbl;
	c += lbl;
	c += lbl;
	a = lbl;
	a.append("Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	validate(a, "0123456789Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	a = c;
	validate(a, c.c_str());
}

BOOST_AUTO_TEST_CASE(AssignTest)
{
	string a, b;
	b = lbl;

	a.assign(3, 'u');
	validate(a, "uuu");

	a.assign(lbl);
	validate(a, lbl);

	a.assign(lbl, 2);
	validate(a, "01");

	a.assign(b, 3, 3);
	validate(a, "345");

	a.assign(b);
	validate(a, lbl);

	a = "";
	validate(a, "");
	string::iterator x = b.begin();
	string::iterator y = b.end();
	a.assign(x, y);
	validate(a, lbl);
}

BOOST_AUTO_TEST_CASE(InsertEraseTest)
{
	string a, b = lbl;

	a = lbl;
	a.insert(5, 3, 'u');
	validate(a, "01234uuu56789");

	a = lbl;
	a.insert(3, lbl);
	validate(a, "01201234567893456789");

	a = lbl;
	a.insert(4, lbl, 2);
	validate(a, "012301456789");

	a = lbl;
	a.insert(5, b, 3, 3);
	validate(a, "0123434556789");

	a = lbl;
	a.insert(5, b);
	validate(a, "01234012345678956789");

	a = lbl;
	a.insert(2, "Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	validate(a, "01Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long23456789");

	a = lbl;
	string::iterator x = a.begin();
	x++;
	a.insert(x, 5, 'u');
	validate(a, "0uuuuu123456789");

	a = lbl;
	x = a.begin();
	x += 2;
	string::iterator f = b.begin();
	string::iterator t = b.end();
	f++; t--;
	a.insert(x, f, t);
	validate(a, "011234567823456789");

	a = lbl;
	a.erase();
	validate(a, "");

	a = lbl;
	a.erase(3, 6);
	validate(a, "0129");

	a = lbl;
	a.erase(3, 16);
	validate(a, "012");

	a = lbl;
	a.erase(3);
	validate(a, "012");

	a = lbl;
	x = a.begin();
	x += 3;
	a.erase(x);
	validate(a, "012456789");

	a = lbl;
	x = a.begin();
	x += 3;
	string::iterator y = a.end();
	y -= 2;
	a.erase(x, y);
	validate(a, "01289");
}

BOOST_AUTO_TEST_CASE(ReplaceTest)
{
	string a;
	const string b = lbl;
	string::iterator f0, t0;
	string::const_iterator f, t;

	a = lbl;
	a.replace(5, 2, 3, 'u');
	validate(a, "01234uuu789");

	a = lbl;
	f0 = a.begin() + 5;
	t0 = f0 + 2;
	a.replace(f0, t0, 3, 'u');
	validate(a, "01234uuu789");

	a = lbl;
	a.replace(3, 3, lbl);
	validate(a, "01201234567896789");

	a = lbl;
	f0 = a.begin() + 3;
	t0 = f0 + 3;
	a.replace(f0, t0, lbl);
	validate(a, "01201234567896789");

	a = lbl;
	a.replace(4, 4, lbl, 2);
	validate(a, "01230189");

	a = lbl;
	f0 = a.begin() + 4;
	t0 = f0 + 4;
	a.replace(f0, t0, lbl, 2);
	validate(a, "01230189");

	a = lbl;
	a.replace(5, 10, b, 3, 3);
	validate(a, "01234345");

	a = lbl;
	f0 = a.begin() + 5;
	t0 = f0 + 10;
	f = b.begin() + 3;
	t = f + 3;
	a.replace(f0, t0, f, t);
	validate(a, "01234345");

	a = lbl;
	a.replace(5, 0, b);
	validate(a, "01234012345678956789");

	a = lbl;
	f0 = a.begin() + 5;
	t0 = f0;
	a.replace(f0, t0, b);
	validate(a, "01234012345678956789");

	a = lbl;
	a.replace(2, 1, "Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long");
	validate(a, "01Something reaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaally long3456789");
}

BOOST_AUTO_TEST_CASE(CopyToTest)
{
	string a, b = lbl;
	char s[40];
	memset(s, 0, 40);

	b.copyTo(s, 5);
	a = s;
	validate(a, "0123");

	b.copyTo(s, 40);
	a = s;
	validate(a, lbl);

//	a.swap(b);
//	validate(b, "3456789");
//	validate(a, lbl);
}


BOOST_AUTO_TEST_CASE(FindTest)
{
	string a = "012345uuu345678";
					//	 9
	string b = "345";

	check(a.find(b), 3);
	check(a.find("45"), 4);
	check(a.find('5'), 5);
	check(a.find("ZZ"), string::npos);

	check(a.rfind(b), 9);
	check(a.rfind("45"), 10);
	check(a.rfind('5'), 11);
	check(a.rfind("ZZ"), string::npos);

	check(a.find("45", 8), 10);

	check(a.find_first_of("aub"), 6);
	check(a.find_first_of(b), 3);
	check(a.find_first_of("54"), 4);
	check(a.find_first_of('5'), 5);
	check(a.find_first_of("ZZ"), string::npos);

	check(a.find_last_of("aub"), 8);
	check(a.find_last_of(b), 11);
	check(a.find_last_of("54"), 11);
	check(a.find_last_of('5'), 11);
	check(a.find_last_of("ZZ"), string::npos);

	check(a.find_first_of("45", 8), 10);

	b = "010";
	check(a.find_first_not_of("aub"), 0);
	check(a.find_first_not_of(b), 2);
	check(a.find_first_not_of("0102"), 3);
	check(a.find_first_not_of('0'), 1);
	check(a.find_first_not_of(a), string::npos);

	b = "878";
	check(a.find_last_not_of("aub"), 14);
	check(a.find_last_not_of(b), 12);
	check(a.find_last_not_of("78"), 12);
	check(a.find_last_not_of('8'), 13);
	check(a.find_last_not_of(a), string::npos);

	check(a.find_first_not_of("u345", 8), 12);
}

BOOST_AUTO_TEST_CASE(SubstrTest)
{
	string a = lbl;
	string b;

	b = a.substr(3, 4);
	validate(b, "3456");

	b = a.substr(5, 20);
	validate(b, "56789");

	b = a.substr(50, 20);
	validate(b, "");
}

BOOST_AUTO_TEST_CASE(LoadFromFileTest)
{
	namespace fs = std::filesystem;
	auto tempFilePath = fs::temp_directory_path() / "test.txt";
	const char* filename = tempFilePath.string().data();
	FILE *x = fopen(tempFilePath.string().data(), "w+");

	std::string_view line1 = "char lbl[] = \"0123456789\";";
	std::string_view line2 = ""; // LoadFormFile read from '\n'
	std::string_view line3 = "//#define CHECK_FATAL_RANGE_EXCEPTION";

	fwrite(line1.data(), 1, line1.length(), x);
	fwrite("\n", 1, 1, x);
	fwrite(line2.data(), 1, line2.length(), x);
	fwrite("\n", 1, 1, x);
	fwrite(line3.data(), 1, line3.length(), x);
	fclose(x);

	x = fopen(filename, "r");
	string b;
	b.LoadFromFile(x);
	validate(b, line1);
	b.LoadFromFile(x);
	validate(b, line2);
	b.LoadFromFile(x);
	validate(b, line3);
	fclose(x);
}

BOOST_AUTO_TEST_CASE(BigAssignmentTest)
{
	string a = "Something moderaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaately long";
	string a1 = a;
	string b = a + a1;
	string b1 = b;
	string c = b + b1;
	string d = c;
	validate(d, c.data());
}

BOOST_AUTO_TEST_CASE(CompareTests)
{
	string a = lbl;
	string b = a;
	string c = "Aaa";
	string d = "AB";
	string e = "Aa";

	BOOST_TEST(a.compare(b) == 0);
	BOOST_TEST(a.compare(c) < 0);
	BOOST_TEST(c.compare(a) > 0);

	BOOST_TEST(c.compare(d) > 0);
	BOOST_TEST(c.compare(e) > 0);

	BOOST_TEST(a.compare(1, 10, b) > 0);
	BOOST_TEST(a.compare(1, 10, b, 1, 10) == 0);
	BOOST_TEST(a.compare(lbl) == 0);
	BOOST_TEST(a.compare(1, 3, lbl + 1, 3) == 0);
}

BOOST_AUTO_TEST_CASE(LTrimTest)
{
	string a = "  011100   ", b;

	b = a;
	b.ltrim();
	validate(b, "011100   ");

	b = a;
	b.rtrim();
	validate(b, "  011100");

	b = a;
	b.trim(" 0");
	validate(b, "111");

	b = a;
	b.alltrim("02 ");
	validate(b, "111");

	b = a;
	b.trim("012");
	validate(b, "  011100   ");

	validate(a, "  011100   ");
}

BOOST_AUTO_TEST_CASE(RTrimTest)
{
	string a = lbl;
	a += '\377';
	string b = a;
	a += " ";
	a.rtrim();
	validate(a, b.c_str());
}

BOOST_AUTO_TEST_CASE(LowerTest)
{
	string a = "AaBbCc", b;

	b = a;
	b.lower();
	validate(b, "aabbcc");

	b = a;
	b.upper();
	validate(b, "AABBCC");

	validate(a, "AaBbCc");
}

// Constants
static constexpr std::string_view EmptyString = "";
static constexpr std::string_view SmallStringValue = "123";
static constexpr std::string_view BigStringValue = "BiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiigString";
static_assert(BigStringValue.length() > Firebird::string::INLINE_BUFFER_SIZE);

// Utils
void checkMovedString(const string& moved)
{
	BOOST_CHECK(moved.c_str() != nullptr);
	BOOST_CHECK(moved.length() == 0);
	BOOST_CHECK(std::string_view(moved.data(), moved.length()) == EmptyString);
}

#define testSmallString(str) BOOST_TEST(std::string_view(str.data(), str.length()) == SmallStringValue)

#define testBigString(str) \
	BOOST_CHECK(std::string_view(str.data(), str.length()) == BigStringValue)


BOOST_AUTO_TEST_SUITE(MoveSematicsTests)

// Move only big strings in the same pool
BOOST_AUTO_TEST_CASE(DefaultPoolMoveTest)
{
	string sourceString(BigStringValue.data(), BigStringValue.length());

	// Move c'tor
	string newString(std::move(sourceString));
	checkMovedString(sourceString);
	testBigString(newString);

	// Reuse
	sourceString.assign(BigStringValue.data(), BigStringValue.length());
	testBigString(sourceString);

	// Move assignment
	newString = std::move(sourceString);
	checkMovedString(sourceString);
	testBigString(newString);
}

BOOST_AUTO_TEST_CASE(NewPoolMoveTest)
{
	AutoMemoryPool pool(MemoryPool::createPool());
	string sourceString(*pool, BigStringValue.data(), BigStringValue.length());

	// Move c'tor
	string newString(*pool, std::move(sourceString));
	checkMovedString(sourceString);
	testBigString(newString);

	// Reuse
	sourceString.assign(BigStringValue.data(), BigStringValue.length());
	testBigString(sourceString);

	// Move assignment
	newString = std::move(sourceString);
	checkMovedString(sourceString);
	testBigString(newString);
}

BOOST_AUTO_TEST_SUITE_END() // MoveSematicsTests


BOOST_AUTO_TEST_SUITE(CannotMoveTests)

// Do not move
BOOST_AUTO_TEST_CASE(DifferentVsDefaultPoolMove)
{
	AutoMemoryPool pool(MemoryPool::createPool());
	string sourceString(*pool, BigStringValue.data(), BigStringValue.length());

	// Move c'tor
	string newString(std::move(sourceString));
	testBigString(sourceString);
	testBigString(newString);

	// Reuse
	sourceString.assign(BigStringValue.data(), BigStringValue.length());
	testBigString(sourceString);

	// Move assignment
	newString = std::move(sourceString);
	testBigString(sourceString);
	testBigString(newString);
}

// Do not move
BOOST_AUTO_TEST_CASE(DifferentPoolsMove)
{
	AutoMemoryPool pool1(MemoryPool::createPool());
	AutoMemoryPool pool2(MemoryPool::createPool());
	string sourceString(*pool1, BigStringValue.data(), BigStringValue.length());

	// Move c'tor
	string newString(*pool2, std::move(sourceString));
	testBigString(sourceString);
	testBigString(newString);

	// Reuse
	sourceString.assign(BigStringValue.data(), BigStringValue.length());
	testBigString(sourceString);

	// Move assignment
	newString = std::move(sourceString);
	testBigString(sourceString);
	testBigString(newString);
}

// Do not move
BOOST_AUTO_TEST_CASE(SmallStringMove)
{
	string sourceString(SmallStringValue.data(), SmallStringValue.length());

	// Move c'tor
	string newString(std::move(sourceString));
	testSmallString(sourceString);
	testSmallString(newString);

	// Reuse
	sourceString.assign(SmallStringValue.data(), SmallStringValue.length());
	testSmallString(sourceString);

	// Move assign
	newString = std::move(sourceString);
	testSmallString(sourceString);
	testSmallString(newString);
}

BOOST_AUTO_TEST_SUITE_END() // CannotMoveTests

BOOST_AUTO_TEST_SUITE_END() // StringFunctionalTests
BOOST_AUTO_TEST_SUITE_END()	// StringSuite
