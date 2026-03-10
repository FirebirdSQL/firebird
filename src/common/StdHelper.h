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
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef FB_COMMON_STD_HELPER_H
#define FB_COMMON_STD_HELPER_H

#include <algorithm>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <cstddef>
#include <source_location>
#include <stdexcept>
#include "boost/type_traits/copy_cv.hpp"

namespace Firebird {


// To be used with std::visit

template <typename... Ts>
struct StdVisitOverloads : Ts...
{
	using Ts::operator()...;
};

template <typename... Ts>
StdVisitOverloads(Ts...) -> StdVisitOverloads<Ts...>;


// Variant helpers

template <typename T>
struct IsVariant : std::false_type {};

template <typename... Ts>
struct IsVariant<std::variant<Ts...>> : std::true_type {};

template <typename... Ts>
struct IsVariant<const std::variant<Ts...>> : std::true_type {};

template <typename T>
concept Variant = IsVariant<T>::value;

template <typename T, typename Variant>
struct VariantIndex;

template <typename T, typename... Types>
struct VariantIndex<T, std::variant<Types...>>
{
	static constexpr std::size_t value = []() constexpr
	{
		std::size_t index = 0;
		const bool found = ((std::is_same_v<T, Types> ? true : (++index, false)) || ...);
		return found ? index : std::variant_npos;
	}();
};

template <typename T, typename... Types>
struct VariantIndex<T, const std::variant<Types...>>
{
	static constexpr std::size_t value = VariantIndex<T, std::variant<Types...>>::value;
};

template <typename T, Variant V>
constexpr std::size_t VariantIndexValue = VariantIndex<T, V>::value;

template <typename T, typename V>
concept VariantContains = Variant<V> && requires
{
	requires VariantIndexValue<T, V> != std::variant_npos;
};

template <typename T, Variant V>
	requires VariantContains<T, V>
constexpr std::size_t getVariantIndex()
{
	return VariantIndexValue<T, V>;
}


template <Variant V, std::size_t... Is>
constexpr std::size_t maxVariantSizeImpl(std::index_sequence<Is...>)
{
	return std::max({sizeof(std::variant_alternative_t<Is, V>)...});
}

template <Variant V>
constexpr std::size_t maxVariantSize()
{
	return maxVariantSizeImpl<V>(std::make_index_sequence<std::variant_size_v<V>>{});
}


template <typename V, std::size_t... Is>
constexpr V createVariantByIndexImpl(std::size_t index, std::index_sequence<Is...>)
{
	using FactoryFunc = V(*)(void);
	constexpr FactoryFunc factories[] = {
		+[] { return V{std::in_place_index<Is>}; }...
	};

	if (index < sizeof...(Is))
		return factories[index]();
	else
		throw std::out_of_range("Invalid variant index: " + std::to_string(index));
}

template <typename V>
constexpr V createVariantByIndex(std::size_t index)
{
	return createVariantByIndexImpl<V>(
		index,
		std::make_index_sequence<std::variant_size_v<V>>{}
	);
}


template <Variant V>
constexpr auto getVariantIndexAndSpan(V& message)
{
	return std::visit(
		[](auto& arg) -> auto
		{
			using T = std::decay_t<decltype(arg)>;

			return std::pair{
				getVariantIndex<T, V>(),
				std::span<boost::copy_cv_t<V, uint8_t>>{
					reinterpret_cast<std::add_pointer_t<boost::copy_cv_t<V, uint8_t>>>(&arg),
					sizeof(T)
				}
			};
		},
		message
	);
}


struct CustomSourceLocation
{
	static constexpr CustomSourceLocation current(
		const std::source_location& location = std::source_location::current())
	{
		return {
			.fileName = location.file_name(),
			.line = static_cast<int>(location.line())
		};
	}

    const char* fileName;
    int line;
};


}	// namespace Firebird

#endif	// FB_COMMON_STD_HELPER_H
