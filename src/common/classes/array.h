/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		array.h
 *	DESCRIPTION:	dynamic array of simple elements
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * Created by: Alex Peshkov <peshkoff@mail.ru>
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * Adriano dos Santos Fernandes
 */

#ifndef CLASSES_ARRAY_H
#define CLASSES_ARRAY_H

#include "../common/gdsassert.h"
#include <initializer_list>
#include <type_traits>
#include <string.h>
#include "../common/classes/vector.h"
#include "../common/classes/alloc.h"

namespace Firebird {

// Static part of the array
template <typename T, FB_SIZE_T Capacity, typename AlignT = T>
class InlineStorage : public AutoStorage
{
public:
	explicit InlineStorage(MemoryPool& p) : AutoStorage(p) { }
	InlineStorage() : AutoStorage() { }
protected:
	T* getStorage() noexcept
	{
		return buffer;
	}
	FB_SIZE_T getStorageSize() const noexcept
	{
		return Capacity;
	}
private:
	alignas(alignof(AlignT)) T buffer[Capacity];
};

// Used when array doesn't have static part
template <typename T>
class EmptyStorage : public AutoStorage
{
public:
	explicit EmptyStorage(MemoryPool& p) : AutoStorage(p) { }
	EmptyStorage() : AutoStorage() { }
protected:
	T* getStorage() noexcept { return NULL; }
	FB_SIZE_T getStorageSize() const noexcept { return 0; }
};

// Dynamic array of simple types
template <typename T, typename Storage = EmptyStorage<T> >
class Array : public Storage
{
#ifndef _MSC_VER
	static_assert(std::is_trivially_copyable<T>(), "Only simple (trivially copyable) types supported in array");
#endif

public:
	typedef FB_SIZE_T size_type;
	typedef FB_SSIZE_T difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;
	typedef pointer iterator;
	typedef const_pointer const_iterator;

	static const size_type npos = ~size_type(0);

	explicit Array(MemoryPool& p)
		: Storage(p),
		  count(0),
		  capacity(this->getStorageSize()),
		  data(this->getStorage())
	{
		// Ensure we can carry byte copy operations.
		fb_assert(capacity < FB_MAX_SIZEOF / sizeof(T));
	}

	Array(MemoryPool& p, const size_type InitialCapacity)
		: Array(p)
	{
		ensureCapacity(InitialCapacity);
	}

	Array(MemoryPool& p, const Array<T, Storage>& source)
		: Array(p)
	{
		copyFrom(source);
	}

	Array()
		: count(0),
		  capacity(this->getStorageSize()),
		  data(this->getStorage())
	{
	}

	explicit Array(const size_type InitialCapacity)
		: Storage(),
		  count(0),
		  capacity(this->getStorageSize()),
		  data(this->getStorage())
	{
		ensureCapacity(InitialCapacity);
	}

	Array(const T* items, const size_type itemsCount)
		: Storage(),
		  count(0),
		  capacity(this->getStorageSize()),
		  data(this->getStorage())
	{
		add(items, itemsCount);
	}

	Array(const Array<T, Storage>& source)
		: Storage(),
		  count(0),
		  capacity(this->getStorageSize()),
		  data(this->getStorage())
	{
		copyFrom(source);
	}

	Array(MemoryPool& p, std::initializer_list<T> items)
		: Array(p)
	{
		for (auto& item : items)
			add(item);
	}

	Array(std::initializer_list<T> items)
		: Array()
	{
		for (auto& item : items)
			add(item);
	}

	~Array()
	{
		freeData();
	}

	void clear() noexcept
	{
		count = 0;
	}

protected:
	const T& getElement(size_type index) const noexcept
	{
  		fb_assert(index < count);
  		return data[index];
	}

	T& getElement(size_type index) noexcept
	{
  		fb_assert(index < count);
  		return data[index];
	}

	void freeData() noexcept
	{
		// CVC: Warning, after this call, "data" is an invalid pointer, be sure to reassign it
		// or make it equal to this->getStorage()
		if (data != this->getStorage())
			Firebird::MemoryPool::globalFree(data);
	}

	void copyFrom(const Array<T, Storage>& source)
	{
		ensureCapacity(source.count, false);
		if (source.count)
			memcpy(static_cast<void*>(data), source.data, sizeof(T) * source.count);
		count = source.count;
	}

public:
	Array<T, Storage>& operator =(const Array<T, Storage>& source)
	{
		copyFrom(source);
		return *this;
	}

