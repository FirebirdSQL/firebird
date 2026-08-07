/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonScalar.cpp
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


#include "JsonScalar.h"

#include "../JsonConsts.h"
#include "JsonTypes.h"
#include "JsonDatetime.h"

#include "firebird.h"
#include "../common/classes/BlrReader.h"
#include "../common/classes/BlrWriter.h"

using namespace FBJSON;


JsonScalar& JsonScalar::operator=(const JsonScalar& rhs)
{
	reset(rhs.type);
	flags = rhs.flags;

	switch (rhs.type)
	{
		case ValueType::STRING:
			value.string = FB_NEW_POOL(getPool()) SmallString(getPool(), *rhs.value.string);
			break;
		case ValueType::DATETIME:
			value.datetime = FB_NEW_POOL(getPool()) JsonDatetime(getPool(), *rhs.value.datetime);
			break;
		default:
			memcpy(&value, &rhs.value, sizeof(DataVariant));
			break;
	}
	return *this;
}

double JsonScalar::getDouble() const
{
	switch (type)
	{
		case ValueType::INT:
			return static_cast<double>(value.integer);
		case ValueType::DOUBLE:
			return value.doubleValue;
		case ValueType::STRING:
			json_skippable_exception::raise(JsonStatusMsg(isc_jdyn_common) <<
				JsonStatusMsg(isc_jdyn_get_double_string_type_error) << JsonStatusMsgStrArg(*value.string));
		default:
			json_skippable_exception::raise(JsonStatusMsg(isc_jdyn_common) <<
				JsonStatusMsg(isc_jdyn_get_double_invalid_type_error));
	}

	// Just in case
	fb_assert(false);
	return 0;
}

SmallString* JsonScalar::str() const
{
	if (type == ValueType::STRING)
		return value.string;
	else
	{
		fb_assert(false);
		json_skippable_exception::raise(JsonStatusMsg(isc_jdyn_common) <<
			JsonStatusMsg(isc_jdyn_internal_error));
		return nullptr;
	}
}

void JsonScalar::set(const std::string_view view)
{
	// Create the new string first in case the new range belongs to the current string
	SmallString* temp = FB_NEW_POOL(getPool()) SmallString(getPool(), view.data(), static_cast<SmallString::size_type>(view.length()));
	reset(ValueType::STRING);

	value.string = temp;
}

JsonDatetime& JsonScalar::setToDatetime()
{
	reset(ValueType::DATETIME);

	value.datetime = FB_NEW_POOL(getPool()) JsonDatetime(getPool());
	return *value.datetime;
}

void JsonScalar::writeScalarAsBytes(Firebird::BlrWriter& array) const
{
	static constexpr USHORT ushortSizeInBits = sizeof(USHORT) * CHAR_BIT;
	array.appendUChar(static_cast<UCHAR>(type));

	switch (type)
	{
		case ValueType::JNULL:
			break;
		case ValueType::BOOL:
			array.appendUChar(value.boolean ? 1 : 0);
			break;
		case ValueType::INT:
			array.appendUShort(value.integer);
			array.appendUShort(value.integer >> ushortSizeInBits);
			array.appendUShort(value.integer >> ushortSizeInBits * 2);
			array.appendUShort(value.integer >> ushortSizeInBits * 3);
			break;
		case ValueType::DOUBLE:
		{
			SINT64 i64value = *((SINT64*)(&value.doubleValue));
			array.appendUShort(i64value);
			array.appendUShort(i64value >> ushortSizeInBits);
			array.appendUShort(i64value >> ushortSizeInBits * 2);
			array.appendUShort(i64value >> ushortSizeInBits * 3);
			break;
		}
		case ValueType::STRING:
			array.appendString(1, *value.string);
			break;
		case ValueType::DATETIME:
			value.datetime->storeAsBytes(array);
			break;
		default:
			fb_assert(false);
			break;
	}
}

void JsonScalar::readScalarFromBytes(Firebird::BlrReader& array)
{
	const ValueType rtype = static_cast<ValueType>(array.getByte());

	switch (rtype)
	{
		case ValueType::JNULL:
			break;
		case ValueType::BOOL:
			set(array.getByte() == 1);
			break;
		case ValueType::INT:
		{
			const SSHORT len = sizeof(SINT64);
			SINT64 buffer = isc_portable_integer(array.getPos(), len);
			array.seekForward(len);

			set(buffer);
			break;
		}
		case ValueType::DOUBLE:
		{
			const SSHORT len = sizeof(SINT64);
			SINT64 buffer = isc_portable_integer(array.getPos(), len);
			array.seekForward(len);

			double dval = *((double*)(&buffer));
			set(dval);
			break;
		}
		case ValueType::STRING:
		{
			set("");
			array.getVerbString(*value.string);
			break;
		}
		case ValueType::DATETIME: // keep it as the last scalar type
		{
			FBJSON::JsonDatetime& datetime = setToDatetime();
			datetime.readFromBytes(array);
			break;
		}
		default:
			fb_assert(false);
	}
}

Jrd::impure_value JsonScalar::makeScalarDsc() const
{
	Jrd::impure_value output = {};
	switch (type)
	{
		case ValueType::JNULL:
			output.vlu_desc.setNull();
			break;
		case ValueType::BOOL:
			output.vlu_misc.vlu_uchar = value.boolean;
			output.vlu_desc.makeBoolean(reinterpret_cast<UCHAR*>(&output.vlu_misc.vlu_uchar));
			break;
		case ValueType::INT:
			output.make_int64(value.integer);
			break;
		case ValueType::DOUBLE:
			output.make_double(value.doubleValue);
			break;
		case ValueType::STRING:
			output.vlu_desc.makeText(value.string->length(), JSON_TTYPE, (UCHAR*)value.string->data());
			break;
		case ValueType::DATETIME: // keep it as the last scalar type
			output.vlu_misc.vlu_timestamp_tz = value.datetime->getTS();
			output.vlu_desc.makeTimestampTz(&output.vlu_misc.vlu_timestamp_tz);
			break;
		default:
			fb_assert(false);
	}

	return output;
}

void JsonScalar::reset(const ValueType newType) noexcept
{
	switch (type)
	{
	case ValueType::EMPTY:
	case ValueType::BOOL:
	case ValueType::INT:
	case ValueType::JNULL:
		break;
	case ValueType::STRING:
		delete value.string;
		break;
	case ValueType::DATETIME:
		delete value.datetime;
		break;
	default:
		releaseExtend();
		break;
	}
	flags = FLAG_NONE;

	value = {};
	type = newType;
}
