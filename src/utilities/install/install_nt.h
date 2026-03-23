/*
 *	PROGRAM:	Windows NT installation utilities
 *	MODULE:		install_nt.h
 *	DESCRIPTION:	Defines for Windows NT installation utilities
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
 */

#ifndef UTILITIES_INSTALL_NT_H
#define UTILITIES_INSTALL_NT_H

static inline constexpr const char* REMOTE_SERVICE			= "FirebirdServer%s";
static inline constexpr const char* REMOTE_DISPLAY_NAME		= "Firebird Server - %s";
static inline constexpr const char* REMOTE_DISPLAY_DESCR	= "Firebird Database Server - www.firebirdsql.org";
static inline constexpr const char* REMOTE_EXECUTABLE		= "firebird";

static inline constexpr const char* ISCGUARD_SERVICE		= "FirebirdGuardian%s";
static inline constexpr const char* ISCGUARD_DISPLAY_NAME	= "Firebird Guardian - %s";
static inline constexpr const char* ISCGUARD_DISPLAY_DESCR	= "Firebird Server Guardian - www.firebirdsql.org";
static inline constexpr const char* ISCGUARD_EXECUTABLE		= "fbguard";

static inline constexpr const char* SERVER_MUTEX			= "FirebirdServerMutex%s";
static inline constexpr const char* GUARDIAN_MUTEX			= "FirebirdGuardianMutex%s";

static inline constexpr const char* FB_DEFAULT_INSTANCE	= "DefaultInstance";

// Starting with 128 the service params are user defined
inline constexpr DWORD SERVICE_CREATE_GUARDIAN_MUTEX	= 128;
//#define REMOTE_DEPENDENCIES		"Tcpip\0\0"

// sw_command
inline constexpr USHORT COMMAND_NONE	= 0;
inline constexpr USHORT COMMAND_INSTALL	= 1;
inline constexpr USHORT COMMAND_REMOVE	= 2;
inline constexpr USHORT COMMAND_START	= 3;
inline constexpr USHORT COMMAND_STOP	= 4;
//inline constexpr USHORT COMMAND_CONFIG	= 5;
inline constexpr USHORT COMMAND_QUERY	= 6;

// sw_startup
inline constexpr USHORT STARTUP_DEMAND	= 0;
inline constexpr USHORT STARTUP_AUTO	= 1;

// sw_guardian
inline constexpr USHORT NO_GUARDIAN		= 0;
inline constexpr USHORT USE_GUARDIAN	= 1;

// sw_mode
inline constexpr USHORT DEFAULT_PRIORITY	= 0;
inline constexpr USHORT NORMAL_PRIORITY		= 1;
inline constexpr USHORT HIGH_PRIORITY		= 2;

// sw_arch
inline constexpr USHORT ARCH_SS = 0;
inline constexpr USHORT ARCH_CS = 1;

// sw_client
inline constexpr USHORT CLIENT_NONE	= 0;
inline constexpr USHORT CLIENT_FB	= 1;
inline constexpr USHORT CLIENT_GDS	= 2;

static inline constexpr const char* GDS32_NAME		= "GDS32.DLL";
static inline constexpr const char* FBCLIENT_NAME	= "FBCLIENT.DLL";

// instsvc status codes
inline constexpr USHORT IB_SERVICE_ALREADY_DEFINED		= 100;
inline constexpr USHORT IB_SERVICE_RUNNING				= 101;
inline constexpr USHORT FB_PRIVILEGE_ALREADY_GRANTED	= 102;
inline constexpr USHORT FB_SERVICE_STATUS_RUNNING		= 100;
inline constexpr USHORT FB_SERVICE_STATUS_STOPPED		= 111;
inline constexpr USHORT FB_SERVICE_STATUS_PENDING		= 112;
inline constexpr USHORT FB_SERVICE_STATUS_NOT_INSTALLED	= 113;
inline constexpr USHORT FB_SERVICE_STATUS_UNKNOWN		= 114;

// instclient status codes
inline constexpr USHORT FB_INSTALL_COPY_REQUIRES_REBOOT	= 200;
inline constexpr USHORT FB_INSTALL_SAME_VERSION_FOUND	= 201;
inline constexpr USHORT FB_INSTALL_NEWER_VERSION_FOUND	= 202;
inline constexpr USHORT FB_INSTALL_FILE_NOT_FOUND 		= 203;
inline constexpr USHORT FB_INSTALL_CANT_REMOVE_ALIEN_VERSION	= 204;
inline constexpr USHORT FB_INSTALL_FILE_PROBABLY_IN_USE	= 205;
inline constexpr USHORT FB_INSTALL_SHARED_COUNT_ZERO  	= 206;

#endif // UTILITIES_INSTALL_NT_H
