/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dpm_proto.h
 *	DESCRIPTION:	Prototype header file for dpm.cpp
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

#ifndef JRD_DPM_PROTO_H
#define JRD_DPM_PROTO_H

#include "../jrd/RecordNumber.h"
#include "../jrd/sbm.h"
#include "../jrd/vio_proto.h"
#include "../jrd/Resources.h"

// fwd. decl.
namespace Firebird::Jrd {
	class blb;
	class jrd_rel;
	struct record_param;
	class Record;
	class jrd_tra;
	struct win;

	// Store allocation policy types.  Parameter to DPM_store()
	// I don't see it stored, but since the first constant was 1, I'm using the same values in the enum.
	enum RecordStorageType
	{
		DPM_primary = 1,	// New primary record
		DPM_secondary,		// Chained version of primary record
		DPM_other			// Independent (or don't care) record
	};
}

namespace Firebird::Jrd::Ods
{
	struct pag;
	struct data_page;
}

namespace Firebird::Jrd {

Ods::pag* DPM_allocate(thread_db*, win*);
void DPM_backout(thread_db*, record_param*);
void DPM_backout_mark(thread_db*, record_param*, const jrd_tra*);
double DPM_cardinality(thread_db*, jrd_rel*, const Format*);
bool DPM_chain(thread_db*, record_param*, record_param*);
void DPM_create_relation(thread_db*, Cached::Relation*);
ULONG DPM_data_pages(thread_db*, Cached::Relation*);
void DPM_delete(thread_db*, record_param*, ULONG);
void DPM_delete_relation(thread_db*, RelationPermanent*);
bool DPM_fetch(thread_db*, record_param*, USHORT);
bool DPM_fetch_back(thread_db*, record_param*, USHORT, SSHORT);
void DPM_fetch_fragment(thread_db*, record_param*, USHORT);
SINT64 DPM_gen_id(thread_db*, SLONG, bool, SINT64);
bool DPM_get(thread_db*, record_param*, SSHORT);
ULONG DPM_get_blob(thread_db*, blb*, jrd_rel*, RecordNumber, bool, ULONG);
void DPM_mark_relation(thread_db*, Cached::Relation*);
bool DPM_next(thread_db*, record_param*, USHORT, FindNextRecordScope);
void DPM_pages(thread_db*, SSHORT, int, ULONG, ULONG);
#ifdef SUPERSERVER_V2
SLONG DPM_prefetch_bitmap(thread_db*, jrd_rel*, PageBitmap*, SLONG);
#endif
ULONG DPM_pointer_pages(thread_db*, jrd_rel*);
void DPM_scan_pages(thread_db*);
void DPM_store(thread_db*, record_param*, PageStack&, const RecordStorageType type);
RecordNumber DPM_store_blob(thread_db*, blb*, jrd_rel*, Record*);
void DPM_rewrite_header(thread_db*, record_param*);
void DPM_scan_marker(thread_db*, MetaId);
void DPM_update(thread_db*, record_param*, PageStack*, const jrd_tra*);

void DPM_create_relation_pages(thread_db*, RelationPermanent*, RelationPages*);
void DPM_delete_relation_pages(thread_db*, RelationPermanent*, RelationPages*);

} // namespace Firebird::Jrd


#endif // JRD_DPM_PROTO_H
