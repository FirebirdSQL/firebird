/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pio_proto.h
 *	DESCRIPTION:	Prototype header file for unix.cpp, vms.cpp & winnt.cpp
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

#ifndef JRD_PIO_PROTO_H
#define JRD_PIO_PROTO_H

#include "../common/classes/fb_string.h"

namespace Firebird::Jrd {
	class jrd_file;
	class Database;
	class BufferDesc;
}

namespace Firebird::Jrd::Ods {
	struct pag;
}

namespace Firebird::Jrd
{
	void	PIO_close(jrd_file*);
	jrd_file*	PIO_create(thread_db*, const PathName&,
								const bool, const bool);
	bool	PIO_expand(const TEXT*, USHORT, TEXT*, FB_SIZE_T);
	bool	PIO_fast_extension_is_supported(const jrd_file& file) noexcept;
	bool	PIO_extend(thread_db* tdbb, jrd_file* file, ULONG extPages, USHORT pageSize);
	void	PIO_flush(thread_db*, jrd_file*);
	void	PIO_force_write(jrd_file*, const bool);
	ULONG	PIO_get_number_of_pages(const jrd_file*, const USHORT);
	bool	PIO_header(thread_db*, UCHAR*, unsigned);
	USHORT	PIO_init_data(thread_db* tdbb, jrd_file* file, FbStatusVector* status_vector, ULONG startPage, USHORT initPages);
	jrd_file*	PIO_open(thread_db*, const PathName&,
							const PathName&);
	bool	PIO_read(thread_db*, jrd_file*, BufferDesc*, Ods::pag*, FbStatusVector*);

	#ifdef SUPERSERVER_V2
	bool	PIO_read_ahead(thread_db*, SLONG, SCHAR*, SLONG,
					struct phys_io_blk*, FbStatusVector*);
	bool	PIO_status(thread_db*, struct phys_io_blk*, FbStatusVector*);
	#endif

	#ifdef SUPPORT_RAW_DEVICES
	bool	PIO_on_raw_device(const PathName&);
	int		PIO_unlink(const PathName&);
	#else
	inline bool PIO_on_raw_device(const PathName&)
	{
		return false;
	}
	#endif
	bool	PIO_write(thread_db*, jrd_file*, BufferDesc*, Ods::pag*, FbStatusVector*);
}	// namespace Firebird::Jrd

#endif // JRD_PIO_PROTO_H
