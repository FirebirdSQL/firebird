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

#include "../common/classes/array.h"
#include "../common/classes/TriState.h"
#include "../jrd/MetaName.h"
#include "../jrd/Resources.h"
#include "../jrd/QualifiedName.h"
#include "../common/obj.h"
#include "../jrd/CacheVector.h"
#include "../jrd/LocalTemporaryTable.h"

#include <functional>
#include <initializer_list>
#include <optional>

struct dsc;

namespace Firebird::Jrd {
	class jrd_tra;
	class Request;
	class Statement;
	class jrd_prc;
	class Format;
	class jrd_rel;
	class CompilerScratch;
	struct Dependency;
	class DmlNode;
	class Database;
	struct bid;
	struct index_desc;
	class jrd_fld;
	class Shadow;
	class DeferredWork;
	struct FieldInfo;
	class ExceptionItem;
	class LocalTemporaryTable;
	class GeneratorItem;
	class BlobFilter;
	class RelationPermanent;
	class Triggers;
	class TrigArray;

	typedef HalfStaticArray<QualifiedName, 4> CharsetVariants;

	struct SubtypeInfo
	{
		SubtypeInfo()
			: attributes(0),
			  ignoreAttributes(true)
		{
		}

		CharsetVariants charsetName;
		QualifiedName collationName;
		MetaName baseCollationName;
		USHORT attributes;
		bool ignoreAttributes;
		UCharBuffer specificAttributes;
	};

	void		MET_activate_shadow(thread_db*);
	ULONG		MET_align(const dsc*, ULONG);
	Cached::Relation*	MET_change_fields(thread_db*, jrd_tra*, const dsc*, const dsc*);
	void		MET_delete_dependencies(thread_db*, const QualifiedName&, int);
	void		MET_delete_shadow(thread_db*, USHORT);
	void		MET_error(const TEXT*, ...);
	Format*	MET_format(thread_db*, RelationPermanent*, USHORT);
	bool		MET_get_char_coll_subtype_info(thread_db*, USHORT, SubtypeInfo* info);
	DmlNode*	MET_get_dependencies(thread_db*, jrd_rel*, const UCHAR*, const ULONG,
									CompilerScratch*, bid*, Statement**,
									CompilerScratch**, const QualifiedName&, int, USHORT,
									jrd_tra*, const QualifiedName& = {});
	jrd_fld*	MET_get_field(const jrd_rel*, USHORT);
	ULONG		MET_get_rel_flags_from_TYPE(USHORT);
	bool		MET_get_repl_state(thread_db*, const QualifiedName&);
	void		MET_get_shadow_files(thread_db*, bool);
	bool		MET_load_exception(thread_db*, ExceptionItem&);
	void		MET_load_trigger(thread_db*, jrd_rel*, const QualifiedName&, std::function<Triggers&(int)>);
	void		MET_lookup_index_for_cnstrt(thread_db*, QualifiedName& index_name, const QualifiedName& constraint);
	void		MET_lookup_cnstrt_for_index(thread_db*, MetaName& constraint, const QualifiedName& index_name);
	void		MET_lookup_cnstrt_for_trigger(thread_db*, MetaName&, QualifiedName&, const QualifiedName&);
	void		MET_lookup_exception(thread_db*, SLONG, /* OUT */ QualifiedName&, /* OUT */ string*);
	ElementBase::ReturnedId	MET_lookup_field(thread_db*, jrd_rel*, const MetaName&);
	BlobFilter*	MET_lookup_filter(thread_db*, SSHORT, SSHORT);
	bool		MET_load_generator(thread_db*, GeneratorItem&, bool* sysGen = 0, SLONG* step = 0);
	SLONG		MET_lookup_generator(thread_db*, const QualifiedName&, bool* sysGen = 0, SLONG* step = 0);
	bool		MET_lookup_generator_id(thread_db*, SLONG, QualifiedName&, bool* sysGen = 0);
	void		MET_update_generator_increment(thread_db* tdbb, SLONG gen_id, SLONG step);
	void		MET_lookup_index_code(thread_db* tdbb, Cached::Relation* relation, index_desc* idx);
	bool		MET_lookup_index_expr_cond_blr(thread_db* tdbb, const QualifiedName& index_name,
					bid& expr_blob_id, bid& cond_blob_id);

	bool		MET_lookup_partner(thread_db* tdbb, RelationPermanent* relation, index_desc* idx,
								const QualifiedName& index_name);
	DmlNode*	MET_parse_blob(thread_db*, const MetaName* schema, Cached::Relation*, bid*,
								CompilerScratch**, Statement**, bool, bool);
	void		MET_prepare(thread_db*, jrd_tra*, USHORT, const UCHAR*);
	void		MET_release_existence(thread_db*, jrd_rel*);
	void		MET_revoke(thread_db*, jrd_tra*, const QualifiedName&,
		const QualifiedName&, const string&);
	void		MET_store_dependencies(thread_db*, Array<Dependency>&, const jrd_rel*,
		const QualifiedName&, int, jrd_tra*);
	void		MET_trigger_msg(thread_db*, string&, const QualifiedName&, USHORT);
	void		MET_update_shadow(thread_db*, Shadow*, USHORT);
	void		MET_update_transaction(thread_db*, jrd_tra*, const bool);
	void		MET_get_domain(thread_db*, MemoryPool& csbPool, const QualifiedName&, dsc*, FieldInfo*);
	void		MET_get_relation_field(thread_db*, MemoryPool& csbPool,
		const QualifiedName&, const MetaName&, dsc*, FieldInfo*);
	void		MET_update_partners(thread_db*);
	int			MET_get_linger(thread_db*);
	TriState	MET_get_ss_definer(thread_db*, const MetaName& schemaName);
	std::optional<ObjectType> MET_qualify_existing_name(thread_db* tdbb, QualifiedName& name,
		std::initializer_list<ObjectType> objTypes,
		const ObjectsArray<MetaString>* schemaSearchPath = nullptr);
	bool MET_check_schema_exists(thread_db* tdbb, const MetaName& name);
	bool MET_get_ltt_index(Attachment* attachment, const QualifiedName& indexName,
		LocalTemporaryTable** outLtt = nullptr, LocalTemporaryTable::Index** outIndex = nullptr);
} // namespace Firebird::Jrd

#endif // JRD_MET_PROTO_H
