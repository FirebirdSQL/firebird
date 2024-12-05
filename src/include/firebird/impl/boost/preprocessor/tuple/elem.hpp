# /* Copyright (C) 2001
#  * Housemarque Oy
#  * http://www.housemarque.com
#  *
#  * Distributed under the Boost Software License, Version 1.0. (See
#  * accompanying file LICENSE_1_0.txt or copy at
#  * http://www.boost.org/LICENSE_1_0.txt)
#  */
#
# /* Revised by Paul Mensonides (2002-2011) */
# /* Revised by Edward Diener (2011,2014,2020) */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_TUPLE_ELEM_HPP
# define FB_BOOST_PREPROCESSOR_TUPLE_ELEM_HPP
#
# include <firebird/impl/boost/preprocessor/cat.hpp>
# include <firebird/impl/boost/preprocessor/config/config.hpp>
# include <firebird/impl/boost/preprocessor/facilities/expand.hpp>
# include <firebird/impl/boost/preprocessor/facilities/overload.hpp>
# include <firebird/impl/boost/preprocessor/tuple/rem.hpp>
# include <firebird/impl/boost/preprocessor/variadic/elem.hpp>
# include <firebird/impl/boost/preprocessor/tuple/detail/is_single_return.hpp>
#
# if FB_BOOST_PP_VARIADICS_MSVC
#     define FB_BOOST_PP_TUPLE_ELEM(...) FB_BOOST_PP_TUPLE_ELEM_I(FB_BOOST_PP_OVERLOAD(FB_BOOST_PP_TUPLE_ELEM_O_, __VA_ARGS__), (__VA_ARGS__))
#     define FB_BOOST_PP_TUPLE_ELEM_I(m, args) FB_BOOST_PP_TUPLE_ELEM_II(m, args)
#     define FB_BOOST_PP_TUPLE_ELEM_II(m, args) FB_BOOST_PP_CAT(m ## args,)
/*
  Use FB_BOOST_PP_REM_CAT if it is a single element tuple ( which might be empty )
  else use FB_BOOST_PP_REM. This fixes a VC++ problem with an empty tuple and FB_BOOST_PP_TUPLE_ELEM
  functionality. See tuple_elem_bug_test.cxx.
*/
#     define FB_BOOST_PP_TUPLE_ELEM_O_2(n, tuple) \
         FB_BOOST_PP_VARIADIC_ELEM(n, FB_BOOST_PP_EXPAND(FB_BOOST_PP_TUPLE_IS_SINGLE_RETURN(FB_BOOST_PP_REM_CAT,FB_BOOST_PP_REM,tuple) tuple)) \
         /**/
# else
#     define FB_BOOST_PP_TUPLE_ELEM(...) FB_BOOST_PP_OVERLOAD(FB_BOOST_PP_TUPLE_ELEM_O_, __VA_ARGS__)(__VA_ARGS__)
#     define FB_BOOST_PP_TUPLE_ELEM_O_2(n, tuple) FB_BOOST_PP_VARIADIC_ELEM(n, FB_BOOST_PP_REM tuple)
# endif
# define FB_BOOST_PP_TUPLE_ELEM_O_3(size, n, tuple) FB_BOOST_PP_TUPLE_ELEM_O_2(n, tuple)
#
# /* directly used elsewhere in Boost... */
#
# define FB_BOOST_PP_TUPLE_ELEM_1_0(a) a
#
# define FB_BOOST_PP_TUPLE_ELEM_2_0(a, b) a
# define FB_BOOST_PP_TUPLE_ELEM_2_1(a, b) b
#
# define FB_BOOST_PP_TUPLE_ELEM_3_0(a, b, c) a
# define FB_BOOST_PP_TUPLE_ELEM_3_1(a, b, c) b
# define FB_BOOST_PP_TUPLE_ELEM_3_2(a, b, c) c
#
# endif
