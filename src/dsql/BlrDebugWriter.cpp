/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * Adriano dos Santos Fernandes - refactored from others modules.
 * Alex Peshkov
 */

#include "firebird.h"
#include "firebird/impl/consts_pub.h"
#include "dyn_consts.h"
#include "iberror.h"
#include "../jrd/jrd.h"
#include "../jrd/exe.h"
#include "../dsql/BlrDebugWriter.h"
#include "../dsql/StmtNodes.h"
#include "../dsql/dsql.h"
#include "firebird/impl/blr.h"
#include "../jrd/DebugInterface.h"
#include "../dsql/errd_proto.h"

using namespace Firebird;

namespace Jrd {

void BlrDebugWriter::raiseError(const Arg::StatusVector& vector)
{
	ERRD_post(vector);
}

void BlrDebugWriter::beginDebug()
{
	fb_assert(debugData.isEmpty());

	debugData.add(fb_dbg_version);
	debugData.add(CURRENT_DBG_INFO_VERSION);
}

void BlrDebugWriter::endDebug()
{
	debugData.add(fb_dbg_end);
}

void BlrDebugWriter::putDebugSrcInfo(ULONG line, ULONG col)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_map_src2blr);

	putValue(line);
	putValue(col);
	putBlrOffset();
}

void BlrDebugWriter::putDebugVariable(USHORT number, const MetaName& name)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_map_varname);

	debugData.add(number);
	debugData.add(number >> 8);

	USHORT len = MIN(name.length(), MAX_UCHAR);
	debugData.add(len);

	debugData.add(reinterpret_cast<const UCHAR*>(name.c_str()), len);
}

void BlrDebugWriter::putDebugArgument(UCHAR type, USHORT number, const TEXT* name)
{
	if (debugData.isEmpty())
		return;

	fb_assert(name);

	debugData.add(fb_dbg_map_argument);

	debugData.add(type);
	debugData.add(number);
	debugData.add(number >> 8);

	USHORT len = static_cast<USHORT>(strlen(name));
	if (len > MAX_UCHAR)
		len = MAX_UCHAR;
	debugData.add(len);

	debugData.add(reinterpret_cast<const UCHAR*>(name), len);
}

void BlrDebugWriter::putDebugDeclaredCursor(USHORT number, const MetaName& name)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_map_curname);

	debugData.add(number);
	debugData.add(number >> 8);

	USHORT len = MIN(name.length(), MAX_UCHAR);
	debugData.add(len);

	debugData.add(reinterpret_cast<const UCHAR*>(name.c_str()), len);
}

void BlrDebugWriter::putDebugForCursor(const MetaName& name)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_map_for_curname);

	putBlrOffset();

	USHORT len = MIN(name.length(), MAX_UCHAR);
	debugData.add(len);

	debugData.add(reinterpret_cast<const UCHAR*>(name.c_str()), len);
}

void BlrDebugWriter::putDebugSubFunction(DeclareSubFuncNode* subFuncNode)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_subfunc);

	dsql_udf* subFunc = subFuncNode->dsqlFunction;
	const auto& name = subFunc->udf_name.object;
	USHORT len = MIN(name.length(), MAX_UCHAR);

	debugData.add(len);
	debugData.add(reinterpret_cast<const UCHAR*>(name.c_str()), len);

	HalfStaticArray<UCHAR, 128>& subDebugData = subFuncNode->blockScratch->debugData;
	const ULONG count = ULONG(subDebugData.getCount());
	putValue(count);
	debugData.add(subDebugData.begin(), count);
}

void BlrDebugWriter::putDebugSubProcedure(DeclareSubProcNode* subProcNode)
{
	if (debugData.isEmpty())
		return;

	debugData.add(fb_dbg_subproc);

	dsql_prc* subProc = subProcNode->dsqlProcedure;
	const auto& name = subProc->prc_name.object;
	USHORT len = MIN(name.length(), MAX_UCHAR);

	debugData.add(len);
	debugData.add(reinterpret_cast<const UCHAR*>(name.c_str()), len);

	HalfStaticArray<UCHAR, 128>& subDebugData = subProcNode->blockScratch->debugData;
	const ULONG count = ULONG(subDebugData.getCount());
	putValue(count);
	debugData.add(subDebugData.begin(), count);
}

void BlrDebugWriter::putValue(ULONG val)
{
	debugData.add(val);
	debugData.add(val >> 8);
	debugData.add(val >> 16);
	debugData.add(val >> 24);
}

void BlrDebugWriter::putBlrOffset()
{
	const ULONG offset = (getBlrData().getCount() - getBaseOffset());
	putValue(offset);
}

}	// namespace Jrd
