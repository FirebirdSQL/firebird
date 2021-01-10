/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		alloc.h
 *	DESCRIPTION:	Memory Pool Manager
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
 *  STL allocator is based on one by Mike Nordell and John Bellardo
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *
 *  The Original Code was created by James A. Starkey for IBPhoenix.
 *
 *  Copyright (c) 2004 James A. Starkey
 *  All Rights Reserved.
 *
 *  Contributor(s):
 *
 *		Alex Peshkoff <peshkoff@mail.ru>
 *				1. added PermanentStorage and AutoStorage classes.
 *				2. merged parts of Nickolay and Jim code to be used together
 *				3. reworked code to avoid slow behavior for medium-size blocks
 *				   and high memory usage for just created pool
 *
 */

#ifndef CLASSES_ALLOC_H
#define CLASSES_ALLOC_H

#include "firebird.h"
#include "fb_types.h"
#include "../common/classes/locks.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_atomic.h"

#include <stdio.h>

#if defined(MVS) || defined(DARWIN) || defined(__clang__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <memory.h>

#define OOM_EXCEPTION std::bad_alloc

#if __cplusplus >= 201103L
#define FB_NO_THROW_SPECIFIER
#endif

#ifdef FB_NO_THROW_SPECIFIER
#define FB_THROW(x)
#define FB_NOTHROW noexcept
#else
#define FB_THROW(x) throw(x)
#define FB_NOTHROW throw()
#endif

#ifdef DEBUG_GDS_ALLOC
#define FB_NEW new(__FILE__, __LINE__)
#define FB_NEW_POOL(pool) new(pool, __FILE__, __LINE__)
#define FB_NEW_RPT(pool, count) new(pool, count, __FILE__, __LINE__)
#else // DEBUG_GDS_ALLOC
#define FB_NEW new
#define FB_NEW_POOL(pool) new(pool)
#define FB_NEW_RPT(pool, count) new(pool, count)
#endif // DEBUG_GDS_ALLOC

namespace Firebird {

// Alignment for all memory blocks
//#define ALLOC_ALIGNMENT 8
#define ALLOC_ALIGNMENT 16

static inline size_t MEM_ALIGN(size_t value)
{
	return FB_ALIGN(value, ALLOC_ALIGNMENT);
}


class MemPool;

class MemoryStats
{
public:
	explicit MemoryStats(MemoryStats* parent = NULL)
		: mst_parent(parent), mst_usage(0), mst_mapped(0), mst_max_usage(0), mst_max_mapped(0)
	{}

	~MemoryStats()
	{}

	size_t getCurrentUsage() const FB_NOTHROW { return mst_usage.value(); }
	size_t getMaximumUsage() const FB_NOTHROW { return mst_max_usage; }
	size_t getCurrentMapping() const FB_NOTHROW { return mst_mapped.value(); }
	size_t getMaximumMapping() const FB_NOTHROW { return mst_max_mapped; }

private:
	// Forbid copying/assignment
	MemoryStats(const MemoryStats&);
	MemoryStats& operator=(const MemoryStats&);

	MemoryStats* mst_parent;

	// Currently allocated memory (without allocator overhead)
	// Useful for monitoring engine memory leaks
	AtomicCounter mst_usage;
	// Amount of memory mapped (including all overheads)
	// Useful for monitoring OS memory consumption
	AtomicCounter mst_mapped;

	// We don't particularily care about extreme precision of these max values,
	// this is why we don't synchronize them
	size_t mst_max_usage;
	size_t mst_max_mapped;

	// These methods are thread-safe due to usage of atomic counters only
	void increment_usage(size_t size) FB_NOTHROW
	{
		for (MemoryStats* statistics = this; statistics; statistics = statistics->mst_parent)
		{
			const size_t temp = statistics->mst_usage.exchangeAdd(size) + size;
			if (temp > statistics->mst_max_usage)
				statistics->mst_max_usage = temp;
		}
	}

	void decrement_usage(size_t size) FB_NOTHROW
	{
		for (MemoryStats* statistics = this; statistics; statistics = statistics->mst_parent)
		{
			statistics->mst_usage -= size;
		}
	}

	void increment_mapping(size_t size) FB_NOTHROW
	{
		for (MemoryStats* statistics = this; statistics; statistics = statistics->mst_parent)
		{
			const size_t temp = statistics->mst_mapped.exchangeAdd(size) + size;
			if (temp > statistics->mst_max_mapped)
				statistics->mst_max_mapped = temp;
		}
	}

