# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2002.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_LOGICAL_COMPL_HPP
# define FB_BOOST_PREPROCESSOR_LOGICAL_COMPL_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
#
# /* FB_BOOST_PP_COMPL */
#
# if ~FB_BOOST_PP_CONFIG_FLAGS() & FB_BOOST_PP_CONFIG_MWCC()
#    define FB_BOOST_PP_COMPL(x) FB_BOOST_PP_COMPL_I(x)
# else
#    define FB_BOOST_PP_COMPL(x) FB_BOOST_PP_COMPL_OO((x))
#    define FB_BOOST_PP_COMPL_OO(par) FB_BOOST_PP_COMPL_I ## par
# endif
#
# if ~FB_BOOST_PP_CONFIG_FLAGS() & FB_BOOST_PP_CONFIG_MSVC()
#    define FB_BOOST_PP_COMPL_I(x) FB_BOOST_PP_COMPL_ ## x
# else
#    define FB_BOOST_PP_COMPL_I(x) FB_BOOST_PP_COMPL_ID(FB_BOOST_PP_COMPL_ ## x)
#    define FB_BOOST_PP_COMPL_ID(id) id
# endif
#
# define FB_BOOST_PP_COMPL_0 1
# define FB_BOOST_PP_COMPL_1 0
#
# endif
