/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		InitCDSLib.h
 *	DESCRIPTION:	support for correct usage of CDS library by the engine
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
 *  The Original Code was created by Vladyslav Khorsun for the
 *  Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2022 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef FB_INIT_CDSLIB_H
#define FB_INIT_CDSLIB_H

#include "../common/classes/alloc.h"

// Uncomment to write final memory usage stats into firebird.log.
// See ~InitCDS() and ~InitPool() in CCH

//#define DEBUG_CDS_MEMORY

namespace Jrd
{

class InitCDS
{
public:

	explicit InitCDS(MemoryPool&);
	~InitCDS();

	// Creates memory pool that will not be deleted until cds finish its work.
	// Should be used to allocate structures by cds classes.
	static Firebird::MemoryPool* createPool();

private:
	static void* alloc(size_t size)
	{
		return m_pool->allocate(size ALLOC_ARGS);
	}

	static void free(void* p)
	{
		m_pool->deallocate(p);
	}

	static Firebird::Array<Firebird::MemoryPool*>* m_pools;
	static Firebird::MemoryPool* m_pool;
	static Firebird::MemoryStats m_stats;
};

} // namespace Jrd

#endif	// FB_INIT_CDSLIB_H
