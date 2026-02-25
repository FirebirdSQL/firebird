/*
 *	PROGRAM:	Engine Code
 *	MODULE:		HazardPtr.h
 *	DESCRIPTION:	Generic hazard pointers based on DHP::Guard from CDS.
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

#ifndef JRD_HAZARDPTR_H
#define JRD_HAZARDPTR_H

#include "../common/gdsassert.h"

#include <cds/gc/dhp.h>
#include <cds/algo/atomic.h>

#include <type_traits>

extern atomics::atomic<int> totHaz;

namespace Jrd {

class HazardObject
{
protected:
	void retire()
	{
		struct Disposer
		{
			void operator()(HazardObject* hazardObject)
			{
				fb_assert(hazardObject);
				delete hazardObject;
			}
		};

		cds::gc::DHP::retire<Disposer>(this);
	}

	virtual ~HazardObject()
	{ }
};

template <typename T>
class HazardPtr : private cds::gc::DHP::Guard
{
	typedef cds::gc::DHP::Guard inherited;
	static_assert(std::is_base_of<HazardObject, T>::value, "class derived from HazardObject should be used");

public:
	HazardPtr()
	{ }

	HazardPtr(const std::atomic<T*>& from)
	{
		protect(from);
	}

	HazardPtr(const HazardPtr& copyFrom)
	{
		copy(copyFrom);
	}

	HazardPtr(HazardPtr&& moveFrom) = default;

	template <class T2>
	HazardPtr(const HazardPtr<T2>& copyFrom)
	{
		copy(copyFrom);
	}

	~HazardPtr()
	{
		clear();
	}

private:
	// talk to CDS block
	void clear()
	{
		if (localPtr)
		{
			localPtr = nullptr;
			inherited::clear();
		}
	}

	template <typename T2>
	void copy(const HazardPtr<T2>& copyFrom)
	{
		checkAssign<T2>();
		localPtr = copyFrom.localPtr;
		inherited::copy(copyFrom);
	}

    T* protect(const std::atomic<T*>& from)
    {
    	localPtr = inherited::protect(from);
    	return localPtr;
    }

	void assign(T* val)
	{
		localPtr = inherited::assign(val);
	}

public:
	template <class T2>
	HazardPtr(HazardPtr<T2>&& moveFrom)
		: inherited(std::move(moveFrom))
	{
		checkAssign<T2>();
		localPtr = moveFrom.localPtr;
	}

	template <class T2>
	HazardPtr& operator=(HazardPtr<T2>&& moveAssign)
	{
		checkAssign<T2>();
		inherited::operator=(std::move(moveAssign));
		localPtr = moveAssign.localPtr;
		return *this;
	}
	// end CDS talk

	T* getPointer() const
	{
		return localPtr;
	}

	T* releasePointer()
	{
		T* rc = getPointer();
		clear();
		return rc;
	}

	void set(const std::atomic<T*>& from)
	{
		protect(from);
	}

	// atomically replaces 'where' with 'newVal', using *this as old value for comparison
	// sets *this to actual data from 'where' if replace failed
	bool replace(std::atomic<T*>& where, T* newVal)
	{
		T* val = getPointer();
		bool rc = where.compare_exchange_strong(val, newVal,
			std::memory_order_release, std::memory_order_acquire);
		if (!rc)
			assign(val);
		return rc;
	}

	T* operator->()
	{
		return getPointer();
	}

	const T* operator->() const
	{
		return getPointer();
	}
/*
	template <typename R>
	R& operator->*(R T::*mem)
	{
		return (this->hazardPointer)->*mem;
	}
 */
	bool operator!() const
	{
		return !hasData();
	}

	bool hasData() const
	{
		return localPtr != nullptr;
	}

	bool operator==(const T* v) const
	{
		return getPointer() == v;
	}

	bool operator!=(const T* v) const
	{
		return getPointer() != v;
	}

	operator bool() const
	{
		return hasData();
	}

	HazardPtr& operator=(const HazardPtr& copyAssign)
	{
		copy(copyAssign);
		return *this;
	}

	template <class T2>
	HazardPtr& operator=(const HazardPtr<T2>& copyAssign)
	{
		copy(copyAssign);
		return *this;
	}

	HazardPtr& operator=(HazardPtr&& moveAssign) = default;

private:
	template <class T2>
	struct checkAssign
	{
		static_assert(std::is_trivially_assignable<T*&, T2*>::value, "Invalid type of pointer assigned");
	};

	T* localPtr = nullptr;
};

template <typename T>
bool operator==(const T* v1, const HazardPtr<T> v2)
{
	return v2 == v1;
}

template <typename T, typename T2>
bool operator==(const T* v1, const HazardPtr<T2> v2)
{
	return v1 == v2.getPointer();
}

template <typename T>
bool operator!=(const T* v1, const HazardPtr<T> v2)
{
	return v2 != v1;
}

} // namespace Jrd

#endif // JRD_HAZARDPTR_H
