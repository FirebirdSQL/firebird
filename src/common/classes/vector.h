/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		vector.h
 *	DESCRIPTION:	static array of simple elements
 *
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 *
 */

#ifndef VECTOR_H
#define VECTOR_H

#include "../common/gdsassert.h"
#include <string.h>

namespace Firebird {

// Very fast static array of simple types
template <typename T, FB_SIZE_T Capacity, typename A = char>
class Vector
{
public:
	Vector() : count(0) {}

	T& operator[](FB_SIZE_T index) noexcept
	{
  		fb_assert(index < count);
  		return data[index];
	}

	const T& operator[](FB_SIZE_T index) const noexcept
	{
  		fb_assert(index < count);
  		return data[index];
	}

	T* begin() noexcept { return data; }
	T* end() noexcept { return data + count; }
	const T* begin() const noexcept { return data; }
	const T* end() const noexcept { return data + count; }
	bool hasData() const noexcept { return (count != 0); }
	FB_SIZE_T getCount() const noexcept { return count; }
	FB_SIZE_T getCapacity() const noexcept { return Capacity; }
	void clear() noexcept { count = 0; }

	void insert(FB_SIZE_T index, const T& item)
	{
	  fb_assert(index <= count);
	  fb_assert(count < Capacity);
	  memmove(data + index + 1, data + index, sizeof(T) * (count++ - index));
	  data[index] = item;
	}

	FB_SIZE_T add(const T& item)
	{
		fb_assert(count < Capacity);
		data[count] = item;
  		return ++count;
	}

	T* remove(FB_SIZE_T index) noexcept
	{
  		fb_assert(index < count);
  		memmove(data + index, data + index + 1, sizeof(T) * (--count - index));
		return &data[index];
	}

	T* removeCount(const FB_SIZE_T index, const FB_SIZE_T n) noexcept
	{
  		fb_assert(index + n <= count);
  		memmove(data + index, data + index + n, sizeof(T) * (count - index - n));
		count -= n;
		return &data[index];
	}

	void shrink(FB_SIZE_T newCount) noexcept
	{
		fb_assert(newCount <= count);
		count = newCount;
	}

	void join(const Vector<T, Capacity>& L) noexcept
	{
		fb_assert(count + L.count <= Capacity);
		memcpy(data + count, L.data, sizeof(T) * L.count);
		count += L.count;
	}

	// prepare vector to be used as a buffer of capacity items
	T* getBuffer(FB_SIZE_T capacityL) noexcept
	{
		fb_assert(capacityL <= Capacity);
		count = capacityL;
		return data;
	}

	void push(const T& item)
	{
		add(item);
	}

	T pop()
	{
		fb_assert(count > 0);
		count--;
		return data[count];
	}

	void push(const T* items, const FB_SIZE_T itemsCount)
	{
		fb_assert(count <= FB_MAX_SIZEOF - itemsCount);
		fb_assert(count + itemsCount <= Capacity);
		memcpy(data + count, items, sizeof(T) * itemsCount);
		count += itemsCount;
	}

	void append(const T* items, const FB_SIZE_T itemsCount)
	{
		push(items, itemsCount);
	}


