/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		vio_proto.h
 *	DESCRIPTION:	Prototype header file for vio.cpp
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
 * 2002.10.21 Nickolay Samofatov: Added support for explicit pessimistic locks
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 */

#ifndef JRD_VIO_PROTO_H
#define JRD_VIO_PROTO_H

namespace Jrd
{
	class jrd_rel;
	class jrd_tra;
	class Record;
	class RecordSource;
	struct record_param;
	class Savepoint;
	class Format;
	class TraceSweepEvent;

	enum FindNextRecordScope
	{
		DPM_next_all,			// all pages
		DPM_next_data_page,		// one data page only
		DPM_next_pointer_page	// data pages from one pointer page
	};

	enum class WriteLockResult
	{
		LOCKED,
		CONFLICTED,
		SKIPPED
	};
}

void	VIO_backout(Jrd::thread_db*, Jrd::record_param*, const Jrd::jrd_tra*);
bool	VIO_chase_record_version(Jrd::thread_db*, Jrd::record_param*,
									Jrd::jrd_tra*, MemoryPool*, bool, bool);
void	VIO_copy_record(Jrd::thread_db*, Jrd::jrd_rel*, Jrd::Record*, Jrd::Record*);
void	VIO_data(Jrd::thread_db*, Jrd::record_param*, MemoryPool*);
bool	VIO_erase(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*);
void	VIO_fini(Jrd::thread_db*);
bool	VIO_garbage_collect(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*);
Jrd::Record*	VIO_gc_record(Jrd::thread_db*, Jrd::jrd_rel*);
bool	VIO_get(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*, MemoryPool*);
bool	VIO_get_current(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*,
						MemoryPool*, bool, bool&);
void	VIO_init(Jrd::thread_db*);
Jrd::WriteLockResult VIO_writelock(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*, bool skipLocked);
bool	VIO_modify(Jrd::thread_db*, Jrd::record_param*, Jrd::record_param*, Jrd::jrd_tra*);
bool	VIO_next_record(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*, MemoryPool*, Jrd::FindNextRecordScope);
Jrd::Record*	VIO_record(Jrd::thread_db*, Jrd::record_param*, const Jrd::Format*, MemoryPool*);
bool	VIO_refetch_record(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*, bool, bool);
void	VIO_store(Jrd::thread_db*, Jrd::record_param*, Jrd::jrd_tra*);
bool	VIO_sweep(Jrd::thread_db*, Jrd::jrd_tra*, Jrd::TraceSweepEvent*);
void	VIO_intermediate_gc(Jrd::thread_db* tdbb, Jrd::record_param* rpb, Jrd::jrd_tra* transaction);
void	VIO_garbage_collect_idx(Jrd::thread_db*, Jrd::jrd_tra*, Jrd::record_param*, Jrd::Record*);
void	VIO_update_in_place(Jrd::thread_db*, Jrd::jrd_tra*, Jrd::record_param*, Jrd::record_param*);

#endif // JRD_VIO_PROTO_H
