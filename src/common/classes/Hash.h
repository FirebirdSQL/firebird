/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		Hash.h
 *	DESCRIPTION:	Hash of objects
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
 *  Copyright (c) 2009 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef CLASSES_HASH_H
#define CLASSES_HASH_H

#include "../common/classes/array.h"

struct dsc;

namespace Firebird
{
	template <typename K>
	class DefaultHash
	{
	public:
		static FB_SIZE_T hash(const void* value, FB_SIZE_T length, FB_SIZE_T hashSize)
		{
			size_t sum = 0;
			size_t val;

			const UCHAR* data = static_cast<const UCHAR*>(value);

			while (length >= sizeof(size_t))
			{
				memcpy(&val, data, sizeof(size_t));
				sum += val;
				data += sizeof(size_t);
				length -= sizeof(size_t);
			}

			if (length)
			{
				val = 0;
				memcpy(&val, data, length);
				sum += val;
			}

			size_t rc = 0;
			while (sum)
			{
				rc += (sum % hashSize);
				sum /= hashSize;
			}

			return rc % hashSize;
		}

		static FB_SIZE_T hash(const K& value, FB_SIZE_T hashSize)
		{
			return hash(&value, sizeof value, hashSize);
		}
	};

	const FB_SIZE_T DEFAULT_HASH_SIZE = 97;			// largest prime number < 100

	template <typename C,
			  FB_SIZE_T HASHSIZE = DEFAULT_HASH_SIZE,
			  typename K = C,						// default key
			  typename KeyOfValue = DefaultKeyValue<C>,	// default keygen
			  typename F = DefaultHash<K> >			// hash function definition
	class HashTable
	{
	public:
		// This class is supposed to be used as a BASE class for class to be hashed
		class Entry
		{
		private:
			Entry** previousElement;
			Entry* nextElement;

		public:
			Entry() : previousElement(NULL) { }

			virtual ~Entry()
			{
				unLink();
			}

			void link(Entry** where)
			{
				unLink();

				// set our pointers
				previousElement = where;
				nextElement = previousElement ? *previousElement : NULL;

				// make next element (if present) to point to us
				if (nextElement)
				{
					nextElement->previousElement = &nextElement;
				}

				// make previous element point to us
				*previousElement = this;
			}

			void unLink()
			{
				// if we are linked
				if (previousElement)
				{
					if (nextElement)
					{
						// adjust previous pointer in next element ...
						nextElement->previousElement = previousElement;
					}
					// ... and next pointer in previous element
					*previousElement = nextElement;
					// finally mark ourselves not linked
					previousElement = NULL;
				}
			}

			Entry** nextPtr()
			{
				return &nextElement;
			}

			Entry* next() const
			{
				return nextElement;
			}

			C* next(const K& key)
			{
				Entry* e = next();
				return (e && e->isEqual(key)) ? e->get() : NULL;
			}

			virtual bool isEqual(const K&) const = 0;
			virtual C* get() = 0;
		}; // class Entry

	private:
		HashTable(const HashTable&);	// not implemented

	public:
		explicit HashTable(MemoryPool&)
			: duplicates(false)
		{
			clean();
		}

		HashTable()
			: duplicates(false)
		{
			clean();
		}

		~HashTable()
		{
			// by default we let hash entries be cleaned by someone else
			cleanup(NULL);
		}

		typedef void CleanupRoutine(C* toClean);
		void cleanup(CleanupRoutine* cleanupRoutine)
		{
			for (FB_SIZE_T n = 0; n < HASHSIZE; ++n)
			{
				while (data[n])
				{
					Entry* entry = data[n];
					entry->unLink();
					if (cleanupRoutine)
						cleanupRoutine(entry->get());
				}
			}
		}

		void enableDuplicates()
		{
			duplicates = true;
		}

	private:
		Entry* data[HASHSIZE];
		bool duplicates;

		Entry** locate(const K& key, FB_SIZE_T h)
		{
			Entry** pointer = &data[h];
			while (*pointer)
			{
				if ((*pointer)->isEqual(key))
				{
					break;
				}
				pointer = (*pointer)->nextPtr();
			}

			return pointer;
		}

