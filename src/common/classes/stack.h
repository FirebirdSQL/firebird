/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		stack.h
 *	DESCRIPTION:	Stack.
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
 *
 */

#ifndef CLASSES_STACK_H
#define CLASSES_STACK_H

#include "../common/classes/vector.h"

namespace Firebird {

	template <typename Object, FB_SIZE_T Capacity = 16>
		class Stack : public AutoStorage
	{
	private:
		Stack<Object, Capacity>(Stack<Object, Capacity>&);	// not implemented

		class Entry : public Vector<Object, Capacity>
		{
		private:
			typedef Vector<Object, Capacity> inherited;
		public:
			Entry* next;

			Entry(Object e, Entry* stk)
				: inherited(), next(stk)
			{
				this->add(e);
			}

			Entry(Entry* stk) : inherited(), next(stk) { }

			~Entry()
			{
				delete next;
			}

			Entry* push(Object e, MemoryPool& p)
			{
				if (inherited::getCount() < this->getCapacity())
				{
					this->add(e);
					return this;
				}
				Entry* newEntry = FB_NEW_POOL(p) Entry(e, this);
				return newEntry;
			}

			Object pop()
			{
				fb_assert(inherited::getCount() > 0);
				return this->data[--this->count];
			}

			Object getObject(FB_SIZE_T pos) const
			{
				return this->data[pos];
			}

			void split(FB_SIZE_T elem, Entry* target)
			{
				fb_assert(elem > 0 && elem < this->count);
				fb_assert(target->count == 0);
				target->count = this->count - elem;
				memcpy(target->data, &this->data[elem], target->count * sizeof(Object));
				this->count = elem;
			}

			Entry* dup(MemoryPool& p)
			{
				Entry* rc = FB_NEW_POOL(p) Entry(next ? next->dup(p) : 0);
				rc->join(*this);
				return rc;
			}

			bool hasMore(FB_SIZE_T value) const
			{
				for (const Entry* stk = this; stk; stk = stk->next)
				{
					FB_SIZE_T c = static_cast<const inherited*>(stk)->getCount();
					if (value < c)
					{
						return true;
					}
					value -= c;
				}

				return false;
			}

		}; // class Entry

		Entry* stk;
		Entry* stk_cache;

	public:
		explicit Stack<Object, Capacity>(MemoryPool& p)
			: AutoStorage(p), stk(0), stk_cache(0)
		{ }

		Stack<Object, Capacity>() : AutoStorage(), stk(0), stk_cache(0) { }

		~Stack()
		{
			delete stk;
			delete stk_cache;
		}

		void push(const Object& e)
		{
			if (!stk && stk_cache)
			{
				stk = stk_cache;
				stk_cache = 0;
			}
			stk = stk ? stk->push(e, getPool())
					  : FB_NEW_POOL(getPool()) Entry(e, 0);
		}

		Object pop()
		{
			fb_assert(stk);
			Object tmp = stk->pop();
			if (!stk->getCount())
			{
				fb_assert(!stk_cache);
				stk_cache = stk;
				stk = stk->next;
				stk_cache->next = 0;

				// don't delete last empty Entry
				if (stk)
				{
					delete stk_cache;
					stk_cache = 0;
				}
			}
			return tmp;
		}

	private:
		// disable use of default operator=
		Stack<Object, Capacity>& operator= (const Stack<Object, Capacity>& s);

	public:
		void takeOwnership (Stack<Object, Capacity>& s)
		{
			fb_assert(&getPool() == &s.getPool());
			delete stk;
			stk = s.stk;
			s.stk = 0;

			if (stk)
			{
				delete stk_cache;
				stk_cache = 0;
			}
		}

		// Push a element on the stack and pop when we go out of scope.
		class AutoPushPop
		{
		public:
			AutoPushPop(Stack<Object, Capacity>& s, const Object& o)
				: stack(s)
			{
				stack.push(o);
			}

			~AutoPushPop()
			{
				stack.pop();
			}

		private:
			Stack<Object, Capacity>& stack;
		};

		// Restore the stack when we go out of scope.
		class AutoRestore
		{
		public:
			explicit AutoRestore(Stack<Object, Capacity>& s)
				: stack(s),
				  count(s.getCount())
			{
			}

			~AutoRestore()
			{
				FB_SIZE_T currentCount = stack.getCount();
				fb_assert(currentCount >= count);

				while (currentCount-- > count)
					stack.pop();
			}

		private:
			Stack<Object, Capacity>& stack;
			FB_SIZE_T count;
		};

		class iterator;
		friend class iterator;
		friend class reverse_iterator;

		// Very basic iterator, that permits to walk linked list in backwards direction
		// and remove elements along the way.
		class reverse_iterator
		{
		private:
			Stack<Object, Capacity>& stack;
			Stack<Entry*> entries;
			Entry* current_entry;
			FB_SIZE_T elem;

		public:
			explicit reverse_iterator(Stack<Object, Capacity>& s)
				: stack(s), entries(s.getPool()), elem(0)
			{
				current_entry = s.stk;
				if (current_entry) {
					while (Entry *next = current_entry->next) {
						entries.push(current_entry);
						current_entry = next;
					}
				}
			}

			bool hasData() const
			{
				return current_entry;
			}

			reverse_iterator& operator++()
			{
				fb_assert(current_entry);
				elem++;

				if (elem >= current_entry->getCount()) {
					elem = 0;
					if (entries.hasData())
						current_entry = entries.pop();
					else
						current_entry = NULL;
				}

				return *this;
			}

			Object object() const
			{
				fb_assert(current_entry);
				return current_entry->getObject(elem);
			}

			void remove() {
				fb_assert(current_entry);
				current_entry->remove(elem);

				if (elem >= current_entry->getCount()) {
					if (elem) {
						// Simple case - just advance pointer
						elem = 0;
						if (entries.hasData())
							current_entry = entries.pop();
						else
							current_entry = NULL;
					}
					else {
						// Complicated case - Entry is empty and we need to delete it
						if (entries.hasData()) {
							Entry* previous = entries.pop();
							previous->next = current_entry->next;
							current_entry->next = NULL;
							delete current_entry;
							current_entry = previous;
						}
						else {
							stack.stk = current_entry->next;
							current_entry->next = NULL;
							delete current_entry;
							current_entry = NULL;
						}
					}
				}
			}

		private:
			reverse_iterator(const reverse_iterator&); // Not implemented
			reverse_iterator& operator= (const reverse_iterator&); // Not implemented
		}; // reverse_iterator

		class iterator
		{
		private:
			// friend definition here is required to implement
			// Merge/Split pair of functions
			friend class ::Firebird::Stack<Object, Capacity>;
			const Entry* stk;
			FB_SIZE_T elem;

		public:
			explicit iterator(Stack<Object, Capacity>& s)
				: stk(s.stk), elem(stk ? stk->getCount() : 0)
			{ }

			iterator(const iterator& i)
				: stk(i.stk), elem(i.elem)
			{ }

			iterator() : stk(0), elem(0) { }

			iterator& operator++()
			{
				fb_assert(stk);
				fb_assert(elem);
				if (--elem == 0)
				{
					if ((stk = stk->next))
					{
						elem = stk->getCount();
					}
				}
				return *this;
			}

			bool hasMore(FB_SIZE_T value) const
			{
				if (elem)
				{
					if (value < elem)
						return true;

					value -= elem;
				}

				if (stk && stk->next)
					return stk->next->hasMore(value);

				return false;
			}

			bool hasData() const
			{
				return stk;
			}

			bool isEmpty() const
			{
				return !stk;
			}

			Object object() const
			{
				fb_assert(stk);
				return stk->getObject(elem - 1);
			}

			bool operator== (const iterator& i) const
			{
				return (stk == i.stk) && (elem == i.elem);
			}

			bool operator!= (const iterator& i) const
			{
				return !(*this == i);
			}

			bool operator== (const Stack<Object, Capacity>& s) const
			{
				return (this->stk == s.stk) && (s.stk ? this->elem == s.stk->getCount() : true);
			}

			bool operator!= (const Stack<Object, Capacity>& s) const
			{
				return !(*this == s);
			}

			iterator& operator= (iterator& i)
			{
				stk = i.stk;
				elem = i.elem;
				return *this;
			}

			iterator& operator= (Stack<Object, Capacity>& s)
			{
				stk = s.stk;
				elem = stk ? stk->getCount() : 0;
				return *this;
			}

			//friend void class Stack<Object, Capacity>::clear (const iterator& mark);
		}; // iterator

		class const_iterator;
		friend class const_iterator;

		class const_iterator
		{
		private:
			friend class ::Firebird::Stack<Object, Capacity>;
			const Entry* stk;
			FB_SIZE_T elem;

		public:
			explicit const_iterator(const Stack<Object, Capacity>& s)
				: stk(s.stk), elem(stk ? stk->getCount() : 0)
			{ }

			const_iterator(const iterator& i)
				: stk(i.stk), elem(i.elem)
			{ }

			const_iterator(const const_iterator& i)
				: stk(i.stk), elem(i.elem)
			{ }

			const_iterator() : stk(0), elem(0) { }

			const_iterator& operator++()
			{
				fb_assert(stk);
				fb_assert(elem);
				if (--elem == 0)
				{
					if ((stk = stk->next))
					{
						elem = stk->getCount();
					}
				}
				return *this;
			}

			bool hasMore(FB_SIZE_T value) const
			{
				if (elem)
				{
					if (value < elem)
						return true;

					value -= elem;
				}

				if (stk && stk->next)
					return stk->next->hasMore(value);

				return false;
			}

			bool hasData() const
			{
				return stk;
			}

			bool isEmpty() const
			{
				return !stk;
			}

			const Object object() const
			{
				fb_assert(stk);
				return stk->getObject(elem - 1);
			}

			bool operator== (const iterator& i) const
			{
				return (stk == i.stk) && (elem == i.elem);
			}

			bool operator== (const const_iterator& i) const
			{
				return (stk == i.stk) && (elem == i.elem);
			}

