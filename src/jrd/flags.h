/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		flags.h
 *	DESCRIPTION:	Various flag definitions
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

#ifndef JRD_FLAGS_H
#define JRD_FLAGS_H

// flags for RDB$FILE_FLAGS. Do not change as they are part of backups

inline constexpr USHORT FILE_shadow			= 1;
inline constexpr USHORT FILE_inactive		= 2;
inline constexpr USHORT FILE_manual			= 4;
inline constexpr USHORT FILE_conditional 	= 16;
inline constexpr USHORT FILE_nodelete		= 32;

// Flags for backup difference files
// File is difference
inline constexpr USHORT FILE_difference		= 32;
// Actively used for backup purposes (ALTER DATABASE BEGIN BACKUP issued)
inline constexpr USHORT FILE_backing_up		= 64;


// flags for RDB$RELATIONS

inline constexpr USHORT REL_sql			= 0x0001;

// flags for RDB$TRIGGERS

inline constexpr USHORT TRG_sql			= 0x1;
inline constexpr USHORT TRG_ignore_perm	= 0x2;		// trigger ignores permissions checks

#endif // JRD_FLAGS_H
