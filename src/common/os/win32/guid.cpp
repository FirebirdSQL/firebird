/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		guid.cpp
 *	DESCRIPTION:	Portable GUID (win32)
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
 *
 *
 */

#ifndef __MINGW32__
// minimum win32 version: win98 / winnt4 SP3
#define _WIN32_WINNT 0x0403
#endif

#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <stdio.h>

#include "firebird.h"
#include "../common/os/guid.h"
#include "fb_exception.h"

namespace Firebird {


void GenerateRandomBytes(void* buffer, FB_SIZE_T size)
{
	if (BCryptGenRandom(nullptr, static_cast<UCHAR*>(buffer), size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != S_OK)
		Firebird::system_call_failed::raise("BCryptGenRandom");
}

void GenerateGuid(UUID* guid)
{
	const HRESULT error = CoCreateGuid(guid);
	if (!SUCCEEDED(error))
		Firebird::system_call_failed::raise("CoCreateGuid", error);
}


}	// namespace
