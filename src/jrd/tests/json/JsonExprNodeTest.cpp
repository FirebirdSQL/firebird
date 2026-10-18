#include "boost/test/unit_test.hpp"
#include "../jrd/json/JsonRuntime.h"
#include "../jrd/json/path/JsonPath.h"

#include "../TestContext.h"

using namespace FBJSON;

BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(JsonNodesTests)

BOOST_AUTO_TEST_SUITE(FilterNodeTests)
BOOST_AUTO_TEST_SUITE(ChildrenRoutinesTests)

BOOST_AUTO_TEST_CASE(CalculationNodeTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	{ // Omit
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::CALCULATION_NODE));

		BOOST_TEST(added == false);
		BOOST_CHECK(root.getChildrenCount() == 0);
	}

	{ // keep
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID));

		BOOST_TEST(added == true);
		BOOST_CHECK(root.getChildrenCount() == 1);
	}

	{ // keep
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::FILTER_NODE, JsonExprNode::FLAG_SOLID));

		BOOST_TEST(added == true);
		BOOST_CHECK(root.getChildrenCount() == 1);
	}

	{ // keep
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::METHOD_NODE, JsonExprNode::FLAG_SOLID));

		BOOST_TEST(added == true);
		BOOST_CHECK(root.getChildrenCount() == 1);
	}

	{ // keep
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::VARIABLE_NODE, JsonExprNode::FLAG_SOLID));

		BOOST_TEST(added == true);
		BOOST_CHECK(root.getChildrenCount() == 1);
	}

	{ // keep
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		const bool added = root.addChild(new JsonExprNode(pool, JsonExprNode::SCALAR_NODE, JsonExprNode::FLAG_SOLID));

		BOOST_TEST(added == true);
		BOOST_CHECK(root.getChildrenCount() == 1);
	}

	{
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		auto child = root.addChild(JsonExprNode::SCALAR_NODE);

		child->testType(JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(root.getChildrenCount() == 1);
		BOOST_TEST(root.getFirstChild() == child);
	}

	{
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		JsonExprNode* child = root.addChild(JsonExprNode::SCALAR_NODE);

		BOOST_CHECK(root.getChildrenCount() == 1);
		BOOST_TEST(root.getLastChild() == child);
	}

	{ // Merge
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		JsonExprNode* child = new JsonExprNode(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		child->addChild(JsonExprNode::SCALAR_NODE);
		child->addOperation(JsonExprOperation::ADDITION);
		child->addChild(JsonExprNode::SCALAR_NODE);
		child->finish();
		root.addChild(child); // merge

		BOOST_CHECK(root.getChildrenCount() == 2);
		BOOST_TEST(root.getFirstChild() != child);
		BOOST_TEST(root.getLastChild() != child);
	}

	{ // no Merge
		JsonExprNode root(pool, JsonExprNode::CALCULATION_NODE);
		JsonExprNode* child = new JsonExprNode(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		child->addChild(JsonExprNode::SCALAR_NODE);
		child->addOperation(JsonExprOperation::ADDITION);
		child->addChild(JsonExprNode::SCALAR_NODE);
		child->addFlag(JsonExprNode::FLAG_SOLID);
		child->finish();
		root.addChild(child); // no merge

		BOOST_TEST(root.getChildrenCount() == ULONG(1));
		BOOST_TEST(root.getFirstChild() == child);
	}

	{ // Operation
		JsonExprNode root(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		// OK
		for (uint32_t i = 0; i < JsonExprNode::OPERATIONS_LIMIT; ++i)
		{
			root.addOperation(JsonExprOperation::DIVISION);
		}

		// Limit
		BOOST_CHECK_THROW(root.addOperation(JsonExprOperation::DIVISION), json_syntax_exception);
	}

	{ // Variable
		JsonExprNode root(pool, FBJSON::JsonExprNode::CALCULATION_NODE);

		PathVariable* var = new PathVariable(pool);
		JsonExprNode* child = root.addVariable(var);
		BOOST_TEST(&root != child);
		BOOST_CHECK(child->isVariable());
		BOOST_TEST(child->getVariable() == var);
	}

	{ // makeHeadNode
		JsonExprNode root(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		root.makeHeadNode(true);

		BOOST_CHECK(root.getLastChild()->isVariable());
		BOOST_CHECK(root.getLastChild()->getVariable()->type == PathVariable::Type::HEAD);

		auto rootPath = root.getLastChild()->getVariable()->path->getRootNode();
		BOOST_CHECK((rootPath->flags & PathNode::FLAG_UNWRAP) != 0);
	}

	{ // makeHeadNode
		JsonExprNode root(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		root.makeHeadNode(false);

		BOOST_TEST(root.getChildrenCount() == ULONG(1));
		BOOST_CHECK(root.getLastChild()->isVariable());
		BOOST_CHECK(root.getLastChild()->getVariable()->type == PathVariable::Type::HEAD);
		BOOST_TEST((root.getLastChild()->getVariable()->path->getRootNode()->flags & PathNode::FLAG_UNWRAP) == false);
	}

	{ // makeHeadNode
		JsonExprNode root(pool, PathMethod::ABS);
		root.makeHeadNode(false);

		BOOST_TEST(root.getChildrenCount() == ULONG(1));
		BOOST_CHECK(root.getFirstChild()->isVariable());
		BOOST_CHECK(root.getFirstChild()->getVariable()->type == PathVariable::Type::HEAD);
		BOOST_CHECK((root.getFirstChild()->getVariable()->path->getRootNode()->flags & PathNode::FLAG_UNWRAP) == false);
	}

	{ // setHead
		JsonExprNode root(pool, PathMethod::ABS);

		JsonExprNode* child = new JsonExprNode(pool, JsonExprNode::SCALAR_NODE);

		root.setHead(child);

		BOOST_TEST(root.getChildrenCount() == ULONG(1));
		BOOST_CHECK(root.getFirstChild() == child);
	}

	{ // setPathMethod
		JsonExprNode root(pool, PathMethod::CEILING);

		BOOST_TEST(root.getChildrenCount() == ULONG(1));
		BOOST_CHECK(root.getFirstChild() == nullptr);
		BOOST_CHECK(root.getPathMethod() == PathMethod::CEILING);
	}

	{ // finish keep
		JsonExprNode* root = new JsonExprNode(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		root->addChild(JsonExprNode::SCALAR_NODE);
		root->addChild(JsonExprNode::SCALAR_NODE);
		BOOST_TEST(root->canBeOmitted() == false);

		auto parent = root->finish();

		BOOST_REQUIRE(parent == root);

		delete root;
	}

	{ // finish omit
		const auto test = 123;

		JsonExprNode* root = new JsonExprNode(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		auto child = root->addChild(JsonExprNode::SCALAR_NODE);
		child->set(test);
		BOOST_TEST(root->canBeOmitted());

		auto parent = root->finish();

		BOOST_TEST(parent == child);
	}
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SettersTests)

BOOST_AUTO_TEST_CASE(SetVariableNodeTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	PathVariable* var = new PathVariable(pool);
	FBJSON::JsonExprNode node(pool, var);

	BOOST_TEST(node.getVariable() == var);
	BOOST_TEST(node.testType(JsonExprNode::VARIABLE_NODE));
}


BOOST_AUTO_TEST_CASE(SetScalarTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	ULONG id = 0;
	//ContextVariables ctx(id, {});
	{ // int
		const auto test = 10;
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(test);

		BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		//BOOST_TEST(node.execute(ctx).getValue().integer == 10);
	}


	{ // int64
		const auto test = 1000000000;
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(test);

		BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		//BOOST_TEST(node.execute(ctx).getValue().integer == test);
	}

	{ // double
		const auto testValue = 3.14;
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(testValue);

		BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		//BOOST_TEST(node.execute(ctx).getValue().doubleValue == testValue);
	}

	{ // bool
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(true);

		BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		//BOOST_TEST(node.execute(ctx).getValue().boolean == true);
	}

	{ // null
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.setNull();

		BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		//BOOST_TEST(node.execute(ctx).isNull());
	}

	{ // string
		const auto testValue = "123";
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(testValue);

		// auto res = node.execute(ctx);

		// BOOST_TEST(node.testType(JsonExprNode::SCALAR_NODE));
		// BOOST_TEST(res.isString());
		// BOOST_TEST(res.getStringView() == testValue);
	}
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(ChildrenRoutineTest)


BOOST_AUTO_TEST_CASE(RoutineTests)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
	BOOST_CHECK(node.getChildrenCount() == 0);

	auto first = node.addChild(JsonExprNode::SCALAR_NODE);
	BOOST_CHECK(node.getFirstChild() == first);
	BOOST_CHECK(node.getLastChild() == first);
	BOOST_CHECK(node.getChildrenCount() == 1);

	auto second = node.addChild(JsonExprNode::SCALAR_NODE);
	BOOST_CHECK(node.getFirstChild() == first);
	BOOST_CHECK(node.getSecondChild() == second);
	BOOST_CHECK(node.getLastChild() == second);
	BOOST_CHECK(node.getChildrenCount() == 2);

	auto third = node.addChild(JsonExprNode::SCALAR_NODE);
	BOOST_CHECK(node.getFirstChild() == first);
	BOOST_CHECK(node.getSecondChild() == second);
	BOOST_CHECK(node.getLastChild() == third);
	BOOST_CHECK(node.getChildrenCount() == 3);

	node.clearChildren();
	BOOST_CHECK(node.getChildrenCount() == 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(CheckersTests)

BOOST_AUTO_TEST_CASE(IsEmptyTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	{ // Scalar child
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(node.isRootEmpty());

		node.addChild(JsonExprNode::SCALAR_NODE)->setNull();
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{ // Empty children
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(node.isRootEmpty());

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == true);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{ // Empty children
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(node.isRootEmpty());

		node.addOperation(JsonExprOperation::AND);
		BOOST_CHECK(node.isRootEmpty() == false);
	}


	{ // Empty children
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::FILTER_NODE);
		BOOST_CHECK(node.isRootEmpty());

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{ // Empty children
		FBJSON::JsonExprNode node(pool, PathMethod::ABS);
		BOOST_CHECK(node.isRootEmpty() == false);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{ // Empty children
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::VARIABLE_NODE);
		BOOST_CHECK(node.isRootEmpty() == false);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::FILTER_NODE);
		BOOST_CHECK(node.isRootEmpty());

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID); // Add calculation
		BOOST_CHECK(node.isRootEmpty() == false);
	}

	{
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::COMPOUND_NODE);
		BOOST_CHECK(node.isRootEmpty() == true);

		node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID); // Add calculation
		BOOST_CHECK(node.isRootEmpty() == false);
	}
}


BOOST_AUTO_TEST_CASE(CommonCheckersTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	{ // Scalar

		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::SCALAR_NODE);
		node.set(1);

		BOOST_CHECK(node.isVariable() == false);
		BOOST_CHECK(node.testType(JsonExprNode::SCALAR_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == false);
		BOOST_CHECK(node.canHasTail() == false);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);
	}

	{ // Variable
		FBJSON::JsonExprNode node(pool, new PathVariable(pool));

		BOOST_CHECK(node.isVariable() == true);
		BOOST_CHECK(node.testType(JsonExprNode::VARIABLE_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == false);
		BOOST_CHECK(node.canHasTail() == true);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);
	}

	{ // Calculation
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);

		BOOST_CHECK(node.isVariable() == false);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::VARIABLE_NODE) == false);
		BOOST_CHECK(node.canHasTail() == false);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);
	}

	{ // Method
		FBJSON::JsonExprNode node(pool, PathMethod::DOUBLE);

		BOOST_CHECK(node.isVariable() == false);
		BOOST_CHECK(node.testType(JsonExprNode::METHOD_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == false);
		BOOST_CHECK(node.canHasTail() == true);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);

	}

	{ // Filter
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::FILTER_NODE);

		BOOST_CHECK(node.isVariable() == false);
		BOOST_CHECK(node.testType(JsonExprNode::FILTER_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == false);
		BOOST_CHECK(node.canHasTail() == true);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);
	}

	{ // Compound
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::COMPOUND_NODE);

		BOOST_CHECK(node.isVariable() == false);
		BOOST_CHECK(node.testType(JsonExprNode::COMPOUND_NODE) == true);
		BOOST_CHECK(node.testType(JsonExprNode::CALCULATION_NODE) == false);
		BOOST_CHECK(node.getTailNode() == nullptr);
		BOOST_CHECK(node.canBeOmitted() == false);
	}
}