	template <typename T2, typename S2 >
	Array& operator=(const Array<T2, S2>& source)
	{
		ensureCapacity(source.getCount(), false);
		for (size_type index = 0; index < count; ++index)
			data[index] = source[index];
		return *this;
	}

	const T& operator[](size_type index) const noexcept
	{
  		return getElement(index);
	}

	T& operator[](size_type index) noexcept
	{
  		return getElement(index);
	}

	const T& front() const
	{
  		fb_assert(count > 0);
		return *data;
	}

	const T& back() const
	{
  		fb_assert(count > 0);
		return *(data + count - 1);
	}

	const T* begin() const noexcept { return data; }

	const T* end() const noexcept { return data + count; }

	T& front()
	{
  		fb_assert(count > 0);
		return *data;
	}

	T& back()
	{
  		fb_assert(count > 0);
		return *(data + count - 1);
	}

	T* begin() noexcept { return data; }

	T* end() noexcept { return data + count; }

	void insert(const size_type index, const T& item)
	{
		fb_assert(index <= count);
		fb_assert(count < FB_MAX_SIZEOF);
		ensureCapacity(count + 1);
		if (count - index)
			memmove(static_cast<void*>(data + index + 1), data + index, sizeof(T) * (count - index));
		data[index] = item;
		++count;
	}

	void insert(const size_type index, const Array<T, Storage>& items)
	{
		fb_assert(index <= count);
		fb_assert(count <= FB_MAX_SIZEOF - items.count);
		ensureCapacity(count + items.count);
		if (count - index)
			memmove(static_cast<void*>(data + index + items.count), data + index, sizeof(T) * (count - index));
		if (items.count)
			memcpy(static_cast<void*>(data + index), items.data, sizeof(T) * items.count);
		count += items.count;
	}

	void insert(const size_type index, const T* items, const size_type itemsCount)
	{
		fb_assert(index <= count);
		fb_assert(count <= FB_MAX_SIZEOF - itemsCount);
		ensureCapacity(count + itemsCount);
		if (count - index)
			memmove(static_cast<void*>(data + index + itemsCount), data + index, sizeof(T) * (count - index));
		if (itemsCount)
			memcpy(static_cast<void*>(data + index), items, sizeof(T) * itemsCount);
		count += itemsCount;
	}

	size_type add(const T& item)
	{
		ensureCapacity(count + 1);
		data[count] = item;
  		return count++;
	}

	T& add()
	{
		ensureCapacity(count + 1);
		return *new(&data[count++]) T();	// initialize new empty data member
	}

	void add(const T* items, const size_type itemsCount)
	{
		fb_assert(count <= FB_MAX_SIZEOF - itemsCount);
		ensureCapacity(count + itemsCount);
		memcpy(static_cast<void*>(data + count), items, sizeof(T) * itemsCount);
		count += itemsCount;
	}

	T* remove(const size_type index) noexcept
	{
		fb_assert(index < count);
		memmove(static_cast<void*>(data + index), data + index + 1, sizeof(T) * (--count - index));
		return &data[index];
	}

	T* removeRange(const size_type from, const size_type to) noexcept
	{
		fb_assert(from <= to);
		fb_assert(to <= count);
		memmove(static_cast<void*>(data + from), data + to, sizeof(T) * (count - to));
		count -= (to - from);
		return &data[from];
	}

	T* removeCount(const size_type index, const size_type n) noexcept
	{
		fb_assert(index + n <= count);
		memmove(static_cast<void*>(data + index), data + index + n, sizeof(T) * (count - index - n));
		count -= n;
		return &data[index];
	}

	T* remove(T* itr) noexcept
	{
		const size_type index = itr - begin();
		fb_assert(index < count);
		memmove(static_cast<void*>(data + index), data + index + 1, sizeof(T) * (--count - index));
		return &data[index];
	}

	T* remove(T* itrFrom, T* itrTo) noexcept
	{
		return removeRange(itrFrom - begin(), itrTo - begin());
	}

	void shrink(size_type newCount) noexcept
	{
		fb_assert(newCount <= count);
		count = newCount;
	}

	// Grow size of our array and zero-initialize new items
	void grow(const size_type newCount)
	{
		fb_assert(newCount >= count);
		ensureCapacity(newCount);
		memset(static_cast<void*>(data + count), 0, sizeof(T) * (newCount - count));
		count = newCount;
	}

	// Resize array according to STL's vector::resize() rules
	void resize(const size_type newCount, const T& val)
	{
		if (newCount > count)
		{
			ensureCapacity(newCount);
			while (count < newCount) {
				data[count++] = val;
			}
		}
		else {
			count = newCount;
		}
	}

