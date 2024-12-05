# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2019.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_FACILITIES_CHECK_EMPTY_HPP
# define FB_BOOST_PREPROCESSOR_FACILITIES_CHECK_EMPTY_HPP
# include <firebird/impl/boost/preprocessor/variadic/has_opt.hpp>
# if FB_BOOST_PP_VARIADIC_HAS_OPT()
# include <firebird/impl/boost/preprocessor/facilities/is_empty_variadic.hpp>
# define FB_BOOST_PP_CHECK_EMPTY(...) FB_BOOST_PP_IS_EMPTY_OPT(__VA_ARGS__)
# endif /* FB_BOOST_PP_VARIADIC_HAS_OPT() */
# endif /* FB_BOOST_PREPROCESSOR_FACILITIES_CHECK_EMPTY_HPP */
