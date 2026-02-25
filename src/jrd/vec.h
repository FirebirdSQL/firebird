/*
 *      PROGRAM:        JRD access method
 *      MODULE:         vec.h
 *      DESCRIPTION:    General purpose vector
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
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 */

#ifndef JRD_VEC_H
#define JRD_VEC_H

#include "fb_blk.h"
#include "../common/ThreadData.h"
#include "../common/classes/array.h"

namespace Jrd {

// general purpose vector
template <class T, BlockType TYPE = type_vec>
class vec_base : protected pool_alloc<TYPE>
{
public:
	typedef typename Firebird::Array<T>::iterator iterator;
	typedef typename Firebird::Array<T>::const_iterator const_iterator;

	/*
	static vec_base* newVector(MemoryPool& p, int len)
	{
		return FB_NEW_POOL(p) vec_base<T, TYPE>(p, len);
	}

	static vec_base* newVector(MemoryPool& p, const vec_base& base)
	{
		return FB_NEW_POOL(p) vec_base<T, TYPE>(p, base);
	}
	*/

	FB_SIZE_T count() const { return v.getCount(); }
	T& operator[](FB_SIZE_T index) { return v[index]; }
	const T& operator[](FB_SIZE_T index) const { return v[index]; }

	iterator begin() { return v.begin(); }
	iterator end() { return v.end(); }

	const_iterator begin() const { return v.begin(); }
	const_iterator end() const { return v.end(); }

	void clear() { v.clear(); }

	T* memPtr() { return &v[0]; }

	void resize(FB_SIZE_T n, T val = T()) { v.resize(n, val); }

	void operator delete(void* mem) { MemoryPool::globalFree(mem); }

	// strip unused nulls in the end
	void trimNulls()
	{
		auto* e = v.end();
		for (; e != v.begin(); --e)
		{
			if (e[-1])
				break;
		}

		if (e != v.end())
			v.resize(e - v.begin());
	}

protected:
	vec_base(MemoryPool& p, int len)
		: v(p, len)
	{
		v.resize(len);
	}

	vec_base(MemoryPool& p, const vec_base& base)
		: v(p)
	{
		v = base.v;
	}

private:
	Firebird::Array<T> v;
};

template <typename T>
class vec : public vec_base<T, type_vec>
{
public:
	static vec* newVector(MemoryPool& p, int len)
	{
		return FB_NEW_POOL(p) vec<T>(p, len);
	}

	static vec* newVector(MemoryPool& p, const vec& base)
	{
		return FB_NEW_POOL(p) vec<T>(p, base);
	}

	static vec* newVector(MemoryPool& p, vec* base, int len)
	{
		if (!base)
			base = FB_NEW_POOL(p) vec<T>(p, len);
		else if (len > (int) base->count())
			base->resize(len);
		return base;
	}

private:
	vec(MemoryPool& p, int len) : vec_base<T, type_vec>(p, len) {}
	vec(MemoryPool& p, const vec& base) : vec_base<T, type_vec>(p, base) {}
};

class vcl : public vec_base<ULONG, type_vcl>
{
public:
	static vcl* newVector(MemoryPool& p, int len)
	{
		return FB_NEW_POOL(p) vcl(p, len);
	}

	static vcl* newVector(MemoryPool& p, const vcl& base)
	{
		return FB_NEW_POOL(p) vcl(p, base);
	}

	static vcl* newVector(MemoryPool& p, vcl* base, int len)
	{
		if (!base)
			base = FB_NEW_POOL(p) vcl(p, len);
		else if (len > (int) base->count())
			base->resize(len);
		return base;
	}

private:
	vcl(MemoryPool& p, int len) : vec_base<ULONG, type_vcl>(p, len) {}
	vcl(MemoryPool& p, const vcl& base) : vec_base<ULONG, type_vcl>(p, base) {}
};

typedef vec<TraNumber> TransactionsVector;

// Threading macros

class Database;
class thread_db;

} // namespace Jrd

/* Define JRD_get_thread_data off the platform specific version.
 * If we're in DEV mode, also do consistancy checks on the
 * retrieved memory structure.  This was originally done to
 * track down cases of no "PUT_THREAD_DATA" on the NLM.
 *
 * This allows for NULL thread data (which might be an error by itself)
 * If there is thread data,
 * AND it is tagged as being a thread_db.
 * AND it has a non-NULL database field,
 * THEN we validate that the structure there is a database block.
 * Otherwise, we return what we got.
 * We can't always validate the database field, as during initialization
 * there is no database set up.
 */

#if defined(DEV_BUILD)

Jrd::thread_db* JRD_get_thread_data();
void CHECK_TDBB(const Jrd::thread_db* tdbb);
void CHECK_DBB(const Jrd::Database* dbb);

#else // PROD_BUILD

inline Jrd::thread_db* JRD_get_thread_data()
{
	return (Jrd::thread_db*) Firebird::ThreadData::getSpecific();
}

inline void CHECK_DBB(const Jrd::Database*)
{
}

inline void CHECK_TDBB(const Jrd::thread_db*)
{
}

#endif

#endif // JRD_VEC_H

