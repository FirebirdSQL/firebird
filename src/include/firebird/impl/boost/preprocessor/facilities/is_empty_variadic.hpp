# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2014,2019.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP
# define FB_BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
# include <firebird/impl/boost/preprocessor/punctuation/is_begin_parens.hpp>
# include <firebird/impl/boost/preprocessor/facilities/detail/is_empty.hpp>
#
#if FB_BOOST_PP_VARIADICS_MSVC && _MSC_VER <= 1400
#
#define FB_BOOST_PP_IS_EMPTY(param) \
    FB_BOOST_PP_DETAIL_IS_EMPTY_IIF \
      ( \
      FB_BOOST_PP_IS_BEGIN_PARENS \
        ( \
        param \
        ) \
      ) \
      ( \
      FB_BOOST_PP_IS_EMPTY_ZERO, \
      FB_BOOST_PP_DETAIL_IS_EMPTY_PROCESS \
      ) \
    (param) \
/**/
#define FB_BOOST_PP_IS_EMPTY_ZERO(param) 0
# else
# if defined(__cplusplus) && __cplusplus > 201703L
# include <firebird/impl/boost/preprocessor/variadic/has_opt.hpp>
#define FB_BOOST_PP_IS_EMPTY(...) \
    FB_BOOST_PP_DETAIL_IS_EMPTY_IIF \
      ( \
      FB_BOOST_PP_VARIADIC_HAS_OPT() \
      ) \
      ( \
      FB_BOOST_PP_IS_EMPTY_OPT, \
      FB_BOOST_PP_IS_EMPTY_NO_OPT \
      ) \
    (__VA_ARGS__) \
/**/
#define FB_BOOST_PP_IS_EMPTY_FUNCTION2(...) \
    __VA_OPT__(0,) 1 \
/**/
#define FB_BOOST_PP_IS_EMPTY_FUNCTION(...) \
    FB_BOOST_PP_IS_EMPTY_FUNCTION2(__VA_ARGS__) \
/**/
#define FB_BOOST_PP_IS_EMPTY_OPT(...) \
    FB_BOOST_PP_VARIADIC_HAS_OPT_ELEM0(FB_BOOST_PP_IS_EMPTY_FUNCTION(__VA_ARGS__),) \
/**/
# else
#define FB_BOOST_PP_IS_EMPTY(...) \
    FB_BOOST_PP_IS_EMPTY_NO_OPT(__VA_ARGS__) \
/**/
# endif /* defined(__cplusplus) && __cplusplus > 201703L */
#define FB_BOOST_PP_IS_EMPTY_NO_OPT(...) \
    FB_BOOST_PP_DETAIL_IS_EMPTY_IIF \
      ( \
      FB_BOOST_PP_IS_BEGIN_PARENS \
        ( \
        __VA_ARGS__ \
        ) \
      ) \
      ( \
      FB_BOOST_PP_IS_EMPTY_ZERO, \
      FB_BOOST_PP_DETAIL_IS_EMPTY_PROCESS \
      ) \
    (__VA_ARGS__) \
/**/
#define FB_BOOST_PP_IS_EMPTY_ZERO(...) 0
# endif /* FB_BOOST_PP_VARIADICS_MSVC && _MSC_VER <= 1400 */
# endif /* FB_BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP */
