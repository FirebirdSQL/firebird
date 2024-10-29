# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2011.                                  *
#  *     (C) Copyright Edward Diener 2011.                                    *
#  *     Distributed under the Boost Software License, Version 1.0. (See      *
#  *     accompanying file LICENSE_1_0.txt or copy at                         *
#  *     http://www.boost.org/LICENSE_1_0.txt)                                *
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_FACILITIES_OVERLOAD_HPP
# define FB_BOOST_PREPROCESSOR_FACILITIES_OVERLOAD_HPP
#
# include <firebird/impl/boost/preprocessor/cat.hpp>
# include <firebird/impl/boost/preprocessor/variadic/size.hpp>
#
# /* FB_BOOST_PP_OVERLOAD */
#
# define FB_BOOST_PP_OVERLOAD(prefix, ...) FB_BOOST_PP_CAT(prefix, FB_BOOST_PP_VARIADIC_SIZE(__VA_ARGS__))
#
# endif
