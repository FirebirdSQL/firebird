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


template<class TView>
class JsonScalarParser
{
public:
	// JsonStatusMsg allocation is super expansive
	Firebird::AutoPtr<JsonStatusMsg> error;

	bool hasError() const
	{
		return error && error->isDirty();
	}

	void parseQuotedString(TView& view, TextPos& current, SmallString& escapelessOutput);

	// strictMode for unary plus/minus and parse errors
	ParsedNumber parseNumber(TView& input, TextPos& current, const bool strictMode);

private:
	JsonStatusMsg& initError();
};


struct StringParseView
{
	std::string_view base;
	char operator[](TextPos i)
	{
		return base[i];
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
