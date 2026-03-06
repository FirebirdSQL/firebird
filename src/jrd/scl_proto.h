/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		scl_proto.h
 *	DESCRIPTION:	Prototype header file for scl.epp
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

#ifndef JRD_SCL_PROTO_H
#define JRD_SCL_PROTO_H

#include "../jrd/scl.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../jrd/MetaName.h"

struct dsc;

namespace Firebird::Jrd
{


	void SCL_check_access(thread_db*, const SecurityClass*,
						SLONG, const QualifiedName&,
						SecurityClass::flags_t, ObjectType type, bool recursive, const QualifiedName&,
						const MetaName& = {});
	void SCL_check_create_access(thread_db*, ObjectType type, const MetaName& schema);
	void SCL_check_charset(thread_db* tdbb, const QualifiedName&, SecurityClass::flags_t);
	void SCL_check_collation(thread_db* tdbb, const QualifiedName&, SecurityClass::flags_t);
	void SCL_check_database(thread_db* tdbb, SecurityClass::flags_t mask);
	void SCL_check_domain(thread_db* tdbb, const QualifiedName&, SecurityClass::flags_t);
	bool SCL_check_exception(thread_db* tdbb, const QualifiedName&, SecurityClass::flags_t);
	bool SCL_check_generator(thread_db* tdbb, const QualifiedName&, SecurityClass::flags_t);
	void SCL_check_index(thread_db*, const QualifiedName&, UCHAR, SecurityClass::flags_t);
	bool SCL_check_package(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t);
	bool SCL_check_procedure(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t);
	bool SCL_check_function(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t);
	void SCL_check_filter(thread_db* tdbb, const MetaName& name, SecurityClass::flags_t);
	void SCL_check_relation(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t,
		bool protectSys = true);
	bool SCL_check_schema(thread_db* tdbb, const MetaName&, SecurityClass::flags_t);
	bool SCL_check_view(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t);
	void SCL_check_role(thread_db* tdbb, const MetaName&, SecurityClass::flags_t);
	SecurityClass* SCL_get_class(thread_db*, const MetaName& name);
	SecurityClass::flags_t SCL_get_mask(thread_db* tdbb, const QualifiedName&, const TEXT*);
	void SCL_clear_classes(thread_db*, const MetaName&);
	void SCL_release_all(SecurityClassList*&);
	bool SCL_role_granted(thread_db* tdbb, const UserId& usr, const TEXT* sql_role);
	SecurityClass::flags_t SCL_get_object_mask(ObjectType object_type, const MetaName& schema);
	ULONG SCL_get_number(const UCHAR*);
	USHORT SCL_convert_privilege(thread_db* tdbb, jrd_tra* transaction, const string& priv);

	typedef Array<UCHAR> Acl;

	bool SCL_move_priv(SecurityClass::flags_t, Acl&);

inline MetaName SCL_getDdlSecurityClassName(ObjectType objectType, const MetaName& schema)
{
	switch (objectType)
	{
		case obj_database:
		case obj_relations:
		case obj_views:
		case obj_procedures:
		case obj_functions:
		case obj_packages:
		case obj_generators:
		case obj_filters:
		case obj_domains:
		case obj_exceptions:
		case obj_roles:
		case obj_charsets:
		case obj_collations:
		case obj_jobs:
		case obj_tablespaces:
		case obj_schemas:
		{
			string str;
			str.printf(SQL_DDL_SECCLASS_FORMAT, (int) objectType, schema.c_str());
			return str;
		}

		default:
			fb_assert(false);
			return "";
	}
}


}	// namespace Firebird::Jrd


#endif // JRD_SCL_PROTO_H
