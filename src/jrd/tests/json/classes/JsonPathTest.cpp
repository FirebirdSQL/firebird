#include "boost/test/unit_test.hpp"
#include "../../TestContext.h"

#include "../jrd/json/path/JsonPath.h"
#include "../jrd/json/classes/JsonScalar.h"
#include "../jrd/json/JsonRuntime.h"

#include <utility>

using namespace FBJSON;


BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(JsonClassesTests)

BOOST_AUTO_TEST_SUITE(PathNodeTests)


BOOST_AUTO_TEST_CASE(RangeTest)
{
	{
		JsonPath::Range range;
		range.init(42);
		BOOST_TEST(range.down == 42);
		BOOST_TEST(range.up == 42);

		range.init(-42, -3);
		BOOST_TEST(range.down == -42);
		BOOST_TEST(range.up == -3);
	}

	{
		JsonPath::Range range;
		range.init(-8, -3);
		BOOST_TEST(range.getPreparedDown(10) == 2);
		BOOST_TEST(range.getPreparedUp(10) == 7);
		range.init(5, 42);
		BOOST_TEST(range.getPreparedDown(10) == 5);
		BOOST_TEST(range.getPreparedUp(120) == 42);
	}

	{
		JsonPath::Range range;
		range.init(-8, -3);
		BOOST_TEST(range.getPreparedDown(10) == 2);
		BOOST_TEST(range.getPreparedUp(10) == 7);
		range.init(5, 42);
		BOOST_TEST(range.getPreparedDown(10) == 5);
		BOOST_TEST(range.getPreparedUp(120) == 42);
	}

	{
		JsonPath::Range range;
		range.init(5, 15);
		BOOST_TEST(range.enters(4) == false);
		BOOST_TEST(range.enters(7) == true);
		BOOST_TEST(range.enters(2) == false);
		BOOST_TEST(range.enters(1) == false);
		BOOST_TEST(range.enters(16) == false);
	}

	{
		JsonPath::Range range;
		range.init(5, 15);
		BOOST_TEST(range.canWrap() == false);
		range.init(0, -2);
		BOOST_TEST(range.canWrap() == true);
		range.init(1, -1);
		BOOST_TEST(range.canWrap() == true);
	}

	{
		JsonPath::Range range;
		range.init(5, 15);
		BOOST_TEST(range.canGetFirst(5) == false);
		range.init(0, -2);
		BOOST_TEST(range.canGetFirst(5) == true);
		range.init(1, -1);
		BOOST_TEST(range.canGetFirst(5) == false);
		BOOST_TEST(range.canGetFirst(1) == true);
	}

	{
		JsonPath::Range range;
		range.init(0, -1);
		BOOST_TEST(range.isComplex() == false);
		range.init(0, -2);
		BOOST_TEST(range.isComplex() == true);
		range.init(3, 4);
		BOOST_TEST(range.isComplex() == false);
		range.init(3, -1);
		BOOST_TEST(range.isComplex() == true);
	}

	{
		JsonPath::Range range;
		range.init(42);
		BOOST_TEST(range.isWildcard() == false);
		range.init(0, JPATH_ARRAY_LAST_INDEX);
		BOOST_TEST(range.isWildcard() == true);
	}
}

JsonExprNode* makeDummyNode()
{
	return JsonExprNode::make<JsonExprNode::SCALAR_NODE>(*getDefaultMemoryPool());
}

