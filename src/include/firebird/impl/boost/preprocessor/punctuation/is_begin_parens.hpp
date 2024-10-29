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
# ifndef FB_BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP
# define FB_BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP

#include <firebird/impl/boost/preprocessor/config/config.hpp>
#include <firebird/impl/boost/preprocessor/punctuation/detail/is_begin_parens.hpp>

#if FB_BOOST_PP_VARIADICS_MSVC && _MSC_VER <= 1400

#define FB_BOOST_PP_IS_BEGIN_PARENS(param) \
    FB_BOOST_PP_DETAIL_IBP_SPLIT \
      ( \
      0, \
      FB_BOOST_PP_DETAIL_IBP_CAT \
        ( \
        FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_, \
        FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_C param \
        ) \
      ) \
/**/

#else

#define FB_BOOST_PP_IS_BEGIN_PARENS(...) \
    FB_BOOST_PP_DETAIL_IBP_SPLIT \
      ( \
      0, \
      FB_BOOST_PP_DETAIL_IBP_CAT \
        ( \
        FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_, \
        FB_BOOST_PP_DETAIL_IBP_IS_VARIADIC_C __VA_ARGS__ \
        ) \
      ) \
/**/

#endif /* FB_BOOST_PP_VARIADICS_MSVC && _MSC_VER <= 1400 */
#endif /* FB_BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP */
