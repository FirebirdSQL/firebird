#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/DoublyLinkedList.h"
#include <iterator>
#include <numeric>
#include <string>

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(DoublyLinkedListSuite)


BOOST_AUTO_TEST_SUITE(DoublyLinkedListTests)

BOOST_AUTO_TEST_CASE(ConstructionWithStdInitializerTest)
{
	DoublyLinkedList<int> list(*getDefaultMemoryPool(), {1, 2, 3, 4});

	BOOST_TEST(list.getCount() == 4);
	BOOST_TEST(list.front() == 1);
	BOOST_TEST(list.back() == 4);
}

BOOST_AUTO_TEST_CASE(ClearTest)
{
	DoublyLinkedList<int> list(*getDefaultMemoryPool(), {1, 2, 3, 4});

	BOOST_TEST(list.getCount() == 4);

	list.clear();
	BOOST_TEST(list.getCount() == 0);
}

BOOST_AUTO_TEST_CASE(SpliceTest)
{
	DoublyLinkedList<int> list1(*getDefaultMemoryPool());
	DoublyLinkedList<int> list2(*getDefaultMemoryPool());

	for (int i = 1; i <= 5; ++i)
	{
		list1.pushBack(i);
		list2.pushBack(i * 10);
	}

	const auto join = [](const DoublyLinkedList<int>& list) {
		if (list.isEmpty())
			return std::string();

		return std::accumulate(std::next(list.begin()), list.end(), std::to_string(list.front()), [](auto str, auto n) {
			return std::move(str) + ',' + std::to_string(n);
		});
	};

	list1.splice(std::next(list1.begin()), list2, list2.cbegin());
	BOOST_TEST(join(list1) == "1,10,2,3,4,5");
	BOOST_TEST(join(list2) == "20,30,40,50");

	list2.splice(std::next(list2.begin()), list1);
	BOOST_TEST(list1.isEmpty());
	BOOST_TEST(join(list2) == "20,1,10,2,3,4,5,30,40,50");

	list1.splice(list1.end(), list2, std::next(list2.cbegin()), std::prev(list2.cend()));
	BOOST_TEST(join(list1) == "1,10,2,3,4,5,30,40");
	BOOST_TEST(join(list2) == "20,50");
}

BOOST_AUTO_TEST_SUITE_END()	// DoublyLinkedListTests


BOOST_AUTO_TEST_SUITE_END()	// DoublyLinkedListSuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
