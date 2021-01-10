/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		ifaceExamples.h
 *	DESCRIPTION:	A number of common defines for all samples.
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
 *  Copyright (c) 2014 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#include <atomic>
typedef std::atomic_int FbSampleAtomic;
#else
typedef int FbSampleAtomic;
#endif

#include <firebird/Interface.h>

#if defined(_WIN32)
#define FB_DLL_EXPORT __declspec(dllexport)
#elif defined(__APPLE__)
#define FB_DLL_EXPORT __attribute__((visibility("default")))
#else
#define FB_DLL_EXPORT
#endif

using namespace Firebird;

#define SAMPLES_DIALECT SQL_DIALECT_V6
