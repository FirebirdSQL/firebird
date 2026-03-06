/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		idx_proto.h
 *	DESCRIPTION:	Prototype header file for idx.cpp
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

#ifndef JRD_IDX_PROTO_H
#define JRD_IDX_PROTO_H

#include "../jrd/btr.h"
#include "../jrd/exe.h"
#include "../jrd/req.h"

namespace Firebird::Jrd
{
	class jrd_rel;
	class jrd_tra;
	struct record_param;
	struct index_desc;
	class CompilerScratch;
	class thread_db;

	bool IDX_activate_index(thread_db*, Cached::Relation*, MetaId);
	void IDX_check_access(thread_db*, CompilerScratch*, Cached::Relation*, Cached::Relation*);
	bool IDX_check_master_types (thread_db*, index_desc&, Cached::Relation*, int&);
	void IDX_create_index(thread_db*, IdxCreate createMethod, jrd_rel*, index_desc*, const QualifiedName&,
						USHORT*, jrd_tra*, SelectivityList&);
	void IDX_mark_index(thread_db*, Cached::Relation*, MetaId);
	void IDX_delete_indices(thread_db*, RelationPermanent*, RelationPages*, bool);
	void IDX_mark_indices(thread_db*, Cached::Relation*);
	void IDX_erase(thread_db*, record_param*, jrd_tra*);
	void IDX_garbage_collect(thread_db*, record_param*, RecordStack&, RecordStack&);
	void IDX_modify(thread_db*, record_param*, record_param*, jrd_tra*);
	void IDX_modify_check_constraints(thread_db*, record_param*, record_param*, jrd_tra*);
	void IDX_statistics(thread_db*, Cached::Relation*, USHORT, SelectivityList&);
	void IDX_store(thread_db*, record_param*, jrd_tra*);
	void IDX_modify_flag_uk_modified(thread_db*, record_param*, record_param*, jrd_tra*);
} // namespace Firebird::Jrd


#endif // JRD_IDX_PROTO_H
