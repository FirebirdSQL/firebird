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
 *  The Original Code was created by Mark O'Donohue
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 Mark O'Donohue <skywalker@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  2005.05.19 Claudio Valderrama: signal tokens that aren't reserved in the
 *      engine thanks to special handling.
 *  Adriano dos Santos Fernandes
 */

#include "firebird.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "keywordsStub.h"

// This method is currently used in isql/isql.epp to check if a
// user field is a reserved word, and hence needs to be quoted.
// Obviously a hash table would make this a little quicker.
// MOD 29-June-2002

extern "C" {

int API_ROUTINE KEYWORD_stringIsAToken(const char* in_str)
{
	for (const TOK* tok_ptr = keywordGetTokens(); tok_ptr->tok_string; ++tok_ptr)
	{
		if (!tok_ptr->nonReserved && !strcmp(tok_ptr->tok_string, in_str))
			return true;
	}

	return false;
}

Tokens API_ROUTINE KEYWORD_getTokens()
{
	// This function should not be used but appeared in FB3.
	// As long as we keep TOK structure as is we may have it deprecated.
	// Later sooner of all it will be removed at all.
	return keywordGetTokens();
}

}
