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

namespace Firebird::Jrd
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

	void	VIO_backout(thread_db*, record_param*, const jrd_tra*);
	bool	VIO_chase_record_version(thread_db*, record_param*,
										jrd_tra*, MemoryPool*, bool, bool);
	void	VIO_copy_record(thread_db*, jrd_rel*, Record*, Record*);
	void	VIO_data(thread_db*, record_param*, MemoryPool*);
	bool	VIO_erase(thread_db*, record_param*, jrd_tra*);
	void	VIO_fini(thread_db*);
	bool	VIO_garbage_collect(thread_db*, record_param*, jrd_tra*);
	bool	VIO_get(thread_db*, record_param*, jrd_tra*, MemoryPool*);
	bool	VIO_get_current(thread_db*, record_param*, jrd_tra*,
							MemoryPool*, bool, bool&);
	void	VIO_init(thread_db*);
	WriteLockResult VIO_writelock(thread_db*, record_param*, jrd_tra*);
	bool	VIO_modify(thread_db*, record_param*, record_param*, jrd_tra*);
	bool	VIO_next_record(thread_db*, record_param*, jrd_tra*, MemoryPool*,
							FindNextRecordScope, const RecordNumber* = nullptr);
	Record*	VIO_record(thread_db*, record_param*, const Format*, MemoryPool*);
	bool	VIO_refetch_record(thread_db*, record_param*, jrd_tra*, bool, bool);
	void	VIO_store(thread_db*, record_param*, jrd_tra*);
	bool	VIO_sweep(thread_db*, jrd_tra*, TraceSweepEvent*);
	void	VIO_intermediate_gc(thread_db* tdbb, record_param* rpb, jrd_tra* transaction);
	void	VIO_garbage_collect_idx(thread_db*, jrd_tra*, record_param*, Record*);
	void	VIO_update_in_place(thread_db*, jrd_tra*, record_param*, record_param*);
}	// namespace Firebird::Jrd

#endif // JRD_VIO_PROTO_H
