/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonTypes.h
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

#ifndef JSON_TYPES_H
#define JSON_TYPES_H

#include "firebird.h"
#include "fb_exception.h"
#include "../JsonConsts.h"
#include "../common/utils_proto.h"

#include "unicode/utf16.h" // U16_IS_TRAIL, U16_GET_SUPPLEMENTARY

#include <math.h>
#include <string_view>
#include <array>


namespace FBJSON {

using ArraySize = ULONG;
using RangeSize = SSHORT;

// JsonPath Extention: Negative values supported
using PathArrayIndex = SLONG;

// Enums

// A sub-enum for JsonType and JsonbType enums
// The order represents compare cardinality:
// null < SQL/JSON scalar < array < object
enum CommonJsonType : UCHAR
{
	JT_EMPTY	= 0, // Special type
	JT_NULL 	= 1,
	JT_SCALAR	= 2,
	JT_ARRAY	= 3,
	JT_OBJECT	= 4
};

enum class JsonType : UCHAR
{
	Empty = JT_EMPTY, // Special type
	Null = JT_NULL,
	Scalar = JT_SCALAR,
	Array = JT_ARRAY,
	Object = JT_OBJECT
};

enum class JsonbType : UCHAR
{
	Empty  = JT_EMPTY,
	Scalar = JT_SCALAR, // Scalar or null
	Array  = JT_ARRAY,
	Object = JT_OBJECT,
	DuplicateKey
};

enum class PathMethod
{
	NONE = 0,
	TYPE = 1,
	SIZE = 2,
	DOUBLE = 3,
	CEILING = 4,
	FLOOR = 5,
	ABS = 6,
	DATETIME = 7,
	KEYVALUE = 8,

	// Pass unary operations as methods in dynamic path to simplify things
	// In a math expr and filter, normal dynamic operators are used
	METHOD_UNARY_PLUS,
	METHOD_UNARY_MINUS,

	// SQL 2023 methods

	STRING,
	BOOLEAN,
	BIGINT,
	DECIMAL,
	INTEGER,
	NUMBER,

	DATE,
	TIME,
	TIME_TZ,
	TIMESTAMP,
	TIMESTAMP_TZ
};

enum class PathType
{
	MAIN = 0,		//$
	MAIN_IN_FILTER,	//$
	ITEM,			//@
	PASSING			//$<name>
};

// Element of JSON Path
enum class ItemType : UCHAR
{
	FIELD,			// $.field
	ARRAY_ELEMENT	// $[0]
};

// Helpers

constexpr void rtrim(std::string_view& view)
{
	size_t stringLength = view.length();
	while (stringLength > 0 && view[stringLength - 1] == ' ')
	{
		--stringLength;
	}
	view.remove_suffix(view.length() - stringLength);
}

// Common types

class json_skippable_exception : public Firebird::status_exception
{
public:
	explicit json_skippable_exception(const ISC_STATUS *status_vector) throw();

	json_skippable_exception(const json_skippable_exception& rhs);
	json_skippable_exception(json_skippable_exception&&) noexcept = delete;

	json_skippable_exception& operator=(const json_skippable_exception&) = delete;
	json_skippable_exception& operator=(json_skippable_exception&&) noexcept = delete;

	virtual ~json_skippable_exception() = default;

	[[noreturn]] static void raise(const JsonStatusVector& statusVector);
};

class json_fatal_exception : public Firebird::status_exception
{
public:
	explicit json_fatal_exception(const ISC_STATUS *status_vector) throw();

	json_fatal_exception(const json_fatal_exception& rhs);
	json_fatal_exception(json_fatal_exception&&) noexcept = delete;

	json_fatal_exception& operator=(const json_fatal_exception&) = delete;
	json_fatal_exception& operator=(json_fatal_exception&&) noexcept = delete;

	virtual ~json_fatal_exception() = default;

	[[noreturn]] static void raise(const JsonStatusVector& statusVector);
};

using json_syntax_exception = json_fatal_exception;
using json_strict_exception = json_fatal_exception;

struct JsonLevelNode
{
	JsonLevelNode* next = nullptr;
	JsonLevelNode* prev = nullptr;

