/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonDatetime.h
 *	DESCRIPTION:	Common types for JSON code.
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

#ifndef JSON_DATETIME_H
#define JSON_DATETIME_H

#include "firebird.h"
#include "fb_exception.h"
#include "../../common/cvt.h"

#include "../JsonConsts.h"

#include <utility>

namespace Firebird {
	// Use this for serialization because they have helpful put/get fuctions
	class BlrReader;
	class BlrWriter;
}

namespace Jrd {
	class thread_db;
}

namespace FBJSON {

class JsonDatetime
{
public:
	using Flags = Firebird::CvtStringContains::TypeFlags;

	JsonDatetime(Firebird::MemoryPool& pool) : m_stringRepresentation(pool)
	{ }

	JsonDatetime(Firebird::MemoryPool& pool, const JsonDatetime& rhs) : m_stringRepresentation(pool)
	{
		m_stringRepresentation = rhs.m_stringRepresentation;
		m_ts = rhs.m_ts;
		m_flags = rhs.m_flags;
	}

	void set(const std::string_view source, std::string_view format = "", Firebird::Callbacks* callback = nullptr)
	{
		m_ts = convertToTimeStamp(source, format, m_flags, callback);
		m_stringRepresentation = source;
	}

	void set(SmallString&& source, const std::string_view format)
	{
		m_ts = convertToTimeStamp(source, format, m_flags);
		m_stringRepresentation = std::move(source);
	}

	void set(Jrd::thread_db* tdbb, const dsc& valueDsc);

	void set(const std::string_view source, const Firebird::EXPECT_DATETIME format)
	{
		m_ts = convertToTimeStamp(source, format, m_flags);
		m_stringRepresentation = source;
	}

	void updateFormat(const std::string_view format, Firebird::Callbacks* callback = nullptr)
	{
		m_ts = convertToTimeStamp(m_stringRepresentation, format, m_flags, callback);
	}

	void updatePrecision(const USHORT precision);

	SmallString&& extractString()
	{
		return std::move(m_stringRepresentation);
	}

	std::string_view asString() const
	{
		return m_stringRepresentation;
	}

	std::string_view getTypeName() const;
	inline ISC_TIMESTAMP_TZ getTS() const noexcept
	{
		return m_ts;
	}

	Firebird::EXPECT_DATETIME getFormat() const;
	void storeAsBytes(Firebird::BlrWriter& array) const;
	void readFromBytes(Firebird::BlrReader& array);

	bool operator>(const JsonDatetime& right) const;
	bool operator>=(const JsonDatetime& right) const;
	bool operator==(const JsonDatetime& right) const;
	bool operator<(const JsonDatetime& right) const;
	bool operator<=(const JsonDatetime& right) const;
	bool operator!=(const JsonDatetime& right) const;

private:
	static constexpr FB_SIZE_T TIMEZONE_SIZE = sizeof(ISC_TIMESTAMP_TZ);

private:
	static ISC_TIMESTAMP_TZ convertToTimeStamp(const std::string_view source, const std::string_view format,
		Flags& flags, Firebird::Callbacks* callback = nullptr);

	static ISC_TIMESTAMP_TZ convertToTimeStamp(const std::string_view source, const Firebird::EXPECT_DATETIME format,
		Flags& flags, Firebird::Callbacks* callback = nullptr);

	SmallString m_stringRepresentation;

	// The TS contains datetime in UTC and the source timezone
	ISC_TIMESTAMP_TZ m_ts = {};
	Flags m_flags = Firebird::CvtStringContains::NONE;
};

}

#endif // JSON_DATETIME_H
