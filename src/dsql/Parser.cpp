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
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include <ctype.h>
#include <math.h>
#include "../dsql/Parser.h"
#include "../dsql/chars.h"
#include "../jrd/jrd.h"
#include "../jrd/DataTypeUtil.h"
#include "../common/keywords.h"
#include "../jrd/intl_proto.h"

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
#define DBL_MAX_10_EXP          308
#endif

using namespace Firebird;
using namespace Jrd;


namespace Jrd
{
	struct Keyword
	{
		Keyword(int aKeyword, MetaName* aStr)
			: keyword(aKeyword), str(aStr)
		{}

		int keyword;
		MetaName* str;
	};

	class KeywordsMap : public GenericMap<Pair<Left<MetaName, Keyword> > >
	{
	public:
		explicit KeywordsMap(MemoryPool& pool)
			: GenericMap<Pair<Left<MetaName, Keyword> > >(pool)
		{
			for (const TOK* token = keywordGetTokens(); token->tok_string; ++token)
			{
				MetaName* str = FB_NEW_POOL(pool) MetaName(token->tok_string);
				put(*str, Keyword(token->tok_ident, str));
			}
		}

		~KeywordsMap()
		{
			Accessor accessor(this);
			for (bool found = accessor.getFirst(); found; found = accessor.getNext())
				delete accessor.current()->second.str;
		}
	};

	KeywordsMap* KeywordsMapAllocator::create()
	{
		thread_db* tdbb = JRD_get_thread_data();
		fb_assert(tdbb);
		Database* dbb = tdbb->getDatabase();
		fb_assert(dbb);

		return FB_NEW_POOL(*dbb->dbb_permanent) KeywordsMap(*dbb->dbb_permanent);
	}

	void KeywordsMapAllocator::destroy(KeywordsMap* inst)
	{
		delete inst;
	}
}

namespace
{
	const Keyword* getKeyword(Database* dbb, const MetaName& str)
	{
		return dbb->dbb_keywords_map().get(str);
	}
}


Parser::Parser(thread_db* tdbb, MemoryPool& pool, DsqlCompilerScratch* aScratch,
			USHORT aClientDialect, USHORT aDbDialect, const TEXT* string, size_t length,
			SSHORT characterSet)
	: PermanentStorage(pool),
	  scratch(aScratch),
	  client_dialect(aClientDialect),
	  db_dialect(aDbDialect),
	  transformedString(pool),
	  strMarks(pool),
	  stmt_ambiguous(false)
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

	lex.start = string;
	lex.line_start = lex.last_token = lex.ptr = string;
	lex.end = string + length;
	lex.lines = 1;
	lex.att_charset = characterSet;
	lex.line_start_bk = lex.line_start;
	lex.lines_bk = lex.lines;
	lex.param_number = 1;
	lex.prev_keyword = -1;

#ifdef DSQL_DEBUG
	if (DSQL_debug & 32)
		dsql_trace("Source DSQL string:\n%.*s", (int) length, string);
#endif

	metadataCharSet = INTL_charset_lookup(tdbb, CS_METADATA);
}


Parser::~Parser()
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


dsql_req* Parser::parse()
{
	if (parseAux() != 0)
	{
		fb_assert(false);
		return NULL;
	}

	transformString(lex.start, lex.end - lex.start, transformedString);

	return DSQL_parse;
}


// Transform strings (or substrings) prefixed with introducer (_charset) to ASCII equivalent.
void Parser::transformString(const char* start, unsigned length, string& dest)
{
	const static char HEX_DIGITS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F'};

	const unsigned fromBegin = start - lex.start;
	HalfStaticArray<char, 256> buffer;
	const char* pos = start;

	// We need only the "introduced" strings, in the bounds of "start" and "length" and in "pos"
	// order. Let collect them.

	SortedArray<StrMark> introducedMarks;

	GenericMap<NonPooled<IntlString*, StrMark> >::ConstAccessor accessor(&strMarks);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
	{
		const StrMark& mark = accessor.current()->second;
		if (mark.introduced && mark.pos >= fromBegin && mark.pos < fromBegin + length)
			introducedMarks.add(mark);
	}

	for (FB_SIZE_T i = 0; i < introducedMarks.getCount(); ++i)
	{
		const StrMark& mark = introducedMarks[i];

		const char* s = lex.start + mark.pos;
		buffer.add(pos, s - pos);

		if (!isspace(UCHAR(pos[s - pos - 1])))
			buffer.add(' ');	// fix _charset'' becoming invalid syntax _charsetX''

		const FB_SIZE_T count = buffer.getCount();
		const FB_SIZE_T newSize = count + 2 + mark.str->getString().length() * 2 + 1;
		buffer.grow(newSize);
		char* p = buffer.begin() + count;

		*p++ = 'X';
		*p++ = '\'';

		const char* s2 = mark.str->getString().c_str();

		for (const char* end = s2 + mark.str->getString().length(); s2 < end; ++s2)
		{
			*p++ = HEX_DIGITS[UCHAR(*s2) >> 4];
			*p++ = HEX_DIGITS[UCHAR(*s2) & 0xF];
		}

		*p = '\'';
		fb_assert(p < buffer.begin() + newSize);

		pos = s + mark.length;
	}

	fb_assert(start + length - pos >= 0);
	buffer.add(pos, start + length - pos);

	dest.assign(buffer.begin(), MIN(string::max_length(), buffer.getCount()));
}


