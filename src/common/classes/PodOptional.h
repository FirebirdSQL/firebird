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
 *  Copyright (c) 2025 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef CLASSES_POD_OPTIONAL_H
#define CLASSES_POD_OPTIONAL_H

#include <optional>
#include <type_traits>

namespace Firebird {


template <typename T>
class PodOptional
{
public:
	PodOptional(std::nullopt_t = std::nullopt) noexcept
		: value(),
		  hasValue(false)
	{
	}

	PodOptional(T aValue) noexcept
		: value(aValue),
		  hasValue(true)
	{
	}

public:
	std::optional<T> toOptional() const noexcept
	{
		static_assert(std::is_trivially_copyable_v<PodOptional<T>> && std::is_standard_layout_v<PodOptional<T>>);
		return hasValue ? std::make_optional(value) : std::nullopt;
	}

private:
	T value;
	bool hasValue;
};


} // namespace Firebird

#endif // CLASSES_POD_OPTIONAL_H