	SmallString field;
	ArraySize indexInArray = 0;
	ArraySize arraySize = 0;
	SSHORT depth = 0;
	CommonJsonType type = JT_EMPTY;
	ItemType itemType = ItemType::FIELD;

	JsonLevelNode(MemoryPool& pool) :
		field(pool)
	{ }

	JsonLevelNode(const JsonLevelNode&) = delete;
	JsonLevelNode(JsonLevelNode&&) noexcept = delete;

	JsonLevelNode& operator=(const JsonLevelNode&) = delete;
	JsonLevelNode& operator=(JsonLevelNode&&) noexcept = delete;

	~JsonLevelNode()
	{
		JsonLevelNode* current = next;
		JsonLevelNode* curNext;
		while (current)
		{
			curNext = current->next;
			current->next = nullptr;
			delete current;
			current = curNext;
		}
	}

	inline ArraySize getArraySize() const noexcept
	{
		// The current level is an element; jsonNode->prev is an array level
		if (prev && prev->isArray())
			return prev->arraySize;
		else
			return 1;
	}

	inline bool isArray() const noexcept
	{
		return type == JT_ARRAY;
	}
};

struct ParsedNumber
{
	SINT64 value = 0;
	SLONG scale = 0;
	bool isDouble = false;

	inline double getDouble() const
	{
		static constexpr double scaleMove = 10.0;

		double result = static_cast<double>(value);
		if (scale > 0)
		{
			for (USHORT i = 0; i < scale; ++i)
				result *= scaleMove;
		}
		else
		{
			for (USHORT i = 0; i < abs(scale); ++i)
				result /= scaleMove;
		}

		return result;
	}
};

struct ParsedUnicode
{
	using ValueType = ULONG;
	ValueType value = 0;
	UCHAR length = 0; // in bytes
};

class BasicParse
{
public:
	static inline constexpr USHORT ESCAPE_LENGTH = 2; // \X
	static inline constexpr USHORT UNICODE_HEX_SEQUENCE_LENGTH = 4; // XXXXX
	static inline constexpr USHORT UNICODE_UTF16_SEQUENCE_LENGTH = UNICODE_HEX_SEQUENCE_LENGTH * 2 + 2; // XXXX\uXXXX
	static inline constexpr USHORT UNICODE_ESCAPED_SEQUENCE_LENGTH = 6; // \uXXXX - with the escape
	static inline constexpr UCHAR INVALID_HEX = 255;
	static inline constexpr UCHAR INVALID_ESCAPE = 0;

	// Keep the size of the enum at 4 bytes, as working with a 4-byte is faster than with a 1-byte
	enum class ControlType
	{
		CHARACTER = 0,
		INVALID,
		QUOTE,
		SLASH
	};

	// A table to get the type of the symbol in a fast way
	static constexpr auto getControlsTable()
	{
		std::array<ControlType, UCHAR_MAX + 1> controls{};

		// 0-32 are non char symbols
		for (UCHAR i = 0; i < 32; ++i)
			controls[i] = ControlType::INVALID;

		controls['\"'] = ControlType::QUOTE;
		controls['\\'] = ControlType::SLASH;
		controls[127]  = ControlType::INVALID; // del

		return controls;
	}

	// A table to check the simple character in a fast way: 0 is a simple char, 1 is a special
	static constexpr auto getSpecialCharacterTable()
	{
		// Accessing a 4-byte value is faster than with a 1-byte step
		std::array<uint32_t, UCHAR_MAX + 1> controls{};

		// 0-32 are non char symbols
		for (UCHAR i = 0; i < 32; ++i)
			controls[i] = 1;

		controls['\"'] = 1;
		controls['\\'] = 1;
		controls[127] = 1; // del

		return controls;
	}

	static constexpr auto getHexToDecTable()
	{
		// Cant use std::optional in constexpr
		std::array<UCHAR, UCHAR_MAX> hexToDec{};
		for (USHORT c = 0; c < hexToDec.size(); ++c)
			hexToDec[c] = INVALID_HEX; // Cannot use fill in constexpr

		for (UCHAR c = '0'; c <= '9'; ++c)
			hexToDec[c] = c - '0';

		for (UCHAR c = 'A'; c <= 'F'; ++c)
			hexToDec[c] = 10 + c - 'A';

		for (UCHAR c = 'a'; c <= 'f'; ++c)
			hexToDec[c] = 10 + c - 'a';

		return hexToDec;
	}

