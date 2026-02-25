/*
 *      PROGRAM:        JRD access method
 *      MODULE:         vec.cpp
 *      DESCRIPTION:    Misc helpers
 *
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
 *
 */

#include "firebird.h"
#include "../jrd/vec.h"
#include "../jrd/jrd.h"
#include "../jrd/err_proto.h"


#if defined(DEV_BUILD)

using namespace Jrd;

thread_db* JRD_get_thread_data()
{
	Firebird::ThreadData* p1 = Firebird::ThreadData::getSpecific();
	if (p1 && p1->getType() == Firebird::ThreadData::tddDBB)
	{
		Jrd::thread_db* p2 = (Jrd::thread_db*) p1;
		if (p2->getDatabase() && !p2->getDatabase()->checkHandle())
		{
			BUGCHECK(147);
		}
	}
	return (Jrd::thread_db*) p1;
}

void CHECK_TDBB(const Jrd::thread_db* tdbb)
{
	fb_assert(tdbb && (tdbb->getType() == Firebird::ThreadData::tddDBB) &&
		(!tdbb->getDatabase() || tdbb->getDatabase()->checkHandle()));
}

void CHECK_DBB(const Database* dbb)
{
	fb_assert(dbb && dbb->checkHandle());
}

#endif // DEV_BUILD