	// Resize array according to STL's vector::resize() rules
	void resize(const size_type newCount)
	{
		if (newCount > count) {
			grow(newCount);
		}
		else {
			count = newCount;
		}
	}

	void join(const Array<T, Storage>& L)
	{
		fb_assert(count <= FB_MAX_SIZEOF - L.count);
		ensureCapacity(count + L.count);
		memcpy(static_cast<void*>(data + count), L.data, sizeof(T) * L.count);
		count += L.count;
	}

	void assign(const Array<T, Storage>& source)
	{
		copyFrom(source);
	}

	void assign(const T* items, const size_type itemsCount)
	{
		resize(itemsCount);
		memcpy(static_cast<void*>(data), items, sizeof(T) * count);
	}

	size_type getCount() const noexcept { return count; }

	bool isEmpty() const noexcept { return count == 0; }

	bool hasData() const noexcept { return count != 0; }

	size_type getCapacity() const noexcept { return capacity; }

	void push(const T& item)
	{
		add(item);
	}

	void push(const T* items, const size_type itemsSize)
	{
		fb_assert(count <= FB_MAX_SIZEOF - itemsSize);
		ensureCapacity(count + itemsSize);
		memcpy(static_cast<void*>(data + count), items, sizeof(T) * itemsSize);
		count += itemsSize;
	}

	void append(const T* items, const size_type itemsSize)
	{
		push(items, itemsSize);
	}

	void append(const Array<T, Storage>& source)
	{
		push(source.begin(), source.getCount());
	}

	T pop()
	{
		fb_assert(count > 0);
		count--;
		return data[count];
	}

	// prepare array to be used as a buffer of capacity items
	T* getBuffer(const size_type capacityL, bool preserve = true)
	{
		ensureCapacity(capacityL, preserve);
		count = capacityL;
		return data;
	}

	// prepare array to be used as a buffer of capacity bytes aligned on given alignment
	T* getAlignedBuffer(const size_type capacityL, const size_type alignL)
	{
		static_assert(sizeof(T) == 1, "sizeof(T) != 1");

		ensureCapacity(capacityL + alignL, false);
		count = capacityL + alignL;
		return FB_ALIGN(data, alignL);
	}

	// clear array and release dynamically allocated memory
	void free()
	{
		clear();
		freeData();
		capacity = this->getStorageSize();
		data = this->getStorage();
	}

	// This methods only assigns "pos" if the element is found.
	// Maybe we should modify it to iterate directy with "pos".
	bool find(const T& item, size_type& pos) const
	{
		for (size_type i = 0; i < count; i++)
		{
			if (data[i] == item)
			{
				pos = i;
				return true;
			}
		}
		return false;
	}

	bool find(const std::function<int(const T& item)>& compare, size_type& pos) const
	{
		for (size_type i = 0; i < count; i++)
		{
			if (compare(data[i]) == 0)
			{
				pos = i;
				return true;
			}
		}
		return false;
	}

	bool findAndRemove(const T& item)
	{
		size_type pos;
		if (find(item, pos))
		{
			remove(pos);
			return true;
		}

		return false;
	}

	bool exist(const T& item) const
	{
		size_type pos;	// ignored
		return find(item, pos);
	}

	bool operator==(const Array& op) const
	{
		if (count != op.count)
			return false;

		// return memcmp(data, op.data, count) == 0;
		// fast but wrong - imagine array element with non-dense elements

		auto my = begin();
		const auto my_end = end();
		for (auto him = op.begin(); my != my_end; ++my, ++him)
		{
			if (! (*my == *him))
				return false;
		}

		return true;
	}

	// Member function only for some debugging tests. Hope nobody is bothered.
	void swapElems()
	{
		const size_type limit = count / 2;
		for (size_type i = 0; i < limit; ++i)
		{
			T temp = data[i];
			data[i] = data[count - 1 - i];
			data[count - 1 - i] = temp;
		}
	}

	void ensureCapacity(size_type newcapacity)
	{
		ensureCapacity(newcapacity, true);
	}

protected:
	size_type count, capacity;
	T* data;

