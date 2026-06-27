
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2023 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include <ctype.h>
#include <math.h>
#include "JPathParser.h"
#include "../JsonUtils.h"
#include "../dsql/chars.h"
#include "../jrd/DataTypeUtil.h"
#include "../jrd/intl_proto.h"

using namespace Firebird;
using namespace FBJSON;


class PathTokens
{
public:
	LeftPooledMap<string, int> keyword;
	LeftPooledMap<string, PathMethod> methods;
	PathTokens(MemoryPool& p) :
		keyword(p), methods(p)
	{
		keyword.put("lax", TOK_LAX);
		keyword.put("strict", TOK_STRICT);
		keyword.put("last", TOK_LAST);
		keyword.put("to", TOK_TO);

		keyword.put("null", TOK_JSON_NULL);
		keyword.put("true", TOK_TRUE);
		keyword.put("false", TOK_FALSE);

		keyword.put("starts", TOK_STARTS);
		keyword.put("with", TOK_WITH);
		keyword.put("like_regex", TOK_LIKE_REGEX);
		keyword.put("is", TOK_IS);
		keyword.put("unknown", TOK_UNKNOWN);
		keyword.put("flags", TOK_FLAGS);
		keyword.put("exists", TOK_EXISTS);

		 // Process the follows methods in a special way
		keyword.put("keyvalue", TOK_KEYVALUE);
		keyword.put("datetime", TOK_DATETIME);
		keyword.put("time", TOK_TIME);
		keyword.put("time_tz", TOK_TIME_TZ);
		keyword.put("timestamp", TOK_TIMESTAMP);
		keyword.put("timestamp_tz", TOK_TIMESTAMP_TZ);
		keyword.put("decimal", TOK_DECIMAL);

		// Methods without an arg
		methods.put("type", PathMethod::TYPE);
		methods.put("size", PathMethod::SIZE);
		methods.put("double", PathMethod::DOUBLE);
		methods.put("ceiling", PathMethod::CEILING);
		methods.put("floor", PathMethod::FLOOR);
		methods.put("abs", PathMethod::ABS);

		methods.put("date", PathMethod::DATE);
		methods.put("string", PathMethod::STRING);
		methods.put("boolean", PathMethod::BOOLEAN);
		methods.put("bigint", PathMethod::BIGINT);
		methods.put("integer", PathMethod::INTEGER);
		methods.put("number", PathMethod::NUMBER);
	}
};

GlobalPtr<PathTokens> tokens;


const int* getKeyword(const string& str)
{
	return tokens->keyword.get(str);
}

const int getNonCharKeyword(const char* str)
{
	switch (getSuperType<char>(str[0], str[1]))
	{
		case getSuperType('|', '|'):
			return TOK_OR;
		case getSuperType('&', '&'):
			return TOK_AND;
		case getSuperType('=', '='):
			return TOK_EQ;
		case getSuperType('!', '='):
		case getSuperType('<', '>'):
			return TOK_NEQ;
		case getSuperType('<', '='):
			return TOK_LEQ;
		case getSuperType('>', '='):
			return TOK_GEQ;
	}

	return 0;
}


PathParser::PathParser(MemoryPool& pool, const std::string_view jsonPath) :
	PermanentStorage(pool),
	m_tempPool(MemoryPool::createPool())
{
	yyps = 0;
	yypath = 0;
	yylvals = 0;
	yylvp = 0;
	yylve = 0;
	yylvlim = 0;
	yylpsns = 0;
	yylpp = 0;
	yylpe = 0;
	yylplim = 0;
	yylexp = 0;
	yylexemes = 0;

	const char* begin = jsonPath.data();
	const char* end = begin + jsonPath.length();

	yyposn.firstLine = 1;
	yyposn.firstColumn = 1;
	yyposn.lastLine = 1;
	yyposn.lastColumn = 1;
	yyposn.firstPos = begin;
	yyposn.leadingFirstPos = begin;
	yyposn.lastPos = end;
	yyposn.trailingLastPos = end;

	lex.start = begin;
	lex.line_start = lex.last_token = lex.ptr = lex.leadingPtr = begin;
	lex.end = end;
	lex.line_start_bk = lex.line_start;
	lex.prev_keyword = -1;

#ifdef DSQL_DEBUG
	if (DSQL_debug & 32)
		dsql_trace("Source DSQL string:\n%.*s", (int) length, string);
#endif
}


