/*
 *	PROGRAM:	Common class definition
 *	MODULE:		zip.cpp
 *	DESCRIPTION:	ZIP compression library loader.
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
 *  Copyright (c) 2012, 2018 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/zip.h"

#ifdef HAVE_ZLIB_H

using namespace Firebird;

ZLib::ZLib(Firebird::MemoryPool&)
{
#ifdef WIN_NT
	Firebird::PathName name("zlib1.dll");
#else
	Firebird::PathName name("libz." SHRLIB_EXT ".1");
#endif
	z.reset(ModuleLoader::fixAndLoadModule(status, name));
	if (z)
		symbols();
}

void ZLib::symbols()
{
#define FB_ZSYMB(A) z->findSymbol(STRINGIZE(A), A); if (!A) { z.reset(NULL); return; }
	FB_ZSYMB(deflateInit_)
	FB_ZSYMB(inflateInit_)
	FB_ZSYMB(deflate)
	FB_ZSYMB(inflate)
	FB_ZSYMB(deflateEnd)
	FB_ZSYMB(inflateEnd)
#undef FB_ZSYMB
}

//	Firebird::InitInstance<ZLib> zlib;

void* ZLib::allocFunc(void*, uInt items, uInt size)
{
	return MemoryPool::globalAlloc(items * size ALLOC_ARGS);
}

void ZLib::freeFunc(void*, void* address)
{
	MemoryPool::globalFree(address);
}

#endif // HAVE_ZLIB_H
