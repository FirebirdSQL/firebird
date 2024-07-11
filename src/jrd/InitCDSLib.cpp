/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		InitCDSLib.cpp
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


#include "firebird.h"
#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/classes/locks.h"
#include "../jrd/InitCDSLib.h"
#include "../yvalve/gds_proto.h"

#include <cds/init.h>		//cds::Initialize, cds::Terminate
#include <cds/gc/dhp.h>		//cds::gc::DHP (Hazard Pointer)

using namespace Firebird;

namespace Jrd
{

Array<MemoryPool*>* InitCDS::m_pools = nullptr;
MemoryPool* InitCDS::m_pool = nullptr;
MemoryStats InitCDS::m_stats;

static GlobalPtr<InitCDS, InstanceControl::PRIORITY_TLS_KEY> initCDS;

InitCDS::InitCDS(MemoryPool&)
{
	m_pool = MemoryPool::createPool(nullptr, m_stats);
	m_pools = FB_NEW_POOL(*m_pool) Array<MemoryPool*>(*m_pool);

	cds::Initialize();
	cds::gc::dhp::smr::set_memory_allocator(alloc, free);
	cds::gc::dhp::smr::construct();
}

InitCDS::~InitCDS()
{
	cds::gc::dhp::smr::destruct(true);

	char str[512];

// CDS_ENABLE_HPSTAT is not defined by default.
// Rebuild of libcds after change is required.

#ifdef CDS_ENABLE_HPSTAT
	cds::gc::DHP::stat const& st = cds::gc::DHP::postmortem_statistics();

	sprintf(str, "DHP statistics:\n"
		"  thread count              = %llu\n"
		"  guard allocated           = %llu\n"
		"  guard freed               = %llu\n"
		"  retired data count        = %llu\n"
		"  free data count           = %llu\n"
		"  HP blocks allocated       = %llu\n"
		"  retired blocks allocated  = %llu\n"
		"  hp array extend() calls   = %llu\n"
		"  retired array extend()    = %llu\n"
		"  scan() call count         = %llu\n"
		"  help_scan() call count    = %llu\n"
		"\n",
		st.thread_rec_count,
		st.guard_allocated, st.guard_freed,
		st.retired_count, st.free_count,
		st.hp_block_count, st.retired_block_count,
		st.hp_extend_count, st.retired_extend_count,
		st.scan_count, st.help_scan_count
	);
	gds__log(str);
#endif
	cds::Terminate();

	// no need to protect m_pools at this point
	while (m_pools->hasData())
	{
		MemoryPool* pool = m_pools->pop();
		MemoryPool::deletePool(pool);
	}

	delete m_pools;
	MemoryPool::deletePool(m_pool);

#ifdef DEBUG_CDS_MEMORY
	sprintf(str, "DHP pool stats:\n"
		"  usage         = %llu\n"
		"  mapping       = %llu\n"
		"  max usage     = %llu\n"
		"  max mapping   = %llu\n"
		"\n",
		m_stats.getCurrentUsage(),
		m_stats.getCurrentMapping(),
		m_stats.getMaximumUsage(),
		m_stats.getMaximumMapping()
	);

	gds__log(str);
#endif
}

static InitInstance<Mutex> mutex;	// guard InitCDS::m_pools

MemoryPool* InitCDS::createPool()
{
	MemoryPool* pool = MemoryPool::createPool(nullptr, m_stats);
	MemoryStats* newStats = FB_NEW_POOL(*pool) MemoryStats;
	pool->setStatsGroup(*newStats);

	MutexLockGuard guard(mutex(), FB_FUNCTION);
	m_pools->push(pool);
	return pool;
}

} // namespace Jrd