	static constexpr auto getEscapeResolverTable()
	{
		// Cant use std::optional in constexpr
		std::array<char, UCHAR_MAX> resolveTable{};
		resolveTable['\\'] = '\\';
		resolveTable['"'] = '"';
		resolveTable['/'] = '/';
		resolveTable['b'] = '\b';
		resolveTable['f'] = '\f';
		resolveTable['n'] = '\n';
		resolveTable['r'] = '\r';
		resolveTable['t'] = '\t';
		return resolveTable;
	}

	static constexpr auto getEscapeTable()
	{
		// Cant use std::optional in constexpr
		std::array<std::string_view, UCHAR_MAX + 1> resolveTable{};
		resolveTable['\\'] = "\\\\";
		resolveTable['"'] = "\\\"";
		resolveTable['\b'] = "\\b";
		resolveTable['\f'] = "\\f";
		resolveTable['\n'] = "\\n";
		resolveTable['\r'] = "\\r";
		resolveTable['\t'] = "\\t";
		return resolveTable;
	}

	// Instead of std::expected
	Firebird::AutoPtr<JsonStatusMsg> error;
	bool hasError() const
	{
		return error && error->isDirty();
	}

	inline ParsedUnicode::ValueType parseHex(const std::string_view sequence)
	{
		if (sequence.length() < UNICODE_HEX_SEQUENCE_LENGTH)
		{
			initError() << JsonStatusMsg(isc_jparser_un_incorrect_code) << sequence;
			return 0;
		}

		constexpr static auto hetToDecTable = getHexToDecTable();

		ParsedUnicode::ValueType hexValue = 0;
		char c;
		for (USHORT i = 0; i < UNICODE_HEX_SEQUENCE_LENGTH; ++i)
		{
			c = sequence[i];
			const auto dec = hetToDecTable[c];
			if (dec != INVALID_HEX)
			{
				hexValue = (hexValue << 4) + dec;
			}
			else
			{
				initError() << JsonStatusMsg(isc_jparser_un_incorrect_code) << sequence;
				return 0;
			}
		}

		return hexValue;
	}

	ParsedUnicode parseUnicode(const std::string_view sequence)
	{
		// Possible values
		// UTF-8, \uXXXX
		// UTF-16, \uXXXX\uXXXX

		// Get a digital code of the handling hex sequence to store it to hexToUnicodeValue
		auto hexToUnicodeValue = parseHex(sequence);
		if (hasError())
			return {};

		UCHAR bytesLength = UNICODE_HEX_SEQUENCE_LENGTH;

		// UTF-16, the first (lead) surrogate is a 16-bit code value in the range U+D800 to U+DBFF
		if (U16_IS_LEAD(hexToUnicodeValue)) // or U_IS_LEAD
		{
			const auto leadSurrogate = hexToUnicodeValue;

			if (sequence.length() < UNICODE_UTF16_SEQUENCE_LENGTH ||
				sequence[UNICODE_HEX_SEQUENCE_LENGTH] != '\\' ||
				sequence[UNICODE_HEX_SEQUENCE_LENGTH + 1] != 'u')
			{
				initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) << sequence;
				return {};
			}

			// The second (tail) surrogate is a 16-bit code value in the range U+DC00 to U+DFFF
			const auto tailSurrogate = parseHex(sequence.substr(UNICODE_HEX_SEQUENCE_LENGTH + 2)); // skip "\u"
			if (hasError())
				return {};

			if (!U16_IS_TRAIL(tailSurrogate))
			{
				initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) << sequence;
				return {};
			}

			hexToUnicodeValue = U16_GET_SUPPLEMENTARY(leadSurrogate, tailSurrogate);
			bytesLength = UNICODE_UTF16_SEQUENCE_LENGTH;
		}
		else if (U16_IS_TRAIL(hexToUnicodeValue))
		{
			initError() << JsonStatusMsg(isc_jparser_un_missing_high_surrogate) << sequence;
			return {};
		}

		if (hexToUnicodeValue == 0)
		{
			initError() << JsonStatusMsg(isc_jparser_un_conv_error);
			return {};
		}

