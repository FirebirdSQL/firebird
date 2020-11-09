/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/SystemPackages.h"
#include "../jrd/TimeZone.h"
#include "../jrd/Profiler.h"

using namespace Firebird;
using namespace Jrd;


namespace
{
	struct SystemPackagesInit
	{
		explicit SystemPackagesInit(MemoryPool& pool)
			: list(FB_NEW_POOL(pool) ObjectsArray<SystemPackage>(pool))
		{
			list->add(TimeZonePackage(pool));
			list->add(ProfilerPackage(pool));
		}

		static InitInstance<SystemPackagesInit> INSTANCE;

		AutoPtr<ObjectsArray<SystemPackage> > list;
	};

	InitInstance<SystemPackagesInit> SystemPackagesInit::INSTANCE;
}


ObjectsArray<SystemPackage>& SystemPackage::get()
{
	return *SystemPackagesInit::INSTANCE().list.get();
}
