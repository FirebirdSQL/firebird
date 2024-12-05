# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2003.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_FACILITIES_IS_1_HPP
# define FB_BOOST_PREPROCESSOR_FACILITIES_IS_1_HPP
#
# include <firebird/impl/boost/preprocessor/cat.hpp>
# include <firebird/impl/boost/preprocessor/facilities/is_empty.hpp>
#
# /* FB_BOOST_PP_IS_1 */
#
# define FB_BOOST_PP_IS_1(x) FB_BOOST_PP_IS_EMPTY(FB_BOOST_PP_CAT(FB_BOOST_PP_IS_1_HELPER_, x))
# define FB_BOOST_PP_IS_1_HELPER_1
#
# endif
