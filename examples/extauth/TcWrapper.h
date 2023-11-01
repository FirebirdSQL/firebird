/*
 * Tomcrypt library <= firebird : c++ wrapper.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <tomcrypt.h>

#include <firebird/Interface.h>
using namespace Firebird;

#include <stdio.h>

void error(ThrowStatusWrapper* status, const char* text);
void check(ThrowStatusWrapper* status, int err, const char* text);
unsigned readHexKey(ThrowStatusWrapper* status, const char* hex, unsigned char* key, unsigned bufSize);

class PseudoRandom
{
public:
#if CRYPT > 0x0100
	typedef ltc_prng_descriptor PrngDescriptor;
#else
	typedef _prng_descriptor PrngDescriptor;
#endif

	void init(ThrowStatusWrapper* status);
	void fini();
	const PrngDescriptor* getDsc();

	int index;
	prng_state state;
};

class Hash
{
protected:
	void init(ThrowStatusWrapper* status, const ltc_hash_descriptor* desc);

public:
	void fini()
	{ }

	int index;
};

class HashSha1 : public Hash
{
public:
	void init(ThrowStatusWrapper* status)
	{
		Hash::init(status, &sha1_desc);
	}
};

class HashSha256 : public Hash
{
public:
	void init(ThrowStatusWrapper* status)
	{
		Hash::init(status, &sha256_desc);
	}
};

// controls reference counter of the object where points
enum NoIncrement {NO_INCREMENT};
template <typename T>
class RefPtr
{
public:
	RefPtr() : ptr(NULL)
	{ }

	explicit RefPtr(T* p) : ptr(p)
	{
		if (ptr)
		{
			ptr->addRef();
		}
	}

	// This special form of ctor is used to create refcounted ptr from interface,
	// returned by a function (which increments counter on return)
	RefPtr(NoIncrement x, T* p) : ptr(p)
	{ }

	RefPtr(const RefPtr& r) : ptr(r.ptr)
	{
		if (ptr)
		{
			ptr->addRef();
		}
	}

	~RefPtr()
	{
		if (ptr)
		{
			ptr->release();
		}
	}

	T* operator=(T* p)
	{
		return assign(p);
	}

	T* operator=(const RefPtr& r)
	{
		return assign(r.ptr);
	}

	operator T*()
	{
		return ptr;
	}

	T* operator->()
	{
		return ptr;
	}

	operator const T*() const
	{
		return ptr;
	}

	const T* operator->() const
	{
		return ptr;
	}

	bool operator !() const
	{
		return !ptr;
	}

	bool operator ==(const RefPtr& r) const
	{
		return ptr == r.ptr;
	}

	bool operator !=(const RefPtr& r) const
	{
		return ptr != r.ptr;
	}

	void clear() noexcept	// Used after detach/commit/close/etc., i.e. release() not needed
	{
		ptr = NULL;
	}

	void assignNoIncrement(T* const p)
	{
		assign(NULL);
		ptr = p;
	}

private:
	T* assign(T* const p)
	{
		if (ptr != p)
		{
			if (p)
			{
				p->addRef();
			}

			T* tmp = ptr;
			ptr = p;

			if (tmp)
			{
				tmp->release();
			}
		}

		return ptr;
	}

	T* ptr;
};

// Often used form of RefPtr
template <class R>
class AutoRelease : public RefPtr<R>
{
public:
	AutoRelease(R* r)
		: RefPtr<R>(NO_INCREMENT, r)
	{ }
};
