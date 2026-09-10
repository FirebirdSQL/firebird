#include "boost/test/unit_test.hpp"
#include "JsonTestUtils.h"

#include "firebird.h"
#include "../common/status.h"

BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(JsonPathTests)

using namespace FBJSON;
using namespace Firebird;
using namespace TestUtils;

std::optional<bool> hasUnwrapFlag(const char* path)
{
	PathWrapper parsedPath(path);
	if (!parsedPath.jpath.hasData())
		return std::nullopt;

	PathNode* pathNode = parsedPath.jpath->getJsonPath()->getRootNode();
	BOOST_TEST(pathNode != nullptr);

	if (pathNode == nullptr)
		return false;

	while (pathNode->next)
	{
		pathNode = pathNode->next;
	}

	BOOST_TEST_INFO(path);
	return pathNode->matchUnwrapPattern();
}


BOOST_AUTO_TEST_SUITE(PathParsingTests)


BOOST_AUTO_TEST_CASE(UnwrapFlagTest)
{
	BOOST_CHECK(hasUnwrapFlag("+$.digits") == true);
	BOOST_CHECK(hasUnwrapFlag("-$.digits") == true);
	BOOST_CHECK(hasUnwrapFlag("$.digits + 22") == false);
	BOOST_CHECK(hasUnwrapFlag("22 + $.digits") == false);
	BOOST_CHECK(hasUnwrapFlag("($.digits + 22) ? (@ > 0)") == false);
	BOOST_CHECK(hasUnwrapFlag("22 + $.digits ? (@ > 0)") == true);
	BOOST_CHECK(hasUnwrapFlag(R"(lax $ ? (@.type() == "array"))") == true);

	BOOST_CHECK(hasUnwrapFlag("strict +$.digits") == false);
	BOOST_CHECK(hasUnwrapFlag("strict -$.digits") == false);
	BOOST_CHECK(hasUnwrapFlag("strict $.digits + 22") == false);
	BOOST_CHECK(hasUnwrapFlag("strict 22 + $.digits") == false);
	BOOST_CHECK(hasUnwrapFlag("strict ($.digits + 22) ? (@ > 0)") == false);
	BOOST_CHECK(hasUnwrapFlag("strict (22 + $.digits) ? (@ > 0)") == false);
	BOOST_CHECK(hasUnwrapFlag("strict 22 + $.digits ? (@ > 0)") == false);

	BOOST_CHECK(hasUnwrapFlag("+$.digits[*]") == true);
	BOOST_CHECK(hasUnwrapFlag("-$.digits[*]") == true);
	BOOST_CHECK(hasUnwrapFlag("$.digits[*] + 22") == false);
	BOOST_CHECK(hasUnwrapFlag("22 + $.digits[*]") == false);
	BOOST_CHECK(hasUnwrapFlag("($.digits[*] + 22) ? (@ > 0)") == false);
	BOOST_CHECK(hasUnwrapFlag("22 + $.digits[*] ? (@ > 0)") == true);
	BOOST_CHECK(hasUnwrapFlag("(22 + $.digits[*]) ? (@ > 0)") == false);
}


BOOST_AUTO_TEST_CASE(KeyValueMethodTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue()");

	// The root node is the method
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(expr->getChildrenCount() == FB_SIZE_T(1));

	// The child is a root
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);
}

BOOST_AUTO_TEST_CASE(KeyValue_PathTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue().value");

	// The keyvalue method
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(expr->getChildrenCount() == FB_SIZE_T(2));

	// The path root
	BOOST_CHECK(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);

	// The path after the method
	BOOST_CHECK(expr->getLastChild()->isVariable());
	var = expr->getLastChild()->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(var->path->getRootNode()->next);
	BOOST_CHECK(var->path->getRootNode()->next->next == nullptr);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("value"));
}

