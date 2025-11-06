/*
 *	PROGRAM:	Common Access Method
 *	MODULE:		init.h
 *	DESCRIPTION:	InitMutex, InitInstance - templates to help with initialization
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

#ifndef CLASSES_INIT_INSTANCE_H
#define CLASSES_INIT_INSTANCE_H

#include "fb_types.h"
#include "../common/classes/alloc.h"
#include <atomic>

namespace Firebird {

namespace StaticMutex
{
	// Support for common mutex for various inits
	extern Mutex* mutex;
	void create() noexcept;
	void release() noexcept;
}

// InstanceControl - interface for almost all global variables

class InstanceControl
{
public:
	enum DtorPriority
	{
		STARTING_PRIORITY,			// Not to be used out of class InstanceControl
		PRIORITY_DETECT_UNLOAD,
		PRIORITY_DELETE_FIRST,
		PRIORITY_REGULAR,
		PRIORITY_TLS_KEY
	};

    InstanceControl();

	//
	// GlobalPtr should not be directly derived from class with virtual functions -
	// virtual table for its instances may become invalid in the moment,
	// when cleanup is needed. Therefore indirect link via InstanceList and
	// InstanceLink is established. This means more calls to memory allocator,
	// but who cares for 100 global variables?
	//

	class InstanceList
	{
	public:
		explicit InstanceList(DtorPriority p);
		virtual ~InstanceList();
		static void destructors();

		// remove self from common list under StaticMutex protection
		void remove();

	private:
		virtual void dtor() = 0;
		void unlist() noexcept;

		InstanceList* next;
		InstanceList* prev;
		DtorPriority priority;
	};

	template <typename T, InstanceControl::DtorPriority P = InstanceControl::PRIORITY_REGULAR>
	class InstanceLink : private InstanceList, public GlobalStorage
	{
	private:
		T* link;

	public:
		explicit InstanceLink(T* l)
			: InstanceControl::InstanceList(P), link(l)
		{
			fb_assert(link);
		}

		void remove()
		{
			InstanceList::remove();
		}

		void dtor()
		{
			fb_assert(link);
			if (link)
			{
				link->dtor();
				link = NULL;
			}
		}
	};

public:
	static void destructors();
	static void registerGdsCleanup(FPTR_VOID cleanup);
	static void registerShutdown(FPTR_VOID shutdown);

	static void cancelCleanup() noexcept;
};


// GlobalPtr - template to help declaring global varables

template <typename T, InstanceControl::DtorPriority P = InstanceControl::PRIORITY_REGULAR>
class GlobalPtr : private InstanceControl
{
private:
	T* instance;

public:
	void dtor() noexcept
	{
		delete instance;
		instance = 0;
	}

	GlobalPtr()
	{
		// This means - for objects with ctors/dtors that want to be global,
		// provide ctor with MemoryPool& parameter. Even if it is ignored!
		instance = FB_NEW_POOL(*getDefaultMemoryPool()) T(*getDefaultMemoryPool());
		// Put ourselves into linked list for cleanup.
		// Allocated pointer is saved by InstanceList::constructor.
		FB_NEW InstanceControl::InstanceLink<GlobalPtr, P>(this);
	}

	T* operator->() noexcept
	{
		return instance;
	}
	operator T&() noexcept
	{
		return *instance;
	}
	T* operator&() noexcept
	{
		return instance;
	}

	operator bool() noexcept
	{
		return instance;
	}
};

// InitMutex - executes static void C::init() once and only once

template <typename C>
class InitMutex
{
private:
	std::atomic<bool> flag;
#ifdef DEV_BUILD
	const char* from;
#endif
public:
	explicit InitMutex(const char* f) noexcept
		: flag(false)
#ifdef DEV_BUILD
			  , from(f)
#define FB_LOCKED_FROM from
#else
#define FB_LOCKED_FROM NULL
#endif
	{ }
	void init()
	{
		if (!flag)
		{
			MutexLockGuard guard(*StaticMutex::mutex, FB_LOCKED_FROM);
			if (!flag)
			{
				C::init();
				flag = true;
			}
		}
	}
	void cleanup()
	{
		if (flag)
		{
			MutexLockGuard guard(*StaticMutex::mutex, FB_LOCKED_FROM);
			if (flag)
			{
				C::cleanup();
				flag = false;
			}
		}
	}
};
#undef FB_LOCKED_FROM

// InitInstance - allocate instance of class T on first request.

template <typename T>
class DefaultInstanceAllocator
{
public:
	static T* create()
	{
		return FB_NEW_POOL(*getDefaultMemoryPool()) T(*getDefaultMemoryPool());
	}

	static void destroy(T* inst) noexcept
	{
		delete inst;
	}
};

template <class I>
class DeleteInstance : private InstanceControl
{
public:
	void registerInstance(I* instance)
	{
		// Put ourselves into linked list for cleanup.
		// Allocated pointer is saved by InstanceList::constructor.
		FB_NEW InstanceControl::InstanceLink<I>(instance);
	}
};

template <class I>
class TraditionalDelete
{
public:
	TraditionalDelete()
		: instance(nullptr)
	{ }

	void registerInstance(I* inst)
	{
		fb_assert(!instance);
		instance = inst;
	}

	~TraditionalDelete()
	{
		if (instance)
			instance->dtor();
	}

private:
	I* instance;
};

template <typename T, class A = DefaultInstanceAllocator<T>, template <class I> class DestroyControl = DeleteInstance >
class InitInstance : private DestroyControl<InitInstance<T, A, DestroyControl> >
{
private:
	T* instance;
	std::atomic<bool> flag;
	A allocator;

public:
	InitInstance() noexcept
		: instance(NULL), flag(false)
	{ }

	T& operator()()
	{
		if (!flag)
		{
			MutexLockGuard guard(*StaticMutex::mutex, "InitInstance");
			if (!flag)
			{
				instance = allocator.create();
				flag = true;
				DestroyControl<InitInstance<T, A, DestroyControl> >::registerInstance(this);
			}
		}
		return *instance;
	}

	void dtor()
	{
		MutexLockGuard guard(*StaticMutex::mutex, "InitInstance - dtor");
		flag = false;
		allocator.destroy(instance);
		instance = NULL;
	}
};

template <typename T>
class DefaultNonLazyInstanceAllocator
{
public:
	template <typename... TArgs>
	static T* create(TArgs&&... args)
	{
		return FB_NEW_POOL(*getDefaultMemoryPool()) T(std::forward<TArgs>(args)...);
	}

	static void destroy(T* inst) noexcept
	{
		delete inst;
	}
};

// Best use case is for static local variable
template <typename T, class TAllocator = DefaultNonLazyInstanceAllocator<T>, template <class I> class DestroyControl = DeleteInstance >
class NonLazyInitInstance : private DestroyControl<NonLazyInitInstance<T, TAllocator, DestroyControl> >
{
public:
	template <typename... TArgs>
	NonLazyInitInstance(TArgs&&... args)
	{
		MutexLockGuard guard(*StaticMutex::mutex, "NonLazyInitInstance");
		m_instance = TAllocator::create(std::forward<TArgs>(args)...);
		DestroyControl<std::remove_cvref_t<decltype(*this)>>::registerInstance(this);
	}

	T& operator()() noexcept
	{
		return *m_instance;
	}
	const T& operator()() const noexcept
	{
		return *m_instance;
	}

	T* get() noexcept
	{
		return m_instance;
	}
	const T* get() const noexcept
	{
		return m_instance;
	}

	void dtor()
	{
		MutexLockGuard guard(*StaticMutex::mutex, "NonLazyInitInstance - dtor");
		TAllocator::destroy(m_instance);
		m_instance = nullptr;
	}

private:
	T* m_instance = nullptr;
};

// Static - create instance of some class in static char[] buffer. Never destroy it.

template <typename T>
class StaticInstanceAllocator
{
private:
	alignas(alignof(T)) char buf[sizeof(T)];

public:
	T* create()
	{
		return new(buf) T();
	}

	static void destroy(T*)
	{ }
};

template <typename T>
class Static : private InitInstance<T, StaticInstanceAllocator<T> >
{
public:
	Static()
	{ }

	T* operator->()
	{
		return &(this->operator()());
	}

	T* operator&()
	{
		return operator->();
	}
};

} //namespace Firebird

#endif // CLASSES_INIT_INSTANCE_H
