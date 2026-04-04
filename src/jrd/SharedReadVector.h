/*
 *	PROGRAM:	Engine Code
 *	MODULE:		SharedReadVector.h
 *	DESCRIPTION:	Shared read here means that any thread
 *			can read from vector using HP to it's generation.
 *			Vector can be modified only in single thread,
 *			and it's caller's responsibility that modifying
 *			thread is single.
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2021 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef JRD_SHAREDREADVECTOR_H
#define JRD_SHAREDREADVECTOR_H

#include "../jrd/HazardPtr.h"

/*
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/auto.h"
*/
#include "fb_blk.h"
/*
#include <utility>

#include "../jrd/tdbb.h"
#include "../jrd/Database.h"
*/

namespace Jrd {

template <typename T, FB_SIZE_T CAP>
class SharedReadVector
{
public:
	class Generation : public HazardObject, public pool_alloc_rpt<T>
	{
	private:
		Generation(FB_SIZE_T size)
			: count(0), capacity(size)
		{ }

		FB_SIZE_T count, capacity;
		T data[1];

	public:
		static Generation* create(FB_SIZE_T cap)
		{
			auto* rc = FB_NEW_RPT(*getDefaultMemoryPool(), cap) Generation(cap);
#ifdef DEBUG_SHARED_VECTOR
			void srvAcc(void*);
			srvAcc(rc);
#endif // DEBUG_SHARED_VECTOR
			return rc;;
		}

		FB_SIZE_T getCount() const
		{
			return count;
		}

		FB_SIZE_T getCapacity() const
		{
			return capacity;
		}

		T* begin()
		{
			return &data[0];
		}

		T* end()
		{
			return &data[count];
		}

		const T& value(FB_SIZE_T i) const
		{
			fb_assert(i < count);
			return data[i];
		}

		T& value(FB_SIZE_T i)
		{
			fb_assert(i < count);
			return data[i];
		}

		bool hasSpace(FB_SIZE_T needs = 1) const
		{
			return count + needs <= capacity;
		}

		bool add(const Generation* from)
		{
			if (!hasSpace(from->count))
				return false;
			memcpy(&data[count], from->data, from->count * sizeof(T));
			count += from->count;
			return true;
		}

		T* addStart()
		{
			if (!hasSpace())
				return nullptr;
			return &data[count];
		}

		void addComplete()
		{
			++count;
		}

		void truncate(const T& notValue)
		{
			while (count && data[count - 1] == notValue)
				count--;
		}

		void clear()
		{
			count = 0;
		}

		void grow(const FB_SIZE_T newCount)
		{
			fb_assert(newCount <= capacity);
			if (newCount > count)
			{
				memset(data + count, 0, sizeof(T) * (newCount - count));
				count = newCount;
			}
		}

		static void destroy(Generation* gen)
		{
			// delay delete - someone else may access it
#ifdef DEBUG_SHARED_VECTOR
			void srvDis(void*);
			srvDis(gen);
#endif // DEBUG_SHARED_VECTOR
			gen->retire();
		}
	};

	typedef HazardPtr<Generation> ReadAccessor;
	typedef Generation* WriteAccessor;

	SharedReadVector()
		: currentData(Generation::create(CAP))
	{ }

	WriteAccessor writeAccessor()
	{
		return currentData.load(std::memory_order_acquire);
	}

	ReadAccessor readAccessor() const
	{
		return HazardPtr<Generation>(currentData);
	}

	void grow(FB_SIZE_T const newSize, bool arrGrow)
	{
		// decide how much vector grows
		Generation* const oldGeneration = writeAccessor();
		FB_SIZE_T cap = oldGeneration->getCapacity();
		FB_SIZE_T doubleSize = oldGeneration->getCapacity() * 2;
		if (newSize > doubleSize)
			doubleSize = newSize;
		FB_SIZE_T singleSize = newSize ? newSize : doubleSize;

		if (cap >= singleSize)
		{
			// grow old generation inplace
			if (arrGrow)
				oldGeneration->grow(singleSize);
			return;
		}

		// create new generation and store it in the vector
		Generation* const newGeneration = Generation::create(doubleSize);
		newGeneration->add(oldGeneration);
		if (arrGrow)
			newGeneration->grow(singleSize);
		currentData.store(newGeneration, std::memory_order_release);

		// cleanup
		Generation::destroy(oldGeneration);
	}

	~SharedReadVector()
	{
		Generation::destroy(currentData.load(std::memory_order_acquire));
	}

	void clear()
	{
		// expected to be called when going to destroy an object
		writeAccessor()->clear();
	}

	bool hasData()
	{
		return readAccessor()->getCount() != 0;
	}

private:
	std::atomic<Generation*> currentData;
};

} // namespace Jrd

#endif // JRD_SHAREDREADVECTOR_H