			bool operator!= (const iterator& i) const
			{
				return !(*this == i);
			}

			bool operator!= (const const_iterator& i) const
			{
				return !(*this == i);
			}

			bool operator== (const Stack<Object, Capacity>& s) const
			{
				return (this->stk == s.stk) && (s.stk ? this->elem == s.stk->getCount() : true);
			}

			bool operator!= (const Stack<Object, Capacity>& s) const
			{
				return !(*this == s);
			}

			const_iterator& operator= (const iterator& i)
			{
				stk = i.stk;
				elem = i.elem;
				return *this;
			}

			const_iterator& operator= (const const_iterator& i)
			{
				stk = i.stk;
				elem = i.elem;
				return *this;
			}

			const_iterator& operator= (const Stack<Object, Capacity>& s)
			{
				stk = s.stk;
				elem = stk ? stk->getCount() : 0;
				return *this;
			}

			//friend const const_iterator class Stack<Object, Capacity>::merge(Stack<Object, Capacity>& s);
			//void class Stack<Object, Capacity>::split (const const_iterator& mark, Stack<Object, Capacity>& s);
		}; // const_iterator


		// Merge stack "s" to the end of current one.
		// Returns - iterator to Split stacks again.
		// This iterator will be used as "bookmark" for Split later.
		const const_iterator merge(Stack<Object, Capacity>& s)
		{
			fb_assert(&getPool() == &s.getPool());
			const const_iterator rc(s);
			Entry **e = &stk;
			while (*e)
			{
				e = &((*e)->next);
			}
			*e = s.stk;
			s.stk = 0;

			if (stk)
			{
				delete stk_cache;
				stk_cache = 0;
			}
			return rc;
		}

		// Split stacks at mark
		void split (const const_iterator& mark, Stack<Object, Capacity>& s)
		{
			fb_assert(&getPool() == &s.getPool());
			fb_assert(!s.stk);

			// if empty stack was merged, there is nothing to do
			if (!mark.stk)
			{
				return;
			}

			// find entry to Split
			Entry **toSplit = &stk;
			while (*toSplit != mark.stk)
			{
				fb_assert(*toSplit);
				toSplit = &((*toSplit)->next);
			}

			// Determine whether some new elements were added
			// to this stack. Depended on this we must
			// Split on entries boundary or cut one entry to halfs.
			fb_assert((*toSplit)->getCount() >= mark.elem);
			if ((*toSplit)->getCount() == mark.elem)
			{
				s.stk = *toSplit;
				*toSplit = 0;
			}
			else
			{
				Entry* newEntry = FB_NEW_POOL(getPool()) Entry(0);
				(*toSplit)->split(mark.elem, newEntry);
				s.stk = *toSplit;
				*toSplit = newEntry;
			}

			if (s.stk)
			{
				delete s.stk_cache;
				s.stk_cache = 0;
			}
		}

		// clear stacks until mark
		void clear (const iterator& mark)
		{
			// for empty mark just clear all stack
			if (!mark.stk)
			{
				clear();
				return;
			}

			// find entry to clear
			while (stk != mark.stk)
			{
				if (!stk)
				{
					return;
				}
				Entry *tmp = stk->next;
				stk->next = 0;
				delete stk;
				stk = tmp;
			}

			// remove extra elements from Entry
			fb_assert(stk->getCount() >= mark.elem);
			if (mark.elem == 0)
			{
				Entry *tmp = stk->next;
				stk->next = 0;
				delete stk;
				stk = tmp;
			}
			else
			{
				stk->shrink(mark.elem);
			}
		}

		FB_SIZE_T getCount() const throw()
		{
			FB_SIZE_T rc = 0;
			for (Entry* entry = stk; entry; entry = entry->next)
			{
				rc += entry->getCount();
			}
			return rc;
		}

		bool hasMore(FB_SIZE_T value) const
		{
			return (stk && stk->hasMore(value));
		}

		// returns topmost element on the stack
		Object object() const
		{
			fb_assert(stk);
			return stk->getObject(stk->getCount() - 1);
		}

		// returns true if stack is not empty
		bool hasData() const
		{
			return stk;
		}

		// returns true if stack is empty
		bool isEmpty() const
		{
			return !stk;
		}

		bool operator== (const iterator& i) const
		{
			return i == *this;
		}

		bool operator!= (const iterator& i) const
		{
			return !(i == *this);
		}

		bool operator== (const const_iterator& i) const
		{
			return i == *this;
		}

		bool operator!= (const const_iterator& i) const
		{
			return !(i == *this);
		}

		void assign(Stack<Object, Capacity>& v)
		{
			delete stk;
			stk = v.stk ? v.stk->dup(getPool()) : 0;

			if (stk)
			{
				delete stk_cache;
				stk_cache = 0;
			}
		}

		void clear()
		{
			delete stk;
			stk = 0;
		}

		MemoryPool& getPool()
		{
			return AutoStorage::getPool();
		}
	}; // class Stack

} // namespace Firebird

#endif // CLASSES_STACK_H

