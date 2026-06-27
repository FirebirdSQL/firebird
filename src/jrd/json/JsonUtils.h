/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonUtils.h
 *	DESCRIPTION:	Common functions to work with JSON.
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
 *  Copyright (c) 2025 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "firebird.h"
#include "../../common/StatusArg.h"

#include <array>
#include <charconv> // std::to_chars
#include <string_view>

#ifdef ANDROID
#include "../common/utils_proto.h"
#endif


namespace FBJSON
{

using NumberConvertBuffer = std::array<char, 32>;

inline constexpr int NUMBER_PRECISION = 14;

template<class T>
inline std::string_view convertNumberToString(NumberConvertBuffer& buffer, T value)
{
	// Use to_chars because other function are locale-depended and can insert a ',' instead of a '.'
	std::to_chars_result result;

	// Do not use begin/end or front/back because they requeue some obscure extra casts to char* on Windows
	char* end = buffer.data() + buffer.size();
	if constexpr (std::is_floating_point_v<T>)
		result = std::to_chars(buffer.data(), end, value, std::chars_format::general, NUMBER_PRECISION);
	else
		result = std::to_chars(buffer.data(), end, value);

	if (result.ec != std::errc())
	{
		fb_assert(false);
		Firebird::Arg::Gds(isc_arith_except).raise();
	}

	return std::string_view(buffer.data(), result.ptr - buffer.data());
}

template<class T>
constexpr USHORT getSuperType(T op1, T op2)
{
	return (static_cast<USHORT>(op1) << 8) | static_cast<UCHAR>(op2);
}

enum BoolSuperType : USHORT
{
	TRUE_VS_FALSE = getSuperType(true, false),
	FALSE_VS_TRUE = getSuperType(false, true),
	TRUE_VS_TRUE = getSuperType(true, true),
	FALSE_VS_FALSE = getSuperType(false, false)
};

} // namespace

#endif // !JSON_UTILS_H
