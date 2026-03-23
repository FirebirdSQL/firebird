/* 	PROGRAM:	JRD Access Method
 *	MODULE:		vio_debug.h
 *	DESCRIPTION:	Definitions for tracing VIO activity
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

#ifndef JRD_VIO_DEBUG_H
#define JRD_VIO_DEBUG_H

//#define VIO_DEBUG

inline constexpr int DEBUG_WRITES			= 1;
inline constexpr int DEBUG_WRITES_INFO		= 2;
inline constexpr int DEBUG_READS			= 3;
inline constexpr int DEBUG_READS_INFO		= 4;
inline constexpr int DEBUG_TRACE			= 5;
inline constexpr int DEBUG_TRACE_INFO		= 6;
inline constexpr int DEBUG_TRACE_ALL		= 7;
inline constexpr int DEBUG_TRACE_ALL_INFO	= 8;

// defined at vio.cpp
extern int vio_debug_flag;
void VIO_trace(int level, const char* format, ...);

#endif /* JRD_VIO_DEBUG_H */

