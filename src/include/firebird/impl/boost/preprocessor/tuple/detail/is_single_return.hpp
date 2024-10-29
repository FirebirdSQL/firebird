# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2014.                                    *
#  *     Distributed under the Boost Software License, Version 1.0. (See      *
#  *     accompanying file LICENSE_1_0.txt or copy at                         *
#  *     http://www.boost.org/LICENSE_1_0.txt)                                *
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_TUPLE_DETAIL_IS_SINGLE_RETURN_HPP
# define FB_BOOST_PREPROCESSOR_TUPLE_DETAIL_IS_SINGLE_RETURN_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
#
# /* FB_BOOST_PP_TUPLE_IS_SINGLE_RETURN */
#
# if FB_BOOST_PP_VARIADICS_MSVC
# include <firebird/impl/boost/preprocessor/control/iif.hpp>
# include <firebird/impl/boost/preprocessor/facilities/is_1.hpp>
# include <firebird/impl/boost/preprocessor/tuple/size.hpp>
# define FB_BOOST_PP_TUPLE_IS_SINGLE_RETURN(sr,nsr,tuple)  \
    FB_BOOST_PP_IIF(FB_BOOST_PP_IS_1(FB_BOOST_PP_TUPLE_SIZE(tuple)),sr,nsr) \
    /**/
# endif /* FB_BOOST_PP_VARIADICS_MSVC */
#
# endif /* FB_BOOST_PREPROCESSOR_TUPLE_DETAIL_IS_SINGLE_RETURN_HPP */