// Make a substring from the command text being parsed.
string Parser::makeParseStr(const Position& p1, const Position& p2)
{
	const char* start = p1.firstPos;
	const char* end = p2.lastPos;

	string str;
	transformString(start, end - start, str);

	string ret;

	if (DataTypeUtil::convertToUTF8(str, ret))
		return ret;

	return str;
}


// Make parameter node.
ParameterNode* Parser::make_parameter()
{
	thread_db* tdbb = JRD_get_thread_data();

	ParameterNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) ParameterNode(*tdbb->getDefaultPool());
	node->dsqlParameterIndex = lex.param_number++;

	return node;
}


// Set the position of a left-hand non-terminal based on its right-hand rules.
void Parser::yyReducePosn(YYPOSN& ret, YYPOSN* termPosns, YYSTYPE* /*termVals*/, int termNo,
	int /*stkPos*/, int /*yychar*/, YYPOSN& /*yyposn*/, void*)
{
	if (termNo == 0)
	{
		// Accessing termPosns[-1] seems to be the only way to get correct positions in this case.
		ret.firstLine = ret.lastLine = termPosns[termNo - 1].lastLine;
		ret.firstColumn = ret.lastColumn = termPosns[termNo - 1].lastColumn;
		ret.firstPos = ret.lastPos = termPosns[termNo - 1].lastPos;
	}
	else
	{
		ret.firstLine = termPosns[0].firstLine;
		ret.firstColumn = termPosns[0].firstColumn;
		ret.firstPos = termPosns[0].firstPos;
		ret.lastLine = termPosns[termNo - 1].lastLine;
		ret.lastColumn = termPosns[termNo - 1].lastColumn;
		ret.lastPos = termPosns[termNo - 1].lastPos;
	}

	/*** This allows us to see colored output representing the position reductions.
	printf("%.*s", int(ret.firstPos - lex.start), lex.start);
	printf("<<<<<");
	printf("\033[1;31m%.*s\033[1;37m", int(ret.lastPos - ret.firstPos), ret.firstPos);
	printf(">>>>>");
	printf("%s\n", ret.lastPos);
	***/
}


int Parser::yylex()
{
	if (!yylexSkipSpaces())
		return -1;

	yyposn.firstLine = lex.lines;
	yyposn.firstColumn = lex.ptr - lex.line_start;
	yyposn.firstPos = lex.ptr - 1;

	lex.prev_keyword = yylexAux();

	const TEXT* ptr = lex.ptr;
	const TEXT* last_token = lex.last_token;
	const TEXT* line_start = lex.line_start;
	const SLONG lines = lex.lines;

	// Lets skip spaces before store lastLine/lastColumn. This is necessary to avoid yyReducePosn
	// produce invalid line/column information - CORE-4381.
	yylexSkipSpaces();

	yyposn.lastLine = lex.lines;
	yyposn.lastColumn = lex.ptr - lex.line_start;

	lex.ptr = ptr;
	lex.last_token = last_token;
	lex.line_start = line_start;
	lex.lines = lines;

	// But the correct value for lastPos is the old (before the second yyLexSkipSpaces)
	// value of lex.ptr.
	yyposn.lastPos = ptr;

	return lex.prev_keyword;
}


bool Parser::yylexSkipSpaces()
{
	UCHAR tok_class;
	SSHORT c;

	// Find end of white space and skip comments

	for (;;)
	{
		if (lex.ptr >= lex.end)
			return false;

		if (yylexSkipEol())
			continue;

		// Process comments

		c = *lex.ptr++;
		if (c == '-' && lex.ptr < lex.end && *lex.ptr == '-')
		{
			// single-line

			lex.ptr++;
			while (lex.ptr < lex.end)
			{
				if (yylexSkipEol())
					break;
				lex.ptr++;
			}
			if (lex.ptr >= lex.end)
				return false;

			continue;
		}
		else if (c == '/' && lex.ptr < lex.end && *lex.ptr == '*')
		{
			// multi-line

			const TEXT& start_block = lex.ptr[-1];
			lex.ptr++;
			while (lex.ptr < lex.end)
			{
				if (yylexSkipEol())
					continue;

				if ((c = *lex.ptr++) == '*')
				{
					if (*lex.ptr == '/')
						break;
				}
			}
			if (lex.ptr >= lex.end)
			{
				// I need this to report the correct beginning of the block,
				// since it's not a token really.
				lex.last_token = &start_block;
				yyerror("unterminated block comment");
				return false;
			}
			lex.ptr++;
			continue;
		}

		tok_class = classes(c);

		if (!(tok_class & CHR_WHITE))
			break;
	}

	return true;
}