BOOST_AUTO_TEST_CASE(OmitTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	{ // 1
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(node.canBeOmitted() == false);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_CHECK(node.canBeOmitted() == true);
	}


	{ // 2
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_CHECK(node.canBeOmitted() == false);
	}

	{ // 3
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE);
		node.addOperation(JsonExprOperation::ADDITION);
		BOOST_CHECK(node.canBeOmitted() == false);
	}
}

BOOST_AUTO_TEST_CASE(TailTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	{ // method

		{
			FBJSON::JsonExprNode* lastNode = nullptr;

			FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::METHOD_NODE);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);
			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);
			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == true);
			BOOST_TEST(node.getTailNode() == lastNode);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);

			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == true);
			BOOST_TEST(node.getTailNode() == lastNode);
		}

		{
			FBJSON::JsonExprNode* lastNode = nullptr;

			FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::VARIABLE_NODE);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);
			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == true);
			BOOST_TEST(node.getTailNode() == lastNode);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == true);

			// lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			// BOOST_TEST(node.hasTail() == false);
			// lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			// BOOST_TEST(node.hasTail() == false);
			// BOOST_TEST(node.getTailNode() == nullptr);

			// lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			// BOOST_TEST(node.hasTail() == false);
			// lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			// BOOST_TEST(node.hasTail() == false);
		}

		{
			FBJSON::JsonExprNode* lastNode = nullptr;

			FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::FILTER_NODE);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);

			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == false);

			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == false);
			BOOST_TEST(node.getTailNode() == nullptr);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == false);

			lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			BOOST_TEST(node.hasTail() == true);
			BOOST_TEST(node.getTailNode() == lastNode);
			lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			BOOST_TEST(node.hasTail() == true);

			// lastNode = node.addChild(JsonExprNode::SCALAR_NODE);
			// BOOST_TEST(node.hasTail() == false);
			// BOOST_TEST(node.getTailNode() == nullptr);
			// lastNode->addFlag(FBJSON::JsonExprNode::FLAG_METHOD_ARGUMENT);
			// BOOST_TEST(node.hasTail() == false);
		}
	}


	{ // variable
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::VARIABLE_NODE);
		BOOST_TEST(node.hasTail() == false);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_TEST(node.hasTail() == true);
	}

	{ // Filter
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::FILTER_NODE);
		BOOST_TEST(node.hasTail() == false);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_TEST(node.hasTail() == false);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_TEST(node.hasTail() == false);
		node.addChild(JsonExprNode::SCALAR_NODE);
		BOOST_TEST(node.hasTail() == true);
	}
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(HelpersTests)
BOOST_AUTO_TEST_CASE(HelperTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	{ // Has Tail
		FBJSON::JsonExprNode* lastNode = nullptr;

		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::VARIABLE_NODE);
		auto* child = node.addChild(FBJSON::JsonExprNode::CALCULATION_NODE, JsonExprNode::FLAG_SOLID);

		BOOST_REQUIRE(node.hasTail());
		node.unwrap(); // Nothing
		BOOST_CHECK(child->testType(FBJSON::JsonExprNode::CALCULATION_NODE));
	}

	{ // No Tail not variable
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		BOOST_REQUIRE(node.hasTail() == false);

		node.unwrap(); // Nothing
		BOOST_CHECK(node.getChildrenCount() == 0);
	}

	{ // No Tail, empty variable
		FBJSON::JsonExprNode* lastNode = nullptr;

		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::VARIABLE_NODE);
		node.unwrap(); // nothing
		BOOST_CHECK(node.getChildrenCount() == 0);
		BOOST_CHECK(node.getVariable() == nullptr);
	}

	{ // Variable
		FBJSON::JsonExprNode* lastNode = nullptr;

		FBJSON::JsonExprNode node(pool, new PathVariable(pool));

		BOOST_CHECK(node.getVariable()->path == nullptr);
		node.unwrap();
		BOOST_CHECK(node.getVariable()->path != nullptr);
		BOOST_CHECK(node.getVariable()->path->getRootNode()->flags & PathNode::FLAG_UNWRAP);
	}

	{ // Variable with path
		FBJSON::JsonExprNode* lastNode = nullptr;

		auto var = new PathVariable(pool);
		var->path = new JsonPath(pool);
		FBJSON::JsonExprNode node(pool, var);

		BOOST_CHECK(node.getVariable()->path != nullptr);
		node.unwrap();
		BOOST_CHECK(node.getVariable()->path == var->path);
		BOOST_CHECK(node.getVariable()->path->getRootNode() == var->path->getRootNode());
		BOOST_CHECK(node.getVariable()->path->getRootNode()->flags & PathNode::FLAG_UNWRAP);
	}
}