		return {hexToUnicodeValue, bytesLength};
	}

	inline char resolveEscapedChar(const char c)
	{
		static constexpr auto resolver = getEscapeResolverTable();

		const auto escaped = resolver[c];
		if (FB_UNLIKELY(escaped == INVALID_ESCAPE))
			initError() << JsonStatusMsg(isc_jparser_invalid_sequence) << JsonStatusMsgStrArg(std::string_view(&c, 1));

		return escaped;
	}

	// Static

	static inline ParsedUnicode::ValueType parseHexUnsafe(const std::string_view sequence)
	{
		constexpr static auto hetToDecTable = getHexToDecTable();

		ParsedUnicode::ValueType hexValue = 0;
		for (USHORT i = 0; i < UNICODE_HEX_SEQUENCE_LENGTH; ++i)
			hexValue = (hexValue << 4) + hetToDecTable[sequence[i]];

		return hexValue;
	}

	static ParsedUnicode parseUnicodeUnsafe(const std::string_view sequence)
	{
		// Possible values
		// UTF-8, \uXXXX
		// UTF-16, \uXXXX\uXXXX

		// Get a digital code of the handling hex sequence to store it to hexToUnicodeValue
		auto hexToUnicodeValue = parseHexUnsafe(sequence);
		UCHAR bytesLength = UNICODE_HEX_SEQUENCE_LENGTH;

		// UTF-16, the first (lead) surrogate is a 16-bit code value in the range U+D800 to U+DBFF
		if (U16_IS_LEAD(hexToUnicodeValue)) // or U_IS_LEAD
		{
			const auto leadSurrogate = hexToUnicodeValue;

			// The second (tail) surrogate is a 16-bit code value in the range U+DC00 to U+DFFF
			const auto tailSurrogate = parseHexUnsafe(sequence.substr(UNICODE_HEX_SEQUENCE_LENGTH));

			hexToUnicodeValue = U16_GET_SUPPLEMENTARY(leadSurrogate, tailSurrogate);
			bytesLength = UNICODE_HEX_SEQUENCE_LENGTH * 2;
		}

		return {hexToUnicodeValue, bytesLength};
	}

	// Put unicode as char if possible, or append unicodeStr (\uXXXX or \uXXXX\uXXXX)
	inline static void appendUnicode(const ParsedUnicode::ValueType unicodeValue, const std::string_view unicodeStr, SmallString& outStr)
	{
		// C0 Controls and Basic Latin Range: 0000–007F
		if (unicodeValue <= 0X007F)
		{
			outStr.append(1, char(unicodeValue));
		}
		else
		{
			outStr.append(unicodeStr.data(), unicodeStr.length());
		}
	}

	static void parseStringUnsafe(const std::string_view input, SmallString& outStr);

protected:
	JsonStatusMsg& initError();
};

template<class TView>
class JsonScalarParser : public BasicParse
{
public:
	void parseQuotedString(TView view, TextPos& current, bool keepEscapeSequence, bool expectEndQuote, SmallString* output);

	// strictMode for unary plus/minus and parse errors
	ParsedNumber parseNumber(TView input, TextPos& current, bool strictMode);
};


struct StringParseView
{
	std::string_view base;
	char operator[](TextPos i)
	{
		return base[i];
	}

	std::string_view getStringView(const TextPos subStart, TextPos subLength) const
	{
		if (subStart + subLength > base.length())
		{
			subLength = base.length() - subStart;
		}

		return base.substr(subStart, subLength);
	}

	SmallString getErrorSubstring(TextPos subStart, TextPos subLength = JSON_MAX_REPORT_SIZE) const
	{
		if (subStart + subLength > base.length())
		{
			subLength = base.length() - subStart;
		}

		bool overload = false;
		if (subLength > JSON_MAX_REPORT_SIZE)
		{
			overload = true;
			subLength = JSON_MAX_REPORT_SIZE;
		}

		SmallString buffer(base.data() + subStart, subLength);
		if (overload)
		{
			buffer += "...";
			return buffer;
		}
		else
			return buffer;
	}

	inline TextPos end() const
	{
		return static_cast<TextPos>(base.length());
	}
};

using StringParser = JsonScalarParser<StringParseView>;
// using InputJsonParser = JsonScalarParser<InputJsonText&>;

}
#endif
