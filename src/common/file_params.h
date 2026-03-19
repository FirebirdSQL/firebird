/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		file_params.h
 *	DESCRIPTION:	File parameter definitions
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "EPSON" define*
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 * 2002.10.30 Sean Leyne - Code Cleanup, removed obsolete "SUN3_3" port
 *
 */

#ifndef COMMON_FILE_PARAMS_H
#define COMMON_FILE_PARAMS_H

// Derived from Firebird major version
#define COMMON_FILE_PREFIX "60"

// Per-database usage
static inline constexpr const char* EVENT_FILE		= "fb_event_%s";
static inline constexpr const char* LOCK_FILE		= "fb_lock_%s";
static inline constexpr const char* MONITOR_FILE	= "fb_monitor_%s";
static inline constexpr const char* REPL_FILE 		= "fb_repl_%s";
static inline constexpr const char* TPC_HDR_FILE	= "fb_tpc_%s";
static inline constexpr const char* TPC_BLOCK_FILE	= "fb_tpc_%s_%" UQUADFORMAT;
static inline constexpr const char* SNAPSHOTS_FILE	= "fb_snap_%s";
static inline constexpr const char* PROFILER_FILE	= "fb_profiler_%s_%" UQUADFORMAT;
static inline constexpr const char* IPC_CHAT_CLIENT_FILE = "fb_ipc_chat_%" UQUADFORMAT "_%" UQUADFORMAT;

// Global usage
static inline constexpr const char* TRACE_FILE		= "fb" COMMON_FILE_PREFIX "_trace";
static inline constexpr const char* USER_MAP_FILE	= "fb" COMMON_FILE_PREFIX "_user_mapping";
static inline constexpr const char* SHARED_EVENT	= "fb" COMMON_FILE_PREFIX "_process%u_signal%d";

// Per-log file usage (for audit logging)
static inline constexpr const char* FB_TRACE_LOG_MUTEX = "fb_trace_log_mutex";

// Per-trace session usage (for interactive trace)
static inline constexpr const char* FB_TRACE_FILE = "fb_trace.";


#ifdef UNIX
static inline constexpr const char* INIT_FILE	= "fb_init";
static inline constexpr const char* SEM_FILE	= "fb_sem";
static inline constexpr const char* PORT_FILE	= "fb_port_%d";
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

#ifdef DARWIN
#undef FB_PREFIX
#define FB_PREFIX		"/all/files/are/in/framework/resources"
#define DARWIN_GEN_DIR		"var"
#define DARWIN_FRAMEWORK_ID	"com.firebirdsql.Firebird"
#endif

// keep MSG_FILE_LANG in sync with build_file.epp
#if defined(WIN_NT)
static inline constexpr const char* WORKFILE	= "c:\\temp\\";
static inline constexpr char MSG_FILE_LANG[]	= "intl\\%.10s.msg";
#elif defined(ANDROID)
static inline constexpr const char* WORKFILE	= "/data/local/tmp/";
static inline constexpr char MSG_FILE_LANG[]	= "intl/%.10s.msg";
#else
static inline constexpr const char* WORKFILE	= "/tmp/";
static inline constexpr char MSG_FILE_LANG[]	= "intl/%.10s.msg";
#endif

static inline constexpr const char* LOCKDIR		= "firebird";		// created in WORKFILE
static inline constexpr const char* LOGFILE		= FB_LOGFILENAME;
static inline constexpr const char* MSG_FILE	= "firebird.msg";
static inline constexpr const char* SECURITY_DB	= "security6.fdb";

// Keep in sync with MSG_FILE_LANG
inline constexpr int LOCALE_MAX	= 10;

#endif // COMMON_FILE_PARAMS_H
