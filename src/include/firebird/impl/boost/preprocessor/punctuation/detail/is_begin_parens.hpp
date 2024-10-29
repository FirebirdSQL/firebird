# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2014.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
#ifndef FB_BOOST_PREPROCESSOR_DETAIL_IS_BEGIN_PARENS_HPP
#define FB_BOOST_PREPROCESSOR_DETAIL_IS_BEGIN_PARENS_HPP

#if FB_BOOST_PP_VARIADICS_MSVC

#include <firebird/impl/boost/preprocessor/facilities/empty.hpp>

#define FB_BOOST_PP_DETAIL_VD_IBP_CAT(a, b) FB_BOOST_PP_DETAIL_VD_IBP_CAT_I(a, b)
#define FB_BOOST_PP_DETAIL_VD_IBP_CAT_I(a, b) FB_BOOST_PP_DETAIL_VD_IBP_CAT_II(a ## b)
#define FB_BOOST_PP_DETAIL_VD_IBP_CAT_II(res) res

#define FB_BOOST_PP_DETAIL_IBP_SPLIT(i, ...) \
    FB_BOOST_PP_DETAIL_VD_IBP_CAT(FB_BOOST_PP_DETAIL_IBP_PRIMITIVE_CAT(FB_BOOST_PP_DETAIL_IBP_SPLIT_,i)(__VA_ARGS__),FB_BOOST_PP_EMPTY()) \
/**/

#define FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_C(...) 1 1

#else

#define FB_BOOST_PP_DETAIL_IBP_SPLIT(i, ...) \
    FB_BOOST_PP_DETAIL_IBP_PRIMITIVE_CAT(FB_BOOST_PP_DETAIL_IBP_SPLIT_,i)(__VA_ARGS__) \
/**/

#define FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_C(...) 1

#endif /* FB_BOOST_PP_VARIADICS_MSVC */

#define FB_BOOST_PP_DETAIL_IBP_SPLIT_0(a, ...) a
#define FB_BOOST_PP_DETAIL_IBP_SPLIT_1(a, ...) __VA_ARGS__

#define FB_BOOST_PP_DETAIL_IBP_CAT(a, ...) FB_BOOST_PP_DETAIL_IBP_PRIMITIVE_CAT(a,__VA_ARGS__)
#define FB_BOOST_PP_DETAIL_IBP_PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_1 1,
#define FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_C 0,

#endif /* FB_BOOST_PREPROCESSOR_DETAIL_IS_BEGIN_PARENS_HPP */
