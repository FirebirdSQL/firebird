/*
 **************************************************************************
 *
 *	PROGRAM:		JRD file split utility program
 *	MODULE:			spit.h
 *	DESCRIPTION:	file split utility program main header file
 *
 *
 **************************************************************************
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

inline constexpr char BLANK					= ' ';
inline constexpr int FILE_IS_FULL			= -9;

inline constexpr int K_BYTES				= 1024;
inline constexpr int M_BYTES				= 1024 * K_BYTES;
inline constexpr int G_BYTES				= 1024 * M_BYTES;
inline constexpr int IO_BUFFER_SIZE			= 16 * K_BYTES;
inline constexpr int SVC_IO_BUFFER_SIZE		= 16 * IO_BUFFER_SIZE;
inline constexpr int GBAK_IO_BUFFER_SIZE	= SVC_IO_BUFFER_SIZE;

inline constexpr size_t MAX_FILE_NM_LEN		= 27;	// size of header_rec.fl_name
inline constexpr int MAX_NUM_OF_FILES		= 9999;
inline constexpr int MIN_FILE_SIZE			= M_BYTES;
inline constexpr char NEW_LINE				= '\n';
inline constexpr char TERMINAL				= '\0';
