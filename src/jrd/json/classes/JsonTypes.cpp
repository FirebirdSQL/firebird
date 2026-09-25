/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonTypes.cpp
 *	DESCRIPTION:	Common types used in JSON code.
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2024 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "JsonTypes.h"

#include "../dsql/chars.h"
#include <cmath>
#include <cstring>

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
static inline constexpr USHORT DBL_MAX_10_EXP = 308
#endif


using namespace FBJSON;

static constexpr FB_UINT64 LIMIT_BY_10 = MAX_SINT64 / 10;
static constexpr USHORT UNICODE_HEX_SEQUENCE_LENGTH = 4; // XXXXX
static constexpr USHORT UNICODE_ESCAPED_SEQUENCE_LENGTH = 6; // \uXXXX - with the escape
static constexpr int LEAD_SURROGATE_NULL = -1;

static inline bool addToValue(SINT64& value, const char c, const bool hasMinus)
{
	if ((static_cast<FB_UINT64>(value) > LIMIT_BY_10) || (value == LIMIT_BY_10 && c > '7'))
	{
		if (!hasMinus || value != LIMIT_BY_10 || c != '8')
			return false;
	}

	value = value * 10 + (c - '0');
	return true;
}

// Exceptions

json_skippable_exception::json_skippable_exception(const ISC_STATUS *status_vector)  throw()
	: status_exception(status_vector)
{ }

json_skippable_exception::json_skippable_exception(const json_skippable_exception& rhs)
	: status_exception(rhs)
{ }

void json_skippable_exception::raise(const JsonStatusVector& statusVector)
{
	throw json_skippable_exception(statusVector.value());
}


json_fatal_exception::json_fatal_exception(const ISC_STATUS *status_vector) throw()
	: status_exception(status_vector)
{ }

json_fatal_exception::json_fatal_exception(const json_fatal_exception& rhs)
	: status_exception(rhs)
{ }


void json_fatal_exception::raise(const JsonStatusVector& statusVector)
{
	throw json_fatal_exception(statusVector.value());
}

// Classes

void BasicParse::parseStringUnsafe(const std::string_view input, SmallString& output)
{
	const ULONG length = input.length();

	output.clear();
	output.reserve(length);

	ULONG startPos = 0;
	while (true)
	{
		// Use memchr because it is usually implemented with vector instructions
		const char* slashIt = reinterpret_cast<const char*>(std::memchr(input.data() + startPos, '\\', length - startPos));
		if (slashIt == nullptr)
		{
			output += input.substr(startPos, length - startPos);
			return;
		}

		const auto currentPos = slashIt - input.data();
		output += input.substr(startPos, currentPos - startPos); // Get part before the escape char

		const char c = slashIt[1];
		if (c == 'u')
		{
			const auto unicodeSubstring = input.substr(currentPos); // \uXXXX
			const auto unicodeValue = parseUnicodeUnsafe(unicodeSubstring.substr(ESCAPE_LENGTH));
			appendUnicode(unicodeValue.value, unicodeSubstring.substr(0, unicodeValue.length + ESCAPE_LENGTH), output);

			startPos = currentPos + unicodeValue.length + ESCAPE_LENGTH;
		}
		else
		{
			output.append(1, '\\');
			output.append(1, c);
			startPos = currentPos + ESCAPE_LENGTH;
		}

	} // while
}

JsonStatusMsg& BasicParse::initError()
{
	error.reset(FB_NEW JsonStatusMsg());
	return *error.get();
}