PathParser::~PathParser()
{
	while (yyps)
	{
		yyparsestate* p = yyps;
		yyps = p->save;
		yyFreeState(p);
	}

	while (yypath)
	{
		yyparsestate* p = yypath;
		yypath = p->save;
		yyFreeState(p);
	}

	delete[] yylvals;
	delete[] yylpsns;
	delete[] yylexemes;
}


JsonPathExpr* PathParser::parse(const PassingKeys* keys)
{
	this->m_passingKeys = keys;

	m_output.reset(FB_NEW_POOL(getPool()) JsonPathExpr(getPool()));

	if (parseAux() != 0)
	{
		fb_assert(false);
	}

	if (!m_allowPathExtensions && m_output->hasMath())
	{
		json_syntax_exception::raise(
				JsonStatusMsg(isc_jpath_common) <<
				JsonStatusMsg(isc_jpath_multiple_roots));
	}

	return m_output.release();
}

// Set the position of a left-hand non-terminal based on its right-hand rules.
void PathParser::yyReducePosn(YYPOSN& ret, YYPOSN* termPosns, YYSTYPE* /*termVals*/, int termNo,
	int /*stkPos*/, int /*yychar*/, YYPOSN& /*yyposn*/, void*)
{
	if (termNo == 0)
	{
		// Accessing termPosns[-1] seems to be the only way to get correct positions in this case.
		ret.firstLine = ret.lastLine = termPosns[termNo - 1].lastLine;
		ret.firstColumn = ret.lastColumn = termPosns[termNo - 1].lastColumn;
		ret.firstPos = ret.lastPos = ret.trailingLastPos = termPosns[termNo - 1].trailingLastPos;
		ret.leadingFirstPos = termPosns[termNo - 1].lastPos;
	}
	else
	{
		ret.firstLine = termPosns[0].firstLine;
		ret.firstColumn = termPosns[0].firstColumn;
		ret.firstPos = termPosns[0].firstPos;
		ret.leadingFirstPos = termPosns[0].leadingFirstPos;
		ret.lastLine = termPosns[termNo - 1].lastLine;
		ret.lastColumn = termPosns[termNo - 1].lastColumn;
		ret.lastPos = termPosns[termNo - 1].lastPos;
		ret.trailingLastPos = termPosns[termNo - 1].trailingLastPos;
	}
}


int PathParser::yylex()
{
	if (!yylexSkipSpaces())
		return -1;

	yyposn.firstLine = 1;
	yyposn.firstColumn = lex.ptr - lex.line_start;
	yyposn.firstPos = lex.ptr - 1;
	yyposn.leadingFirstPos = lex.leadingPtr;

	lex.prev_keyword = yylexAux();

	yyposn.lastPos = lex.ptr;
	lex.leadingPtr = lex.ptr;

	bool spacesSkipped = yylexSkipSpaces();

	yyposn.lastLine = 1;
	yyposn.lastColumn = lex.ptr - lex.line_start;

	if (spacesSkipped)
		--lex.ptr;

	yyposn.trailingLastPos = lex.ptr;

	return lex.prev_keyword;
}


bool PathParser::yylexSkipSpaces()
{
	UCHAR tok_class;
	SSHORT c;

	// Find end of white space
	for (;;)
	{
		if (lex.ptr >= lex.end)
			return false;

		if (yylexSkipEol())
			continue;

		c = *lex.ptr++;
		tok_class = classes(c);

		if (!(tok_class & CHR_WHITE))
			break;
	}

	return true;
}


bool PathParser::yylexSkipEol()
{
	bool eol = false;
	const TEXT c = *lex.ptr;

	if (c == '\r')
	{
		lex.ptr++;
		if (lex.ptr < lex.end && *lex.ptr == '\n')
			lex.ptr++;

		eol = true;
	}
	else if (c == '\n')
	{
		lex.ptr++;
		eol = true;
	}

	if (eol)
	{
		lex.line_start = lex.ptr; // + 1; // CVC: +1 left out.
	}

	return eol;
}

static inline bool isTokenChar(const char c)
{
	return (classes(c) & CHR_LETTER) || c == '_';
}

static inline bool isStringChar(const char c)
{
	return classes(c) & CHR_IDENT;
}

static inline const char* skipSpaces(const char* start, const char* end)
{
	const char* it = start;
	for (; it < end && isStringChar(*it); ++it);

	return it;
}