BOOST_AUTO_TEST_CASE(KeyValue_KeyValueTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue().keyvalue()");
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_REQUIRE(expr->getChildrenCount() == 2);
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);

	// Is keyvalue node
	JsonExprNode* keyvalue = expr->getLastChild();
	BOOST_CHECK(keyvalue->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(keyvalue->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(keyvalue->getChildrenCount() == ULONG(1));

	// Only a placement argument
	BOOST_REQUIRE(keyvalue->getFirstChild()->isVariable());
	BOOST_CHECK(keyvalue->getFirstChild()->getVariable()->type == PathVariable::Type::HEAD);
}

BOOST_AUTO_TEST_CASE(KeyValue_Path_KeyValueTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue().value.keyvalue()");
	//  MN(keyvalue())
	//  |          |
	// VN($)       VN(&.value)
	//             |
	//             MN(keyvalue())
	//             |
	//             VN(&)

	// I. The root path part - "keyvalue()"
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE); // keyvalue()
	BOOST_REQUIRE(expr->getChildrenCount() == 2);

	// I. Arg 1. The root path - "$"
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);

	// I. Arg 2. The afterpath variable - ".value"
	JsonExprNode* afterpath = expr->getLastChild();
	BOOST_CHECK(afterpath->testType(JsonExprNode::VARIABLE_NODE));
	var = afterpath->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(var->path->getRootNode()->next);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("value"));
	BOOST_CHECK(var->path->getRootNode()->next->matchUnwrapPattern());
	BOOST_CHECK(var->path->getRootNode()->next->next == nullptr);

	// II. The second method - keyvalue()
	BOOST_REQUIRE(afterpath->getChildrenCount() == ULONG(1));
	JsonExprNode* keyvalue = afterpath->getLastChild();
	BOOST_CHECK(keyvalue->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(keyvalue->getPathMethod() == PathMethod::KEYVALUE);

	// II. Arg 1. The chain variable - & (empty path)
	BOOST_TEST(keyvalue->getChildrenCount() == ULONG(1));
	JsonExprNode* chainArg = keyvalue->getLastChild();
	BOOST_CHECK(chainArg->testType(JsonExprNode::VARIABLE_NODE));
	BOOST_CHECK(chainArg->getVariable());
	var = chainArg->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_CHECK(var->path->isEmpty(0));
}


BOOST_AUTO_TEST_CASE(KeyValueMethodWithPathTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue().keyvalue().value");

	// The first keyvalue
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(expr->getChildrenCount() == 2U);

	// The root
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);

	// The second keyvalue
	JsonExprNode* keyvalue = expr->getLastChild();
	BOOST_CHECK(keyvalue->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(keyvalue->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(keyvalue->getChildrenCount() == 2U);

	// The dummy argument
	BOOST_REQUIRE(keyvalue->getFirstChild()->isVariable());
	var = keyvalue->getFirstChild()->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);

	// Keyvalue unwraps the argument
	BOOST_REQUIRE(var->path != nullptr);
	BOOST_REQUIRE(var->path->getRootNode() != nullptr);
	BOOST_CHECK(var->path->getRootNode()->matchUnwrapPattern());

	// The path after the method
	BOOST_REQUIRE(keyvalue->getLastChild()->isVariable());
	var = keyvalue->getLastChild()->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(var->path->getRootNode()->next);
	BOOST_CHECK(var->path->getRootNode()->next->next == nullptr);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("value"));
}


