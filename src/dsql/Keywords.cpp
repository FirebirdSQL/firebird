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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../dsql/Keywords.h"
#include "../jrd/jrd.h"
#include "../common/Token.h"

#define _yacc_defines_yystype
#include "gen/parse.h"

using namespace Firebird;
using namespace Jrd;


#define PARSER_TOKEN(ident, str, nonReserved) \
	{ident, str, nonReserved},

static const Token tokens[] =
{
#include "../common/ParserTokens.h"
	{0, NULL, false}
};


Keywords* Keywords::Allocator::create()
{
	thread_db* tdbb = JRD_get_thread_data();
	fb_assert(tdbb);
	Database* dbb = tdbb->getDatabase();
	fb_assert(dbb);

	return FB_NEW_POOL(*dbb->dbb_permanent) Keywords(*dbb->dbb_permanent);
}

Keywords::Keywords(MemoryPool& pool)
	: map(pool)
{
	for (auto token = tokens; token->tok_string; ++token)
	{
		auto str = FB_NEW_POOL(pool) MetaName(token->tok_string);
		map.put(*str, {token->tok_ident, str, token->nonReserved});
	}
}