BOOST_AUTO_TEST_CASE(ScalarHelpersTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::SCALAR_NODE);
		node.set(1);
		BOOST_CHECK(node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == -1); // TODO: Get value via execute method

		BOOST_CHECK(node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == 1);

		BOOST_CHECK(node.applyUnaryOp(JsonExprOperation::UNARY_PLUS));
		BOOST_CHECK(node.getRangeNumber() == 1);

		node.set(-42);
		BOOST_CHECK(node.applyUnaryOp(JsonExprOperation::UNARY_PLUS));
		BOOST_CHECK(node.getRangeNumber() == -42);

		node.set(42);
		BOOST_CHECK(node.applyUnaryOp(JsonExprOperation::UNARY_PLUS));
		BOOST_CHECK(node.getRangeNumber() == 42);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::CALCULATION_NODE);
		BOOST_CHECK(!node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == std::nullopt);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::VARIABLE_NODE);
		BOOST_CHECK(!node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == std::nullopt);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::METHOD_NODE);
		BOOST_CHECK(!node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == std::nullopt);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::FILTER_NODE);
		BOOST_CHECK(!node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == std::nullopt);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::COMPOUND_NODE);
		BOOST_CHECK(!node.applyUnaryOp(JsonExprOperation::UNARY_MINUS));
		BOOST_CHECK(node.getRangeNumber() == std::nullopt);
	}

	{
		FBJSON::JsonExprNode node(pool, JsonExprNode::SCALAR_NODE);
		node.set(4000000);
		BOOST_CHECK_THROW(node.getRangeNumber(), json_fatal_exception);
		node.set(-4000000);
		BOOST_CHECK_THROW(node.getRangeNumber(), json_fatal_exception);
	}
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(FilterTests)

