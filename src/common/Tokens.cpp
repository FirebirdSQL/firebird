/*
 *	PROGRAM:		String parser
 *	MODULE:			Tokens.cpp
 *	DESCRIPTION:	Enhanced variant of strtok()
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
 *  Copyright (c) 2015 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/Tokens.h"

#include "../common/StatusArg.h"

namespace {

const Firebird::Tokens::Comment sqlComments[3] = {
	{ "/*", "*/", false },
	{ "--", "\n", true },
	{ NULL, NULL, false }
};
const char* sqlSpaces = " \t\r\n";
const char* sqlSeps = "!\"#%&'()*+,-./:;<=>?@[\\]^`{|}~";
const char* sqlQuotes = "\"'";

} // anonymous namespace

namespace Firebird {

Tokens::Tokens()
	: tokens(getPool()),
	  str(getPool()),
	  wsps(sqlSpaces),
	  qs(sqlQuotes),
	  comms(sqlComments),
	  seps(sqlSeps)
{
}

void Tokens::parse(FB_SIZE_T length, const char* toParse)
{
	tokens.clear();

	if (!length)
		length = strlen(toParse);
	str.assign(toParse, length);

	char inStr = '\0';
	Tok* inToken = NULL;
	FB_SIZE_T startp = 0;
	FB_SIZE_T origin = 0;

	FB_SIZE_T p = 0;

	while (p < str.length())
	{
		if (comms && !inStr)
		{
			bool foundComment = false;

			for (const Comment* comm = comms; comm->start; ++comm)
			{
				if (strncmp(comm->start, &str[p], strlen(comm->start)) == 0)
				{
					FB_SIZE_T p2 = p + strlen(comm->start);
					p2 = str.find(comm->stop, p2);

					if (p2 == str.npos)
					{
						if (!comm->endOnEol)
							error("Missing close comment for %s", comm->start);
						p2 = str.length();
					}
					else
						p2 += strlen(comm->stop);

					str.erase(p, p2 - p);
					origin += (p2 - p);
					foundComment = true;
					break;
				}
			}

			if (foundComment)
				continue;
		}

		char c = str[p];

		if (inStr)
		{
			if (c == inStr)
			{
				++p;
				++origin;

				if (p >= str.length() || str[p] != inStr)
				{
					inStr = '\0';
					inToken->length = p - startp;
					inToken = NULL;
					continue;
				}

				// double quote - continue processing string
			}

			++p;
			++origin;
			continue;
		}

		if (wsps && strchr(wsps, c))
		{
			if (inToken)
			{
				inToken->length = p - startp;
				inToken = NULL;
			}

			++p;
			++origin;
			continue;
		}

		bool quote = qs && strchr(qs, c);

		if (quote)
		{
			if (inToken)
			{
				inToken->length = p - startp;
				inToken = NULL;
			}

			// start string
			inStr = c;
		}

		if ((!quote) && seps && strchr(seps, c))
		{
			// close current token
			if (inToken)
			{
				inToken->length = p - startp;
				inToken = NULL;
			}

			// and create new one for one symbol
			inToken = createToken(p, origin);
			inToken->length = 1;
			inToken = NULL;
		}
		else if (!inToken)
		{
			// start token
			startp = p;
			inToken = createToken(p, origin);
		}


		// done with char
		++p;
		++origin;
	}

	if (inStr)
		error("Missing close quote <%c>", inStr);

	if (inToken)
		inToken->length = p - startp;

//#define DEBUG_TOKENS
#ifdef DEBUG_TOKENS
	for (unsigned dbg = 0; dbg < getCount(); ++dbg)
		printf("%2d %.*s\n", dbg, tokens[dbg].length, tokens[dbg].text);
#endif
}

Tokens::Tok* Tokens::createToken(FB_SIZE_T p, FB_SIZE_T origin)
{
	tokens.grow(tokens.getCount() + 1);
	Tok* tok = &tokens[tokens.getCount() - 1];
	tok->text = &str[p];
	tok->origin = origin;
	return tok;
}

void Tokens::error(const char* fmt, ...)
{
	string buffer;

	va_list params;
	va_start(params, fmt);
	buffer.vprintf(fmt, params);
	va_end(params);

	(Arg::Gds(isc_random) << "Parse to tokens error" << Arg::Gds(isc_random) << buffer).raise();
}

NoCaseString Tokens::Tok::stripped() const
{
	NoCaseString rc;
	char q = text[0];

	for (FB_SIZE_T i = 1; i < length - 1; ++i)
	{
		if (text[i] == q)
			++i;
		rc += text[i];
	}

	return rc;
}

} // namespace Firebird
