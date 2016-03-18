/*
 *	PROGRAM:		String parser
 *	MODULE:			Tokens.h
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
 *
 *
 */

#ifndef FB_TOKENS_H
#define FB_TOKENS_H

#include "firebird/Interface.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace Firebird {

class Tokens : public AutoStorage
{
public:
	struct Comment
	{
		const char* start;
		const char* stop;
		bool endOnEol;
	};

	struct Tok
	{
		const char* text;
		FB_SIZE_T length, origin;
		NoCaseString stripped() const;
	};

	Tokens();

	void spaces(const char* s)
	{
		wsps = s;
	}

	void quotes(const char* s)
	{
		qs = s;
	}

	void comments(const Comment* ptr)
	{
		comms = ptr;
	}

	void separators(const char* s)
	{
		seps = s;
	}

	void parse(FB_SIZE_T length, const char* string);

	const Tok& operator[](FB_SIZE_T pos) const
	{
		return tokens[pos];
	}

	FB_SIZE_T getCount() const
	{
		return tokens.getCount();
	}

private:
	static void error(const char* fmt, ...);
	Tok* createToken(FB_SIZE_T p, FB_SIZE_T origin);

	HalfStaticArray<Tok, 16> tokens;
	string str;

	const char* wsps;
	const char* qs;
	const Comment* comms;
	const char* seps;
};

} // namespace Firebird


#endif // FB_AUTH_H