	// This method only assigns "pos" if the element is found.
	// Maybe we should modify it to iterate directy with "pos".
	bool find(const T& item, FB_SIZE_T& pos) const
	{
		for (FB_SIZE_T i = 0; i < count; i++)
		{
			if (data[i] == item)
			{
				pos = i;
				return true;
			}
		}
		return false;
	}

protected:
	union
	{
		FB_SIZE_T count;
		A align;
	};
	// Do not insert data members between align and data:
	// alignment of data is ensured by preceding union
	T data[Capacity];
};

// Template for default value comparsion
template <typename T>
class DefaultComparator
{
public:
	static bool greaterThan(const T& i1, const T& i2)
	{
	    return i1 > i2;
	}
};

// Template to convert value to index directly
template <typename T>
class DefaultKeyValue
{
public:
	static const T& generate(const void* /*sender*/, const T& item) { return item; }
	static const T& generate(const T& item) { return item; }
};

// Fast sorted array of simple objects
// It is used for B+ tree nodes lower, but can still be used by itself
template <typename Value, FB_SIZE_T Capacity, typename Key = Value,
	typename KeyOfValue = DefaultKeyValue<Value>,
	typename Cmp = DefaultComparator<Key> >
class SortedVector : public Vector<Value, Capacity>
{
public:
	SortedVector() : Vector<Value, Capacity>() {}
	bool find(const Key& item, FB_SIZE_T& pos) const
	{
		FB_SIZE_T highBound = this->count, lowBound = 0;
		while (highBound > lowBound)
		{
			const FB_SIZE_T temp = (highBound + lowBound) >> 1;
			if (Cmp::greaterThan(item, KeyOfValue::generate(this, this->data[temp])))
				lowBound = temp + 1;
			else
				highBound = temp;
		}
		pos = lowBound;
		return highBound != this->count &&
			!Cmp::greaterThan(KeyOfValue::generate(this, this->data[lowBound]), item);
	}
	FB_SIZE_T add(const Value& item)
	{
	    FB_SIZE_T pos;
  	    find(KeyOfValue::generate(this, item), pos);
		this->insert(pos, item);
		return pos;
	}
};


// Templates to allow to iterate thru array\vector of values and process items
// in some way. Processed items are marked and skipped at next iteration circle.
// Idea is to not remove processed items from array and avoid costly memory
// moving. Also, iterator is able to move upper and lower bounds of array if
// last (or first) items are marked, making next iterations more efficient.

template <typename T>
class DefaultMarkValue
{
public:
	static T getMarkValue()
	{
		return T(0);
	}
};

template <typename T, typename MarkValue = DefaultMarkValue<T> >
class DefaultMarker
{
public:
	static void mark(T* const item)
	{
		*item = MarkValue::getMarkValue();
	}

	static bool isMarked(const T* const item)
	{
		return *item == MarkValue::getMarkValue();
	}
};

template <typename T, typename Marker = DefaultMarker<T> >
class MarkIterator
{
public:
	MarkIterator(T* begin, FB_SIZE_T count) :
		m_begin(begin),
		m_end(begin + count),
		m_curr(begin),
		m_last(begin)
	{
	}

	// Mark current item as processed
	void mark()
	{
		Marker::mark(m_curr);

		if (m_last == m_curr)
			m_last--;
	}

	// Move iterator position to the first not processed item
	void rewind()
	{
		m_curr = m_begin;
		m_end = m_last + 1;
		m_last = m_begin;
	}

	T& operator*() const
	{
		fb_assert(m_begin <= m_curr);
		fb_assert(m_curr < m_end);
		return *m_curr;
	}

	// Advance iterator to the next not processed item, if exist
	void operator++()
	{
		fb_assert(m_begin <= m_curr);
		fb_assert(m_curr < m_end);

		const bool at_begin = (m_begin == m_curr);
		m_curr++;

		while (m_curr < m_end && Marker::isMarked(m_curr))
			m_curr++;

		if (m_curr == m_end)
			return;

		if (at_begin)
		{
			if (Marker::isMarked(m_begin))
				m_begin = m_curr;
			else if (m_begin != m_curr - 1)
			{
				m_curr[-1] = *m_begin;
				m_begin = m_curr - 1;
			}
		}

		if (!Marker::isMarked(m_curr))
			m_last = m_curr;
	}

	// Show if current position is valid
	bool isEof() const
	{
		return m_curr >= m_end;
	}

	// Show if not processed items still exists
	bool isEmpty() const
	{
		return m_begin >= m_end;
	}

private:
	T* m_begin;
	T* m_end;
	T* m_curr;
	T* m_last;
};

} // namespace Firebird

#endif
