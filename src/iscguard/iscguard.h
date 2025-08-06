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
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */
#ifndef ISCGUARD_H
#define ISCGUARD_H

static inline constexpr const char* GUARDIAN_APP_NAME	= "Firebird Guardian";
static inline constexpr const char* GUARDIAN_APP_LABEL	= "Firebird Guardian";
static inline constexpr const char* GUARDIAN_CLASS_NAME	= "FB_Guard";
static inline constexpr const char* FBSERVER			= "firebird.exe";

// Help Constants
inline constexpr DWORD ibs_server_directory	= 8060;
inline constexpr DWORD ibs_guard_version	= 8080;
inline constexpr DWORD ibs_guard_log		= 8090;

#define WM_SWITCHICONS  WM_USER + 3

inline constexpr short START_ONCE		= 0;
inline constexpr short START_FOREVER	= 1;

inline constexpr DWORD NORMAL_EXIT		= 0;
inline constexpr DWORD CRASHED			= (DWORD) -1;

typedef void (*FPTR_VOID) ();

struct log_info
{
	char log_action[25];
	char log_date[25];
	char log_time[25];
	log_info* next;
};

#endif // ISCGUARD_H
