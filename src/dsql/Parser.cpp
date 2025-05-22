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
#include "../dsql/metd_proto.h"
#include "../jrd/intl_proto.h"

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
#define DBL_MAX_10_EXP          308
#endif

using namespace Firebird;
using namespace Jrd;


Parser::Parser(thread_db* tdbb, MemoryPool& pool, MemoryPool* aStatementPool, DsqlCompilerScratch* aScratch,
			USHORT aClientDialect, USHORT aDbDialect, bool aRequireSemicolon,
			const TEXT* string, size_t length, SSHORT charSetId)
	: PermanentStorage(pool),
	  statementPool(aStatementPool),
	  scratch(aScratch),
	  client_dialect(aClientDialect),
	  db_dialect(aDbDialect),
	  requireSemicolon(aRequireSemicolon),
	  transformedString(pool),
	  strMarks(pool),
	  stmt_ambiguous(false)
{
	charSet = INTL_charset_lookup(tdbb, charSetId);

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

	yyposn.firstLine = 1;
	yyposn.firstColumn = 1;
	yyposn.lastLine = 1;
	yyposn.lastColumn = 1;
	yyposn.firstPos = string;
	yyposn.leadingFirstPos = string;
	yyposn.lastPos = string + length;
	yyposn.trailingLastPos = string + length;

	lex.start = string;
	lex.line_start = lex.last_token = lex.ptr = lex.leadingPtr = string;
	lex.end = string + length;
	lex.lines = 1;
	lex.charSetId = charSetId;
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


DsqlStatement* Parser::parse()
{
	if (parseAux() != 0)
	{
		fb_assert(false);
		return NULL;
	}

	transformString(lex.start, lex.end - lex.start, transformedString);

	return parsedStatement;
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

		if (!fb_utils::isspace(pos[s - pos - 1]))
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
	const char* start = p1.leadingFirstPos;
	const char* end = p2.trailingLastPos;

	string str;
	transformString(start, end - start, str);
	str.trim(" \t\r\n");

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
	yyposn.leadingFirstPos = lex.leadingPtr;

	lex.prev_keyword = yylexAux();

	yyposn.lastPos = lex.ptr;
	lex.leadingPtr = lex.ptr;

	// Lets skip spaces before store lastLine/lastColumn. This is necessary to avoid yyReducePosn
	// produce invalid line/column information - CORE-4381.
	bool spacesSkipped = yylexSkipSpaces();

	yyposn.lastLine = lex.lines;
	yyposn.lastColumn = lex.ptr - lex.line_start;

	if (spacesSkipped)
		--lex.ptr;

	yyposn.trailingLastPos = lex.ptr;

	return lex.prev_keyword;
}


bool Parser::yylexSkipSpaces()
{
	USHORT tok_class;
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

	SSHORT c = lex.ptr[-1];
	USHORT tok_class = classes(c);
	char string[MAX_TOKEN_LEN];

	// Depending on tok_class of token, parse token

	lex.last_token = lex.ptr - 1;

	if (tok_class & CHR_INTRODUCER)
	{
		// restriction for underscores before numeric literals
		if ((classes(*lex.ptr) & CHR_DIGIT) || *lex.ptr == '.')
			exceptionNumericLiterals(Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));

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

		if (p > string + MAX_SQL_IDENTIFIER_LEN || p > string + METADATA_IDENTIFIER_CHAR_LEN)
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
		char* p = buffer;

		do
		{
			do
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
				*p++ = *lex.ptr++;
			} while (true);

			if (c != '\'')
				break;

			LexerState saveLex = lex;

			if (!yylexSkipSpaces() || lex.ptr[-1] != '\'')
			{
				lex = saveLex;
				break;
			}
		} while (true);

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
					name.length(), (const UCHAR*) name.c_str(), false);

				if (charLength == 0)
					yyabandon(yyposn, -104, isc_dyn_zero_len_id);

				if (name.length() > MAX_SQL_IDENTIFIER_LEN || charLength > METADATA_IDENTIFIER_CHAR_LEN)
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
		++lex.ptr;

		bool hexerror = false;
		Firebird::string temp;
		int leadNibble = -1;

		// Scan over the hex string converting adjacent bytes into nibble values.
		// Every other nibble, write the saved byte to the temp space.
		// At the end of this, the temp.space area will contain the binary representation of the hex constant.
		// Full string could be composed of multiple segments.

		while (!hexerror)
		{
			int leadNibble = -1;

			// Scan over the hex string converting adjacent bytes into nibble values.
			// Every other nibble, write the saved byte to the temp space.
			// At the end of this, the temp.space area will contain the binary representation of the hex constant.

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
				else if (c != ' ')
				{
					if (!(classes(c) & CHR_HEX))	// Illegal character
					{
						hexerror = true;
						break;
					}

					c = UPPER7(c);

					if (c >= 'A')
						c = (c - 'A') + 10;
					else
						c = (c - '0');

					if (leadNibble == -1)
						leadNibble = c;
					else
					{
						temp.append(1, char((leadNibble << 4) + (UCHAR) c));
						leadNibble = -1;
					}
				}

				++lex.ptr;	// and advance...
			}

			hexerror = hexerror || leadNibble != -1;

			LexerState saveLex = lex;

			if (!yylexSkipSpaces() || lex.ptr - 1 == saveLex.ptr || lex.ptr[-1] != '\'')
			{
				lex = saveLex;
				break;
			}
		}

		if (!hexerror)
		{
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
		auto currentCharSet = charSet;

		if (introducerCharSetName)
		{
			const auto symbol = METD_get_charset(scratch->getTransaction(),
				introducerCharSetName->length(), introducerCharSetName->c_str());

			if (!symbol)
			{
				// character set name is not defined
				ERRD_post(
					Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					Arg::Gds(isc_charset_not_found) << *introducerCharSetName);
			}

			currentCharSet = INTL_charset_lookup(tdbb, symbol->intlsym_ttype);
		}

		StrMark mark;
		mark.pos = lex.last_token - lex.start;

		const auto* endChar = ++lex.ptr;
		ULONG endCharSize = 0;

		if (!IntlUtil::readOneChar(currentCharSet, reinterpret_cast<const UCHAR**>(&lex.ptr),
				reinterpret_cast<const UCHAR*>(lex.end), &endCharSize))
		{
			endCharSize = 1;
		}

		if (endCharSize == 1)
		{
			switch (*endChar)
			{
				case '{':
					endChar = "}";
					break;
				case '(':
					endChar = ")";
					break;
				case '[':
					endChar = "]";
					break;
				case '<':
					endChar = ">";
					break;
			}
		}

		const auto start = lex.ptr + endCharSize;
		ULONG charSize = endCharSize;

		while (IntlUtil::readOneChar(currentCharSet, reinterpret_cast<const UCHAR**>(&lex.ptr),
					reinterpret_cast<const UCHAR*>(lex.end), &charSize))
		{
			if (charSize == endCharSize &&
				memcmp(lex.ptr, endChar, endCharSize) == 0 &&
				lex.ptr[endCharSize] == '\'')
			{
				size_t len = lex.ptr - start;

				if (len > MAX_STR_SIZE)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_string_byte_length) <<
							  Arg::Num(len) <<
							  Arg::Num(MAX_STR_SIZE));
				}

				yylval.intlStringPtr = newIntlString(Firebird::string(start, len));

				lex.ptr += endCharSize + 1;

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

	// Non-decimal integer literals (SQL:2023 T661)
	// Underscores in numeric literal support (SQ:2023 T662)
	// See README.decimal_and_non_decimal_literals

	if (c == '0' && lex.ptr + 1 < lex.end)
	{
		auto base = 0;
		SSHORT currExpcChar;

		if (*lex.ptr == 'x' || *lex.ptr == 'X')
		{
			base = 4; // 2^4 0b1111
			currExpcChar = CHR_HEX;
		}
		else if (*lex.ptr == 'o' || *lex.ptr == 'O')
		{
			base = 3; // 2^3 0b111
			currExpcChar = CHR_OCT;
		}
		else if (*lex.ptr == 'b' || *lex.ptr == 'B')
		{
			base = 1; // 2^1 0b1
			currExpcChar = CHR_BIN;
		}

		if (base)
		{
			const auto decimalConversion = 10;
			auto isLastIntroducer = false;
			Int128 value128;
			value128.set(0.0);

			const CInt128 MAX_VALUE(MAX_Int128 >> base);

			// Skip the 'X' or 'O' or 'B' and point to the first digit
			for (++lex.ptr; lex.ptr < lex.end; lex.ptr++)
			{
				c = *lex.ptr;

				if ((classes(c) & CHR_INTRODUCER))
				{
					if (isLastIntroducer)
					{
						exceptionNumericLiterals(
							Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
					}

					isLastIntroducer = true;
				}
				else if ((classes(c) & currExpcChar))
				{
					// check overflow
					if (value128 > MAX_VALUE)
						exceptionNumericLiterals(Firebird::string("Overflow of the number"));

					auto ch = UPPER(c);
					if (ch >= 'A')
						ch = (ch - 'A') + decimalConversion;
					else
						ch = (ch - '0');

					value128 *= 1 << base;
					value128 += ch;

					isLastIntroducer = false;
				}
				else if ((classes(c) & CHR_IDENT) && !(classes(c) & CHR_BRACE))
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}
				else // We have reached the separator
					break;
			}

			// Error 0x
			const auto minimalLength = 3U;
			if ((lex.ptr - lex.last_token) < minimalLength)
			{
				exceptionNumericLiterals(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token));
			}

			// Error of having '_' at the end
			if (isLastIntroducer)
			{
				exceptionNumericLiterals(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token));
			}

			Int128 tmp;
			tmp.set(MAX_SINT64, 0);
			if (value128 > tmp)
			{
				Firebird::string strValue;
				value128.toString(0, strValue);
				yylval.lim64ptr = newLim64String(strValue, 0);
				return TOK_NUM128;
			}

			tmp.set(MAX_SLONG);
			if (value128 > tmp)
			{
				yylval.scaledNumber.number = value128.toInt64(0);
				yylval.scaledNumber.scale = 0;
				yylval.scaledNumber.hex = false;
				return TOK_NUMBER64BIT;
			}
			else
			{
				yylval.int32Val = (SLONG)value128.toInteger(0);
				return TOK_NUMBER32BIT;
			}
		}
	}

	if ((tok_class & CHR_DIGIT) ||
		((c == '.') && (lex.ptr < lex.end) && (classes(*lex.ptr) & CHR_DIGIT)))
	{
		Firebird::string pureString;
		auto isLastIntroducer = false;
		SCHAR scale = 0;
		auto exponentValue = 0;
		auto isOverExponent64b = false;
		auto isOverMantisa64b = false;
		auto isOverMantisa128b = false;
		auto signExponent = 0;

		Int128 mantisaValue;
		mantisaValue.set(0.0);

		const auto decimalConversion = 10;
		const CInt128 MAX_MANTISA_128(MAX_Int128 / decimalConversion);
		const CInt128 MAX_MANTISA_64(MAX_SINT64 / decimalConversion);

		enum
		{
			state_mantisa = 0,
			state_precision,
			state_exponent,
		} state = state_mantisa;

		for (--lex.ptr; lex.ptr < lex.end; lex.ptr++)
		{
			c = *lex.ptr;

			if (classes(c) & CHR_INTRODUCER)
			{
				if (isLastIntroducer)
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}

				const char lastSymbol = *(lex.ptr - 1);
				if ((lastSymbol == '.') || (UPPER(lastSymbol) == 'E') || (lastSymbol == '-') ||
					(lastSymbol == '+'))
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}

				isLastIntroducer = true;
				continue;
			}
			if (classes(c) & CHR_DIGIT)
			{
				pureString += static_cast<string::char_type>(c);
				auto ch = (c - '0');

				if (state == state_exponent)
				{
					if (signExponent == 0)
						signExponent = 1;

					exponentValue *= decimalConversion;

					if (signExponent == 1)
						exponentValue += ch;
					else
						exponentValue -= ch;

					if (!isOverExponent64b)
					{
						if (exponentValue > DBL_MAX_10_EXP || exponentValue < DBL_MIN_10_EXP)
							isOverExponent64b = true;
					}
					else if (exponentValue > DECQUAD_Emax || exponentValue < DECQUAD_Emin)
						exceptionNumericLiterals(Firebird::string("Overflow of the exponent"));
				}
				else
				{
					if (!isOverMantisa64b)
					{
						if (mantisaValue >= MAX_MANTISA_64 &&
							((mantisaValue > MAX_MANTISA_64) || (c >= '8')))
							isOverMantisa64b = true;
					}
					else if (!isOverMantisa128b)
					{
						if ((mantisaValue >= MAX_MANTISA_128) &&
							((mantisaValue > MAX_MANTISA_128) || (c >= '8')))
						{
							isOverMantisa128b = true;
							isOverExponent64b = true;
						}
					}

					if (!isOverMantisa64b || !isOverMantisa128b)
					{
						mantisaValue *= decimalConversion;
						mantisaValue += ch;
					}

					if (state == state_precision)
					{
						--scale;
						// protection against too low precision over 15 characters
						// next, we assume that the number is "Decimal 128-bit"
						if (-scale > DBL_DIG)
						{
							isOverMantisa128b = true;
							isOverExponent64b = true;
						}
					}
				}
			}
			else if (c == '.')
			{
				if (isLastIntroducer)
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}

				pureString += static_cast<string::char_type>(c);

				if (state == state_mantisa)
					state = state_precision;
				else
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}
			}
			else if (UPPER(c) == 'E')
			{
				if (isLastIntroducer)
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}

				pureString += static_cast<string::char_type>(c);

				if (state != state_exponent)
					state = state_exponent;
				else
				{
					exceptionNumericLiterals(
						Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
				}
			}
			else if ((classes(c) & CHR_IDENT) && (c != '{') && (c != '}'))
			{
				exceptionNumericLiterals(
					Firebird::string(lex.last_token, lex.ptr - lex.last_token + 1));
			}
			else // We have reached the separator
			{
				if ((c == '-') || (c == '+'))
				{
					if (state == state_exponent && signExponent == 0)
					{
						pureString += static_cast<string::char_type>(c);

						if (c == '-')
							signExponent = -1;
						else
							signExponent = 1;
						continue;
					}
				}
				break;
			}

			isLastIntroducer = false;
		}
		// We have reached the separator or the end of the line

		if (isLastIntroducer)
			exceptionNumericLiterals(Firebird::string(lex.last_token, lex.ptr - lex.last_token));

		if (state == state_exponent && signExponent == 0)
			exceptionNumericLiterals(Firebird::string(lex.last_token, lex.ptr - lex.last_token));

		if (state == state_precision && scale == 0)
			exceptionNumericLiterals(Firebird::string(lex.last_token, lex.ptr - lex.last_token));

		lex.last_token_bk = lex.last_token;
		lex.line_start_bk = lex.line_start;
		lex.lines_bk = lex.lines;

		// Any with an exponent "E" or a very big number
		if (state == state_exponent || isOverMantisa128b)
		{
			// Check for a more complex overflow case
			if ((!isOverExponent64b) && (signExponent == 1) && (exponentValue > (-scale)))
			{
				const auto degreeBase = 10.0;
				exponentValue += scale;
				double check_num = DBL_MAX / pow(degreeBase, exponentValue);
				if (mantisaValue.toDouble() > check_num)
					isOverExponent64b = true;
			}

			yylval.stringPtr = newString(pureString);
			// Long double or double
			return isOverExponent64b ? TOK_DECIMAL_NUMBER : TOK_FLOAT_NUMBER;
		}

		// 128-bit
		if (isOverMantisa64b)
		{
			yylval.lim64ptr = newLim64String(pureString, scale);
			return TOK_NUM128;
		}

		Int128 tmp;
		if (state != state_precision)
		{
			tmp.set(MAX_SLONG, 0);
			if (tmp >= mantisaValue)
			{
				// A natural 32 bit number
				yylval.int32Val = (SLONG)mantisaValue.toInteger(0);
				return TOK_NUMBER32BIT;
			}
		}
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
			ERRD_post_warning(Arg::Warning(isc_dsql_warning_number_ambiguous) << Arg::Str(
								  Firebird::string(lex.last_token, lex.ptr - lex.last_token)));
			ERRD_post_warning(Arg::Warning(isc_dsql_warning_number_ambiguous1));
		}

		if (client_dialect < SQL_DIALECT_V6_TRANSITION)
		{
			yylval.stringPtr = newString(pureString);
			return TOK_FLOAT_NUMBER;
		}

		yylval.scaledNumber.number = mantisaValue.toInt64(0);
		yylval.scaledNumber.scale = scale;
		yylval.scaledNumber.hex = false;
		return state == state_precision ? TOK_SCALEDINT : TOK_NUMBER64BIT;
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

		if (p > &string[MAX_SQL_IDENTIFIER_LEN] || p > &string[METADATA_IDENTIFIER_CHAR_LEN])
			yyabandon(yyposn, -104, isc_dyn_name_longer);

		const MetaName str(string, p - string);

		if (const auto keyVer = dbb->dbb_keywords().get(str);
			keyVer && (keyVer->keyword != TOK_COMMENT || lex.prev_keyword == -1))
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

	if (lex.last_token + 1 < lex.end && !fb_utils::isspace(lex.last_token[1]))
	{
		const MetaName str(lex.last_token, 2);

		if (const auto keyVer = dbb->dbb_keywords().get(str))
		{
			++lex.ptr;
			return keyVer->keyword;
		}
	}

	// Single character punctuation are simply passed on

	return (UCHAR) c;
}


void Parser::yyerror_detailed(const TEXT* /*error_string*/, int yychar, YYSTYPE&, YYPOSN& posn)
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
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Unexpected end of command
				  Arg::Gds(isc_command_end_err2) << Arg::Num(posn.firstLine) <<
													Arg::Num(posn.firstColumn));
	}
	else
	{
		ERRD_post (Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Token unknown - line %d, column %d
				  Arg::Gds(isc_dsql_token_unk_err) << Arg::Num(posn.firstLine) <<
				  									  Arg::Num(posn.firstColumn) <<
				  // Show the token
				  Arg::Gds(isc_random) << Arg::Str(string(posn.firstPos, posn.lastPos - posn.firstPos)));
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

void Parser::yyerrorIncompleteCmd(const YYPOSN& pos)
{
	ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			  // Unexpected end of command
			  Arg::Gds(isc_command_end_err2) << Arg::Num(pos.lastLine) <<
												Arg::Num(pos.lastColumn + 1));
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