	void ensureCapacity(size_type newcapacity, bool preserve)
	{
		if (newcapacity > capacity)
		{
			if (capacity <= FB_MAX_SIZEOF / 2)
			{
				if (newcapacity < capacity * 2)
					newcapacity = capacity * 2;
			}
			else
			{
				newcapacity = FB_MAX_SIZEOF;
			}

			// Ensure we can carry byte copy operations.
			// What to do here, throw in release build?
			fb_assert(newcapacity < FB_MAX_SIZEOF / sizeof(T));

			T* newdata = static_cast<T*>
				(this->getPool().allocate(sizeof(T) * newcapacity));
			if (preserve)
				memcpy(static_cast<void*>(newdata), data, sizeof(T) * count);
			freeData();
			data = newdata;
			capacity = newcapacity;
		}
	}
};

static inline constexpr int FB_ARRAY_SORT_MANUAL = 0;
static inline constexpr int FB_ARRAY_SORT_WHEN_ADD = 1;

// Dynamic sorted array of simple objects
template <typename Value,
	typename Storage = EmptyStorage<Value>,
	typename Key = Value,
	typename KeyOfValue = DefaultKeyValue<Value>,
	typename Cmp = DefaultComparator<Key> >
class SortedArray : public Array<Value, Storage>
{
public:
	typedef typename Array<Value, Storage>::size_type size_type;
	SortedArray(MemoryPool& p, size_type s)
		: Array<Value, Storage>(p, s), sortMode(FB_ARRAY_SORT_WHEN_ADD), sorted(true)
	{ }

	explicit SortedArray(MemoryPool& p)
		: Array<Value, Storage>(p), sortMode(FB_ARRAY_SORT_WHEN_ADD), sorted(true)
	{ }

	explicit SortedArray(size_type s)
		: Array<Value, Storage>(s), sortMode(FB_ARRAY_SORT_WHEN_ADD), sorted(true)
	{ }

	SortedArray()
		: Array<Value, Storage>(), sortMode(FB_ARRAY_SORT_WHEN_ADD), sorted(true)
	{ }

	// When item is not found, set pos to the position where the element should be
	// stored if/when added.
	bool find(const Key& item, size_type& pos) const
	{
		fb_assert(sorted);

		size_type highBound = this->count, lowBound = 0;
		while (highBound > lowBound)
		{
			const size_type temp = (highBound + lowBound) >> 1;
			if (Cmp::greaterThan(item, KeyOfValue::generate(this->data[temp])))
				lowBound = temp + 1;
			else
				highBound = temp;
		}
		pos = lowBound;
		return highBound != this->count &&
			!Cmp::greaterThan(KeyOfValue::generate(this->data[lowBound]), item);
	}

	bool findAndRemove(const Key& item)
	{
		size_type pos;
		if (find(item, pos))
		{
			this->remove(pos);
			return true;
		}

		return false;
	}

	bool exist(const Key& item) const
	{
		size_type pos;	// ignored
		return find(item, pos);
	}

	size_type add(const Value& item)
	{
		size_type pos;
		if (sortMode == FB_ARRAY_SORT_WHEN_ADD)
			find(KeyOfValue::generate(item), pos);
		else
		{
			sorted = false;
			pos = this->getCount();
		}
		this->insert(pos, item);
		return pos;
	}

	size_type addUniq(const Value& item)
	{
		size_type pos;
		fb_assert(sortMode == FB_ARRAY_SORT_WHEN_ADD);
		if (!find(KeyOfValue::generate(item), pos))
		{
			this->insert(pos, item);
			return pos;
		}
		return this->npos;
	}

	void setSortMode(int sm)
	{
		if (sortMode != FB_ARRAY_SORT_WHEN_ADD && sm == FB_ARRAY_SORT_WHEN_ADD && !sorted)
		{
			sort();
		}
		sortMode = sm;
	}

	void sort()
	{
		if (sorted)
			return;

		auto compare = [] (const void* a, const void* b) {
			const Key& first(KeyOfValue::generate(*static_cast<const Value*>(a)));
			const Key& second(KeyOfValue::generate(*static_cast<const Value*>(b)));

			if (Cmp::greaterThan(first, second))
				return 1;
			if (Cmp::greaterThan(second, first))
				return -1;

			return 0;
		};

		qsort(this->begin(), this->getCount(), sizeof(Value), compare);
		sorted = true;
	}

private:
	int sortMode;
	bool sorted;
};

// Nice shorthand for arrays with static part
template <typename T, FB_SIZE_T InlineCapacity, typename AlignT = T>
using HalfStaticArray = Array<T, InlineStorage<T, InlineCapacity, AlignT> >;

typedef HalfStaticArray<UCHAR, BUFFER_TINY> UCharBuffer;

}	// namespace Firebird

#endif // CLASSES_ARRAY_H
