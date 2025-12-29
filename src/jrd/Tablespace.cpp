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
 *  The Original Code was created by Roman Simakov
 *  for the RedDatabase project.
 *
 *  Copyright (c) 2018 <roman.simakov@red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/Tablespace.h"
#include "../jrd/lck_proto.h"

using namespace Firebird;


namespace Jrd {

Tablespace::~Tablespace()
{
	fb_assert(useCount == 0);
	delete existenceLock;
}


void Tablespace::addRef(thread_db *tdbb)
{
	useCount++;
	/*if (useCount == 1)
	{
		LCK_lock(tdbb, existenceLock, LCK_SR, LCK_WAIT);
	}*/
}

void Tablespace::release(thread_db *tdbb)
{
	/*if (useCount == 1)
	{
		LCK_release(tdbb, existenceLock);
	}*/
	useCount--;
}


}	// namespace Jrd