bool Parser::yylexSkipEol()
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
		lex.lines++;
		lex.line_start = lex.ptr; // + 1; // CVC: +1 left out.
	}

	return eol;
}


int Parser::yylexAux()
{
	thread_db* tdbb = JRD_get_thread_data();
	Database* const dbb = tdbb->getDatabase();
	MemoryPool& pool = *tdbb->getDefaultPool();

	unsigned maxByteLength, maxCharLength;

	if (scratch->flags & DsqlCompilerScratch::FLAG_INTERNAL_REQUEST)
	{
		maxByteLength = MAX_SQL_IDENTIFIER_LEN;
		maxCharLength = METADATA_IDENTIFIER_CHAR_LEN;
	}
	else
	{
		maxByteLength = dbb->dbb_config->getMaxIdentifierByteLength();
		maxCharLength = dbb->dbb_config->getMaxIdentifierCharLength();
	}

	SSHORT c = lex.ptr[-1];
	UCHAR tok_class = classes(c);
	char string[MAX_TOKEN_LEN];

	// Depending on tok_class of token, parse token

	lex.last_token = lex.ptr - 1;

	if (tok_class & CHR_INTRODUCER)
	{
		// The Introducer (_) is skipped, all other idents are copied
		// to become the name of the character set.
		char* p = string;
		for (; lex.ptr < lex.end && (classes(*lex.ptr) & CHR_IDENT); lex.ptr++)
		{
			if (lex.ptr >= lex.end)
				return -1;

			check_copy_incr(p, UPPER7(*lex.ptr), string);
		}

		check_bound(p, string);

		if (p > string + maxByteLength || p > string + maxCharLength)
			yyabandon(yyposn, -104, isc_dyn_name_longer);

		*p = 0;

		// make a string value to hold the name, the name is resolved in pass1_constant.
		yylval.metaNamePtr = FB_NEW_POOL(pool) MetaName(pool, string, p - string);

		return TOK_INTRODUCER;
	}

	// parse a quoted string, being sure to look for double quotes

	if (tok_class & CHR_QUOTE)
	{
		StrMark mark;
		mark.pos = lex.last_token - lex.start;

		char* buffer = string;
		SLONG buffer_len = sizeof(string);
		const char* buffer_end = buffer + buffer_len - 1;
		char* p;
		for (p = buffer; ; ++p)
		{
			if (lex.ptr >= lex.end)
			{
				if (buffer != string)
					gds__free (buffer);
				yyerror("unterminated string");
				return -1;
			}
			// Care about multi-line constants and identifiers
			if (*lex.ptr == '\n')
			{
				lex.lines++;
				lex.line_start = lex.ptr + 1;
			}
			// *lex.ptr is quote - if next != quote we're at the end
			if ((*lex.ptr == c) && ((++lex.ptr == lex.end) || (*lex.ptr != c)))
				break;
			if (p > buffer_end)
			{
				char* const new_buffer = (char*) gds__alloc (2 * buffer_len);
				// FREE: at outer block
				if (!new_buffer)		// NOMEM:
				{
					if (buffer != string)
						gds__free (buffer);
					return -1;
				}
				memcpy (new_buffer, buffer, buffer_len);
				if (buffer != string)
					gds__free (buffer);
				buffer = new_buffer;
				p = buffer + buffer_len;
				buffer_len = 2 * buffer_len;
				buffer_end = buffer + buffer_len - 1;
			}
			*p = *lex.ptr++;
		}

		if (p - buffer > MAX_STR_SIZE)
		{
			if (buffer != string)
				gds__free (buffer);

			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_string_byte_length) <<
					  Arg::Num(p - buffer) <<
					  Arg::Num(MAX_STR_SIZE));
		}

		if (c == '"')
		{
			stmt_ambiguous = true;
			// string delimited by double quotes could be
			// either a string constant or a SQL delimited
			// identifier, therefore marks the SQL statement as ambiguous

			if (client_dialect == SQL_DIALECT_V6_TRANSITION)
			{
				if (buffer != string)
					gds__free (buffer);
				yyabandon(yyposn, -104, isc_invalid_string_constant);
			}
			else if (client_dialect >= SQL_DIALECT_V6)
			{
				if (p - buffer >= MAX_TOKEN_LEN)
				{
					if (buffer != string)
						gds__free (buffer);
					yyabandon(yyposn, -104, isc_token_too_long);
				}
				else if (p > &buffer[MAX_SQL_IDENTIFIER_LEN])
				{
					if (buffer != string)
						gds__free (buffer);
					yyabandon(yyposn, -104, isc_dyn_name_longer);
				}
				else if (p - buffer == 0)
				{
					if (buffer != string)
						gds__free (buffer);
					yyabandon(yyposn, -104, isc_dyn_zero_len_id);
				}

				Attachment* const attachment = tdbb->getAttachment();
				const MetaName name(attachment->nameToMetaCharSet(tdbb, MetaName(buffer, p - buffer)));
				const unsigned charLength = metadataCharSet->length(
					name.length(), (const UCHAR*) name.c_str(), true);

				if (name.length() > maxByteLength || charLength > maxCharLength)
					yyabandon(yyposn, -104, isc_dyn_name_longer);

				yylval.metaNamePtr = FB_NEW_POOL(pool) MetaName(pool, name);

				if (buffer != string)
					gds__free (buffer);

				return TOK_SYMBOL;
			}
		}
		yylval.intlStringPtr = newIntlString(Firebird::string(buffer, p - buffer));
		if (buffer != string)
			gds__free (buffer);

		mark.length = lex.ptr - lex.last_token;
		mark.str = yylval.intlStringPtr;
		strMarks.put(mark.str, mark);

		return TOK_STRING;
	}

	/*
	 * Check for a numeric constant, which starts either with a digit or with
	 * a decimal point followed by a digit.
	 *
	 * This code recognizes the following token types:
	 *
	 * NUMBER32BIT: string of digits which fits into a 32-bit integer
	 *
	 * NUMBER64BIT: string of digits whose value might fit into an SINT64,
	 *   depending on whether or not there is a preceding '-', which is to
	 *   say that "9223372036854775808" is accepted here.
	 *
	 * SCALEDINT: string of digits and a single '.', where the digits
	 *   represent a value which might fit into an SINT64, depending on
	 *   whether or not there is a preceding '-'.
	 *
	 * FLOAT: string of digits with an optional '.', and followed by an "e"
	 *   or "E" and an optionally-signed exponent.
	 *
	 * NOTE: we swallow leading or trailing blanks, but we do NOT accept
	 *   embedded blanks:
	 *
	 * Another note: c is the first character which need to be considered,
	 *   ptr points to the next character.
	 */

	fb_assert(lex.ptr <= lex.end);

	// Hexadecimal string constant.  This is treated the same as a
	// string constant, but is defined as: X'bbbb'
	//
	// Where the X is a literal 'x' or 'X' character, followed
	// by a set of nibble values in single quotes.  The nibble
	// can be 0-9, a-f, or A-F, and is converted from the hex.
	// The number of nibbles should be even.
	//
	// The resulting value is stored in a string descriptor and
	// returned to the parser as a string.  This can be stored
	// in a character or binary item.
	if ((c == 'x' || c == 'X') && lex.ptr < lex.end && *lex.ptr == '\'')
	{
		bool hexerror = false;

		// Remember where we start from, to rescan later.
		// Also we'll need to know the length of the buffer.

		const char* hexstring = ++lex.ptr;
		int charlen = 0;

		// Time to scan the string. Make sure the characters are legal,
		// and find out how long the hex digit string is.

		for (;;)
		{
			if (lex.ptr >= lex.end)	// Unexpected EOS
			{
				hexerror = true;
				break;
			}

			c = *lex.ptr;

			if (c == '\'')			// Trailing quote, done
			{
				++lex.ptr;			// Skip the quote
				break;
			}

			if (!(classes(c) & CHR_HEX))	// Illegal character
			{
				hexerror = true;
				break;
			}

			++charlen;	// Okay, just count 'em
			++lex.ptr;	// and advance...
		}

		hexerror = hexerror || (charlen & 1);	// IS_ODD(charlen)

		// If we made it this far with no error, then convert the string.
		if (!hexerror)
		{
			// Figure out the length of the actual resulting hex string.
			// Allocate a second temporary buffer for it.

			Firebird::string temp;

			// Re-scan over the hex string we got earlier, converting
			// adjacent bytes into nibble values.  Every other nibble,
			// write the saved byte to the temp space.  At the end of
			// this, the temp.space area will contain the binary
			// representation of the hex constant.

			UCHAR byte = 0;
			for (int i = 0; i < charlen; i++)
			{
				c = UPPER7(hexstring[i]);

				// Now convert the character to a nibble

				if (c >= 'A')
					c = (c - 'A') + 10;
				else
					c = (c - '0');

				if (i & 1) // nibble?
				{
					byte = (byte << 4) + (UCHAR) c;
					temp.append(1, (char) byte);
				}
				else
					byte = c;
			}

			if (temp.length() / 2 > MAX_STR_SIZE)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_string_byte_length) <<
						  Arg::Num(temp.length() / 2) <<
						  Arg::Num(MAX_STR_SIZE));
			}

			yylval.intlStringPtr = newIntlString(temp, "BINARY");

			return TOK_STRING;
		}  // if (!hexerror)...

		// If we got here, there was a parsing error.  Set the
		// position back to where it was before we messed with
		// it.  Then fall through to the next thing we might parse.

		c = *lex.last_token;
		lex.ptr = lex.last_token + 1;
	}

	if ((c == 'q' || c == 'Q') && lex.ptr + 3 < lex.end && *lex.ptr == '\'')
	{
		StrMark mark;
		mark.pos = lex.last_token - lex.start;

		char endChar = *++lex.ptr;
		switch (endChar)
		{
			case '{':
				endChar = '}';
				break;
			case '(':
				endChar = ')';
				break;
			case '[':
				endChar = ']';
				break;
			case '<':
				endChar = '>';
				break;
		}

		while (++lex.ptr + 1 < lex.end)
		{
			if (*lex.ptr == endChar && lex.ptr[1] == '\'')
			{
				size_t len = ++lex.ptr - lex.last_token - 4;

				if (len > MAX_STR_SIZE)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_string_byte_length) <<
							  Arg::Num(len) <<
							  Arg::Num(MAX_STR_SIZE));
				}

				yylval.intlStringPtr = newIntlString(Firebird::string(lex.last_token + 3, len));

				++lex.ptr;

				mark.length = lex.ptr - lex.last_token;
				mark.str = yylval.intlStringPtr;
				strMarks.put(mark.str, mark);

				return TOK_STRING;
			}
		}

		// If we got here, there was a parsing error.  Set the
		// position back to where it was before we messed with
		// it.  Then fall through to the next thing we might parse.

		c = *lex.last_token;
		lex.ptr = lex.last_token + 1;
	}

	// Hexadecimal numeric constants - 0xBBBBBB
	//
	// where the '0' and the 'X' (or 'x') are literal, followed
	// by a set of nibbles, using 0-9, a-f, or A-F.  Odd numbers
	// of nibbles assume a leading '0'.  The result is converted
	// to an integer, and the result returned to the caller.  The
	// token is identified as a NUMBER32BIT if it's a 32-bit or less
	// value, or a NUMBER64INT if it requires a 64-bit number.
	if (c == '0' && lex.ptr + 1 < lex.end && (*lex.ptr == 'x' || *lex.ptr == 'X') &&
		(classes(lex.ptr[1]) & CHR_HEX))
	{
		bool hexerror = false;

		// Remember where we start from, to rescan later.
		// Also we'll need to know the length of the buffer.

		++lex.ptr;  // Skip the 'X' and point to the first digit
		const char* hexstring = lex.ptr;
		int charlen = 0;

		// Time to scan the string. Make sure the characters are legal,
		// and find out how long the hex digit string is.

		while (lex.ptr < lex.end)
		{
			c = *lex.ptr;

			if (!(classes(c) & CHR_HEX))	// End of digit string
				break;

			++charlen;			// Okay, just count 'em
			++lex.ptr;			// and advance...

			if (charlen > 16)	// Too many digits...
			{
				hexerror = true;
				break;
			}
		}

		// we have a valid hex token. Now give it back, either as
		// an NUMBER32BIT or NUMBER64BIT.
		if (!hexerror)
		{
			// if charlen > 8 (something like FFFF FFFF 0, w/o the spaces)
			// then we have to return a NUMBER64BIT. We'll make a string
			// node here, and let make.cpp worry about converting the
			// string to a number and building the node later.
			if (charlen > 8)
			{
				char cbuff[32];
				fb_assert(charlen <= 16);	// charlen is always <= 16, see 10-15 lines upper
				cbuff[0] = 'X';
				fb_utils::copy_terminate(&cbuff[1], hexstring, charlen + 1);

				char* p = &cbuff[1];
				UCHAR byte = 0;
				bool nibble = strlen(p) & 1;

				yylval.scaledNumber.number = 0;
				yylval.scaledNumber.scale = 0;
				yylval.scaledNumber.hex = true;

				while (*p)
				{
					if ((*p >= 'a') && (*p <= 'f'))
						*p = UPPER(*p);

					// Now convert the character to a nibble
					SSHORT c;

					if (*p >= 'A')
						c = (*p - 'A') + 10;
					else
						c = (*p - '0');

					if (nibble)
					{
						byte = (byte << 4) + (UCHAR) c;
						nibble = false;
						yylval.scaledNumber.number = (yylval.scaledNumber.number << 8) + byte;
					}
					else
					{
						byte = c;
						nibble = true;
					}

					++p;
				}

				// The return value can be a negative number.
				return TOK_NUMBER64BIT;
			}
			else
			{
				// we have an integer value. we'll return NUMBER32BIT.
				// but we have to make a number value to be compatible
				// with existing code.

				// See if the string length is odd.  If so,
				// we'll assume a leading zero.  Then figure out the length
				// of the actual resulting hex string.  Allocate a second
				// temporary buffer for it.

				bool nibble = (charlen & 1);  // IS_ODD(temp.length)

				// Re-scan over the hex string we got earlier, converting
				// adjacent bytes into nibble values.  Every other nibble,
				// write the saved byte to the temp space.  At the end of
				// this, the temp.space area will contain the binary
				// representation of the hex constant.

				UCHAR byte = 0;
				SINT64 value = 0;

				for (int i = 0; i < charlen; i++)
				{
					c = UPPER(hexstring[i]);

					// Now convert the character to a nibble

					if (c >= 'A')
						c = (c - 'A') + 10;
					else
						c = (c - '0');

					if (nibble)
					{
						byte = (byte << 4) + (UCHAR) c;
						nibble = false;
						value = (value << 8) + byte;
					}
					else
					{
						byte = c;
						nibble = true;
					}
				}

				yylval.int32Val = (SLONG) value;
				return TOK_NUMBER32BIT;
			} // integer value
		}  // if (!hexerror)...

		// If we got here, there was a parsing error.  Set the
		// position back to where it was before we messed with
		// it.  Then fall through to the next thing we might parse.

		c = *lex.last_token;
		lex.ptr = lex.last_token + 1;
	} // headecimal numeric constants

	if ((tok_class & CHR_DIGIT) ||
		((c == '.') && (lex.ptr < lex.end) && (classes(*lex.ptr) & CHR_DIGIT)))
	{
		// The following variables are used to recognize kinds of numbers.

		bool have_error = false;		// syntax error or value too large
		bool have_digit = false;		// we've seen a digit
		bool have_decimal = false;		// we've seen a '.'
		bool have_exp = false;			// digit ... [eE]
		bool have_exp_sign = false;		// digit ... [eE] {+-]
		bool have_exp_digit = false;	// digit ... [eE] ... digit
		bool have_overflow = false;		// value of digits > MAX_SINT64
		bool positive_overflow = false;	// number is exactly (MAX_SINT64 + 1)
		bool have_128_over = false;		// value of digits > MAX_INT128
		FB_UINT64 number = 0;
		Int128 num128;
		int expVal = 0;
		FB_UINT64 limit_by_10 = MAX_SINT64 / 10;
		int scale = 0;
		int expSign = 1;

		for (--lex.ptr; lex.ptr < lex.end; lex.ptr++)
		{
			c = *lex.ptr;
			if (have_exp_digit && (! (classes(c) & CHR_DIGIT)))
				// First non-digit after exponent and digit terminates the token.
				break;

			if (have_exp_sign && (! (classes(c) & CHR_DIGIT)))
			{
				// only digits can be accepted after "1E-"
				have_error = true;
				break;
			}

			if (have_exp)
			{
				// We've seen e or E, but nothing beyond that.
				if ( ('-' == c) || ('+' == c) )
				{
					have_exp_sign = true;
					if ('-' == c)
						expSign = -1;
				}
				else if ( classes(c) & CHR_DIGIT )
				{
					// We have a digit: we haven't seen a sign yet, but it's too late now.
					have_exp_digit = have_exp_sign  = true;
					if (!have_overflow)
					{
						expVal = expVal * 10 + (c - '0');
						if (expVal > DBL_MAX_10_EXP)
							have_overflow = true;
					}
				}
				else
				{
					// end of the token
					have_error = true;
					break;
				}
			}
			else if ('.' == c)
			{
				if (!have_decimal)
					have_decimal = true;
				else
				{
					have_error = true;
					break;
				}
			}
			else if (classes(c) & CHR_DIGIT)
			{
				// Before computing the next value, make sure there will be no overflow.

				if (!have_overflow)
				{
					have_digit = true;

					if (number >= limit_by_10)
					{
						// possibility of an overflow
						if ((number > limit_by_10) || (c >= '8'))
						{
							have_overflow = true;
							fb_assert(number <= MAX_SINT64);
							num128.set((SINT64)number, 0);
							if ((number == limit_by_10) && (c == '8'))
								positive_overflow = true;
						}
					}
				}
				else
				{
					positive_overflow = false;
					if (!have_128_over)
					{
						static const CInt128 MAX_BY10(MAX_Int128 / 10);
						if ((num128 >= MAX_BY10) && ((num128 > MAX_BY10) || (c >= '8')))
							have_128_over = true;
					}
				}

				if (!have_overflow)
					number = number * 10 + (c - '0');
				else if (!have_128_over)
				{
					num128 *= 10;
					num128 += (c - '0');
				}

				if (have_decimal)
					--scale;
			}
			else if ( (('E' == c) || ('e' == c)) && have_digit )
				have_exp = true;
			else
				// Unexpected character: this is the end of the number.
				break;
		}

		// We're done scanning the characters: now return the right kind
		// of number token, if any fits the bill.

		if (!have_error)
		{
			fb_assert(have_digit);

			if (positive_overflow)
				have_overflow = false;

			if (scale < MIN_SCHAR || scale > MAX_SCHAR)
			{
				have_overflow = true;
				positive_overflow = false;
				have_128_over = true;
			}

			// check for a more complex overflow case
			if ((!have_overflow) && (expSign > 0) && (expVal > -scale))
			{
				expVal += scale;
				double maxNum = DBL_MAX / pow(10.0, expVal);
				if (double(number) > maxNum)
				{
					have_overflow = true;
					positive_overflow = false;
					have_128_over = true;
				}
			}

			// Special case - on the boarder of positive number
			if (positive_overflow)
			{
				yylval.lim64ptr = newLim64String(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token), scale);
				lex.last_token_bk = lex.last_token;
				lex.line_start_bk = lex.line_start;
				lex.lines_bk = lex.lines;

				return scale ? TOK_LIMIT64_NUMBER : TOK_LIMIT64_INT;
			}

			// Should we use floating point type?
			if (have_exp_digit || have_128_over)
			{
				yylval.stringPtr = newString(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token));
				lex.last_token_bk = lex.last_token;
				lex.line_start_bk = lex.line_start;
				lex.lines_bk = lex.lines;

				return have_overflow ? TOK_DECIMAL_NUMBER : TOK_FLOAT_NUMBER;
			}

			// May be 128-bit integer?
			if (have_overflow)
			{
				yylval.lim64ptr = newLim64String(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token), scale);
				lex.last_token_bk = lex.last_token;
				lex.line_start_bk = lex.line_start;
				lex.lines_bk = lex.lines;

				return TOK_NUM128;
			}

			if (!have_exp)
			{
				// We should return some kind (scaled-) integer type
				// except perhaps in dialect 1.

				if (!have_decimal && (number <= MAX_SLONG))
				{
					yylval.int32Val = (SLONG) number;
					//printf ("parse.y %p %d\n", yylval.legacyStr, number);
					return TOK_NUMBER32BIT;
				}
				else
				{
					/* We have either a decimal point with no exponent
					   or a string of digits whose value exceeds MAX_SLONG:
					   the returned type depends on the client dialect,
					   so warn of the difference if the client dialect is
					   SQL_DIALECT_V6_TRANSITION.
					*/

					if (SQL_DIALECT_V6_TRANSITION == client_dialect)
					{
						/* Issue a warning about the ambiguity of the numeric
						 * numeric literal.  There are multiple calls because
						 * the message text exceeds the 119-character limit
						 * of our message database.
						 */
						ERRD_post_warning(Arg::Warning(isc_dsql_warning_number_ambiguous) <<
										  Arg::Str(Firebird::string(lex.last_token, lex.ptr - lex.last_token)));
						ERRD_post_warning(Arg::Warning(isc_dsql_warning_number_ambiguous1));
					}

					lex.last_token_bk = lex.last_token;
					lex.line_start_bk = lex.line_start;
					lex.lines_bk = lex.lines;

					if (client_dialect < SQL_DIALECT_V6_TRANSITION)
					{
						yylval.stringPtr = newString(
							Firebird::string(lex.last_token, lex.ptr - lex.last_token));
						return TOK_FLOAT_NUMBER;
					}

					yylval.scaledNumber.number = number;
					yylval.scaledNumber.scale = scale;
					yylval.scaledNumber.hex = false;

					if (have_decimal)
						return TOK_SCALEDINT;

					return TOK_NUMBER64BIT;
				}
			} // else if (!have_exp)
		} // if (!have_error)

		// we got some kind of error or overflow, so don't recognize this
		// as a number: just pass it through to the next part of the lexer.
	}

	// Restore the status quo ante, before we started our unsuccessful
	// attempt to recognize a number.
	lex.ptr = lex.last_token;
	c = *lex.ptr++;
	// We never touched tok_class, so it doesn't need to be restored.

	// end of number-recognition code

	if (tok_class & CHR_LETTER)
	{
		char* p = string;
		check_copy_incr(p, UPPER (c), string);
		for (; lex.ptr < lex.end && (classes(*lex.ptr) & CHR_IDENT); lex.ptr++)
		{
			if (lex.ptr >= lex.end)
				return -1;
			check_copy_incr(p, UPPER (*lex.ptr), string);
		}

		check_bound(p, string);
		*p = 0;

		if (p > &string[maxByteLength] || p > &string[maxCharLength])
			yyabandon(yyposn, -104, isc_dyn_name_longer);

		const MetaName str(string, p - string);
		const Keyword* const keyVer = getKeyword(dbb, str);

		if (keyVer && (keyVer->keyword != TOK_COMMENT || lex.prev_keyword == -1))
		{
			yylval.metaNamePtr = keyVer->str;
			lex.last_token_bk = lex.last_token;
			lex.line_start_bk = lex.line_start;
			lex.lines_bk = lex.lines;
			return keyVer->keyword;
		}

		yylval.metaNamePtr = FB_NEW_POOL(pool) MetaName(pool, str);
		lex.last_token_bk = lex.last_token;
		lex.line_start_bk = lex.line_start;
		lex.lines_bk = lex.lines;
		return TOK_SYMBOL;
	}

	// Must be punctuation -- test for double character punctuation

	if (lex.last_token + 1 < lex.end && !isspace(UCHAR(lex.last_token[1])))
	{
		const MetaName str(lex.last_token, 2);
		const Keyword* const keyVer = getKeyword(dbb, str);

		if (keyVer)
		{
			++lex.ptr;
			return keyVer->keyword;
		}
	}

	// Single character punctuation are simply passed on

	return (UCHAR) c;
}


