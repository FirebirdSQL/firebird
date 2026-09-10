/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonDatetime.cpp
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


#include "JsonDatetime.h"
#include "../JsonConsts.h"

// Engine stuff for datetime parsing
#include "../common/cvt.h"
#include "../common/dsc.h"
#include "../common/TimeZoneUtil.h"
#include "../common/CvtFormat.h"
#include "../common/classes/BlrReader.h"
#include "../common/classes/BlrWriter.h"

#include "../jrd/cvt_proto.h"
#include "../jrd/mov_proto.h"

namespace FBJSON{

using DATETIME_SUBTYPE = Firebird::EXPECT_DATETIME;


// Use custom throw function because EngineCallbacks::instance make tdbb_status dirty
class JsonErrorCallback : public Jrd::EngineCallbacks
{
public:
	JsonErrorCallback(MemoryPool&) :
		Jrd::EngineCallbacks(throwError)
	{ }

	[[noreturn]] static void throwError(const Firebird::Arg::StatusVector& v)
	{
		v.raise();
	}
};
Firebird::GlobalPtr<JsonErrorCallback> jsonCallback;

struct DatetimeUtils
{
	ISC_TIMESTAMP_TZ inputUtcTimestamp;
	ISC_TIMESTAMP localBuffer;
	ISC_TIME_TZ timeTzBuffer;

	static DATETIME_SUBTYPE getFormat(const Firebird::CvtStringContains::TypeFlags flags)
	{
		using namespace std::literals;
		using namespace Firebird::CvtStringContains;

		switch (flags)
		{
		case TIME | TIMEZONE:
			return DATETIME_SUBTYPE::expect_sql_time_tz;
		case TIME:
			return DATETIME_SUBTYPE::expect_sql_time;
		case DATE | TIMEZONE:
		case DATE | TIME | TIMEZONE:
			return DATETIME_SUBTYPE::expect_timestamp_tz;
		case DATE | TIME:
			return DATETIME_SUBTYPE::expect_timestamp;
		case DATE:
			return DATETIME_SUBTYPE::expect_sql_date;
		default:
			fb_assert(false);
			return DATETIME_SUBTYPE::expect_sql_time_tz;
		}
	}

	ISC_TIMESTAMP& getLocal()
	{
		return localBuffer = Firebird::TimeZoneUtil::timeStampTzToTimeStamp(inputUtcTimestamp,
			JRD_get_thread_data()->getAttachment()->att_current_timezone);
	}

