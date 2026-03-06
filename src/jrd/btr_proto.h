/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		btr_proto.h
 *	DESCRIPTION:	Prototype header file for btr.cpp
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

#ifndef JRD_BTR_PROTO_H
#define JRD_BTR_PROTO_H

#include "../jrd/btr.h"
#include "../jrd/ods.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/QualifiedName.h"

namespace Firebird::Jrd
{
	void	BTR_all(thread_db*, Cached::Relation*, IndexDescList&, RelationPages*);
	bool	BTR_activate_index(thread_db*, Cached::Relation*, MetaId);
	bool	BTR_cleanup_index(thread_db*, const QualifiedName&, jrd_tra*, MetaId);
	void	BTR_complement_key(temporary_key*);
	void	BTR_create(thread_db*, IndexCreation&, SelectivityList&);
	bool	BTR_delete_index(thread_db*, win*, MetaId, bool);
	bool	BTR_description(thread_db*, Cached::Relation*, const Ods::index_root_page*, index_desc*,
							MetaId, bool raise = true);
	DSC*	BTR_eval_expression(thread_db*, index_desc*, Record*);
	void	BTR_evaluate(thread_db*, const IndexRetrieval*, RecordBitmap**, RecordBitmap*);
	UCHAR*	BTR_find_leaf(Ods::btree_page*, temporary_key*, UCHAR*, USHORT*, bool, int);
	Ods::btree_page*	BTR_find_page(thread_db*, const IndexRetrieval*, win*, index_desc*,
		temporary_key*, temporary_key*);
	void	BTR_insert(thread_db*, win*, index_insertion*);
	USHORT	BTR_key_length(thread_db*, jrd_rel*, index_desc*);
	Ods::btree_page*	BTR_left_handoff(thread_db*, win*, Ods::btree_page*, SSHORT);
	bool	BTR_lookup(thread_db*, Cached::Relation*, MetaId, index_desc*, RelationPages*);
	bool	BTR_make_bounds(thread_db*, const IndexRetrieval*, IndexScanListIterator*,
							temporary_key*, temporary_key*, USHORT&);
	idx_e	BTR_make_key(thread_db*, USHORT, const ValueExprNode* const*, const SSHORT*,
							const index_desc*, temporary_key*, USHORT, bool*);
	void	BTR_make_null_key(thread_db*, const index_desc*, temporary_key*);
	void	BTR_mark_index_for_delete(thread_db*, Cached::Relation*, MetaId, win*, Ods::index_root_page*);
	bool	BTR_next_index(thread_db*, Cached::Relation*, jrd_tra*, index_desc*, win*);
	void	BTR_remove(thread_db*, win*, index_insertion*);
	void	BTR_reserve_slot(thread_db*, IndexCreation&, IndexCreateLock&);
	void	BTR_selectivity(thread_db*, Cached::Relation*, MetaId, SelectivityList&);
	bool	BTR_types_comparable(const dsc& target, const dsc& source);
	Ods::index_root_page* BTR_fetch_root_for_update(const char* from, thread_db* tdbb, win* window);
	const Ods::index_root_page* BTR_fetch_root(const char* from, thread_db* tdbb, win* window);
}	// namespace Firebird::Jrd

#endif // JRD_BTR_PROTO_H