BOOST_AUTO_TEST_CASE(NodeMethodsTest)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();

	{
		PathNode node(pool);
		BOOST_CHECK(node.state == PathNodeState::NORMAL);
		BOOST_CHECK(!node.isTemporary());
		BOOST_CHECK(!node.isDisabled());
		BOOST_CHECK(node.isEmpty(0));
	}

	{ // add
		PathNode node(pool);
		PathNode* next = node.add();
		BOOST_CHECK(node.next == next);
		BOOST_CHECK(next->prev == &node);

		BOOST_CHECK(next->depth == node.depth + 1);
	}

	{ // enable disable first
		PathNode node(pool);
		auto next = node.add();
		node.enable(); // nothing
		BOOST_CHECK(node.state == PathNodeState::NORMAL);
		BOOST_CHECK(!node.isTemporary());
		BOOST_CHECK(!node.isDisabled());
		BOOST_CHECK(!node.isEmpty(0));
		BOOST_TEST(next->depth == node.depth + 1);

		node.disable();
		BOOST_CHECK(node.state == PathNodeState::DISABLED);
		BOOST_CHECK(!node.isTemporary());
		BOOST_CHECK(node.isDisabled());
		BOOST_CHECK(!node.isEmpty(0));
		BOOST_TEST(next->depth == node.depth);

		node.enable();
		BOOST_CHECK(node.state == PathNodeState::NORMAL);
		BOOST_CHECK(!node.isTemporary());
		BOOST_CHECK(!node.isDisabled());
		BOOST_CHECK(!node.isEmpty(0));
		BOOST_TEST(next->depth == node.depth + 1);
	}

	{ // enable disable middle
		PathNode first(pool);
		PathNode* middle = first.add();
		PathNode* last = middle->add();

		middle->disable();
		BOOST_CHECK(&first == last->prev);
		BOOST_CHECK(first.next == middle);
		BOOST_CHECK(first.depth + 1 == last->depth);

		BOOST_CHECK(middle->next == last); // Does not changes
		BOOST_CHECK(middle->depth == last->depth);
		middle->enable();
		BOOST_CHECK(middle == last->prev);
		BOOST_CHECK(first.next == middle);
		BOOST_CHECK(first.depth + 1 == middle->depth);
		BOOST_CHECK(middle->next == last);
		BOOST_CHECK(middle->depth + 1 == last->depth);
	}

	{ // enable disable last
		PathNode first(pool);
		PathNode* middle = first.add();
		PathNode* last = middle->add();

		last->disable();
		BOOST_CHECK(last->isDisabled());
		BOOST_CHECK(last->next == nullptr);
		last->enable();
		BOOST_CHECK(last->state == FBJSON::PathNodeState::NORMAL);
	}

	{ // selectAll
		PathNode node(pool);
		node.selectAll();
		BOOST_CHECK(node.isNextEmpty());
		BOOST_CHECK(node.type == ItemType::ARRAY_ELEMENT);
		BOOST_REQUIRE(node.ranges.hasData());
		BOOST_TEST(node.ranges[0].down == 0);
		BOOST_TEST(node.ranges[0].up == JPATH_ARRAY_LAST_INDEX);
	}

	{ // Updates disabled
		PathNode node(pool);
		PathNode* middle = node.add();
		PathNode* last = middle->add();

		middle->disable();
		auto updated = middle->update();
		BOOST_CHECK(updated == middle);

		BOOST_CHECK(node.next == middle);
		BOOST_CHECK(middle->next == last);

		BOOST_TEST(node.depth + 1 == middle->depth);
		BOOST_TEST(middle->depth + 1 == last->depth);
	}

	{ // Updates temporally middle
		PathNode node(pool);
		PathNode* middle = node.add();
		middle->state = PathNodeState::TEMPORARY;
		PathNode* last = middle->add();

		auto updated = middle->update(); // deletes middle
		BOOST_CHECK(updated == last);

		BOOST_CHECK(node.next == last);
		BOOST_CHECK(last->prev == &node);

		BOOST_TEST(node.depth + 1 == last->depth);
	}

	{ // Updates temporally last
		PathNode node(pool);
		PathNode* middle = node.add();
		PathNode* last = middle->add();
		last->state = PathNodeState::TEMPORARY;

		auto updated = last->update();
		BOOST_CHECK(updated == middle);
		BOOST_CHECK(middle->next == nullptr);
	}

	{ // updateLaxPatterns
		PathNode node(pool);
		node.ranges.add().init(0, 3);
		node.ranges.add().init(-1, 3);
		node.ranges.add().init(5, 10);
		node.updateLaxPatterns();
		BOOST_CHECK(node.matchWrapPattern());
		node.ranges.clear();

		node.ranges.add().init(-1, 3);
		node.ranges.add().init(0, -3);
		node.updateLaxPatterns();
		BOOST_CHECK(node.matchWrapPattern());
		node.ranges.clear();

		node.ranges.add().init(-1, 3);
		node.ranges.add().init(0, -3);
		node.ranges.add().init(5, 10);
		node.updateLaxPatterns();
		BOOST_CHECK(node.matchWrapPattern());
		node.ranges.clear();
	}

	{ // insertUnwrapNode last
		PathNode node(pool);
		node.filterNode = makeDummyNode();
		PathNode* next = node.insertUnwrapNode();

		BOOST_CHECK(next->state == FBJSON::PathNodeState::TEMPORARY);
		BOOST_CHECK(next->filterNode.hasData());
		BOOST_CHECK(next->prev == &node);
		BOOST_CHECK(next == node.next);
		BOOST_CHECK(!next->matched);
		BOOST_REQUIRE(next->ranges.hasData());
		BOOST_CHECK(next->ranges[0].canWrap());
	}

	{ // insertUnwrapNode middle
		PathNode node(pool);
		PathNode* middle = node.add();
		PathNode* last = middle->add();

		middle->filterNode = makeDummyNode();
		PathNode* temp = middle->insertUnwrapNode();

		BOOST_CHECK(temp->state == FBJSON::PathNodeState::TEMPORARY);
		BOOST_CHECK(temp->filterNode.hasData());
		BOOST_CHECK(temp->prev == middle);
		BOOST_CHECK(middle->next == temp);
		BOOST_CHECK(temp->next == last);
		BOOST_CHECK(last->prev == temp);
	}

	{ // insert
		PathNode node(pool);
		PathNode* next = node.insert();

		BOOST_CHECK(next->state == FBJSON::PathNodeState::NORMAL);
		BOOST_CHECK(next->prev == &node);
		BOOST_CHECK(next == node.next);
	}

	{ // insert
		PathNode node(pool);
		PathNode* middle = node.add();
		PathNode* last = middle->add();
		PathNode* temp = middle->insert();

		BOOST_CHECK(temp->state == FBJSON::PathNodeState::NORMAL);
		BOOST_CHECK(temp->prev == middle);
		BOOST_CHECK(middle->next == temp);
		BOOST_CHECK(temp->next == last);
		BOOST_CHECK(last->prev == temp);
	}


	{ // removeThis
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		PathNode* middle = first->add();
		PathNode* last = middle->add();

		first.release()->removeThis();

		BOOST_CHECK(middle->prev == nullptr);
		BOOST_TEST(middle->depth == 0);
		BOOST_TEST(last->depth == 1);
	}

	{ // removeThis
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		PathNode* middle = first->add();
		PathNode* last = middle->add();

		middle->removeThis();

		BOOST_CHECK(first->next == last);
		BOOST_CHECK(last->prev == first);
		BOOST_TEST(first->depth + 1 == last->depth);
	}

	{ // removeThis
		Firebird::AutoPtr<PathNode>  first = new PathNode(pool);
		PathNode* middle = first->add();
		PathNode* last = middle->add();

		last->removeThis();

		BOOST_CHECK(middle->next == nullptr);
	}


	{ // removeThis with temporally
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		PathNode* last = first->add();
		auto* middle = first->insertUnwrapNode();
		middle->filterNode = makeDummyNode();

		middle->removeThis();

		BOOST_CHECK(first->filterNode.hasData());
	}


	{ // moveTo
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->matched = true;
		PathNode* last = middle->add();
		last->matched = true;

		BOOST_CHECK(last->moveTo(last->depth) == last);
		BOOST_CHECK(last->matched == true);
		BOOST_CHECK(last->moveTo(middle->depth) == middle);
		BOOST_CHECK(last->matched == false);
		BOOST_CHECK(middle->matched == false);

		last->matched = true;
		middle->matched = true;
		BOOST_CHECK(last->moveTo(first->depth) == first);
		BOOST_CHECK(first->matched == false);

		first->state = FBJSON::PathNodeState::TEMPORARY;
		last->matched = true;
		middle->matched = true;
		BOOST_CHECK(last->moveTo(first->depth) == first);
		BOOST_CHECK(first->matched == false);
	}

	{ // moveTo
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->matched = true;
		PathNode* last = middle->add();
		last->disable();

		BOOST_CHECK(last->moveTo(last->depth) == middle);
	}


	{ // moveTo
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->matched = true;
		PathNode* last = middle->add();
		last->disable();

		BOOST_CHECK(first->moveTo(middle->depth) == middle);
		BOOST_CHECK(first->moveTo(middle->depth + 1) == nullptr);

		last->disable();
		BOOST_CHECK(first->moveTo(last->depth) == nullptr);

		last->enable();
		middle->disable();
		BOOST_CHECK(first->moveTo(middle->depth) == last);
	}

	{ // moveTo
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->matched = true;
		PathNode* last = middle->add();

		BOOST_CHECK(first->moveTo(10) == nullptr);
	}


	{ // moveTo
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->matched = true;
		PathNode* last = middle->add();

		BOOST_CHECK(first->moveTo(middle->depth) == middle);
		BOOST_CHECK(first->moveTo(last->depth) == last);

		last->disable();
		BOOST_CHECK(first->moveTo(last->depth) == nullptr);

		last->enable();
		middle->disable();
		BOOST_CHECK(first->moveTo(middle->depth) == last);
	}


	{ // getRangeIndex
		PathNode node(pool);
		node.ranges.add().init(0,2);

		USHORT indexInRange = 0;
		BOOST_CHECK(node.getRangeIndex(5, 0, indexInRange) == true);
		BOOST_CHECK(indexInRange == 0);

		indexInRange++;
		BOOST_CHECK(node.getRangeIndex(5, 0, indexInRange) == false);
	}


	{ // getRangeIndex
		PathNode node(pool);
		node.ranges.add().init(0,2);
		node.ranges.add().init(0,5);
		node.ranges.add().init(3,5);

		USHORT indexInRange = 0;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == true);
		BOOST_TEST(indexInRange == 1);

		indexInRange++;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == true);
		BOOST_TEST(indexInRange == 2);

		indexInRange++;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == false);
	}


	{ // getRangeIndex
		PathNode node(pool);
		node.ranges.add().init(0,2);
		node.ranges.add().init(0,5);
		node.ranges.add().init(3,5);

		USHORT indexInRange = 0;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == true);
		BOOST_CHECK(indexInRange == 1);

		indexInRange++;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == true);
		BOOST_CHECK(indexInRange == 2);

		indexInRange++;
		BOOST_CHECK(node.getRangeIndex(5, 4, indexInRange) == false);
	}


	{ // matchFieldName
		PathNode node(pool);
		node.field = "hello";

		BOOST_CHECK(node.matchFieldName("") == false);
		BOOST_CHECK(node.matchFieldName("HELLO") == false);
		BOOST_CHECK(node.matchFieldName("*") == false);
		BOOST_CHECK(node.matchFieldName("hello") == true);
	}

	{ // matchFieldName
		PathNode node(pool);
		node.field = "*";

		BOOST_CHECK(node.matchFieldName("") == false);
		BOOST_CHECK(node.matchFieldName("HELLO") == true);
		BOOST_CHECK(node.matchFieldName("*") == true);
		BOOST_CHECK(node.matchFieldName("hello") == true);
	}

	{ // isInSimpleRange
		PathNode node(pool);
		BOOST_CHECK(node.isInSimpleRange(0) == false);
		node.ranges.add().init(1,2);
		node.ranges.add().init(1,5);
		node.ranges.add().init(3,10);

		BOOST_CHECK(node.isInSimpleRange(1) == true);
		BOOST_CHECK(node.isInSimpleRange(5) == true);
		BOOST_CHECK(node.isInSimpleRange(0) == false);
		BOOST_CHECK(node.isInSimpleRange(6) == true);
		BOOST_CHECK(node.isInSimpleRange(10) == true);
		BOOST_CHECK(node.isInSimpleRange(11) == false);
	}

	{ // validateRange
		PathNode node(pool);
		BOOST_CHECK_NO_THROW(node.validateRange(0));
		node.ranges.add().init(1,5);
		node.ranges.add().init(-5,-1);

		BOOST_CHECK_THROW(node.validateRange(0), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(2), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(5), json_strict_exception);
		BOOST_CHECK_NO_THROW(node.validateRange(6));

		node.ranges.clear();
		node.ranges.add().init(-1,-2); // invalid range
		BOOST_CHECK_THROW(node.validateRange(0), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(1), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(2), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(3), json_strict_exception);


		node.ranges.clear();
		node.ranges.add().init(-2,-1); // invalid range
		BOOST_CHECK_THROW(node.validateRange(0), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(1), json_strict_exception);
		BOOST_CHECK_NO_THROW(node.validateRange(2));
		BOOST_CHECK_NO_THROW(node.validateRange(3));

		node.ranges.clear();
		node.ranges.add().init(3,1); // invalid range
		BOOST_CHECK_THROW(node.validateRange(0), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(1), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(2), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(3), json_strict_exception);
		BOOST_CHECK_THROW(node.validateRange(4), json_strict_exception);
	}

	{ // isNextEmpty
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		BOOST_CHECK(first->isNextEmpty()); // the next is missing

		PathNode* middle = first->add();
		BOOST_CHECK(!first->isNextEmpty()); // the next is `middle`

		middle->disable();
		BOOST_CHECK(first->isNextEmpty()); // the next is missing (disabled)

		PathNode* last = middle->add();
		BOOST_CHECK(!first->isNextEmpty()); // the next is `last`

		BOOST_CHECK(last->isNextEmpty());
	}

	{ // equals
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		PathNode* middle = first->add();

		JsonLevelNode jsonNode(pool);

		BOOST_CHECK(!first->equalsNoComplex(&jsonNode));
		BOOST_CHECK(first->matched == false);

		BOOST_CHECK(!middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == false);

		first->matched = true;
		BOOST_CHECK(!middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == false);

		jsonNode.depth = middle->depth;
		jsonNode.itemType = ItemType::FIELD;
		jsonNode.field = "hello";
		middle->type = ItemType::FIELD;
		middle->field = "123";
		BOOST_CHECK(!middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == false);

		middle->field = "hello";
		BOOST_CHECK(middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == true);

		PathNode* last = middle->add();
		BOOST_CHECK(!middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == true);

		last->disable();
		BOOST_CHECK(middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == true);

		jsonNode.depth++;
		BOOST_CHECK(!middle->equalsNoComplex(&jsonNode));
		BOOST_CHECK(middle->matched == true);
	}

	{ // equals field with unwrap
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();
		middle->type = ItemType::FIELD;

		JsonLevelNode jsonNode(pool);
		jsonNode.depth = middle->depth;
		jsonNode.indexInArray = 2;
		jsonNode.itemType = ItemType::ARRAY_ELEMENT;

		BOOST_CHECK(middle->equalsNoComplex(&jsonNode) == false);
		BOOST_CHECK(!middle->matched);

		first->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(middle->equalsNoComplex(&jsonNode) == false);
		BOOST_CHECK(!middle->matched);
		BOOST_CHECK(first->next != middle);
		BOOST_CHECK(first->next->next == middle);
		BOOST_CHECK(first->next->state == PathNodeState::TEMPORARY);
		BOOST_CHECK(first->next->matched);
	}

	{ // equals array
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		first->matched = true;
		PathNode* middle = first->add();

		JsonLevelNode jsonNode(pool);
		jsonNode.depth = middle->depth;
		jsonNode.indexInArray = 2;
		jsonNode.itemType = ItemType::ARRAY_ELEMENT;

		middle->ranges.add().init(0, 1);
		middle->type = ItemType::ARRAY_ELEMENT;
		middle->equalsNoComplex(&jsonNode);
		BOOST_CHECK(!middle->matched);

		middle->ranges.add().init(1, 2);
		middle->equalsNoComplex(&jsonNode);
		BOOST_CHECK(middle->matched);

		jsonNode.itemType = ItemType::FIELD;
		middle->equalsNoComplex(&jsonNode);
		BOOST_CHECK(!middle->matched);

		// $[0].a
		// {"a":42}
		jsonNode.itemType = ItemType::FIELD;
		jsonNode.field = "123";

		middle->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(middle->equalsNoComplex(&jsonNode) == false); // not working without a next node
		BOOST_CHECK(!middle->matched);

		auto last = middle->add();
		last->field = "123";
		last->type = ItemType::FIELD;
		jsonNode.depth = 1;
		middle->matched = false;
		last->matched = false;

		BOOST_CHECK(middle->equalsNoComplex(&jsonNode) == false); // Check the wrapping level
		BOOST_CHECK(middle->matched);
		BOOST_CHECK(last->matched);

		last->add();
		middle->matched = false;
		last->matched = false;
		BOOST_CHECK(middle->equalsNoComplex(&jsonNode) == false); // last->next is not empty
		BOOST_CHECK(middle->matched);
		BOOST_CHECK(last->matched);
	}

	{
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);

		JsonLevelNode jsonNode(pool);

		first->depth = 0;
		jsonNode.depth = 0;
		BOOST_CHECK(first->equalsZeroDepth(&jsonNode));

		first->depth = 0;
		jsonNode.depth = 1;
		BOOST_CHECK(!first->equalsZeroDepth(&jsonNode));

		first->depth = 1;
		jsonNode.depth = 0;
		BOOST_CHECK(!first->equalsZeroDepth(&jsonNode));

		first->depth = 1;
		jsonNode.depth = 1;
		BOOST_CHECK(!first->equalsZeroDepth(&jsonNode));

	}

	{ // canWrap: Example: for {"field":42}, we can convert $[*].field to $.field
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);

		JsonLevelNode jsonNode(pool);
		jsonNode.indexInArray = 2;
		jsonNode.itemType = ItemType::ARRAY_ELEMENT;

		first->matched = false;
		jsonNode.depth = 2;
		first->flags = 0;
		BOOST_CHECK(!first->canWrap(&jsonNode));

		first->matched = true;
		jsonNode.depth = 2;
		first->flags = 0;
		BOOST_CHECK(!first->canWrap(&jsonNode));

		first->matched = true;
		jsonNode.depth = 0;
		first->flags = 0;
		BOOST_CHECK(!first->canWrap(&jsonNode));

		first->matched = true;
		jsonNode.depth = 0;
		first->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(!first->canWrap(&jsonNode));

		first->add();
		BOOST_CHECK(!first->canWrap(&jsonNode));
		first->next->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(first->canWrap(&jsonNode));

		first->matched = false;
		jsonNode.depth = 0;
		first->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(!first->canWrap(&jsonNode));

		first->matched = true;
		jsonNode.depth = 2;
		first->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(!first->canWrap(&jsonNode));
	}

	{ // canWrapEnd: Example: for '{"data":1}' we can convert '$.data[0].a to $.data
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);

		JsonLevelNode jsonNode(pool);
		jsonNode.indexInArray = 2;
		jsonNode.itemType = ItemType::ARRAY_ELEMENT;

		first->matched = true;
		jsonNode.depth = 0;

		auto next = first->add();
		next->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(first->canWrap(&jsonNode)); // Walidate wrap is valid
		BOOST_CHECK(first->canWrapEnd(&jsonNode));
		next->add();
		BOOST_CHECK(!first->canWrapEnd(&jsonNode));
	}


	{ // canUnwrapEnd
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		BOOST_CHECK(!first->canUnwrapEnd());

		first->matched = false;
		first->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(!first->canUnwrapEnd());

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(first->canUnwrapEnd());

		first->matched = true;
		first->flags = 0;
		BOOST_CHECK(!first->canUnwrapEnd());

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		first->add();
		BOOST_CHECK(!first->canUnwrapEnd());
	}


	{ // canUnwrapMiddle: сan insert [*] to path
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);

		JsonLevelNode jsonNode(pool);
		jsonNode.indexInArray = 2;

		first->matched = false;
		first->flags = 0;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags = 0;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->add(); // next

		first->matched = false;
		first->flags = 0;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags = 0;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = false;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_OBJECT;
		BOOST_CHECK(!first->canUnwrapMiddle(&jsonNode));

		first->matched = true;
		first->flags |= PathNode::FLAG_UNWRAP;
		jsonNode.type = FBJSON::JT_ARRAY;
		BOOST_CHECK(first->canUnwrapMiddle(&jsonNode));
	}

	{ // canUnwrapEnd
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);

		BOOST_CHECK(!first->matchWrapPattern());
		BOOST_CHECK(!first->matchUnwrapPattern());

		first->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(!first->matchWrapPattern());
		BOOST_CHECK(first->matchUnwrapPattern());

		first->flags = 0;
		first->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(first->matchWrapPattern());
		BOOST_CHECK(!first->matchUnwrapPattern());

		first->flags |= PathNode::FLAG_WRAP;
		first->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(first->matchWrapPattern());
		BOOST_CHECK(first->matchUnwrapPattern());
	}

	{ // getEnabledPrev
		Firebird::AutoPtr<PathNode> first = new PathNode(pool);
		PathNode* middle = first->add();
		PathNode* last = middle->add();

		BOOST_CHECK(first->getEnabledPrev() == nullptr);
		BOOST_CHECK(middle->getEnabledPrev() == first);
		BOOST_CHECK(last->getEnabledPrev() == middle);

		middle->disable();
		BOOST_CHECK(last->getEnabledPrev() == first);

		first->disable();
		BOOST_CHECK(last->getEnabledPrev() == nullptr);
	}

	{ // isEmpty
		PathNode node(pool);
		BOOST_CHECK(node.isEmpty(0));
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_UNWRAP));
		node.add();
		BOOST_CHECK(node.isEmpty(0) == false);
	}

	{ // isEmpty
		PathNode node(pool);
		node.flags |= (PathNode::FLAG_UNWRAP | PathNode::FLAG_WRAP);
		BOOST_CHECK(node.isEmpty(0));
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_UNWRAP) == false);
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_WRAP) == false);
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_UNWRAP | PathNode::FLAG_WRAP) == false);
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_PROCESSED) == true);
	}

	{ // isEmpty
		PathNode node(pool);
		node.filterNode = makeDummyNode();
		BOOST_CHECK(node.isEmpty(PathNode::FLAG_UNWRAP) == false);
	}
}


