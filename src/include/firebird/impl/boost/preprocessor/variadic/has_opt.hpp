# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2019.                                    *
#  *     Distributed under the Boost Software License, Version 1.0. (See      *
#  *     accompanying file LICENSE_1_0.txt or copy at                         *
#  *     http://www.boost.org/LICENSE_1_0.txt)                                *
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_VARIADIC_HAS_OPT_HPP
# define FB_BOOST_PREPROCESSOR_VARIADIC_HAS_OPT_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
#
# /* FB_BOOST_PP_VARIADIC_HAS_OPT */
#
# if defined(__cplusplus) && __cplusplus > 201703L
#  if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 8 && __GNUC__ < 10
#   define FB_BOOST_PP_VARIADIC_HAS_OPT() 0
#  elif defined(__clang__) && __clang_major__ < 9
#   define FB_BOOST_PP_VARIADIC_HAS_OPT() 0
#  else
#   include <firebird/impl/boost/preprocessor/variadic/detail/has_opt.hpp>
#   define FB_BOOST_PP_VARIADIC_HAS_OPT() \
  FB_BOOST_PP_VARIADIC_HAS_OPT_ELEM2(FB_BOOST_PP_VARIADIC_HAS_OPT_FUNCTION(?),) \
/**/
#  endif
# else
# define FB_BOOST_PP_VARIADIC_HAS_OPT() 0
# endif
#
# endif
