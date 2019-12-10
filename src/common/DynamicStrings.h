/*
 *	PROGRAM:		Firebird status vector support.
 *	MODULE:			DynamicStrings.h
 *	DESCRIPTION:	Dynamically store strings in status vector.
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_DYNAMIC_STRINGS
#define FB_DYNAMIC_STRINGS

#include "firebird/Interface.h"
#include "fb_exception.h"

namespace Firebird {

unsigned makeDynamicStrings(unsigned len, ISC_STATUS* const dst, const ISC_STATUS* const src);
char* findDynamicStrings(unsigned len, ISC_STATUS* ptr) throw();

} // namespace Firebird


#endif // FB_DYNAMIC_STRINGS
