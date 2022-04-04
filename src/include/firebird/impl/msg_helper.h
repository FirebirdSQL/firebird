/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2021 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef FIREBIRD_IMPL_MSG_HELPER_H
#define FIREBIRD_IMPL_MSG_HELPER_H

#define FB_IMPL_MSG_FACILITY_JRD 0
#define FB_IMPL_MSG_FACILITY_QLI 1
#define FB_IMPL_MSG_FACILITY_GFIX 3
#define FB_IMPL_MSG_FACILITY_GPRE 4
#define FB_IMPL_MSG_FACILITY_DSQL 7
#define FB_IMPL_MSG_FACILITY_DYN 8
#define FB_IMPL_MSG_FACILITY_INSTALL 10
#define FB_IMPL_MSG_FACILITY_TEST 11
#define FB_IMPL_MSG_FACILITY_GBAK 12
#define FB_IMPL_MSG_FACILITY_SQLERR 13
#define FB_IMPL_MSG_FACILITY_SQLWARN 14
#define FB_IMPL_MSG_FACILITY_JRD_BUGCHK 15
#define FB_IMPL_MSG_FACILITY_ISQL 17
#define FB_IMPL_MSG_FACILITY_GSEC 18
#define FB_IMPL_MSG_FACILITY_GSTAT 21
#define FB_IMPL_MSG_FACILITY_FBSVCMGR 22
#define FB_IMPL_MSG_FACILITY_UTL 23
#define FB_IMPL_MSG_FACILITY_NBACKUP 24
#define FB_IMPL_MSG_FACILITY_FBTRACEMGR 25
#define FB_IMPL_MSG_FACILITY_JAYBIRD 26
#define FB_IMPL_MSG_FACILITY_R2DBC_FIREBIRD 27

#define FB_IMPL_MSG_MASK ((ISC_STATUS) 0x14000000)	/* Defines the code as a valid ISC code */

#define FB_IMPL_MSG_ENCODE(code, facility) \
	((ISC_STATUS(((ISC_USHORT) facility) & 0x1F) << 16) | (((ISC_STATUS) code) & 0x3FFF) | FB_IMPL_MSG_MASK)

#endif	/* FIREBIRD_IMPL_MSG_HELPER_H */
