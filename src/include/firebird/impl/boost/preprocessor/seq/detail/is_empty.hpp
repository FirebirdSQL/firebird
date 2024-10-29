# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2015.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef FB_BOOST_PREPROCESSOR_SEQ_DETAIL_IS_EMPTY_HPP
# define FB_BOOST_PREPROCESSOR_SEQ_DETAIL_IS_EMPTY_HPP
#
# include <firebird/impl/boost/preprocessor/config/config.hpp>
# include <firebird/impl/boost/preprocessor/arithmetic/dec.hpp>
# include <firebird/impl/boost/preprocessor/logical/bool.hpp>
# include <firebird/impl/boost/preprocessor/logical/compl.hpp>
# include <firebird/impl/boost/preprocessor/seq/size.hpp>
#
/* An empty seq is one that is just FB_BOOST_PP_SEQ_NIL */
#
# define FB_BOOST_PP_SEQ_DETAIL_IS_EMPTY(seq) \
    FB_BOOST_PP_COMPL \
        ( \
        FB_BOOST_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq) \
        ) \
/**/
#
# define FB_BOOST_PP_SEQ_DETAIL_IS_EMPTY_SIZE(size) \
    FB_BOOST_PP_COMPL \
        ( \
        FB_BOOST_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(size) \
        ) \
/**/
#
# define FB_BOOST_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq) \
    FB_BOOST_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(FB_BOOST_PP_SEQ_DETAIL_EMPTY_SIZE(seq)) \
/**/
#
# define FB_BOOST_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(size) \
    FB_BOOST_PP_BOOL(size) \
/**/
#
# define FB_BOOST_PP_SEQ_DETAIL_EMPTY_SIZE(seq) \
    FB_BOOST_PP_DEC(FB_BOOST_PP_SEQ_SIZE(seq (nil))) \
/**/
#
# endif