template<class TView>
void FBJSON::JsonScalarParser<TView>::parseQuotedString(TView input, TextPos& current, bool keepEscapeSequence, bool expectEndQuote, SmallString* output)
{
	const bool hasOutput = output != nullptr;

	const TextPos end = input.end();
	constexpr static USHORT chunkSize = 256;

	// To check the most common case, using just a simple uint8_t mast is about 10% faster then using an enum
	static constexpr auto isSpecial = getSpecialCharacterTable();
	// Interestingly, accessing this std::array is about 20% faster than accessing a C-style ControlType[256] array!
	static constexpr auto controlsChars = getControlsTable();

	while (current < end)
	{
		std::string_view chunk = input.getStringView(current, chunkSize);
		USHORT startPos = 0;

		for (USHORT i = 0; i < chunk.length();)
		{
			UCHAR c = chunk[i++];

			// Check the most common case without switch!
			// 2x time better performance!
			if (FB_LIKELY(!isSpecial[c]))
				continue;

			// Switch is faster then if-else chain by 10%
			const auto charType = controlsChars[c];
			switch (charType)
			{
			case ControlType::QUOTE:
			{
				if (hasOutput)
					*output += chunk.substr(startPos, i - startPos - 1); // Exclude the last quote

				current += i;
				return;
			}
			case ControlType::INVALID:
			{
				Firebird::string hexStr;
				hexStr.printf("%02x", c);
				initError() << JsonStatusMsg(isc_jparser_unescaped_character) << hexStr;
				current += i - 1;
				return;
			}
			case ControlType::SLASH:
			{
				// Prefetch chunk for possible unicode
				const USHORT lengthWithPrefetchedSize = i + UNICODE_UTF16_SEQUENCE_LENGTH;
				if (lengthWithPrefetchedSize >= chunk.length()  && current + lengthWithPrefetchedSize < end)
				{
					// Part before the escape sequence
					const auto appendSize = i - (startPos + 1);

					if (hasOutput)
						*output += chunk.substr(startPos, appendSize);

					current += appendSize;
					chunk = input.getStringView(current, chunkSize);
					i = 1; // symbol after slash
					fb_assert(chunk[0] == '\\');

					if (current >= end)
					{
						initError() << JsonStatusMsg(isc_jparser_invalid_quoted_string);
						return;
					}
				}
				else if (hasOutput)
				{
					*output += chunk.substr(startPos, i - (startPos + 1));
				}

				c = chunk[i++];

				if (c == 'u')
				{
					// Unicode escape sequence \uXXXX

					const auto unicode = parseUnicode(chunk.substr(i));
					if (hasOutput)
						appendUnicode(unicode.value, chunk.substr(i - 2, unicode.length + 2), *output);

					i += unicode.length;
				}
				else if (hasOutput)
				{
					if (keepEscapeSequence)
					{
						output->append(1, '\\');
						output->append(1, c);
					}
					else
						output->append(1, resolveEscapedChar(c));
				}

				startPos = i;
				break;
			} //case
			} //switch
		} // for

		const auto end = chunk.length();
		if (hasOutput)
			*output += chunk.substr(startPos, end - startPos); // Exclude the last quote

		current += end;
	} // while

	if (expectEndQuote)
	{
		initError() << JsonStatusMsg(isc_jparser_invalid_quoted_string);
	}
}


template<class TView>
ParsedNumber FBJSON::JsonScalarParser<TView>::parseNumber(TView input, TextPos& current, bool strictMode)
{
	const TextPos start = current;
	const TextPos end = input.end();

	ParsedNumber lex;
	lex.value = 0;

	if (strictMode)
	{
		if (current == end)
		{
			initError() << JsonStatusMsg(isc_jparser_empty_input);
			return lex;
		}
	}

	// Skip spaces, catch first sign
	bool hasUnaryMinus = false;
	for (; current < end; ++current)
	{
		const char c = input[current];
		switch (c)
		{
		case ' ':
			continue;
		case '-':
			++current;
			hasUnaryMinus = true;
			break;
		case '+':
		default:
			break;
		}

		break;
	}

	// Delay overflow error print to lex the full number range and put it into error
	bool hasOverflow = false;

	// First part
	for (; current < end; ++current)
	{
		const char c = input[current];

		if (!(classes(c) & CHR_DIGIT))
			break;

		if (!addToValue(lex.value, c, hasUnaryMinus))
		{
			hasOverflow = true;
		}
	}

	// Decimal part
	if (current < end && input[current] == '.')
	{
		++current;
		lex.isDouble = true;
		for (; current < end; ++current)
		{
			const char c = input[current];
			if (!(classes(c) & CHR_DIGIT))
				break;

			if (!addToValue(lex.value, c, hasUnaryMinus))
			{
				hasOverflow = true;
			}

			--lex.scale;
		}
	}

	if (lex.scale < MIN_SCHAR || lex.scale > MAX_SCHAR)
	{
		hasOverflow = true;
	}

	// Exponent sign
	SLONG expValue = 0;
	if (current < end && (input[current] == 'e' || input[current] == 'E'))
	{
		lex.isDouble = true;

		if (++current < end)
		{
			const char next = input[current];

			bool haveExponentSign = false;
			if (next == '+')
			{
				++current;
				haveExponentSign = false;
			}
			else if (next == '-')
			{
				++current;
				haveExponentSign = true;
			}

			for (; current < end; ++current)
			{
				const char c = input[current];
				if (!(classes(c) & CHR_DIGIT))
					break;

				expValue = expValue * 10 + (c - '0');
			}

			if (expValue > DBL_MAX_10_EXP)
			{
				hasOverflow = true;
			}

			if (haveExponentSign)
			{
				expValue = -expValue;
			}
		}

		const double maxNum = DBL_MAX / static_cast<double>(std::pow(10, expValue));
		if (double(lex.value) > maxNum)
		{
			hasOverflow = true;
		}

		lex.scale += static_cast<SSHORT>(expValue);
	}

	if (hasOverflow)
	{
		initError() << JsonStatusMsg(isc_jparser_number_overflow) <<
				JsonStatusMsgStrArg(input.getErrorSubstring(start, end - start));

		return lex;
	}

	if (hasUnaryMinus)
		lex.value = -lex.value;

	if (strictMode)
	{
		for (; current < end; ++current)
		{
			const char c = input[current];

			if (!(classes(c) & CHR_WHITE))
			{
				initError() << JsonStatusMsg(isc_jparser_invalid_number_parse) <<
					JsonStatusMsgStrArg(input.getErrorSubstring(start));
				break;
			}
		}
	}

	return lex;
}


template class FBJSON::JsonScalarParser<StringParseView>;
// template class FBJSON::JsonScalarParser<InputJsonText>;