BOOST_AUTO_TEST_CASE(FilterTrueFalseTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	const auto big = 10;
	const auto low = 5;
	{ // true
		FBJSON::JsonExprNode* lastNode = nullptr;

		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE)->set(big);
		node.addOperation(JsonExprOperation::MORE);
		node.addChild(JsonExprNode::SCALAR_NODE)->set(low);
		node.finish();

		// ULONG id = 0;
		// ContextVariables ctx(id, {});
		// auto res = JsonExprNode::passFilter(ctx, &node);

		// BOOST_CHECK(res == JsonExprNode::FilterResult::RTRUE);
	}

	{ // false
		FBJSON::JsonExprNode* lastNode = nullptr;

		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE)->set(big);
		node.addOperation(JsonExprOperation::LESS);
		node.addChild(JsonExprNode::SCALAR_NODE)->set(low);
		node.finish();

		ULONG id = 0;
		// ContextVariables ctx(id, {});
		// auto res = JsonExprNode::passFilter(ctx, &node);

		// BOOST_CHECK(res == JsonExprNode::FilterResult::RFALSE);
	}

	{ // Unknown
		FBJSON::JsonExprNode node(pool, FBJSON::JsonExprNode::CALCULATION_NODE);
		node.addChild(JsonExprNode::SCALAR_NODE)->set(big);
		node.addOperation(JsonExprOperation::LESS);
		node.addChild(JsonExprNode::SCALAR_NODE)->set("");
		node.finish();

		ULONG id = 0;
		// ContextVariables ctx(id, {});
		// auto res = JsonExprNode::passFilter(ctx, &node);

		// BOOST_CHECK(res == JsonExprNode::FilterResult::RUNKNOWN);
	}
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()	// FilterTests

BOOST_AUTO_TEST_SUITE_END()	// JsonNodesTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