BOOST_AUTO_TEST_SUITE_END()	// PathNodeTests


BOOST_AUTO_TEST_SUITE(JsonPathsTests)

BOOST_AUTO_TEST_CASE(JsonPathTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	{
		JsonPath path(pool, false);
		BOOST_CHECK(path.getRootNode() == nullptr);
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(path.getRootNode() != nullptr);
		BOOST_CHECK(path.isZeroPath());
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(!path.hasFields());
		path.setFieldsFlag();
		BOOST_CHECK(path.hasFields());
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(!path.hasIndexes());
		path.setIndexesFlag();
		BOOST_CHECK(path.hasIndexes());
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(!path.hasComplexRange());
		path.setComplexRangeFlag();
		BOOST_CHECK(path.hasComplexRange());
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(path.isLax());
		BOOST_CHECK(!path.isStrict());
		path.setStrictFlag();
		BOOST_CHECK(!path.isLax());
		BOOST_CHECK(path.isStrict());
	}

	{
		JsonPath path(pool, true);
		BOOST_CHECK(!path.isWrappingZeroPath());
		auto* second = path.getRootNode()->add();
		BOOST_CHECK(!path.isWrappingZeroPath());
		second->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(path.isWrappingZeroPath());

		second->add();
		BOOST_CHECK(!path.isWrappingZeroPath());
	}

	{
		JsonPath path(pool, false);
		BOOST_CHECK(path.isEmpty(0));
		auto node = path.resetRootNode();
		BOOST_CHECK(path.isEmpty(0));

		node->flags |= PathNode::FLAG_WRAP;
		BOOST_CHECK(path.isEmpty(0));
		BOOST_CHECK(!path.isEmpty(PathNode::FLAG_WRAP));

		node->add();
		BOOST_CHECK(!path.isEmpty(0));
	}

	{
		BOOST_CHECK(!JsonPath::isUnwrapMethod(PathMethod::TYPE));
		BOOST_CHECK(!JsonPath::isUnwrapMethod(PathMethod::NONE));
		BOOST_CHECK(!JsonPath::isUnwrapMethod(PathMethod::SIZE));
		for (ULONG i = ULONG(PathMethod::SIZE) + 1; i < ULONG(PathMethod::TIMESTAMP_TZ); ++i)
		{
			BOOST_CHECK(JsonPath::isUnwrapMethod(PathMethod(i)));
		}
	}
}

