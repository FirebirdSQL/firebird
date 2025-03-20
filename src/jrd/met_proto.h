/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_proto.h
 *	DESCRIPTION:	Prototype header file for met.cpp
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

#ifndef JRD_MET_PROTO_H
#define JRD_MET_PROTO_H

#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../jrd/obj.h"
#include <initializer_list>
#include <optional>

struct dsc;

namespace Jrd
{
	class jrd_tra;
	class Request;
	class Statement;
	class jrd_prc;
	class Format;
	class jrd_rel;
	class CompilerScratch;
	class DmlNode;
	class Database;
	struct bid;
	struct index_desc;
	class jrd_fld;
	class Shadow;
	class DeferredWork;
	struct FieldInfo;
	class ExceptionItem;

	// index status
	enum IndexStatus
	{
		MET_object_active,
		MET_object_deferred_active,
		MET_object_inactive,
		MET_object_unknown
	};
}

struct SubtypeInfo
{
	SubtypeInfo()
		: attributes(0),
		  ignoreAttributes(true)
	{
	}

	Jrd::QualifiedName charsetName;
	Jrd::QualifiedName collationName;
	Firebird::string baseCollationName;
	USHORT attributes;
	bool ignoreAttributes;
	Firebird::UCharBuffer specificAttributes;
};

void		MET_activate_shadow(Jrd::thread_db*);
ULONG		MET_align(const dsc*, ULONG);
Jrd::DeferredWork*	MET_change_fields(Jrd::thread_db*, Jrd::jrd_tra*, const dsc*, const dsc*);
Jrd::Format*	MET_current(Jrd::thread_db*, Jrd::jrd_rel*);
void		MET_delete_dependencies(Jrd::thread_db*, const Jrd::QualifiedName&, int, Jrd::jrd_tra*);
void		MET_delete_shadow(Jrd::thread_db*, USHORT);
bool		MET_dsql_cache_use(Jrd::thread_db* tdbb, Jrd::sym_type type, const Jrd::QualifiedName& name);
void		MET_dsql_cache_release(Jrd::thread_db* tdbb, Jrd::sym_type type, const Jrd::QualifiedName& name);
void		MET_error(const TEXT*, ...);
Jrd::Format*	MET_format(Jrd::thread_db*, Jrd::jrd_rel*, USHORT);
bool		MET_get_char_coll_subtype(Jrd::thread_db*, USHORT*, const Jrd::QualifiedName&);
bool		MET_get_char_coll_subtype_info(Jrd::thread_db*, USHORT, SubtypeInfo* info);
Jrd::DmlNode*	MET_get_dependencies(Jrd::thread_db*, Jrd::jrd_rel*, const UCHAR*, const ULONG,
								Jrd::CompilerScratch*, Jrd::bid*, Jrd::Statement**,
								Jrd::CompilerScratch**, const Jrd::QualifiedName&, int, USHORT,
								Jrd::jrd_tra*, const Jrd::QualifiedName& = {});
Jrd::jrd_fld*	MET_get_field(const Jrd::jrd_rel*, USHORT);
ULONG		MET_get_rel_flags_from_TYPE(USHORT);
bool		MET_get_repl_state(Jrd::thread_db*, const Jrd::QualifiedName&);
void		MET_get_shadow_files(Jrd::thread_db*, bool);
void		MET_load_db_triggers(Jrd::thread_db*, int);
void		MET_load_ddl_triggers(Jrd::thread_db* tdbb);
bool		MET_load_exception(Jrd::thread_db*, Jrd::ExceptionItem&);
void		MET_load_trigger(Jrd::thread_db*, Jrd::jrd_rel*, const Jrd::QualifiedName&, Jrd::TrigVector**);
void		MET_lookup_cnstrt_for_index(Jrd::thread_db*, Jrd::MetaName& constraint, const Jrd::QualifiedName& index_name);
void		MET_lookup_cnstrt_for_trigger(Jrd::thread_db*, Jrd::MetaName&, Jrd::QualifiedName&, const Jrd::QualifiedName&);
void		MET_lookup_exception(Jrd::thread_db*, SLONG, /* OUT */ Jrd::QualifiedName&, /* OUT */ Firebird::string*);
int			MET_lookup_field(Jrd::thread_db*, Jrd::jrd_rel*, const Jrd::MetaName&);
Jrd::BlobFilter*	MET_lookup_filter(Jrd::thread_db*, SSHORT, SSHORT);
bool		MET_load_generator(Jrd::thread_db*, Jrd::GeneratorItem&, bool* sysGen = 0, SLONG* step = 0);
SLONG		MET_lookup_generator(Jrd::thread_db*, const Jrd::QualifiedName&, bool* sysGen = 0, SLONG* step = 0);
bool		MET_lookup_generator_id(Jrd::thread_db*, SLONG, Jrd::QualifiedName&, bool* sysGen = 0);
void		MET_update_generator_increment(Jrd::thread_db* tdbb, SLONG gen_id, SLONG step);
void		MET_lookup_index(Jrd::thread_db*, Jrd::QualifiedName&, const Jrd::QualifiedName&, USHORT);
void		MET_lookup_index_condition(Jrd::thread_db*, Jrd::jrd_rel*, Jrd::index_desc*);
void		MET_lookup_index_expression(Jrd::thread_db*, Jrd::jrd_rel*, Jrd::index_desc*);
bool		MET_lookup_index_expr_cond_blr(Jrd::thread_db* tdbb, const Jrd::QualifiedName& index_name,
	Jrd::bid& expr_blob_id, Jrd::bid& cond_blob_id);
SLONG		MET_lookup_index_name(Jrd::thread_db*, const Jrd::QualifiedName&, SLONG*, Jrd::IndexStatus* status);
bool		MET_lookup_partner(Jrd::thread_db*, Jrd::jrd_rel*, struct Jrd::index_desc*, const Jrd::QualifiedName&);
Jrd::jrd_prc*	MET_lookup_procedure(Jrd::thread_db*, const Jrd::QualifiedName&, bool);
Jrd::jrd_prc*	MET_lookup_procedure_id(Jrd::thread_db*, USHORT, bool, bool, USHORT);
Jrd::jrd_rel*	MET_lookup_relation(Jrd::thread_db*, const Jrd::QualifiedName&);
Jrd::jrd_rel*	MET_lookup_relation_id(Jrd::thread_db*, SLONG, bool);
Jrd::DmlNode*	MET_parse_blob(Jrd::thread_db*, const Jrd::MetaName* schema, Jrd::jrd_rel*, Jrd::bid*,
	Jrd::CompilerScratch**, Jrd::Statement**, bool, bool);
void		MET_post_existence(Jrd::thread_db*, Jrd::jrd_rel*);
void		MET_prepare(Jrd::thread_db*, Jrd::jrd_tra*, USHORT, const UCHAR*);
Jrd::jrd_prc*	MET_procedure(Jrd::thread_db*, USHORT, bool, USHORT);
Jrd::jrd_rel*	MET_relation(Jrd::thread_db*, USHORT);
void		MET_release_existence(Jrd::thread_db*, Jrd::jrd_rel*);
void		MET_release_trigger(Jrd::thread_db*, Jrd::TrigVector**, const Jrd::QualifiedName&);
void		MET_release_triggers(Jrd::thread_db*, Jrd::TrigVector**, bool);
#ifdef DEV_BUILD
void		MET_verify_cache(Jrd::thread_db*);
#endif
void		MET_clear_cache(Jrd::thread_db*);
bool		MET_routine_in_use(Jrd::thread_db*, Jrd::Routine*);
void		MET_revoke(Jrd::thread_db*, Jrd::jrd_tra*, const Jrd::QualifiedName&,
	const Jrd::QualifiedName&, const Firebird::string&);
void		MET_scan_partners(Jrd::thread_db*, Jrd::jrd_rel*);
void		MET_scan_relation(Jrd::thread_db*, Jrd::jrd_rel*);
void		MET_trigger_msg(Jrd::thread_db*, Firebird::string&, const Jrd::QualifiedName&, USHORT);
void		MET_update_shadow(Jrd::thread_db*, Jrd::Shadow*, USHORT);
void		MET_update_transaction(Jrd::thread_db*, Jrd::jrd_tra*, const bool);
void		MET_get_domain(Jrd::thread_db*, MemoryPool& csbPool, const Jrd::QualifiedName&, dsc*, Jrd::FieldInfo*);
Jrd::MetaName MET_get_relation_field(Jrd::thread_db*, MemoryPool& csbPool,
	const Jrd::QualifiedName&, const Jrd::MetaName&, dsc*, Jrd::FieldInfo*);
void		MET_update_partners(Jrd::thread_db*);

void MET_store_dependencies(Jrd::thread_db*, Firebird::Array<Jrd::CompilerScratch::Dependency>& dependencies,
	const Jrd::jrd_rel*, const Jrd::QualifiedName&, int, Jrd::jrd_tra*);

int			MET_get_linger(Jrd::thread_db*);
Firebird::TriState	MET_get_ss_definer(Jrd::thread_db*, const Jrd::MetaName& schemaName);
std::optional<ObjectType> MET_qualify_existing_name(Jrd::thread_db* tdbb, Jrd::QualifiedName& name,
	std::initializer_list<ObjectType> objTypes,
	const Firebird::ObjectsArray<Firebird::MetaString>* schemaSearchPath = nullptr);
bool MET_check_schema_exists(Jrd::thread_db* tdbb, const Jrd::MetaName& name);

#endif // JRD_MET_PROTO_H