	void decrement_mapping(size_t size) FB_NOTHROW
	{
		for (MemoryStats* statistics = this; statistics; statistics = statistics->mst_parent)
		{
			statistics->mst_mapped -= size;
		}
	}

	friend class MemPool;
};


class MemoryPool
{
private:
	MemPool* pool;

	MemoryPool(MemPool* p)
		: pool(p)
	{ }

	// Default statistics group for process
	static MemoryStats* default_stats_group;

public:
	// This is maximum block size which is cached (not allocated directly from OS)
	enum RecommendedBufferSize { MAX_MEDIUM_BLOCK_SIZE = 64384 };	// MediumLimits::TOP_LIMIT - 128

	static MemoryPool* defaultMemoryManager;

public:
	// Create memory pool instance
	static MemoryPool* createPool(MemoryPool* parent = NULL, MemoryStats& stats = *default_stats_group);
	// Delete memory pool instance
	static void deletePool(MemoryPool* pool);

#ifdef DEBUG_GDS_ALLOC
#define ALLOC_ARGS , __FILE__, __LINE__
#define ALLOC_PARAMS , const char* file, int line
#define ALLOC_PASS_ARGS , file, line
#else
#define ALLOC_ARGS
#define ALLOC_PARAMS
#define ALLOC_PASS_ARGS
#endif // DEBUG_GDS_ALLOC

	void* calloc(size_t size ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION);

#ifdef LIBC_CALLS_NEW
	static void* globalAlloc(size_t s ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION);
#else
	static void* globalAlloc(size_t s ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION)
	{
		return defaultMemoryManager->allocate(s ALLOC_PASS_ARGS);
	}
#endif // LIBC_CALLS_NEW

	void* allocate(size_t size ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION);

	static void globalFree(void* mem) FB_NOTHROW;
	void deallocate(void* mem) FB_NOTHROW;

	// Set context pool for current thread of execution
	static MemoryPool* setContextPool(MemoryPool* newPool);

	// Get context pool for current thread of execution
	static MemoryPool* getContextPool();

	// Set statistics group for pool. Usage counters will be decremented from
	// previously set group and added to new
	void setStatsGroup(MemoryStats& stats) FB_NOTHROW;

	// Initialize and finalize global memory pool
	static void init();
	static void cleanup();

	// Initialize context pool
	static void contextPoolInit();

	// Print out pool contents. This is debugging routine
	static const unsigned PRINT_USED_ONLY = 0x01;
	static const unsigned PRINT_RECURSIVE = 0x02;
	void print_contents(FILE*, unsigned flags = 0, const char* filter_path = 0) FB_NOTHROW;
	// The same routine, but more easily callable from the debugger
	void print_contents(const char* filename, unsigned flags = 0, const char* filter_path = 0) FB_NOTHROW;

public:
	struct Finalizer
	{
		virtual ~Finalizer()
		{
		}

		virtual void finalize() = 0;

		Finalizer* prev = nullptr;
		Finalizer* next = nullptr;
	};

	template <typename T> Finalizer* registerFinalizer(void (*func)(T*), T* object)
	{
		struct FinalizerImpl : Finalizer
		{
			FinalizerImpl(void (*aFunc)(T*), T* aObject)
				: func(aFunc),
				  object(aObject)
			{
			}

			void finalize() override
			{
				func(object);
			}

			void (*func)(T*);
			T* object;
		};

		fb_assert(func);
		FinalizerImpl* finalizer = FB_NEW_POOL(*this) FinalizerImpl(func, object);

		internalRegisterFinalizer(finalizer);

		return finalizer;
	}

	void unregisterFinalizer(Finalizer*& finalizer);

private:
	void internalRegisterFinalizer(Finalizer* entry);

private:
	Finalizer* finalizers = nullptr;

	friend class MemPool;
};

} // namespace Firebird

static inline Firebird::MemoryPool* getDefaultMemoryPool() FB_NOTHROW
{
	fb_assert(Firebird::MemoryPool::defaultMemoryManager);
	return Firebird::MemoryPool::defaultMemoryManager;
}

