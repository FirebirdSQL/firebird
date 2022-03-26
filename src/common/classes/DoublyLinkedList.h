/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2022 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_DOUBLY_LINKED_LIST_H
#define CLASSES_DOUBLY_LINKED_LIST_H

#include "../common/classes/alloc.h"
#include <list>
#include <utility>


namespace Firebird
{

template <typename T>
class DoublyLinkedList
{
private:
	using StdList = std::list<T, PoolAllocator<T>>;

public:
	using Iterator = typename StdList::iterator;
	using ConstIterator = typename StdList::const_iterator;

public:
	explicit DoublyLinkedList(MemoryPool& p)
		: stdList(p)
	{
	}

public:
	constexpr T& front() noexcept
	{
		return stdList.front();
	}

	constexpr const T& front() const noexcept
	{
		return stdList.front();
	}

	constexpr T& back() noexcept
	{
		return stdList.back();
	}

	constexpr const T& back() const noexcept
	{
		return stdList.back();
	}

	constexpr Iterator begin() noexcept
	{
		return stdList.begin();
	}

	constexpr ConstIterator begin() const noexcept
	{
		return stdList.begin();
	}

	constexpr ConstIterator cbegin() const noexcept
	{
		return stdList.cbegin();
	}

	constexpr Iterator end() noexcept
	{
		return stdList.end();
	}

	constexpr ConstIterator end() const noexcept
	{
		return stdList.end();
	}

	constexpr ConstIterator cend() const noexcept
	{
		return stdList.cend();
	}

	constexpr bool isEmpty() const noexcept
	{
		return stdList.empty();
	}

	constexpr void clear() noexcept
	{
		stdList.clear();
	}

	constexpr void erase(Iterator pos)
	{
		stdList.erase(pos);
	}

	constexpr void erase(ConstIterator pos)
	{
		stdList.erase(pos);
	}

	constexpr void pushBack(const T& value)
	{
		stdList.push_back(value);
	}

	constexpr void pushBack(T&& value)
	{
		stdList.push_back(std::move(value));
	}

	constexpr void splice(ConstIterator pos, DoublyLinkedList<T>& other, ConstIterator it)
	{
		fb_assert(stdList.get_allocator() == other.stdList.get_allocator());
		stdList.splice(pos, other.stdList, it);
	}

	constexpr void splice(ConstIterator pos, DoublyLinkedList<T>&& other, ConstIterator it)
	{
		fb_assert(stdList.get_allocator() == other.stdList.get_allocator());
		stdList.splice(pos, std::move(other.stdList), it);
	}

private:
	StdList stdList;
};

}	// namespace Firebird

#endif	// CLASSES_DOUBLY_LINKED_LIST_H
