#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../common/classes/alloc.h"
#include "../common/classes/tree.h"
#include "../common/classes/sparse_bitmap.h"

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(SparseBitmapSuite)


template<typename T>
void initSparseBitmap(SparseBitmap<T>* bitmap, const std::vector<T>& values)
{
	for(auto v: values)
		bitmap->set(v);
}

template<typename T>
void testSparseBitmapValues(SparseBitmap<T>* bitmap, const std::vector<T>& values)
{
	for(auto v: values)
		BOOST_TEST(bitmap->test(v));

	size_t setCount = 0;
	for (size_t i = 0; i <= 10100; i++)
		if (bitmap->test(i))
			setCount++;

	BOOST_TEST(setCount == values.size());
}

template<typename T>
using SparseBitmapOp = std::function<SparseBitmap<T>** (SparseBitmap<T>**, SparseBitmap<T>**)>;

template<typename T>
void testSparseBitmap(std::initializer_list<T> leftValues, std::initializer_list<T> rightValues,
					  SparseBitmapOp<T> op, std::initializer_list<T> expectedValues)
{
	SparseBitmap<ULONG> A(*getDefaultMemoryPool());
	SparseBitmap<ULONG> B(*getDefaultMemoryPool());

	initSparseBitmap(&A, std::vector(leftValues));
	initSparseBitmap(&B, std::vector(rightValues));

	auto bitmapA = &A;
	auto bitmapB = &B;

	auto C = op(&bitmapA, &bitmapB);

	if (C)
		testSparseBitmapValues(*C, std::vector(expectedValues));
	else
		BOOST_TEST(expectedValues.size() == 0);
}


BOOST_AUTO_TEST_SUITE(SparseBitmapTests)

BOOST_AUTO_TEST_CASE(ConstructionWithMemoryPool)
{
	SparseBitmap<ULONG> bitmap(*getDefaultMemoryPool());
	initSparseBitmap(&bitmap, {0, 100, 10000});
	testSparseBitmapValues(&bitmap, {0, 100, 10000});
}

BOOST_AUTO_TEST_CASE(bitOr)
{
	SparseBitmapOp<ULONG> opOr = [](SparseBitmap<ULONG>** left, SparseBitmap<ULONG>** right)
						{ return SparseBitmap<ULONG>::bit_or(left, right); };

	testSparseBitmap<ULONG>({1, 100, 10000}, {10, 100, 1000}, opOr, {1, 10, 100, 1000, 10000});
}

BOOST_AUTO_TEST_CASE(bitAnd)
{
	SparseBitmapOp<ULONG> opAnd = [](SparseBitmap<ULONG>** left, SparseBitmap<ULONG>** right)
						{ return SparseBitmap<ULONG>::bit_and(left, right); };

	testSparseBitmap<ULONG>({1, 100, 10000}, {10, 100, 1000}, opAnd, {100});
}

BOOST_AUTO_TEST_CASE(bitSubtract)
{
	SparseBitmapOp<ULONG> opSubtract = [](SparseBitmap<ULONG>** left, SparseBitmap<ULONG>** right)
						{ return SparseBitmap<ULONG>::bit_subtract(left, right); };

	testSparseBitmap<ULONG>({1, 100, 101, 10000}, {10, 100, 1000}, opSubtract, {1, 101, 10000});
	testSparseBitmap<ULONG>({1, 100, 101, 10000}, {10}, opSubtract, {1, 100, 101, 10000});
	testSparseBitmap<ULONG>({1, 100, 101, 10000}, {100}, opSubtract, {1, 101, 10000});
	testSparseBitmap<ULONG>({1, 100, 101, 10000}, {}, opSubtract, {1, 100, 101, 10000});
	testSparseBitmap<ULONG>({1}, {10}, opSubtract, {1});
	testSparseBitmap<ULONG>({1}, {1}, opSubtract, {});
	testSparseBitmap<ULONG>({1}, {1, 10}, opSubtract, {});
	testSparseBitmap<ULONG>({}, {}, opSubtract, {});
	testSparseBitmap<ULONG>({}, {1, 10, 100}, opSubtract, {});
}


BOOST_AUTO_TEST_SUITE_END()	// SortedArrayTests


BOOST_AUTO_TEST_SUITE_END()	// ArraySuite
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
