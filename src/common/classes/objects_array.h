/*
 *	PROGRAM:	Common class definition
 *	MODULE:		object_array.h
 *	DESCRIPTION:	half-static array of any objects,
 *			having MemoryPool'ed constructor.
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_OBJECTS_ARRAY_H
#define CLASSES_OBJECTS_ARRAY_H

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"

namespace Firebird
{
	template <typename T, typename A = Array<T*, InlineStorage<T*, 8> > >
	class ObjectsArray : protected A
	{
	private:
		typedef A inherited;
	public:
		class const_iterator; // fwd decl.
		typedef FB_SIZE_T size_type;

		class iterator
		{
			friend class ObjectsArray<T, A>;
			friend class const_iterator;
		private:
			ObjectsArray *lst;
			size_type pos;
			iterator(ObjectsArray *l, size_type p) : lst(l), pos(p) { }
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = T;
			using pointer = T*;
			using reference = T&;

			iterator() : lst(0), pos(0) { }
			iterator(const iterator& it) : lst(it.lst), pos(it.pos) { }

			iterator& operator++()
			{
				++pos;
				return (*this);
			}
			iterator operator++(int)
			{
				iterator tmp = *this;
				++pos;
				 return tmp;
			}
			iterator& operator--()
			{
				fb_assert(pos > 0);
				--pos;
				return (*this);
			}
			iterator operator--(int)
			{
				fb_assert(pos > 0);
				iterator tmp = *this;
				--pos;
				 return tmp;
			}
			T* operator->()
			{
				fb_assert(lst);
				T* pointer = lst->getPointer(pos);
				return pointer;
			}
			T& operator*()
			{
				fb_assert(lst);
				T* pointer = lst->getPointer(pos);
				return *pointer;
			}
			bool operator!=(const iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos != v.pos : true;
			}
			bool operator==(const iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos == v.pos : false;
			}
		};

		class const_iterator
		{
			friend class ObjectsArray<T, A>;
		private:
			const ObjectsArray *lst;
			size_type pos;
			const_iterator(const ObjectsArray *l, size_type p) : lst(l), pos(p) { }
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = T;
			using pointer = const T*;
			using reference = const T&;

			const_iterator() : lst(0), pos(0) { }
			const_iterator(const iterator& it) : lst(it.lst), pos(it.pos) { }
			const_iterator(const const_iterator& it) : lst(it.lst), pos(it.pos) { }

			const_iterator& operator++()
			{
				++pos;
				return (*this);
			}
			const_iterator operator++(int)
			{
				const_iterator tmp = *this;
				++pos;
				 return tmp;
			}
			const_iterator& operator--()
			{
				fb_assert(pos > 0);
				--pos;
				return (*this);
			}
			const_iterator operator--(int)
			{
				fb_assert(pos > 0);
				const_iterator tmp = *this;
				--pos;
				 return tmp;
			}
			const T* operator->()
			{
				fb_assert(lst);
				const T* pointer = lst->getPointer(pos);
				return pointer;
			}
			const T& operator*()
			{
				fb_assert(lst);
				const T* pointer = lst->getPointer(pos);
				return *pointer;
			}
			bool operator!=(const const_iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos != v.pos : true;
			}
			bool operator==(const const_iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos == v.pos : false;
			}
			// Against iterator
			bool operator!=(const iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos != v.pos : true;
			}
			bool operator==(const iterator& v) const
			{
				fb_assert(lst == v.lst);
				return lst ? pos == v.pos : false;
			}

		};

	public:
		MemoryPool& getPool() const
		{
			return inherited::getPool();
		}

		void insert(size_type index, const T& item)
		{
			T* dataL = FB_NEW_POOL(this->getPool()) T(this->getPool(), item);
			inherited::insert(index, dataL);
		}

		T& insert(size_type index)
		{
			T* dataL = FB_NEW_POOL(this->getPool()) T(this->getPool());
			inherited::insert(index, dataL);
			return *dataL;
		}

		size_type add(const T& item)
		{
			T* dataL = FB_NEW_POOL(this->getPool()) T(this->getPool(), item);
			return inherited::add(dataL);
		}

		T& add()
		{
			T* dataL = FB_NEW_POOL(this->getPool()) T(this->getPool());
			inherited::add(dataL);
			return *dataL;
		}

		void push(const T& item)
		{
			add(item);
		}

		T pop()
		{
			T* pntr = inherited::pop();
			T rc = *pntr;
			delete pntr;
			return rc;
		}

		void remove(size_type index)
		{
			fb_assert(index < getCount());
			delete getPointer(index);
			inherited::remove(index);
		}

		void remove(iterator itr)
		{
  			fb_assert(itr.lst == this);
			remove(itr.pos);
		}

		void shrink(size_type newCount)
		{
			for (size_type i = newCount; i < getCount(); i++) {
				delete getPointer(i);
			}
			inherited::shrink(newCount);
		}

		void grow(size_type newCount)
		{
			size_type oldCount = getCount();
			inherited::grow(newCount);
			for (size_type i = oldCount; i < newCount; i++) {
				inherited::getElement(i) = FB_NEW_POOL(this->getPool()) T(this->getPool());
			}
		}

		void resize(const size_type newCount, const T& val)
		{
			if (newCount > getCount())
			{
				size_type oldCount = getCount();
				inherited::grow(newCount);
				for (size_type i = oldCount; i < newCount; i++) {
					inherited::getElement(i) = FB_NEW_POOL(this->getPool()) T(this->getPool(), val);
				}
			}
			else {
				shrink(newCount);
			}
		}

		void resize(const size_type newCount)
		{
			if (newCount > getCount())
			{
				grow(newCount);
			}
			else {
				shrink(newCount);
			}
		}

		iterator begin()
		{
			return iterator(this, 0);
		}

		iterator end()
		{
			return iterator(this, getCount());
		}

		iterator back()
		{
  			fb_assert(getCount() > 0);
			return iterator(this, getCount() - 1);
		}

		const_iterator begin() const
		{
			return const_iterator(this, 0);
		}

		const_iterator end() const
		{
			return const_iterator(this, getCount());
		}

		const T& operator[](size_type index) const
		{
  			return *getPointer(index);
		}

		const T* getPointer(size_type index) const
		{
  			return inherited::getElement(index);
		}

		T& operator[](size_type index)
		{
  			return *getPointer(index);
		}

		T* getPointer(size_type index)
		{
  			return inherited::getElement(index);
		}

		explicit ObjectsArray(MemoryPool& p, const ObjectsArray<T, A>& o)
			: A(p)
		{
			add(o);
		}

		explicit ObjectsArray(MemoryPool& p)
			: A(p)
		{
		}

		ObjectsArray(const ObjectsArray<T, A>& o)
			: A()
		{
			add(o);
		}

		ObjectsArray(MemoryPool& p, std::initializer_list<T> items)
			: A(p)
		{
			for (auto& item : items)
				add(item);
		}

		ObjectsArray() :
			A()
		{
		}

		~ObjectsArray()
		{
			for (size_type i = 0; i < getCount(); i++)
				delete getPointer(i);
		}

		size_type getCount() const throw()
		{
			return inherited::getCount();
		}

		size_type getCapacity() const
		{
			return inherited::getCapacity();
		}

		bool hasData() const
		{
			return getCount() != 0;
		}

		bool isEmpty() const
		{
			return getCount() == 0;
		}

		void clear()
		{
			for (size_type i = 0; i < getCount(); i++)
				delete getPointer(i);

			inherited::clear();
		}

		ObjectsArray<T, A>& operator =(const ObjectsArray<T, A>& o)
		{
			while (this->count > o.count)
				delete inherited::pop();

			add(o);

			return *this;
		}

		bool find(const T& item, FB_SIZE_T& pos) const
		{
			for (size_type i = 0; i < this->count; i++)
			{
				if (*getPointer(i) == item)
				{
					pos = i;
					return true;
				}
			}
			return false;
		}

	private:
		void add(const ObjectsArray<T, A>& o)
		{
			for (size_type i = 0; i < o.count; i++)
			{
				if (i < this->count)
					(*this)[i] = o[i];
				else
					add(o[i]);
			}
		}
	};

	// Template to convert object value to index directly
	template <typename T>
	class ObjectKeyValue
	{
	public:
		static const T& generate(const T* item) { return item; }
	};

	// Template for default value comparator
	template <typename T>
	class ObjectComparator
	{
	public:
		static bool greaterThan(const T i1, const T i2)
		{
			return *i1 > *i2;
		}
	};

	// Dynamic sorted array of simple objects
	template <typename ObjectValue,
		typename ObjectStorage = InlineStorage<ObjectValue*, 32>,
		typename ObjectKey = ObjectValue,
		typename ObjectKeyOfValue = DefaultKeyValue<ObjectValue*>,
		typename ObjectCmp = ObjectComparator<const ObjectKey*> >
	class SortedObjectsArray : public ObjectsArray<ObjectValue,
			SortedArray <ObjectValue*, ObjectStorage, const ObjectKey*,
			ObjectKeyOfValue, ObjectCmp> >
	{
	private:
		typedef ObjectsArray <ObjectValue, SortedArray<ObjectValue*,
				ObjectStorage, const ObjectKey*, ObjectKeyOfValue,
				ObjectCmp> > inherited;

	public:
		typedef typename inherited::size_type size_type;
		explicit SortedObjectsArray(MemoryPool& p) :
			ObjectsArray <ObjectValue, SortedArray<ObjectValue*,
				ObjectStorage, const ObjectKey*, ObjectKeyOfValue,
				ObjectCmp> >(p)
		{ }

		explicit SortedObjectsArray() :
			ObjectsArray <ObjectValue, SortedArray<ObjectValue*,
				ObjectStorage, const ObjectKey*, ObjectKeyOfValue,
				ObjectCmp> >()
		{ }

		explicit SortedObjectsArray(MemoryPool& p, const SortedObjectsArray& o) :
			ObjectsArray <ObjectValue, SortedArray<ObjectValue*,
				ObjectStorage, const ObjectKey*, ObjectKeyOfValue,
				ObjectCmp> >(p, o)
		{
		}

		bool find(const ObjectKey& item, size_type& pos) const
		{
			const ObjectKey* const pItem = &item;
			return static_cast<const SortedArray<ObjectValue*,
				ObjectStorage, const ObjectKey*, ObjectKeyOfValue,
				ObjectCmp>*>(this)->find(pItem, pos);
		}

		bool exist(const ObjectKey& item) const
		{
			size_type pos;	// ignored
			return find(item, pos);
		}

		size_type add(const ObjectValue& item)
		{
			return inherited::add(item);
		}

		void setSortMode(int sm)
		{
			inherited::setSortMode(sm);
		}

		void sort()
		{
			inherited::sort();
		}

	private:
		ObjectValue& add();	// Unusable when sorted
	};

	// Sorted pointers array - contains 2 arrays: simple for values (POD)
	// and sorted for pointers to them. Effective for big sizeof(POD).
	template <typename Value,
		typename Storage = EmptyStorage<Value>,
		typename Key = Value,
		typename KeyOfValue = DefaultKeyValue<Value*>,
		typename Cmp = ObjectComparator<const Key*> >
	class PointersArray
	{
	private:
		Array<Value, Storage> values;
		SortedArray<Value*, InlineStorage<Value*, 8>, const Key*, KeyOfValue, Cmp> pointers;

		void checkPointers(const Value* oldBegin)
		{
			Value* newBegin = values.begin();
			if (newBegin != oldBegin)
			{
				for (Value** ptr = pointers.begin(); ptr < pointers.end(); ++ptr)
				{
					*ptr = newBegin + (*ptr - oldBegin);
				}
			}
		}

	public:
		typedef FB_SIZE_T size_type;

		class const_iterator
		{
		private:
			const Value* const* ptr;

		public:
			const_iterator() : ptr(NULL) { }
			const_iterator(const const_iterator& it) : ptr(it.ptr) { }
			explicit const_iterator(const PointersArray& a) : ptr(a.pointers.begin()) { }

			const_iterator& operator++()
			{
				fb_assert(ptr);
				ptr++;
				return *this;
			}

			const_iterator operator++(int)
			{
				fb_assert(ptr);
				const_iterator tmp = *this;
				ptr++;
				return tmp;
			}

			const_iterator& operator--()
			{
				fb_assert(ptr);
				ptr--;
				return *this;
			}

			const_iterator operator--(int)
			{
				fb_assert(ptr);
				const_iterator tmp = *this;
				ptr--;
				return tmp;
			}

			const_iterator& operator+=(size_type v)
			{
				fb_assert(ptr);
				ptr += v;
				return *this;
			}

			const_iterator& operator-=(size_type v)
			{
				fb_assert(ptr);
				ptr -= v;
				return *this;
			}

			const Value* operator->()
			{
				fb_assert(ptr);
				return *ptr;
			}

			const Value& operator*()
			{
				fb_assert(ptr && *ptr);
				return **ptr;
			}

			bool operator==(const const_iterator& v) const
			{
				return ptr && ptr == v.ptr;
			}

			bool operator!=(const const_iterator& v) const
			{
				return !operator==(v);
			}
		};

		class iterator
		{
		private:
			Value** ptr;

		public:
			iterator() : ptr(NULL) { }
			iterator(const iterator& it) : ptr(it.ptr) { }
			explicit iterator(PointersArray& a) : ptr(a.pointers.begin()) { }

			iterator& operator++()
			{
				fb_assert(ptr);
				ptr++;
				return *this;
			}

			iterator operator++(int)
			{
				fb_assert(ptr);
				iterator tmp = *this;
				ptr++;
				return tmp;
			}

			iterator& operator--()
			{
				fb_assert(ptr);
				ptr--;
				return *this;
			}

			iterator operator--(int)
			{
				fb_assert(ptr);
				iterator tmp = *this;
				ptr--;
				return tmp;
			}

			iterator& operator+=(size_type v)
			{
				fb_assert(ptr);
				ptr += v;
				return *this;
			}

			iterator& operator-=(size_type v)
			{
				fb_assert(ptr);
				ptr -= v;
				return *this;
			}

			Value* operator->()
			{
				fb_assert(ptr);
				return *ptr;
			}

			Value& operator*()
			{
				fb_assert(ptr && *ptr);
				return **ptr;
			}

			bool operator==(const iterator& v) const
			{
				return ptr && ptr == v.ptr;
			}

			bool operator!=(const iterator& v) const
			{
				return !operator==(v);
			}
		};

	public:
		size_type add(const Value& item)
		{
			const Value* oldBegin = values.begin();
			values.add(item);
			checkPointers(oldBegin);
			return pointers.add(values.end() - 1);
		}

		const_iterator begin() const
		{
			return const_iterator(*this);
		}

		const_iterator end() const
		{
			const_iterator rc(*this);
			rc += getCount();
			return rc;
		}

		iterator begin()
		{
			return iterator(*this);
		}

		iterator end()
		{
			iterator rc(*this);
			rc += getCount();
			return rc;
		}

		const Value& operator[](size_type index) const
		{
  			return *getPointer(index);
		}

		const Value* getPointer(size_type index) const
		{
  			return pointers[index];
		}

		Value& operator[](size_type index)
		{
  			return *getPointer(index);
		}

		Value* getPointer(size_type index)
		{
  			return pointers[index];
		}

		explicit PointersArray(MemoryPool& p) : values(p), pointers(p) { }
		PointersArray() : values(), pointers() { }
		~PointersArray() { }

		size_type getCount() const throw()
		{
			fb_assert(values.getCount() == pointers.getCount());
			return values.getCount();
		}

		size_type getCapacity() const
		{
			return values.getCapacity();
		}

		void clear()
		{
			pointers.clear();
			values.clear();
		}

		PointersArray& operator=(const PointersArray& o)
		{
			values = o.values;
			pointers = o.pointers;
			checkPointers(o.values.begin());
			return *this;
		}

		bool find(const Key& item, size_type& pos) const
		{
			return pointers.find(&item, pos);
		}

		bool exist(const Key& item) const
		{
			return pointers.exist(item);
		}

		void insert(size_type pos, const Value& item)
		{
			const Value* oldBegin = values.begin();
			values.add(item);
			checkPointers(oldBegin);
			pointers.insert(pos, values.end() - 1);
		}
	};

} // namespace Firebird

#endif	// CLASSES_OBJECTS_ARRAY_H
