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
 *       2001.08.03   John Bellardo changed LIMIT token to SKIP
 *       2001.07.28   John Bellardo added tokens for FIRST and LIMIT
   See dsql/parse.y for a chronological list. */

#include "../common/keywords.h"
#include "ibase.h"

// These symbols are exported
extern "C"
{
	int API_ROUTINE KEYWORD_stringIsAToken(const char*);
	Tokens FB_API_DEPRECATED API_ROUTINE KEYWORD_getTokens();
}