static inline const char* readToken(const char* start, const char* end)
{
	const char* it = start;
	for (; it < end && isTokenChar(*it); ++it);

	return it;
}

static inline const char* readString(const char* start, const char* end)
{
	const char* it = start;
	for (; it < end && isStringChar(*it); ++it);

	return it;
}

static inline const char* readToken(const char* start, const char* end, const char stop)
{
	const char* it = start;
	for (; it < end && isTokenChar(*it) && *it != stop; ++it);

	return it;
}

int PathParser::yylexAux()
{
	SSHORT c = lex.ptr[-1];
	UCHAR tok_class = classes(c);
	lex.last_token = lex.ptr - 1;

	// It does not work for lex errors to set the flag in the yacc file
	switch (lex.prev_keyword)
	{
		case static_cast<int>('?'):
			m_lexInFilter = true;
			m_filterBracketCounter = 0;
			break;
		case static_cast<int>('('):
			++m_filterBracketCounter;
			break;
		case static_cast<int>(')'):
			if (m_lexInFilter && --m_filterBracketCounter == 0)
				m_lexInFilter = false;
			break;
	}


	// Parse a quoted string, being sure to look for double quotes
	if (tok_class & CHR_QUOTE)
	{
		StringParser scalarParser;
		TextPos offset = 0;
		std::string_view view(lex.ptr, lex.end - lex.ptr);
		StringParseView viewp{view};
		string* stringPtr = mkTempString("");
		scalarParser.parseQuotedString(viewp, offset, *stringPtr);

		lex.ptr += offset;
		if (scalarParser.hasError())
		{
			raiseFullErrorVector(*scalarParser.error, yyposn);
		}

		yylval.stringPtr = stringPtr;
		return TOK_QUOTED_STRING;
	}

	fb_assert(lex.ptr <= lex.end);

	if ((tok_class & CHR_DIGIT) ||
		((c == '.') && (lex.ptr < lex.end) && (classes(*lex.ptr) & CHR_DIGIT)))
	{
		--lex.ptr;

		StringParser scalarParser;
		TextPos offset = 0;
		std::string_view view(lex.ptr, lex.end - lex.ptr);
		StringParseView viewp{view};
		const ParsedNumber number = scalarParser.parseNumber(viewp, offset, false);

		lex.ptr += offset;
		if (scalarParser.hasError())
		{
			raiseFullErrorVector(*scalarParser.error, yyposn);
		}

		// Should we use floating point type?
		if (number.isDouble)
		{
			lex.last_token_bk = lex.last_token;
			lex.line_start_bk = lex.line_start;

			yylval.doubleVal = number.getDouble();
			return TOK_DOUBLE;
		}
		else
		{
			lex.last_token_bk = lex.last_token;
			lex.line_start_bk = lex.line_start;

			if (number.value <= MAX_SSHORT)
			{
				yylval.int16Val = (SSHORT) number.value;
				return TOK_INTEGER16;
			}
			else if (number.value <= MAX_SLONG)
			{
				yylval.int32Val = (SLONG) number.value;
				return TOK_INTEGER32;
			}
			else
			{
				yylval.int64Val = number.value;
				return TOK_INTEGER64;
			}
		}
	}

	// Restore the status quo ante, before we started our unsuccessful
	// attempt to recognize a number.
	lex.ptr = lex.last_token;
	c = *lex.ptr++;
	// We never touched tok_class, so it doesn't need to be restored.

	if (c == '$' && lex.last_token + 1 < lex.end && isStringChar(*lex.ptr) && *lex.ptr != '$')
	{
		const char* start = lex.ptr - 1;
		lex.ptr = readString(lex.ptr, lex.end);

		const string rawPassingStr(start + 1, lex.ptr - start - 1); // Skip $
		yylval.stringPtr = mkTempString(rawPassingStr);
		return TOK_PASSING_NAME;
	}
	else if (c == '.')
	{
		const char* methodStart = skipSpaces(lex.ptr, lex.end);
		const char* end = readToken(methodStart, lex.end, '(');
		if (*end == '(')
		{
			return TOK_METHOD_DOT;
		}
	}
	else if (tok_class & CHR_LETTER)
	{
		// An quoteless filed name or a token or a passing variable

		// First get only token chars
		const char* start = lex.ptr - 1;
		lex.ptr = readToken(lex.ptr, lex.end);

		string rawStr(start, lex.ptr - start);
		const int* keyVer = getKeyword(rawStr);
		if (keyVer)
		{
			lex.last_token_bk = lex.last_token;
			lex.line_start_bk = lex.line_start;
			return *keyVer;
		}
		else
		{
			FBJSON::PathMethod* method = tokens->methods.get(rawStr);
			if (method)
			{
				const TEXT* src = lex.ptr;
				for (; lex.ptr < lex.end && isspace(*lex.ptr); lex.ptr++)
				{
					if (lex.ptr >= lex.end)
						return -1;
				}
				if (*lex.ptr == '(')
				{
					yylval.methodType = *method;
					lex.last_token_bk = lex.last_token;
					lex.line_start_bk = lex.line_start;
					return TOK_METHOD;
				}
				else
					lex.ptr = src;
			}
		}

		// Get the rest on allowed tokens for a string
		lex.ptr = skipSpaces(lex.ptr, lex.end);
		rawStr.assign(start, lex.ptr - start);


		yylval.stringPtr = mkTempString(rawStr);
		lex.last_token_bk = lex.last_token;
		lex.line_start_bk = lex.line_start;
		return TOK_STRING;
	}

	// Must be punctuation -- test for double character punctuation
	if (lex.last_token + 1 < lex.end && !isspace(UCHAR(lex.last_token[1])))
	{
		const int keyVer = getNonCharKeyword(lex.last_token);
		if (keyVer != 0)
		{
			++lex.ptr;
			return keyVer;
		}
	}

	// Single character punctuation are simply passed on
	return (UCHAR) c;
}


