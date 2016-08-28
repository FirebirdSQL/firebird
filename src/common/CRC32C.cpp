/*
 *	PROGRAM:	Common Library
 *	MODULE:		CRC32C.cpp
 *	DESCRIPTION:	Hardware-accelerated hash calculation
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

// Can be used only on x86 architectures
// WARNING: With GCC must be compiled separately with -msse4.2 flag
#if defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386__)

#include <nmmintrin.h>

unsigned int CRC32C(unsigned int length, const unsigned char* value)
{
	unsigned int hash_value = 0;

	if (length == 1)
		return _mm_crc32_u8(hash_value, *value);

	if (length == 2)
		return _mm_crc32_u16(hash_value, *(unsigned short*) value);

	while (length >= 4)
	{
		hash_value = _mm_crc32_u32(hash_value, *(unsigned int*) value);
		value += 4;
		length -= 4;
	}

	if (length >= 2)
	{
		hash_value = _mm_crc32_u16(hash_value, *(unsigned short*) value);
		value += 2;
		length -= 2;
	}

	if (length)
	{
		hash_value = _mm_crc32_u8(hash_value, *value);
	}

	return hash_value;
}

#endif // architecture check
