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
#include "../jrd/ods.h"
#include "../jrd/ini.h"
#include "../jrd/TimeZone.h"

using namespace Firebird;
using namespace Jrd;


namespace
{
	struct SystemPackageInit
	{
		explicit SystemPackageInit(MemoryPool& pool)
			: list(FB_NEW_POOL(pool) ObjectsArray<SystemPackage>(pool, {
				SystemPackage(
					pool,
					"RDB$TIME_ZONE_UTIL",
					ODS_13_0,
					// procedures
					{
						SystemProcedure(
							pool,
							"TRANSITIONS",
							[]
							(ThrowStatusExceptionWrapper* status, IExternalContext* /*context*/,
								IRoutineMetadata* /*metadata*/, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
							{
								return FB_NEW TimeZoneTransitionsProcedure(status, inBuilder, outBuilder);
							},
							prc_selectable,
							// input parameters
							{
								{"RDB$TIME_ZONE_NAME", fld_tz_name, false},
								{"RDB$FROM_TIMESTAMP", fld_timestamp_tz, false},
								{"RDB$TO_TIMESTAMP", fld_timestamp_tz, false}
							},
							// output parameters
							{
								{"RDB$START_TIMESTAMP", fld_timestamp_tz, false},
								{"RDB$END_TIMESTAMP", fld_timestamp_tz, false},
								{"RDB$ZONE_OFFSET", fld_tz_offset, false},
								{"RDB$DST_OFFSET", fld_tz_offset, false},
								{"RDB$EFFECTIVE_OFFSET", fld_tz_offset, false}
							}
						)
					},
					// functions
					{
						SystemFunction(
							pool,
							"DATABASE_VERSION",
							[]
							(ThrowStatusExceptionWrapper* status, IExternalContext* /*context*/,
								IRoutineMetadata* /*metadata*/, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
							{
								return FB_NEW TimeZoneDatabaseVersionFunction(status, inBuilder, outBuilder);
							},
							// parameters
							{},
							{ fld_tz_db_version, false }
						)
					}
				)
			}))
		{
		}

		static InitInstance<SystemPackageInit> INSTANCE;

		AutoPtr<ObjectsArray<SystemPackage> > list;
	};

	InitInstance<SystemPackageInit> SystemPackageInit::INSTANCE;
}


ObjectsArray<SystemPackage>& SystemPackage::get()
{
	return *SystemPackageInit::INSTANCE().list.get();
}
