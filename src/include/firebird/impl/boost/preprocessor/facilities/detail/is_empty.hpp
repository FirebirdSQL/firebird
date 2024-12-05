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
#ifndef FB_BOOST_PREPROCESSOR_DETAIL_IS_EMPTY_HPP
#define FB_BOOST_PREPROCESSOR_DETAIL_IS_EMPTY_HPP

#include <firebird/impl/boost/preprocessor/punctuation/is_begin_parens.hpp>

#if FB_BOOST_PP_VARIADICS_MSVC

# pragma warning(once:4002)

#define FB_BOOST_PP_DETAIL_IS_EMPTY_IIF_0(t, b) b
#define FB_BOOST_PP_DETAIL_IS_EMPTY_IIF_1(t, b) t

#else

#define FB_BOOST_PP_DETAIL_IS_EMPTY_IIF_0(t, ...) __VA_ARGS__
#define FB_BOOST_PP_DETAIL_IS_EMPTY_IIF_1(t, ...) t

#endif

#if FB_BOOST_PP_VARIADICS_MSVC && _MSC_VER <= 1400

#define FB_BOOST_PP_DETAIL_IS_EMPTY_PROCESS(param) \
    FB_BOOST_PP_IS_BEGIN_PARENS \
        ( \
        FB_BOOST_PP_DETAIL_IS_EMPTY_NON_FUNCTION_C param () \
        ) \
/**/

#else

#define FB_BOOST_PP_DETAIL_IS_EMPTY_PROCESS(...) \
    FB_BOOST_PP_IS_BEGIN_PARENS \
        ( \
        FB_BOOST_PP_DETAIL_IS_EMPTY_NON_FUNCTION_C __VA_ARGS__ () \
        ) \
/**/

#endif

#define FB_BOOST_PP_DETAIL_IS_EMPTY_PRIMITIVE_CAT(a, b) a ## b
#define FB_BOOST_PP_DETAIL_IS_EMPTY_IIF(bit) FB_BOOST_PP_DETAIL_IS_EMPTY_PRIMITIVE_CAT(FB_BOOST_PP_DETAIL_IS_EMPTY_IIF_,bit)
#define FB_BOOST_PP_DETAIL_IS_EMPTY_NON_FUNCTION_C(...) ()

#endif /* FB_BOOST_PREPROCESSOR_DETAIL_IS_EMPTY_HPP */
