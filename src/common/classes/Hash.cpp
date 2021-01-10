/*
 *	PROGRAM:	Common Library
 *	MODULE:		Hash.cpp
 *	DESCRIPTION:	Hash of data
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  The Original Code was created by Dmitry Sibiryakov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Dmitry Sibiryakov
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../common/classes/Hash.h"
#include "../common/dsc.h"

#if defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

using namespace Firebird;

unsigned int CRC32C(unsigned int length, const unsigned char* value);

namespace
{
	typedef unsigned int (*hash_func_t)(unsigned int length, const UCHAR* value);

	unsigned int basicHash(unsigned int length, const UCHAR* value)
	{
		unsigned int hash_value = 0;

		UCHAR* const p = (UCHAR*) &hash_value;
		const UCHAR* q = value;

		while (length >= 4)
		{
			p[0] += q[0];
			p[1] += q[1];
			p[2] += q[2];
			p[3] += q[3];
			length -= 4;
			q += 4;
		}

		if (length >= 2)
		{
			p[0] += q[0];
			p[1] += q[1];
			length -= 2;
			q += 2;
		}

		if (length)
		{
			*p += *q;
		}

		return hash_value;
	}

#if defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386__)

	bool SSE4_2Supported()
	{
#ifdef _MSC_VER
		const int bit_SSE4_2 = 1 << 20;
		// MS VC has its own definition of __cpuid
		int flags[4];
		__cpuid(flags, 1);
		return (flags[2] & bit_SSE4_2) != 0;
#else
#if defined(__clang__) && !defined(bit_SSE4_2)
		const int bit_SSE4_2 = bit_SSE42;
#endif

		// GCC - its own
		unsigned int eax,ebx,ecx,edx;
		__cpuid(1, eax, ebx, ecx, edx);
		return (ecx & bit_SSE4_2) != 0;
#endif
	}

	hash_func_t internalHash = SSE4_2Supported() ? CRC32C : basicHash;

#else	// architecture check

	hash_func_t internalHash = basicHash;

#endif	// architecture check

}

unsigned int InternalHash::hash(unsigned int length, const UCHAR* value)
{
	return internalHash(length, value);
}


void WeakHashContext::update(const void* data, FB_SIZE_T length)
{
	const UCHAR* p = static_cast<const UCHAR*>(data);

	for (const UCHAR* end = p + length; p != end; ++p)
	{
		hashNumber = (hashNumber << 4) + *p;

		const SINT64 n = hashNumber & FB_CONST64(0xF000000000000000);
		if (n)
			hashNumber ^= n >> 56;

		hashNumber &= ~n;
	}
}

void WeakHashContext::finish(dsc& result)
{
	result.makeInt64(0, &hashNumber);
}