void Parser::yyerror_detailed(const TEXT* /*error_string*/, int yychar, YYSTYPE&, YYPOSN&)
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
	const TEXT* line_start = lex.line_start;
	SLONG lines = lex.lines;
	if (lex.last_token < line_start)
	{
		line_start = lex.line_start_bk;
		lines--;
	}

	if (yychar < 1)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Unexpected end of command
				  Arg::Gds(isc_command_end_err2) << Arg::Num(lines) <<
													Arg::Num(lex.last_token - line_start + 1));
	}
	else
	{
		ERRD_post (Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Token unknown - line %d, column %d
				  Arg::Gds(isc_dsql_token_unk_err) << Arg::Num(lines) <<
				  									  Arg::Num(lex.last_token - line_start + 1) << // CVC: +1
				  // Show the token
				  Arg::Gds(isc_random) << Arg::Str(string(lex.last_token, lex.ptr - lex.last_token)));
	}
}


// The argument passed to this function is ignored. Therefore, messages like
// "syntax error" and "yacc stack overflow" are never seen.
void Parser::yyerror(const TEXT* error_string)
{
	YYSTYPE errt_value;
	YYPOSN errt_posn;
	yyerror_detailed(error_string, -1, errt_value, errt_posn);
}

void Parser::yyerrorIncompleteCmd()
{
	const TEXT* line_start = lex.line_start;
	SLONG lines = lex.lines;

	ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			  // Unexpected end of command
			  Arg::Gds(isc_command_end_err2) << Arg::Num(lines) <<
												Arg::Num(lex.ptr - line_start + 1));
}

