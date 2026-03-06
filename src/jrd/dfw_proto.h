/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dfw_proto.h
 *	DESCRIPTION:	Prototype header file for dfw.cpp
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

#ifndef JRD_DFW_PROTO_H
#define JRD_DFW_PROTO_H

#include "../jrd/btr.h"		// defines SelectivityList
#include "../jrd/intl.h"	// defined TTypeId
#include "../jrd/Resources.h"
#include "../common/classes/fb_string.h"

namespace Firebird::Jrd {
	enum dfw_t : int;

	class thread_db;
	class jrd_tra;
	class DeferredWork;

	USHORT DFW_assign_index_type(thread_db*, const QualifiedName&, SSHORT, TTypeId);
	void DFW_delete_deferred(jrd_tra*, SavNumber);
	SortedArray<int>& DFW_get_ids(DeferredWork* work);
	void DFW_merge_work(jrd_tra*, SavNumber, SavNumber);
	void DFW_perform_work(thread_db*, jrd_tra*);
	void DFW_perform_post_commit_work(jrd_tra*);
	void DFW_raiseRelationInUseError(const Cached::Relation*);
	DeferredWork* DFW_post_work(jrd_tra*, dfw_t, const dsc* nameDesc, const dsc* schemaDesc, USHORT,
		const MetaName& package = {});
	DeferredWork* DFW_post_work(jrd_tra*, dfw_t, const string&, const MetaName& schema,
		USHORT, const MetaName& package = {});
	DeferredWork* DFW_post_work_arg(jrd_tra*, DeferredWork*, const dsc* nameDesc, const dsc* schemaDesc,
		USHORT);
	DeferredWork* DFW_post_work_arg(jrd_tra*, DeferredWork*, const dsc* nameDesc, const dsc* schemaDesc,
		USHORT, dfw_t);
	void DFW_update_index(const QualifiedName&, USHORT, const SelectivityList&, jrd_tra*);
	void DFW_reset_icu(thread_db*);
	string DFW_remove_icu_info_from_attributes(const QualifiedName&, const string&);
} // namespace Firebird::Jrd

#endif // JRD_DFW_PROTO_H
