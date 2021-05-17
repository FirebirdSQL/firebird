/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		guid.h
 *	DESCRIPTION:	Portable GUID definition
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
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *  Adriano dos Santos Fernandes
 *
 */

#ifndef FB_GUID_H
#define FB_GUID_H

#include <stdlib.h>
#include <stdio.h>

#ifdef WIN_NT
#include <rpc.h>
#else
#include "fb_types.h"

struct UUID	// Compatible with Win32 UUID struct layout
{
	ULONG Data1;
	USHORT Data2;
	USHORT Data3;
	UCHAR Data4[8];
};
#endif

namespace Firebird {

typedef UUID Guid;

const int GUID_BUFF_SIZE = 39;
const int GUID_BODY_SIZE = 36;

// Some versions of MSVC cannot recognize hh specifier but MSVC 2015 has it
const char* const GUID_FORMAT =
	"{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}";
const int GUID_FORMAT_ARGS = 11;

void GenerateRandomBytes(void* buffer, FB_SIZE_T size);
// Generates platform-dependent UUID compatible with RFC 4122
void GenerateGuid(Guid* guid);

// These functions receive buffers of at least GUID_BUFF_SIZE length

inline void GuidToString(char* buffer, const Guid* guid)
{
	sprintf(buffer, GUID_FORMAT,
		guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

inline bool StringToGuid(Guid* guid, const char* buffer)
{
	const auto result = sscanf(buffer, GUID_FORMAT,
		&guid->Data1, &guid->Data2, &guid->Data3,
		&guid->Data4[0], &guid->Data4[1], &guid->Data4[2], &guid->Data4[3],
		&guid->Data4[4], &guid->Data4[5], &guid->Data4[6], &guid->Data4[7]);

	return (result == GUID_FORMAT_ARGS);
}

}	// namespace

#endif	// FB_GUID_H
