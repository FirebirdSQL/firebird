/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonScalar.h
 *	DESCRIPTION:	A scalar variant type used in JSON code.
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
 *  Copyright (c) 2026 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JSON_SCALAR_H
#define JSON_SCALAR_H

#include "../JsonConsts.h"

#include "../jrd/val.h"

#include <utility>

namespace Firebird {
	// Use this for serialization because they have helpful put/get fuctions
	class BlrReader;
	class BlrWriter;
}

namespace FBJSON {

class JsonDatetime;
class OutputJsonText;
class Jsonb;
class JsonbView;

// All possible types during runtime calculation
enum class ValueType : UCHAR
{
	EMPTY = 0,
	ERROR_VALUE,
	UNKNOWN,

	// * JsonScalar scope begin
	JNULL,
	BOOL,
	INT,
	//JFLOAT,
	DOUBLE,
	STRING,
	DATETIME, // keep it as the last scalar type
	// * JsonScalar scope end

	JSON_TEXT,
	JSON_BINARY,
	JSON_BINARY_VIEW
};
constexpr std::pair<ValueType, ValueType> SCALAR_WITH_NULL_TYPES_RANGE = {ValueType::JNULL, ValueType::DATETIME};
constexpr std::pair<ValueType, ValueType> SCALAR_TYPES_RANGE = {ValueType::BOOL, ValueType::DATETIME};

// All possible values to store during runtime calculation
union DataVariant
{
	// Scalars
	bool boolean;
	SINT64 integer;
	double doubleValue;
	SmallString* string;
	JsonDatetime* datetime;

	// Runtime values, not used in JsonScalar
	//OutputJsonText* json;	// temp comment
	//Jsonb* jsonb;			// temp comment
	//JsonbView* jsonView;	// temp comment
};
static_assert(sizeof(DataVariant) <= 8, "Big types should be stored as pointers to save memory");

// A storage to keep parsed JSON scalar and NULL value
// The scalar has the ability to be stored in a byted representation
class JsonScalar : public Firebird::PermanentStorage
{
public:

	enum Flags : UCHAR
	{
		FLAG_NONE = 0,
		FLAG_HAS_QUOTES = 1
	};

	class StringView;

public:
	JsonScalar(MemoryPool& pool) : Firebird::PermanentStorage(pool)
	{ }

	JsonScalar(JsonScalar&& rhs) : Firebird::PermanentStorage(rhs.getPool())
	{
		operator=(std::forward<JsonScalar>(rhs));
	}

	JsonScalar(const JsonScalar& rhs) : Firebird::PermanentStorage(rhs.getPool())
	{
		operator=(rhs);
	}

	JsonScalar(MemoryPool& pool, const JsonScalar& rhs) : Firebird::PermanentStorage(pool)
	{
		operator=(rhs);
	}

	JsonScalar& operator=(const JsonScalar& rhs);

	JsonScalar& operator=(JsonScalar&& rhs)
	{
		type = rhs.type;
		flags = rhs.flags;
		memcpy(&value, &rhs.value, sizeof(DataVariant));
		rhs.value = {};

		rhs.type = ValueType::JNULL;
		return *this;
	}

	virtual ~JsonScalar()
	{
		reset(ValueType::JNULL);
	}

public: // Getters
	const DataVariant& getValue() const noexcept
	{
		return value;
	};

	DataVariant& getValue() noexcept
	{
		return value;
	};


	constexpr ValueType getType() const noexcept
	{
		return type;
	}

	double getDouble() const;

	inline std::string_view getStringView() const
	{
		SmallString* asString = str();
		return std::string_view(asString->data(), asString->length());
	}

	SmallString* str() const;

public: // Checkers
	constexpr bool hasFlag(const Flags flag) const noexcept
	{
		return flags & flag;
	}

	constexpr bool isScalar() const noexcept
	{
		return SCALAR_TYPES_RANGE.first <= type && type <= SCALAR_TYPES_RANGE.second;
	}

	constexpr bool isScalarOrNull() const noexcept
	{
		return SCALAR_WITH_NULL_TYPES_RANGE.first <= type && type <= SCALAR_WITH_NULL_TYPES_RANGE.second;
	}

	inline bool isNumber() const noexcept
	{
		return type == ValueType::INT || type == ValueType::DOUBLE;
	}

	constexpr bool isString() const noexcept
	{
		return type == ValueType::STRING;
	}

	constexpr bool isNull() const noexcept
	{
		return type == ValueType::JNULL;
	}

	constexpr bool isTrue() const noexcept
	{
		return type == ValueType::BOOL && value.boolean;
	}

public: // Setters
	constexpr void addFlag(const Flags flag) noexcept
	{
		flags |= flag;
	}

	inline void setToNull()
	{
		reset(ValueType::JNULL);
	}

	inline void set(const bool boolValue)
	{
		reset(ValueType::BOOL);
		value.boolean = boolValue;
	}

	inline void set(const int intValue)
	{
		reset(ValueType::INT);
		value.integer = intValue;
	}

	inline void set(const SINT64 bigIntValue)
	{
		reset(ValueType::INT);
		value.integer = bigIntValue;
	}

	inline void set(const double doublevalue)
	{
		reset(ValueType::DOUBLE);
		value.doubleValue = doublevalue;
	}

	//! Add explicit overload for c-string to avoid casting const char* to bool
	void set(const char* const view)
	{
		set(std::string_view(view));
	}

	void set(const std::string_view view);

	//! Remove random pointer to bool casts
	template<typename T>
	void set(T*) = delete;

	JsonDatetime& setToDatetime();

public: // Serialization
	void writeScalarAsBytes(Firebird::BlrWriter& array) const;
	void readScalarFromBytes(Firebird::BlrReader& array);

	Jrd::impure_value makeScalarDsc() const;

protected:
	void reset(const ValueType newType) noexcept;

	// Used in JsonVariant to release json/jsonb pointers
	virtual void releaseExtend()
	{ }

	// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
protected:
	DataVariant value = {};
	ValueType type = ValueType::JNULL;
	UCHAR flags = FLAG_NONE; // Only runtime
	// NOLINTEND(misc-non-private-member-variables-in-classes)
};

class JsonScalar::StringView
{
public:
	StringView(MemoryPool& pool, SmallString* stringToSet) :
		m_scalar(pool)
	{
		m_scalar.value.string = stringToSet;
		m_scalar.type = ValueType::STRING;
	}

	StringView(const StringView& ref) = delete;
	StringView(StringView&& ref) = delete;

	auto operator=(const StringView&) = delete;
	auto operator=(StringView&&) = delete;

	~StringView()
	{
		m_scalar.type = ValueType::EMPTY;
	}

	const JsonScalar& operator*() noexcept
	{
		return m_scalar;
	}

private:
	JsonScalar m_scalar;
};

}
#endif