BOOST_AUTO_TEST_CASE(JsonPathExprTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	JsonPathExpr expr(pool);
	BOOST_CHECK(expr.getJsonPath() != nullptr);

	auto a = JsonExprNode::make<FBJSON::JsonExprNode::SCALAR_NODE>(pool);
	auto b = JsonExprNode::make<FBJSON::JsonExprNode::SCALAR_NODE>(pool);
	expr.resetExpr(a, b);

	BOOST_CHECK(expr.getTail() == a);
	BOOST_CHECK(expr.getMath() == b);

	// JsonPathExpr PathVariable

	a = new JsonExprNode(pool, new PathVariable(pool, PathVariable::Type::ROOT));
	b = new JsonExprNode(pool, new PathVariable(pool, PathVariable::Type::ROOT));
	expr.resetExpr(a, b);
	BOOST_CHECK(expr.getTail() == nullptr);  // Empty because ROOT
	BOOST_CHECK(expr.getMath() == nullptr);

	a = new JsonExprNode(pool, new PathVariable(pool, PathVariable::Type::ITEM));
	b = new JsonExprNode(pool, new PathVariable(pool, PathVariable::Type::ITEM));
	expr.resetExpr(a, b);
	BOOST_CHECK(expr.getTail() == nullptr); // Empty because isRootEmpty
	BOOST_CHECK(expr.getMath() != nullptr);

	b = new JsonExprNode(pool, new PathVariable(pool, PathVariable::Type::ITEM));
	expr.resetExpr(nullptr, b);
	BOOST_CHECK(expr.getTail() == nullptr); // Empty because isRootEmpty
	BOOST_CHECK(expr.getMath() != nullptr);
}