void Parser::check_bound(const char* const to, const char* const string)
{
	if ((to - string) >= Parser::MAX_TOKEN_LEN)
		yyabandon(yyposn, -104, isc_token_too_long);
}

void Parser::check_copy_incr(char*& to, const char ch, const char* const string)
{
	check_bound(to, string);
	*to++ = ch;
}


void Parser::yyabandon(const Position& position, SLONG sql_code, ISC_STATUS error_symbol)
{
/**************************************
 *
 *	y y a b a n d o n
 *
 **************************************
 *
 * Functional description
 *	Abandon the parsing outputting the supplied string
 *
 **************************************/

	ERRD_post(
		Arg::Gds(isc_sqlerr) << Arg::Num(sql_code) << Arg::Gds(error_symbol) <<
		Arg::Gds(isc_dsql_line_col_error) <<
			Arg::Num(position.firstLine) << Arg::Num(position.firstColumn));
}

void Parser::yyabandon(const Position& position, SLONG sql_code, const Arg::StatusVector& status)
{
/**************************************
 *
 *	y y a b a n d o n
 *
 **************************************
 *
 * Functional description
 *	Abandon the parsing outputting the supplied string
 *
 **************************************/
	ERRD_post(
		Arg::Gds(isc_sqlerr) << Arg::Num(sql_code) << status <<
		Arg::Gds(isc_dsql_line_col_error) <<
			Arg::Num(position.firstLine) << Arg::Num(position.firstColumn));
}

void Parser::checkTimeDialect()
{
	if (client_dialect < SQL_DIALECT_V6_TRANSITION)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_sql_dialect_datatype_unsupport) << Arg::Num(client_dialect) <<
																  Arg::Str("TIME"));
	}
	if (db_dialect < SQL_DIALECT_V6_TRANSITION)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_sql_db_dialect_dtype_unsupport) << Arg::Num(db_dialect) <<
																  Arg::Str("TIME"));
	}
}
