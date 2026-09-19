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
 *  The Original Code was created by Vladyslav Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2026 Vladyslav Khorsun <fbvlad@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_INDEX_SEGMENT_H
#define JRD_INDEX_SEGMENT_H

#include "firebird.h"
#include "../jrd/MetaName.h"
#include "../common/classes/alloc.h"

namespace Jrd
{

struct IndexSegment
{
	IndexSegment(MemoryPool& pool) :
		name(pool)
	{
	}

	IndexSegment(MemoryPool& pool, const IndexSegment& other) :
		name(pool, other.name),
		length(other.length)
	{
	}

	MetaName name;
	SSHORT length = 0;		// Length in characters, if zero - equal to the field's length
};

} // namespace Jrd

#endif // JRD_INDEX_SEGMENT_H