BOOST_AUTO_TEST_CASE(PathVariableTest)
{
	MemoryPool& pool = *getDefaultMemoryPool();

	{
		PathVariable var(pool);
		BOOST_CHECK(!var.hasPath(0));

		var.path = new JsonPath(pool, true);
		BOOST_CHECK(!var.hasPath(0));

		var.path->getRootNode()->flags |= PathNode::FLAG_UNWRAP;
		BOOST_CHECK(var.hasPath(PathNode::FLAG_UNWRAP));
		BOOST_CHECK(!var.hasPath(0));
	}

	{
		PathVariable var(pool, PathVariable::Type::JSON);
		BOOST_CHECK(!var.isItem());
		BOOST_CHECK(!var.isPassing());
	}

	{
		PathVariable var(pool, PathVariable::Type::ITEM);
		BOOST_CHECK(var.isItem());
		BOOST_CHECK(!var.isPassing());
	}

	{
		PathVariable var(pool, PathVariable::Type::PASSING);
		BOOST_CHECK(!var.isItem());
		BOOST_CHECK(var.isPassing());
	}

	{
		PathVariable var(pool, PathVariable::Type::ROOT);
		BOOST_CHECK(!var.isItem());
		BOOST_CHECK(!var.isPassing());
	}

	{
		PathVariable var(pool, PathVariable::Type::HEAD);
		BOOST_CHECK(!var.isItem());
		BOOST_CHECK(!var.isPassing());
	}
}

BOOST_AUTO_TEST_SUITE_END()	// JsonPathClassTests

BOOST_AUTO_TEST_SUITE_END()	// JsonClassesTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
