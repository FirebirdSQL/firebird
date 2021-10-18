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
 *  The Original Code was created by Vladyslav Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2019 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_VERSION_H
#define JRD_VERSION_H

#define STRINGIZE_AUX(x)	#x
#define STRINGIZE(x)		STRINGIZE_AUX(x)


#ifdef RC_TARGET_chacha
#define VER_FILEDESC "Wire Encryption plugin using ChaCha cypher"

#elif RC_TARGET_engine13
#define VER_FILEDESC "Engine plugin"

#elif defined RC_TARGET_fb_lock_print
#define VER_FILEDESC "Lock Print tool"

#elif defined RC_TARGET_fbguard
#define VER_FILEDESC "Guardian"

#elif defined RC_TARGET_fbrmclib
#define VER_FILEDESC "RM/COBOL Helper library"

#elif defined RC_TARGET_firebird
#define VER_FILEDESC "Server executable"

#elif defined RC_TARGET_fbsvcmgr
#define VER_FILEDESC "Services Management tool"

#elif defined RC_TARGET_fbtrace
#define VER_FILEDESC "Trace plugin"

#elif defined RC_TARGET_fbtracemgr
#define VER_FILEDESC "Trace Management tool"

#elif defined RC_TARGET_gbak
#define VER_FILEDESC "Gbak tool"

#elif defined RC_TARGET_gfix
#define VER_FILEDESC "Gfix tool"

#elif defined RC_TARGET_gpre
#define VER_FILEDESC "Gpre tool"

#elif defined RC_TARGET_gsec
#define VER_FILEDESC "Gsec tool"

#elif defined RC_TARGET_gsplit
#define VER_FILEDESC "Gsplit tool"

#elif defined RC_TARGET_gstat
#define VER_FILEDESC "Gstat tool"

#elif defined RC_TARGET_ib_util
#define VER_FILEDESC "UDF Helper library"

#elif defined RC_TARGET_instclient
#define VER_FILEDESC "Install Client tool"

#elif defined RC_TARGET_instreg
#define VER_FILEDESC "Install Registry tool"

#elif defined RC_TARGET_instsvc
#define VER_FILEDESC "Install Service tool"

#elif defined RC_TARGET_fbintl
#define VER_FILEDESC "International Characters support library"

#elif defined RC_TARGET_isql
#define VER_FILEDESC "Interactive Query tool"

#elif defined RC_TARGET_legacy_auth
#define VER_FILEDESC "Legacy Auth plugin"

#elif defined RC_TARGET_legacy_usermanager
#define VER_FILEDESC "Legacy User Manager plugin"

#elif defined RC_TARGET_nbackup
#define VER_FILEDESC "Physical Backup Management tool"

#elif defined RC_TARGET_srp
#define VER_FILEDESC "SRP User Manager plugin"

#elif defined RC_TARGET_udf_compat
#define VER_FILEDESC "UDF compatibility library"

#elif defined RC_TARGET_udr_engine
#define VER_FILEDESC "User Defined Routines engine"

#elif defined RC_TARGET_fbclient
#define VER_FILEDESC "Client library"

#elif defined RC_TARGET_build_msg
#define VER_FILEDESC "Build Message File tool"

#elif defined RC_TARGET_codes
#define VER_FILEDESC "Generate Error Codes tool"

#elif defined RC_TARGET_gpre_boot
#define VER_FILEDESC "Bootstrap Gpre tool"

#elif defined RC_TARGET_udrcpp_example
#define VER_FILEDESC "UDR C++ example"

#elif defined RC_TARGET_fbudf
#define VER_FILEDESC "UDF library"

#elif defined RC_TARGET_ib_udf
#define VER_FILEDESC "UDF library"

#else
#define VER_FILEDESC "SQL Server"

#endif


#ifdef NDEBUG
#define VER_DBG
#else
#define VER_DBG " debug"
#endif


#ifdef RC_ARH_x64
#define VER_ARCH "64-bit"
#else
#define VER_ARCH "32-bit"
#endif

#endif // JRD_VERSION_H
