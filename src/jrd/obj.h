/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		obj.h
 *	DESCRIPTION:	Object types in meta-data
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

#ifndef JRD_OBJ_H
#define JRD_OBJ_H

#include "../common/gdsassert.h"

// Object types used in RDB$DEPENDENCIES and RDB$USER_PRIVILEGES and stored in backup.
// Note: some values are hard coded in grant.gdl
// Keep existing constants unchanged.

typedef SSHORT ObjectType;

inline constexpr ObjectType obj_relation = 0;
inline constexpr ObjectType obj_view = 1;
inline constexpr ObjectType obj_trigger = 2;
inline constexpr ObjectType obj_computed = 3;
inline constexpr ObjectType obj_validation = 4;
inline constexpr ObjectType obj_procedure = 5;
inline constexpr ObjectType obj_index_expression = 6;
inline constexpr ObjectType obj_exception = 7;
inline constexpr ObjectType obj_user = 8;
inline constexpr ObjectType obj_field = 9;
inline constexpr ObjectType obj_index = 10;
inline constexpr ObjectType obj_charset = 11;
inline constexpr ObjectType obj_user_group = 12;
inline constexpr ObjectType obj_sql_role = 13;
inline constexpr ObjectType obj_generator = 14;
inline constexpr ObjectType obj_udf = 15;
inline constexpr ObjectType obj_blob_filter = 16;
inline constexpr ObjectType obj_collation = 17;
inline constexpr ObjectType obj_package_header = 18;
inline constexpr ObjectType obj_package_body = 19;
inline constexpr ObjectType obj_privilege = 20;

// objects types for ddl operations
inline constexpr ObjectType obj_database = 21;
inline constexpr ObjectType obj_relations = 22;
inline constexpr ObjectType obj_views = 23;
inline constexpr ObjectType obj_procedures = 24;
inline constexpr ObjectType obj_functions = 25;
inline constexpr ObjectType obj_packages = 26;
inline constexpr ObjectType obj_generators = 27;
inline constexpr ObjectType obj_domains = 28;
inline constexpr ObjectType obj_exceptions = 29;
inline constexpr ObjectType obj_roles = 30;
inline constexpr ObjectType obj_charsets = 31;
inline constexpr ObjectType obj_collations = 32;
inline constexpr ObjectType obj_filters = 33;

// Add new codes here if they are used in RDB$DEPENDENCIES or RDB$USER_PRIVILEGES or stored in backup
// Codes for DDL operations add in isDdlObject function as well (find it below).
inline constexpr ObjectType obj_jobs = 34;
inline constexpr ObjectType obj_tablespace = 35;
inline constexpr ObjectType obj_tablespaces = 36;
inline constexpr ObjectType obj_index_condition = 37;

inline constexpr ObjectType obj_schema = 38;
inline constexpr ObjectType obj_schemas = 39;

inline constexpr ObjectType obj_package_constant = 40;

inline constexpr ObjectType obj_type_MAX = 41;

// used in the parser only / no relation with obj_type_MAX (should be greater)
inline constexpr ObjectType obj_user_or_role = 100;
inline constexpr ObjectType obj_parameter = 101;
inline constexpr ObjectType obj_column = 102;
inline constexpr ObjectType obj_publication = 103;

inline constexpr ObjectType obj_any = 255;


inline bool isSchemaBoundObject(ObjectType objectType) noexcept
{
	switch (objectType)
	{
		case obj_relation:
		case obj_view:
		case obj_trigger:
		case obj_procedure:
		case obj_exception:
		case obj_field:
		case obj_index:
		case obj_charset:
		case obj_generator:
		case obj_udf:
		case obj_collation:
		case obj_package_header:
		case obj_package_constant:
			return true;

		default:
			return false;
	}
}


inline bool isDdlObject(ObjectType objectType, bool* useSchema = nullptr) noexcept
{
	if (useSchema)
		*useSchema = false;

	switch (objectType)
	{
		case obj_relations:
		case obj_views:
		case obj_procedures:
		case obj_functions:
		case obj_packages:
		case obj_generators:
		case obj_domains:
		case obj_exceptions:
		case obj_charsets:
		case obj_collations:
			if (useSchema)
				*useSchema = true;
			[[fallthrough]];

		case obj_database:
		case obj_filters:
		case obj_roles:
		case obj_jobs:
		case obj_tablespaces:
		case obj_schemas:
			return true;

		default:
			return false;
	}
}


inline constexpr const char* getDdlSecurityName(ObjectType object_type) noexcept
{
	switch (object_type)
	{
		case obj_database:
			return "SQL$DATABASE";
		case obj_relations:
			return "SQL$TABLES";
		case obj_views:
			return "SQL$VIEWS";
		case obj_procedures:
			return "SQL$PROCEDURES";
		case obj_functions:
			return "SQL$FUNCTIONS";
		case obj_packages:
			return "SQL$PACKAGES";
		case obj_generators:
			return "SQL$GENERATORS";
		case obj_filters:
			return "SQL$FILTERS";
		case obj_domains:
			return "SQL$DOMAINS";
		case obj_exceptions:
			return "SQL$EXCEPTIONS";
		case obj_roles:
			return "SQL$ROLES";
		case obj_charsets:
			return "SQL$CHARSETS";
		case obj_collations:
			return "SQL$COLLATIONS";
		case obj_jobs:
			return "SQL$JOBS";
		case obj_tablespaces:
			return "SQL$TABLESPACES";
		case obj_schemas:
			return "SQL$SCHEMAS";
		case obj_package_constant:
			return "SQL$PACKAGE_CONSTANT";
		default:
			return "";
	}
}


inline const char* getDdlObjectName(ObjectType object_type)
{
	switch (object_type)
	{
		case obj_database:
			return "DATABASE";
		case obj_relations:
			return "TABLE";
		case obj_packages:
			return "PACKAGE";
		case obj_procedures:
			return "PROCEDURE";
		case obj_functions:
			return "FUNCTION";
		case obj_column:
			return "COLUMN";
		case obj_charsets:
			return "CHARACTER SET";
		case obj_collations:
			return "COLLATION";
		case obj_domains:
			return "DOMAIN";
		case obj_exceptions:
			return "EXCEPTION";
		case obj_generators:
			return "GENERATOR";
		case obj_views:
			return "VIEW";
		case obj_roles:
			return "ROLE";
		case obj_filters:
			return "FILTER";
		case obj_jobs:
			return "JOB";
		case obj_tablespaces:
			return "TABLESPACE";
		case obj_schemas:
			return "SCHEMA";
		default:
			fb_assert(false);
			return "<unknown object type>";
	}
}


inline const char* getObjectName(ObjectType objType)
{
	switch (objType)
	{
		case obj_relation:
			return "TABLE";
		case obj_trigger:
			return "TRIGGER";
		case obj_package_header:
			return "PACKAGE";
		case obj_procedure:
			return "PROCEDURE";
		case obj_udf:
			return "FUNCTION";
		case obj_column:
			return "COLUMN";
		case obj_charset:
			return "CHARACTER SET";
		case obj_collation:
			return "COLLATION";
		case obj_field:
			return "DOMAIN";
		case obj_exception:
			return "EXCEPTION";
		case obj_generator:
			return "GENERATOR";
		case obj_view:
			return "VIEW";
		case obj_sql_role:
			return "ROLE";
		case obj_blob_filter:
			return "FILTER";
		case obj_schema:
			return "SCHEMA";
		case obj_package_constant:
			return "PACKAGE CONSTANT";
		default:
			fb_assert(false);
			return "<unknown object type>";
	}
}


#endif // JRD_OBJ_H
