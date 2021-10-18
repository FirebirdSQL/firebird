/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 */

#ifndef FIREBIRD_IBERROR_H
#define FIREBIRD_IBERROR_H

#include "firebird/impl/msg_helper.h"

#ifdef __cplusplus /* c++ definitions */

const ISC_STATUS isc_facility = 20;
const ISC_STATUS isc_base = isc_facility << 24;
const ISC_STATUS isc_factor = 1;

const ISC_STATUS isc_arg_end			= 0;	// end of argument list
const ISC_STATUS isc_arg_gds			= 1;	// generic DSRI status value
const ISC_STATUS isc_arg_string			= 2;	// string argument
const ISC_STATUS isc_arg_cstring		= 3;	// count & string argument
const ISC_STATUS isc_arg_number			= 4;	// numeric argument (long)
const ISC_STATUS isc_arg_interpreted	= 5;	// interpreted status code (string)
const ISC_STATUS isc_arg_vms			= 6;	// VAX/VMS status code (long)
const ISC_STATUS isc_arg_unix			= 7;	// UNIX error code
const ISC_STATUS isc_arg_domain			= 8;	// Apollo/Domain error code
const ISC_STATUS isc_arg_dos			= 9;	// MSDOS/OS2 error code
const ISC_STATUS isc_arg_mpexl			= 10;	// HP MPE/XL error code
const ISC_STATUS isc_arg_mpexl_ipc		= 11;	// HP MPE/XL IPC error code
const ISC_STATUS isc_arg_next_mach		= 15;	// NeXT/Mach error code
const ISC_STATUS isc_arg_netware		= 16;	// NetWare error code
const ISC_STATUS isc_arg_win32			= 17;	// Win32 error code
const ISC_STATUS isc_arg_warning		= 18;	// warning argument
const ISC_STATUS isc_arg_sql_state		= 19;	// SQLSTATE

#define FB_IMPL_MSG_NO_SYMBOL(facility, number, text)

#define FB_IMPL_MSG_SYMBOL(facility, number, symbol, text) \
	const ISC_STATUS isc_##symbol = FB_IMPL_MSG_ENCODE(number, FB_IMPL_MSG_FACILITY_##facility);

#define FB_IMPL_MSG(facility, number, symbol, sqlCode, sqlClass, sqlSubClass, text)	\
	FB_IMPL_MSG_SYMBOL(facility, number, symbol, text)

#include "firebird/impl/msg/all.h"

#undef FB_IMPL_MSG_NO_SYMBOL
#undef FB_IMPL_MSG_SYMBOL
#undef FB_IMPL_MSG

const ISC_STATUS isc_err_max = 0
	#define FB_IMPL_MSG_NO_SYMBOL(facility, number, text)
	#define FB_IMPL_MSG_SYMBOL(facility, number, symbol, text)
	#define FB_IMPL_MSG(facility, number, symbol, sqlCode, sqlClass, sqlSubClass, text) + 1

	#include "firebird/impl/msg/all.h"

	#undef FB_IMPL_MSG_NO_SYMBOL
	#undef FB_IMPL_MSG_SYMBOL
	#undef FB_IMPL_MSG
	;


#else /* c definitions */

#define isc_facility 20
#define isc_base (isc_facility << 24)
#define isc_factor 1

#define isc_arg_end			0	/* end of argument list */
#define isc_arg_gds			1	/* generic DSRI status value */
#define isc_arg_string		2	/* string argument */
#define isc_arg_cstring		3	/* count & string argument */
#define isc_arg_number		4	/* numeric argument (long) */
#define isc_arg_interpreted	5	/* interpreted status code (string) */
#define isc_arg_vms			6	/* VAX/VMS status code (long) */
#define isc_arg_unix		7	/* UNIX error code */
#define isc_arg_domain		8	/* Apollo/Domain error code */
#define isc_arg_dos			9	/* MSDOS/OS2 error code */
#define isc_arg_mpexl		10	/* HP MPE/XL error code */
#define isc_arg_mpexl_ipc	11	/* HP MPE/XL IPC error code */
#define isc_arg_next_mach	15	/* NeXT/Mach error code */
#define isc_arg_netware		16	/* NetWare error code */
#define isc_arg_win32		17	/* Win32 error code */
#define isc_arg_warning		18	/* warning argument */
#define isc_arg_sql_state	19	/* SQLSTATE */

#include "firebird/impl/iberror_c.h"

#endif

#endif /* FIREBIRD_IBERROR_H */
