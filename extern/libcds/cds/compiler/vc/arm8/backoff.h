// Copyright (c) 2006-2018 Maxim Khizhinsky
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef CDSLIB_COMPILER_VC_ARM8_BACKOFF_H
#define CDSLIB_COMPILER_VC_ARM8_BACKOFF_H

//@cond none
#include <intrin.h>

namespace cds { namespace backoff {
    namespace vc { namespace arm8 {

#       define CDS_backoff_hint_defined
        static inline void backoff_hint()
        {
            __yield();
        }

#       define CDS_backoff_nop_defined
        static inline void backoff_nop()
        {
            __nop();
        }

    }} // namespace vc::arm8

    namespace platform {
        using namespace vc::arm8;
    }
}}  // namespace cds::backoff

//@endcond
#endif  // #ifndef CDSLIB_COMPILER_VC_ARM8_BACKOFF_H