namespace Firebird {

// Class intended to manage execution context pool stack
// Declare instance of this class when you need to set new context pool and it
// will be restored automatically as soon holder variable gets out of scope
class ContextPoolHolder
{
public:
	explicit ContextPoolHolder(MemoryPool* newPool)
	{
		savedPool = MemoryPool::setContextPool(newPool);
	}
	~ContextPoolHolder()
	{
		MemoryPool::setContextPool(savedPool);
	}
private:
	MemoryPool* savedPool;
};

// template enabling common use of old and new pools control code
// to be dropped when old-style code goes away
template <typename SubsystemThreadData, typename SubsystemPool>
class SubsystemContextPoolHolder : public ContextPoolHolder
{
public:
	SubsystemContextPoolHolder <SubsystemThreadData, SubsystemPool>
	(
		SubsystemThreadData* subThreadData,
		SubsystemPool* newPool
	)
		: ContextPoolHolder(newPool),
		savedThreadData(subThreadData),
		savedPool(savedThreadData->getDefaultPool())
	{
		savedThreadData->setDefaultPool(newPool);
	}
	~SubsystemContextPoolHolder()
	{
		savedThreadData->setDefaultPool(savedPool);
	}
private:
	SubsystemThreadData* savedThreadData;
	SubsystemPool* savedPool;
};

} // namespace Firebird

using Firebird::MemoryPool;

// operators new and delete
extern void* operator new(size_t s ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION);
extern void* operator new[](size_t s ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION);
extern void operator delete(void* mem ALLOC_PARAMS) FB_NOTHROW;
extern void operator delete[](void* mem ALLOC_PARAMS) FB_NOTHROW;


inline void* operator new(size_t s, Firebird::MemoryPool& pool ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION)
{
	return pool.allocate(s ALLOC_PASS_ARGS);
}
inline void* operator new[](size_t s, Firebird::MemoryPool& pool ALLOC_PARAMS) FB_THROW (OOM_EXCEPTION)
{
	return pool.allocate(s ALLOC_PASS_ARGS);
}

inline void operator delete(void* mem, Firebird::MemoryPool& pool ALLOC_PARAMS) FB_NOTHROW
{
	MemoryPool::globalFree(mem);
}
inline void operator delete[](void* mem, Firebird::MemoryPool& pool ALLOC_PARAMS) FB_NOTHROW
{
	MemoryPool::globalFree(mem);
}

#if __cplusplus >= 201402L
inline void operator delete(void* mem, std::size_t s ALLOC_PARAMS) FB_NOTHROW
{
	MemoryPool::globalFree(mem);
}
inline void operator delete[](void* mem, std::size_t s ALLOC_PARAMS) FB_NOTHROW
{
	MemoryPool::globalFree(mem);
}
#endif

#ifdef DEBUG_GDS_ALLOC

extern void operator delete(void* mem) FB_NOTHROW;
extern void operator delete[](void* mem) FB_NOTHROW;

#endif // DEBUG_GDS_ALLOC

namespace Firebird
{
	// Global storage makes it possible to use new and delete for classes,
	// based on it, to behave traditionally, i.e. get memory from permanent pool.
	class GlobalStorage
	{
	public:
		void* operator new(size_t size ALLOC_PARAMS)
		{
			return getDefaultMemoryPool()->allocate(size ALLOC_PASS_ARGS);
		}

		void operator delete(void* mem)
		{
			getDefaultMemoryPool()->deallocate(mem);
		}

		MemoryPool& getPool() const
		{
			return *getDefaultMemoryPool();
		}
	};


	// Permanent storage is used as base class for all objects,
	// performing memory allocation in methods other than
	// constructors of this objects. Permanent means that pool,
	// which will be later used for such allocations, must
	// be explicitly passed in all constructors of such object.
	class PermanentStorage
	{
	protected:
		explicit PermanentStorage(MemoryPool& p) : pool(p) { }

	public:
		MemoryPool& getPool() const { return pool; }

	private:
		MemoryPool& pool;
	};

	// Automatic storage is used as base class for objects,
	// that may have constructors without explicit MemoryPool
	// parameter. In this case AutoStorage sends AutoMemoryPool
	// to PermanentStorage. To ensure this operation to be safe
	// such trick possible only for local (on stack) variables.
	class AutoStorage : public PermanentStorage
	{
	private:
#if defined(DEV_BUILD)
		void ProbeStack() const;
#endif
	public:
		static MemoryPool& getAutoMemoryPool();
	protected:
		AutoStorage()
			: PermanentStorage(getAutoMemoryPool())
		{
#if defined(DEV_BUILD)
			ProbeStack();
#endif
		}
		explicit AutoStorage(MemoryPool& p) : PermanentStorage(p) { }
	};

	template <>
	inline void SimpleDelete<MemoryPool>::clear(MemoryPool* pool)
	{
		if (pool)
			MemoryPool::deletePool(pool);
	}

	typedef AutoPtr<MemoryPool> AutoMemoryPool;

} // namespace Firebird


#endif // CLASSES_ALLOC_H