		Entry** locate(const K& key)
		{
			FB_SIZE_T hashValue = F::hash(key, HASHSIZE);
			fb_assert(hashValue < HASHSIZE);
			return locate(key, hashValue % HASHSIZE);
		}

	public:
		bool add(C* value)
		{
			Entry** e = locate(KeyOfValue::generate(*value));
			if ((!duplicates) && (*e))
			{
				return false;	// sorry, duplicate
			}
			value->link(e);
			return true;
		}

		C* lookup(const K& key)
		{
			Entry** e = locate(key);
			return (*e) ? (*e)->get() : NULL;
		}

		C* remove(const K& key)
		{
			Entry** e = locate(key);
			if (*e)
			{
				Entry* entry = *e;
				entry->unLink();
				return entry->get();
			}
			return NULL;
		}

	private:
		// disable use of default operator=
		HashTable& operator= (const HashTable&);

		void clean()
		{
			memset(data, 0, sizeof data);
		}

	public:
		class iterator
		{
		private:
			const HashTable* hash;
			FB_SIZE_T elem;
			Entry* current;

			iterator(const iterator& i);
			iterator& operator= (const iterator& i);

			void next()
			{
				while (!current)
				{
					if (++elem >= HASHSIZE)
					{
						break;
					}
					current = hash->data[elem];
				}
			}

		public:
			explicit iterator(const HashTable& h)
				: hash(&h), elem(0), current(hash->data[elem])
			{
				next();
			}

			iterator& operator++()
			{
				if (hasData())
				{
					current = current->next();
					next();
				}
				return *this;
			}

			bool hasData() const
			{
				return current ? true : false;
			}

			operator C*() const
			{
				fb_assert(hasData());
				return current->get();
			}

			C* operator ->() const
			{
				fb_assert(hasData());
				return current->get();
			}

			bool operator== (const iterator& h) const
			{
				return (hash == h.hash) && (elem == h.elem) && (current == h.current);
			}

			bool operator!= (const iterator& h) const
			{
				return !(*this == h);
			}
		}; // class iterator
	}; // class HashTable

	class InternalHash
	{
	public:
		static unsigned int hash(unsigned int length, const UCHAR* value);

		static unsigned int hash(unsigned int length, const UCHAR* value, unsigned int hashSize)
		{
			return hash(length, value) % hashSize;
		}
	};

	class HashContext
	{
	public:
		virtual ~HashContext()
		{
		}

	public:
		virtual void update(const void* data, FB_SIZE_T length) = 0;
		virtual void finish(dsc& result) = 0;
	};

	class WeakHashContext FB_FINAL : public HashContext
	{
	public:
		virtual void update(const void* data, FB_SIZE_T length);
		virtual void finish(dsc& result);

	private:
		SINT64 hashNumber = 0;
	};

	class LibTomCryptHashContext : public HashContext
	{
	public:
		struct Descriptor;

	private:
		struct State;

	protected:
		LibTomCryptHashContext(MemoryPool& pool, const Descriptor* descriptor);

	public:
		virtual ~LibTomCryptHashContext();

	public:
		virtual void update(const void* data, FB_SIZE_T length);
		virtual void finish(dsc& result);

	private:
		const Descriptor* descriptor;
		State* statePtr;
		UCharBuffer buffer;
	};

	class Md5HashContext FB_FINAL : public LibTomCryptHashContext
	{
	public:
		Md5HashContext(MemoryPool& pool);
	};

	class Sha1HashContext FB_FINAL : public LibTomCryptHashContext
	{
	public:
		Sha1HashContext(MemoryPool& pool);
	};

	class Sha256HashContext FB_FINAL : public LibTomCryptHashContext
	{
	public:
		Sha256HashContext(MemoryPool& pool);
	};

	class Sha512HashContext FB_FINAL : public LibTomCryptHashContext
	{
	public:
		Sha512HashContext(MemoryPool& pool);
	};

	class Crc32HashContext FB_FINAL : public HashContext
	{
	public:
		Crc32HashContext(MemoryPool& pool);
		~Crc32HashContext();

		virtual void update(const void* data, FB_SIZE_T length);
		virtual void finish(dsc& result);

	private:
		struct State;
		State* statePtr;
		SLONG hash;
	};

} // namespace Firebird

#endif // CLASSES_HASH_H