void PathParser::raiseFullErrorVector(const JsonStatusMsg& error, const YYPOSN& posn)
{
	string token(posn.firstPos);
	if (token.length() > JSON_MAX_REPORT_SIZE)
	{
		token.resize(JSON_MAX_REPORT_SIZE);
		token += "...";
	}

	if (posn.firstLine == 1)
	{
		json_syntax_exception::raise(
			JsonStatusMsg(m_lexInFilter ? isc_jfilter_common : isc_jpath_common) <<
			error <<
			JsonStatusMsg(isc_jpath_problem_place) <<
			JsonStatusMsgIntArg(FB_UINT64(posn.firstColumn)) <<
			JsonStatusMsgStrArg(token));
	}
	else
	{
		json_syntax_exception::raise(
			JsonStatusMsg(m_lexInFilter ? isc_jfilter_common : isc_jpath_common) <<
			error <<
			JsonStatusMsgIntArg(FB_UINT64((posn.firstColumn))) <<
			JsonStatusMsgIntArg(FB_UINT64((posn.firstLine))) <<
			JsonStatusMsgStrArg(token));
	}
}

void PathParser::yyerror_detailed(const TEXT* /*error_string*/, int yychar, YYSTYPE&, YYPOSN& posn)
{
/**************************************
 *
 *	y y e r r o r _ d e t a i l e d
 *
 **************************************
 *
 * Functional description
 *	Print a syntax error.
 *
 **************************************/

	if (yychar < 1)
	{
		if (posn.firstLine == 1)
		{
			json_syntax_exception::raise(JsonStatusMsg(m_lexInFilter ? isc_jfilter_common : isc_jpath_common) <<
				JsonStatusMsg(isc_jlexer_end) <<
				JsonStatusMsgIntArg(FB_UINT64(posn.firstColumn)));
		}
		else
		{
			json_syntax_exception::raise(JsonStatusMsg(m_lexInFilter ? isc_jfilter_common : isc_jpath_common) <<
				JsonStatusMsg(isc_jlexer_end_with_line) <<
				JsonStatusMsgIntArg(FB_UINT64(posn.firstColumn)) <<
				JsonStatusMsgIntArg(FB_UINT64(posn.firstLine)));
		}
	}
	else
	{
		raiseFullErrorVector(JsonStatusMsg(isc_jlexer_invalid_syntax), posn);
	}
}


void PathParser::checkPassing(const SmallString& passName) const
{
	if (m_passingKeys == nullptr)
	{
		json_syntax_exception::raise(JsonStatusMsg(isc_jpath_common) <<
			JsonStatusMsg(isc_jlexer_passing_not_defined));
	}
	else if (!m_passingKeys->exist(passName))
	{
		json_syntax_exception::raise(JsonStatusMsg(isc_jpath_common) <<
			JsonStatusMsg(isc_jlexer_passing_var_not_defined) <<
			JsonStatusMsgStrArg(passName));
	}
}
