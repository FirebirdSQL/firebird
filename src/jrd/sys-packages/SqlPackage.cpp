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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/sys-packages/SqlPackage.h"
#include "../dsql/DsqlRequests.h"
#include "../jrd/Statement.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../dsql/dsql_proto.h"
#include "../jrd/mov_proto.h"

using namespace Jrd;
using namespace Firebird;


//--------------------------------------


IExternalResultSet* SqlPackage::explainProcedure(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const ExplainInput::Type* in, ExplainOutput::Type* out)
{
	return FB_NEW ExplainResultSet(status, context, in, out);
}


//--------------------------------------


SqlPackage::ExplainResultSet::ExplainResultSet(ThrowStatusExceptionWrapper* status,
		IExternalContext* context, const ExplainInput::Type* in, ExplainOutput::Type* aOut)
	: out(aOut)
{
	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();
	const auto transaction = tdbb->getTransaction();

	dsc sqlDesc;
	sqlDesc.makeBlob(isc_blob_text, CS_METADATA, const_cast<ISC_QUAD*>(&in->sql));
	MoveBuffer sqlBuffer;
	UCHAR* sqlAddress;
	ULONG sqlLength = MOV_make_string2(tdbb, &sqlDesc, CS_METADATA, &sqlAddress, sqlBuffer, false);

	const auto dsqlRequest = DSQL_prepare(tdbb, attachment, transaction, sqlLength, (const char*) sqlAddress,
		SQL_DIALECT_CURRENT, 0, nullptr, nullptr, false);

	Cleanup dsqlRequestCleanup([&]
	{
		DsqlRequest::destroy(tdbb, dsqlRequest);
	});

	if (!dsqlRequest->getStatement())
		return;

	PlanEntry rootEntry;
	dsqlRequest->getStatement()->getPlan(tdbb, rootEntry);

	Array<NonPooledPair<const PlanEntry*, const PlanEntry*>> planList;
	rootEntry.asFlatList(planList);

	unsigned planLine = 0;

	for (const auto& [planEntry, parentPlanEntry] : planList)
	{
		if (planLine == 0)
		{
			++planLine;
			continue;
		}

		auto& resultEntry = resultEntries.add();

		resultEntry.planLineNull = FB_FALSE;
		resultEntry.planLine = planLine++;

		resultEntry.recordSourceIdNull = FB_FALSE;
		resultEntry.recordSourceId = planEntry->accessPath->getRecSourceId();

		resultEntry.parentRecordSourceIdNull = parentPlanEntry->accessPath ? FB_FALSE : FB_TRUE;
		if (parentPlanEntry->accessPath)
			resultEntry.parentRecordSourceId = parentPlanEntry->accessPath->getRecSourceId();

		resultEntry.levelNull = FB_FALSE;
		resultEntry.level = planEntry->level;

		resultEntry.objectTypeNull = !planEntry->objectType.has_value();
		if (planEntry->objectType.has_value())
			resultEntry.objectType = planEntry->objectType.value();

		resultEntry.packageNameNull = planEntry->packageName.hasData() ? FB_FALSE : FB_TRUE;
		if (planEntry->packageName.hasData())
			resultEntry.packageName.set(planEntry->packageName.c_str(), planEntry->packageName.length());

		resultEntry.objectNameNull = planEntry->objectName.hasData() ? FB_FALSE : FB_TRUE;
		if (planEntry->objectName.hasData())
			resultEntry.objectName.set(planEntry->objectName.c_str(), planEntry->objectName.length());

		resultEntry.aliasNull = planEntry->alias.hasData() ? FB_FALSE : FB_TRUE;
		if (planEntry->alias.hasData())
			resultEntry.alias.set(planEntry->alias.c_str(), planEntry->alias.length());

		resultEntry.cardinalityNull = planEntry->level > 0 ? FB_FALSE : FB_TRUE;
		resultEntry.cardinality = planEntry->accessPath->getCardinality();

		resultEntry.recordLengthNull = planEntry->recordLength ? FB_FALSE : FB_TRUE;
		resultEntry.recordLength = planEntry->recordLength;

		resultEntry.keyLengthNull = planEntry->keyLength ? FB_FALSE : FB_TRUE;
		resultEntry.keyLength = planEntry->keyLength;

		string accessPath;
		planEntry->getDescriptionAsString(accessPath);

		constexpr UCHAR bpb[] = {
			isc_bpb_version1,
			isc_bpb_type, 1, isc_bpb_type_stream,
			isc_bpb_storage, 1, isc_bpb_storage_temp
		};

		bid blobId;
		const auto blob = blb::create2(tdbb, transaction, &blobId, sizeof(bpb), bpb);
		blob->BLB_put_data(tdbb, (const UCHAR*) accessPath.c_str(), accessPath.length());
		blob->BLB_close(tdbb);

		resultEntry.accessPathNull = FB_FALSE;
		resultEntry.accessPath = blobId;
	}

	resultIterator = resultEntries.begin();
}

FB_BOOLEAN SqlPackage::ExplainResultSet::fetch(ThrowStatusExceptionWrapper* status)
{
	if (resultIterator >= resultEntries.end())
		return false;

	*out = *resultIterator++;

	return true;
}


//--------------------------------------


SqlPackage::SqlPackage(MemoryPool& pool)
	: SystemPackage(
		pool,
		"RDB$SQL",
		ODS_14_0,
		// procedures
		{
			SystemProcedure(
				pool,
				"EXPLAIN",
				SystemProcedureFactory<ExplainInput, ExplainOutput, explainProcedure>(),
				prc_selectable,
				// input parameters
				{
					{"SQL", fld_description, false}
				},
				// output parameters
				{
					{"PLAN_LINE", fld_integer, false},
					{"RECORD_SOURCE_ID", fld_gen_val, false},
					{"PARENT_RECORD_SOURCE_ID", fld_gen_val, true},
					{"LEVEL", fld_integer, false},
					{"OBJECT_TYPE", fld_obj_type, true},
					{"PACKAGE_NAME", fld_pkg_name, true},
					{"OBJECT_NAME", fld_r_name, true},
					{"ALIAS", fld_short_description, true},
					{"CARDINALITY", fld_statistics, true},
					{"RECORD_LENGTH", fld_integer, true},
					{"KEY_LENGTH", fld_integer, true},
					{"ACCESS_PATH", fld_description, false}
				}
			),
		},
		// functions
		{
		}
	)
{
}