BOOST_AUTO_TEST_CASE(MultipleKeyValuesTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue().value.a.keyvalue().value.b.keyvalue().name");

	// I. The first keyvalue
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(expr->getChildrenCount() == ULONG(2));

	// The root
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);
	BOOST_CHECK(expr->getFirstChild()->getVariable()->path == nullptr);

	// The path "value.a"
	JsonExprNode* afterpath = expr->getLastChild();
	BOOST_REQUIRE(afterpath->isVariable());
	var = afterpath->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(var->path);
	BOOST_REQUIRE(var->path->getRootNode());
	BOOST_CHECK(var->path->getRootNode()->next);
	BOOST_REQUIRE(var->path->getRootNode()->next->next);
	BOOST_CHECK(var->path->getRootNode()->next->next->next == nullptr);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("value"));
	BOOST_CHECK(var->path->getRootNode()->next->next->field.equals("a"));

	// II. The second keyvalue
	JsonExprNode* keyvalue = afterpath->getLastChild();
	BOOST_CHECK(keyvalue->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(keyvalue->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(keyvalue->getChildrenCount() == ULONG(2));

	// The second keyvalue chain arg
	JsonExprNode* chainArg = keyvalue->getFirstChild();
	BOOST_REQUIRE(chainArg->isVariable());
	var = chainArg->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(!var->hasPath(0));

	// The second keyvalue afterpath
	afterpath = keyvalue->getLastChild();
	BOOST_REQUIRE(afterpath->isVariable());
	var = afterpath->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(var->path);
	BOOST_REQUIRE(var->path->getRootNode());
	BOOST_CHECK(var->path->getRootNode()->next);
	BOOST_REQUIRE(var->path->getRootNode()->next->next);
	BOOST_CHECK(var->path->getRootNode()->next->next->next == nullptr);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("value"));
	BOOST_CHECK(var->path->getRootNode()->next->next->field.equals("b"));


	// III. The third keyvalue
	keyvalue = afterpath->getLastChild();
	BOOST_CHECK(keyvalue->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(keyvalue->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(keyvalue->getChildrenCount() == ULONG(2));

	// Chin arg
	afterpath = keyvalue->getFirstChild();
	BOOST_REQUIRE(afterpath->isVariable());
	var = afterpath->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_REQUIRE(!var->hasPath(0));

	// The path "value" after the thread keyvalue
	BOOST_REQUIRE(keyvalue->getLastChild()->isVariable());
	var = keyvalue->getLastChild()->getVariable();
	BOOST_CHECK(var->type == PathVariable::Type::HEAD);
	BOOST_CHECK(var->path->getRootNode()->next);
	BOOST_CHECK(var->path->getRootNode()->next->next == nullptr);
	BOOST_CHECK(var->path->getRootNode()->next->field.equals("name"));
}



BOOST_AUTO_TEST_CASE(KeyValueMethodWithFilterTest)
{
	const JsonExprNode* expr = nullptr;
	const PathVariable* var = nullptr;

	PathWrapper holder;
	expr = holder.getPathTailNodes("$.keyvalue() ? (@ > 3)");

	// The first keyvalue
	BOOST_CHECK(expr->testType(JsonExprNode::METHOD_NODE));
	BOOST_CHECK(expr->getPathMethod() == PathMethod::KEYVALUE);
	BOOST_TEST(expr->getChildrenCount() == 2U);

	// The root
	BOOST_REQUIRE(expr->getFirstChild()->isVariable());
	BOOST_CHECK(expr->getFirstChild()->getVariable()->type == PathVariable::Type::ROOT);

	// The filter
	JsonExprNode* afterpath = expr->getLastChild();
	BOOST_REQUIRE(afterpath->testType(JsonExprNode::VARIABLE_NODE));

	auto* path = afterpath->getVariable()->path->getRootNode();
	BOOST_REQUIRE(path->filterNode != nullptr);
}

BOOST_AUTO_TEST_CASE(FilterForZeroPathTest)
{
	const PathVariable* var = nullptr;

	PathWrapper pathWrapper("$ ? (@ == 42)");
	auto* path = pathWrapper.jpath->getJsonPath()->getRootNode();
	BOOST_REQUIRE(path);
	BOOST_REQUIRE(path);
	BOOST_TEST(path->next == nullptr);
	BOOST_TEST(path->filterNode != nullptr);
}

BOOST_AUTO_TEST_CASE(SyntaxTest)
{
	PathWrapper parser;
	parser.keys.put("pass");
	parser.keys.put("hello");
	parser.keys.put("passing");
	{
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$"));
		BOOST_CHECK_THROW(parser.parseNoCheck("@"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$passing")); // Allow
		BOOST_CHECK_THROW(parser.parseNoCheck("4"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("null"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("false"), Firebird::Exception);

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("lax $"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("strict $"));

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.field"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.\"field\""));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.*"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.123"), json_syntax_exception);

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[1,2,3]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[1]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[*]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[1 to 2]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[2 to 1]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[-1]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[-1 to -4]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[-1 to last]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[-1 to $pass]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[$pass to 2]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[$pass to $hello]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[$.field to (1 + 2)]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[($.field ? (@ > 3)) to (1 + 2)]"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$[($.field.abs()) to (1 + 2)]"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$[$unknown to 2]"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$[sdd]"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$[1 to dd]"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$[1 to to]"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$[to 2]"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$[2 to]"), Firebird::Exception);

		
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value > 3)"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value > 3).hello"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value > 3).hello ? (@.r == 2)"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value > 3).hello ? (@.r == 2)[1,2,3]"));
		
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.abs()"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.kfds()"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.abs().value"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.keyvalue()"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.keyvalue().value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.keyvalue().value"));

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value.abs() > 3)"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (@.value.abs().value > 3)"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@.value.keyvalue().value > 3)"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? ((@ > 3)) && ($ > 3))"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? ((@ > 3) && ($ > 3))"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@ > 3 && $ > 3)"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (($passing > 3) && (4 > 3))"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (($.value.abs() > 3) && (4 > 3))"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? ($passing > 3 && 4 > 3)"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? ($.value.abs() > 3 && 4 > 3)"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (($passing > 3)) && (4 > 3))"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (($.value.abs() > 3)) && (4 > 3))"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? ((4 && 3))"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (4 && 3)"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (4 && 3)"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name ? (false && false)"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name ? (@ > 3) ? (@ > 3)"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name.abs().abs()"));
		BOOST_CHECK_THROW(parser.parseNoCheck("$.name.abs().value"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name.keyvalue().value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.keyvalue() ? (@ > 3).v ? (@ > 3)"));


		BOOST_CHECK_NO_THROW(parser.parseNoCheck("3 + $.value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.value + 3"));

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$passing + 3"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("3 + $passing"));

		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$passing + $.value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.value + $passing"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.value + 4"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("4 + $.value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$passing ? ($.value > 2)")); // Allow passing as main expr
		BOOST_CHECK_THROW(parser.parseNoCheck("$passing ? ($.value)"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$.value ? ($.value)"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("$@ ? ($.value)"), Firebird::Exception);
		BOOST_CHECK_THROW(parser.parseNoCheck("@ ? ($.value)"), Firebird::Exception);
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name + 3"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name + $.value"));
		BOOST_CHECK_NO_THROW(parser.parseNoCheck("$.name + $.abs() + 4"));

	}
}


BOOST_AUTO_TEST_SUITE_END()	// PathParsingTests

BOOST_AUTO_TEST_SUITE_END()	// JsonPathTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
