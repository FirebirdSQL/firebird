# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2011.                                    *
#  *     (C) Copyright Paul Mensonides 2011.                                  *
#  *     Distributed under the Boost Software License, Version 1.0. (See      *
#  *     accompanying file LICENSE_1_0.txt or copy at                         *
#  *     http://www.boost.org/LICENSE_1_0.txt)                                *
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_TUPLE_SIZE_HPP
# define FB_BOOST_PREPROCESSOR_TUPLE_SIZE_HPP
#
# include <firebird/impl/boost/preprocessor/cat.hpp>
# include <firebird/impl/boost/preprocessor/config/config.hpp>
# include <firebird/impl/boost/preprocessor/control/if.hpp>
# include <firebird/impl/boost/preprocessor/variadic/has_opt.hpp>
# include <firebird/impl/boost/preprocessor/variadic/size.hpp>
#
# if FB_BOOST_PP_VARIADIC_HAS_OPT()
#     if FB_BOOST_PP_VARIADICS_MSVC
#         define FB_BOOST_PP_TUPLE_SIZE(tuple) FB_BOOST_PP_TUPLE_SIZE_CHECK(FB_BOOST_PP_CAT(FB_BOOST_PP_VARIADIC_SIZE tuple,))
#     else
#         define FB_BOOST_PP_TUPLE_SIZE(tuple) FB_BOOST_PP_TUPLE_SIZE_CHECK(FB_BOOST_PP_VARIADIC_SIZE tuple)
#     endif
#     define FB_BOOST_PP_TUPLE_SIZE_CHECK(size) FB_BOOST_PP_IF(size,size,1)
# elif FB_BOOST_PP_VARIADICS_MSVC
#     define FB_BOOST_PP_TUPLE_SIZE(tuple) FB_BOOST_PP_CAT(FB_BOOST_PP_VARIADIC_SIZE tuple,)
# else
#     define FB_BOOST_PP_TUPLE_SIZE(tuple) FB_BOOST_PP_VARIADIC_SIZE tuple
# endif
#
# endif
