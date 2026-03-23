/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pio.h
 *	DESCRIPTION:	File system interface definitions
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
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef JRD_PIO_H
#define JRD_PIO_H

#include "../include/fb_blk.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/array.h"
#include "../common/classes/File.h"

namespace Jrd {

#ifdef UNIX

class jrd_file : public pool_alloc_rpt<SCHAR, type_fil>
{
public:
	int fil_desc;
	Firebird::Mutex fil_mutex;
	USHORT fil_flags;
	SCHAR fil_string[1];		// Expanded file name
};

#endif


#ifdef WIN_NT
#ifdef SUPERSERVER_V2
inline constexpr int MAX_FILE_IO = 32;	// Maximum "allocated" overlapped I/O events
#endif

class jrd_file : public pool_alloc_rpt<SCHAR, type_fil>
{
public:

	~jrd_file()
	{
		delete fil_ext_lock;
	}

	HANDLE fil_desc;					// File descriptor
	Firebird::RWLock* fil_ext_lock;		// file extend lock
	USHORT fil_flags;
	SCHAR fil_string[1];				// Expanded file name
};

#endif


inline constexpr USHORT FIL_force_write		= 1;
inline constexpr USHORT FIL_no_fs_cache		= 2;	// not using file system cache
inline constexpr USHORT FIL_readonly		= 4;	// file opened in readonly mode
inline constexpr USHORT FIL_sh_write		= 8;	// file opened in shared write mode
inline constexpr USHORT FIL_no_fast_extend	= 16;	// file not supports fast extending
inline constexpr USHORT FIL_raw_device		= 32;	// file is raw device

// Physical IO trace events

inline constexpr SSHORT trace_create	= 1;
inline constexpr SSHORT trace_open		= 2;
inline constexpr SSHORT trace_page_size	= 3;
inline constexpr SSHORT trace_read		= 4;
inline constexpr SSHORT trace_write		= 5;
inline constexpr SSHORT trace_close		= 6;

// Physical I/O status block, used only in SS v2 for Win32

#ifdef SUPERSERVER_V2
struct phys_io_blk
{
	jrd_file* piob_file;		// File being read/written
	SLONG piob_desc;			// File descriptor
	SLONG piob_io_length;		// Requested I/O transfer length
	SLONG piob_actual_length;	// Actual I/O transfer length
	USHORT piob_wait;			// Async or synchronous wait
	UCHAR piob_flags;
	SLONG piob_io_event[8];		// Event to signal I/O completion
};

// piob_flags
inline constexpr UCHAR PIOB_error	= 1;	// I/O error occurred
inline constexpr UCHAR PIOB_success	= 2;	// I/O successfully completed
inline constexpr UCHAR PIOB_pending	= 4;	// Asynchronous I/O not yet completed
#endif

} //namespace Jrd

#endif // JRD_PIO_H

