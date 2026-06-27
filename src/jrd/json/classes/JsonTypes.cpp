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

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
static inline constexpr DBL_MAX_10_EXP = 308
#endif

#include "unicode/utf16.h" // U16_IS_TRAIL

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

template<class TView>
void FBJSON::JsonScalarParser<TView>::parseQuotedString(TView& input, TextPos& current, SmallString& escapelessOutput)
{
	escapelessOutput.clear();
	escapelessOutput.reserve(BUFFER_SMALL);

	const TextPos end = input.end();

	// UTF-16, two \uXXXX in a row
	int	leadSurrogate = LEAD_SURROGATE_NULL;

	while (current < end)
	{
		char c = input[current++];

		if (c == '"')
		{
			return;
		}
		else if ((unsigned char)c < 32)
		{
			escapelessOutput.printf("%02x", (unsigned char)c);
			initError() << JsonStatusMsg(isc_jparser_unescaped_character) << escapelessOutput;

			escapelessOutput.clear();
			return;
		}
		else if (c == '\\')
		{
			if (current == end)
			{
				initError() << JsonStatusMsg(isc_jparser_invalid_quoted_string);

				escapelessOutput.clear();
				return;
			}

			c = input[current++];

			if (c == 'u')
			{
				// Unicode escape sequence \uXXXX

				if (current + UNICODE_HEX_SEQUENCE_LENGTH >= end)
				{
					initError() << JsonStatusMsg(isc_jparser_invalid_sequence) << input.getErrorSubstring(current, end - current);
					current = end;

					escapelessOutput.clear();
					return;
				}

				// Get a digital code of the handling hex sequence to store it to hexToUnicodeValue
				int hexToUnicodeValue = 0;
				for (USHORT i = 0; i < UNICODE_HEX_SEQUENCE_LENGTH; ++i)
				{
					c = input[current++];
					const auto cherMeta = classes_array[static_cast<USHORT>(c)];
					if (cherMeta & CHR_DIGIT)
					{
						hexToUnicodeValue = (hexToUnicodeValue << 4) + (c - '0');
					}
					else
					{
						c = UPPER(c);
						if (cherMeta & CHR_HEX)
						{
							hexToUnicodeValue = (hexToUnicodeValue << 4) + (c - 'A') + 10;
						}
						else
						{
							initError() << JsonStatusMsg(isc_jparser_un_incorrect_code) <<
								input.getErrorSubstring(current - i - 1, UNICODE_HEX_SEQUENCE_LENGTH);
							return;
						}
					}
				}

				// The first (lead) surrogate is a 16-bit code value in the range U+D800 to U+DBFF
				if (U16_IS_LEAD(hexToUnicodeValue)) // or U_IS_LEAD
				{
					if (leadSurrogate != LEAD_SURROGATE_NULL)
					{
						// Two lead codes in a row
						initError() << JsonStatusMsg(isc_jparser_un_double_high_surrogate) <<
							input.getErrorSubstring(current - UNICODE_HEX_SEQUENCE_LENGTH, UNICODE_HEX_SEQUENCE_LENGTH);
						return;
					}
					leadSurrogate = hexToUnicodeValue;
					continue;
				}
				// The second (tail) surrogate is a 16-bit code value in the range U+DC00 to U+DFFF
				else if (U16_IS_TRAIL(hexToUnicodeValue)) // or U_IS_TRAIL
				{
					if (leadSurrogate == LEAD_SURROGATE_NULL)
					{
						initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) <<
							input.getErrorSubstring(current - UNICODE_HEX_SEQUENCE_LENGTH, UNICODE_HEX_SEQUENCE_LENGTH);
						return;
					}
					hexToUnicodeValue = U16_GET_SUPPLEMENTARY(leadSurrogate, hexToUnicodeValue);
				}
				else if (leadSurrogate != LEAD_SURROGATE_NULL)
				{
					initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) <<
							input.getErrorSubstring(current - UNICODE_HEX_SEQUENCE_LENGTH, UNICODE_HEX_SEQUENCE_LENGTH);
					return;
				}

				if (hexToUnicodeValue == 0)
				{
					initError() << JsonStatusMsg(isc_jparser_un_conv_error);
					return;
				}
				// C0 Controls and Basic Latin Range: 0000–007F (127)
				else if (hexToUnicodeValue <= CHAR_MAX)
				{
					escapelessOutput.append(1, (char)hexToUnicodeValue);
				}
				else
				{
					// Append the raw code

					// We are standing to the last hex digit so start and end with offset of 1
					const TextPos unicodeEndPos = current;

					// We need to get the full code sequence including the escape
					if (leadSurrogate != LEAD_SURROGATE_NULL)
					{
						for (TextPos i = unicodeEndPos - (2 * UNICODE_ESCAPED_SEQUENCE_LENGTH); i < unicodeEndPos; ++i)
						{
							escapelessOutput.append(1, input[i]);
						}

						// Two codes, 12 chars - \uXXXX\uXXXX
						leadSurrogate = LEAD_SURROGATE_NULL;
					}
					else // 6 chars - \uXXXX
					{
						for (TextPos i = unicodeEndPos - UNICODE_ESCAPED_SEQUENCE_LENGTH; i < unicodeEndPos; ++i)
						{
							escapelessOutput.append(1, input[i]);
						}
					}
				}

				continue; // End of unicode sequence handling
			}

			switch (c)
			{
				case '\'':
				case '"':
				case '\\':
				case '/':
					escapelessOutput.append(1, c);
					break;
				case 'b':
					escapelessOutput.append(1, '\b');
					break;
				case 'f':
					escapelessOutput.append(1, '\f');
					break;
				case 'n':
					escapelessOutput.append(1, '\n');
					break;
				case 'r':
					escapelessOutput.append(1, '\r');
					break;
				case 't':
					escapelessOutput.append(1, '\t');
					break;
				default:
					escapelessOutput = c;
					initError() << JsonStatusMsg(isc_jparser_invalid_sequence) << JsonStatusMsgStrArg(escapelessOutput);
					return;
			} // !switch
		}
		else
		{
			// Handle a simple character
			escapelessOutput.append(1, c);
		}

		// The unicode tail is missing
		// Handle a non-unicode escape character
		if (leadSurrogate != LEAD_SURROGATE_NULL)
		{
			initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) <<
				input.getErrorSubstring(current - UNICODE_HEX_SEQUENCE_LENGTH, UNICODE_HEX_SEQUENCE_LENGTH);
			return;
		}

	} // for

	if (current >= end)
	{
		initError() << JsonStatusMsg(isc_jparser_invalid_quoted_string);
	}
}


template<class TView>
ParsedNumber FBJSON::JsonScalarParser<TView>::parseNumber(TView& input, TextPos& current, bool strictMode)
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

template<class T>
JsonStatusMsg& JsonScalarParser<T>::initError()
{
	error.reset(FB_NEW JsonStatusMsg());
	return *error.get();
}

template class FBJSON::JsonScalarParser<StringParseView>;
// template class FBJSON::JsonScalarParser<InputJsonText&>;
