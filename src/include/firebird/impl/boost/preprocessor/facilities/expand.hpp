# /* Copyright (C) 2001
#  * Housemarque Oy
#  * http://www.housemarque.com
#  *
#  * Distributed under the Boost Software License, Version 1.0. (See
#  * accompanying file LICENSE_1_0.txt or copy at
#  * http://www.boost.org/LICENSE_1_0.txt)
#  */
#
# /* Revised by Paul Mensonides (2002) */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_FACILITIES_EXPAND_HPP
# define FB_BOOST_PREPROCESSOR_FACILITIES_EXPAND_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
#
# if ~FB_BOOST_PP_CONFIG_FLAGS() & FB_BOOST_PP_CONFIG_MWCC() && ~FB_BOOST_PP_CONFIG_FLAGS() & FB_BOOST_PP_CONFIG_DMC()
#    define FB_BOOST_PP_EXPAND(x) FB_BOOST_PP_EXPAND_I(x)
# else
#    define FB_BOOST_PP_EXPAND(x) FB_BOOST_PP_EXPAND_OO((x))
#    define FB_BOOST_PP_EXPAND_OO(par) FB_BOOST_PP_EXPAND_I ## par
# endif
#
# define FB_BOOST_PP_EXPAND_I(x) x
#
# endif