	dsc makeDatetimeDsc(const ISC_TIMESTAMP_TZ utcTimestamp, const JsonDatetime::Flags flags)
	{
		inputUtcTimestamp = utcTimestamp;

		dsc timestampDsc;

		switch (getFormat(flags))
		{
		case DATETIME_SUBTYPE::expect_sql_time:
			timestampDsc.makeTime(&getLocal().timestamp_time);
			break;
		case DATETIME_SUBTYPE::expect_sql_time_tz:
			timeTzBuffer.utc_time = utcTimestamp.utc_timestamp.timestamp_time;
			timeTzBuffer.time_zone = utcTimestamp.time_zone;
			timestampDsc.makeTimeTz(&timeTzBuffer);
			break;
		case DATETIME_SUBTYPE::expect_sql_date:
			timestampDsc.makeDate(&getLocal().timestamp_date);
			break;
		case DATETIME_SUBTYPE::expect_timestamp:
			timestampDsc.makeTimestamp(&getLocal());
			break;
		case DATETIME_SUBTYPE::expect_timestamp_tz:
			timestampDsc.makeTimestampTz(&inputUtcTimestamp);
			break;
		default:
			fb_assert(false);
			timestampDsc.makeTimestampTz(&inputUtcTimestamp);
			break;
		}

		return timestampDsc;
	}
};


void JsonDatetime::set(Jrd::thread_db* tdbb, const dsc& valueDsc)
{
	using namespace Firebird::CvtStringContains;
	m_stringRepresentation = MOV_make_string2(tdbb, &valueDsc, JSON_TTYPE);

	// m_ts = MOV_get_timestamp_tz(&valueDsc);
	// Can't use MOV_get_timestamp_tz because the CVT_string_to_datetime function adds TimeZoneUtil::TIME_TZ_BASE_DATE
	// and other instance stuff. It is more reliably to just use the same function instead of tring to mimic its output

	Firebird::EXPECT_DATETIME format = DATETIME_SUBTYPE::expect_timestamp_tz;
	switch (valueDsc.dsc_dtype)
	{
		case dtype_sql_time:
			format = DATETIME_SUBTYPE::expect_sql_time;
			break;
		case dtype_sql_time_tz:
		case dtype_ex_time_tz:
			format = DATETIME_SUBTYPE::expect_sql_time_tz;
			break;
		case dtype_sql_date:
			format = DATETIME_SUBTYPE::expect_sql_date;
			break;
		case dtype_timestamp:
			format = DATETIME_SUBTYPE::expect_timestamp;
			break;
		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			format = DATETIME_SUBTYPE::expect_timestamp_tz;
			break;
		default:
			fb_assert(false);
	}

	m_ts = convertToTimeStamp(m_stringRepresentation, format, m_flags);
}

void JsonDatetime::updatePrecision(const USHORT precision)
{
	using namespace Firebird;

	TimeStamp::round_time(m_ts.utc_timestamp.timestamp_time, precision);

	DatetimeUtils maker;
	dsc timestampDsc = maker.makeDatetimeDsc(m_ts, m_flags);

	m_stringRepresentation = MOV_make_string2(JRD_get_thread_data(), &timestampDsc, JSON_TTYPE);
	m_stringRepresentation.rtrim();
}

std::string_view JsonDatetime::getTypeName() const
{
	using namespace std::literals;
	using namespace Firebird::CvtStringContains;
	using namespace Firebird;

	switch (getFormat())
	{
	case DATETIME_SUBTYPE::expect_sql_time_tz:
		return Tokens::TypeName::TIME_WITH_TZ;
	case DATETIME_SUBTYPE::expect_sql_time:
		return Tokens::TypeName::TIME_WITHOUT_TZ;
	case DATETIME_SUBTYPE::expect_timestamp_tz:
		return Tokens::TypeName::TIMESTAMT_WITH_TZ;
	case DATETIME_SUBTYPE::expect_timestamp:
		return Tokens::TypeName::TIMESTAMT_WITHOUT_TZ;
	case DATETIME_SUBTYPE::expect_sql_date:
		return Tokens::TypeName::DATE;
	default:
		fb_assert(false);
		return Tokens::TypeName::UNKNOWN;
	}
}

Firebird::EXPECT_DATETIME JsonDatetime::getFormat() const
{
	return DatetimeUtils::getFormat(m_flags);
}

void JsonDatetime::storeAsBytes(Firebird::BlrWriter& array) const
{
	array.appendBytes(reinterpret_cast<const UCHAR*>(&m_ts), TIMEZONE_SIZE);
	array.appendMetaString(m_stringRepresentation.c_str());
	array.appendUChar(m_flags);
}

void JsonDatetime::readFromBytes(Firebird::BlrReader& array)
{
	const UCHAR* pos = array.getPos();
	array.seekForward(TIMEZONE_SIZE);
	m_ts = *reinterpret_cast<const ISC_TIMESTAMP_TZ*>(pos);

	array.getString(m_stringRepresentation);
	m_flags = static_cast<Flags>(array.getByte());
}

bool JsonDatetime::operator>(const JsonDatetime& right) const
{
	return Firebird::TimeZoneUtil::compareUtcTimeStamps(m_ts.utc_timestamp, right.m_ts.utc_timestamp) > 0;
}

bool JsonDatetime::operator>=(const JsonDatetime& right) const
{
	return Firebird::TimeZoneUtil::compareUtcTimeStamps(m_ts.utc_timestamp, right.m_ts.utc_timestamp) >= 0;
}

bool JsonDatetime::operator==(const JsonDatetime& right) const
{
	return Firebird::TimeZoneUtil::compareUtcTimeStamps(m_ts.utc_timestamp, right.m_ts.utc_timestamp) == 0;
}

bool JsonDatetime::operator<(const JsonDatetime& right) const
{
	return !operator>=(right);
}

bool JsonDatetime::operator<=(const JsonDatetime& right) const
{
	return !operator>(right);
}

bool JsonDatetime::operator!=(const JsonDatetime& right) const
{
	return !operator==(right);
}


ISC_TIMESTAMP_TZ JsonDatetime::convertToTimeStamp(const std::string_view source, const std::string_view format,
	Flags& outFlags, Firebird::Callbacks* callback)
{
	using namespace Firebird;

	fb_assert(source.length() < MAX_USHORT);

	outFlags = Firebird::CvtStringContains::NONE;

	dsc strdesc;
	strdesc.makeText(static_cast<USHORT>(source.length()), CS_NONE, (UCHAR*)source.data());

	// CVT_string_to_datetime throws exceptions not only via callback, so setting a custom ErrorFunction is not an option
	if (callback == nullptr)
		callback = &jsonCallback;

	ISC_TIMESTAMP_TZ dest = {};

	// CVT functions return timestamp in UTC
	if (format.length())
	{
		// Expected type does not matter
		// Exceptions only on invalid input
		dest = CVT_format_string_to_datetime(&strdesc,
			string(format.data(), static_cast<string::size_type>(format.length())),
			DATETIME_SUBTYPE::expect_timestamp_tz, callback, &outFlags);
	}
	else
	{
		// We need to detect the type so first try to parse the string as timestamp
		// On failure - try sql_time
		// If format is invalid - catch the exception in the JsonVariant::convertToDatetime method
		try
		{
			CVT_string_to_datetime(&strdesc, &dest, &outFlags,
				EXPECT_DATETIME::expect_timestamp_tz, true, callback);
		}
		catch (...)
		{
			dest = {};
			CVT_string_to_datetime(&strdesc, &dest, &outFlags,
				Firebird::EXPECT_DATETIME::expect_sql_time_tz, true, callback);
		}

		// When passing incorrect expected type (for example, expect_timestamp_tz for a date),
		// the source type will be adjusted to the expected
		// Use flags to get the real output type and extract correct value
		const auto realType =  DatetimeUtils::getFormat(outFlags);
		dest = {};
		CVT_string_to_datetime(&strdesc, &dest, &outFlags, realType, true, callback);
	}

	return dest;
}

ISC_TIMESTAMP_TZ JsonDatetime::convertToTimeStamp(const std::string_view source, const Firebird::EXPECT_DATETIME format,
	Flags& outFlags, Firebird::Callbacks* callback)
{
	using namespace Firebird;

	fb_assert(source.length() < MAX_USHORT);

	outFlags = Firebird::CvtStringContains::NONE;

	// Pass string as dsc
	dsc strdesc;
	strdesc.makeText(static_cast<USHORT>(source.length()), CS_NONE, (UCHAR*)source.data());

	// CVT_string_to_datetime throws exceptions not only via callback, so setting a custom ErrorFunction is not an option
	if (callback == nullptr)
		callback = &jsonCallback;

	ISC_TIMESTAMP_TZ timeStamp = {};
	CVT_string_to_datetime(&strdesc, &timeStamp, &outFlags, format, true, callback);

	return timeStamp;
}

}
