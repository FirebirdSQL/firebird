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

// Object types used in RDB$DEPENDENCIES and RDB$USER_PRIVILEGES and stored in backup.
// Note: some values are hard coded in grant.gdl
// Keep existing constants unchanged.

enum ObjectType
{
	obj_relation = 0,
	obj_view,			// 1
	obj_trigger,
	obj_computed,
	obj_validation,
	obj_procedure,		// 5
	obj_expression_index,
	obj_exception,
	obj_user,
	obj_field,
	obj_index,			// 10
	obj_charset,
	obj_user_group,
	obj_sql_role,
	obj_generator,
	obj_udf,			// 15
	obj_blob_filter,
	obj_collation,
	obj_package_header,
	obj_package_body,
	obj_privilege,		// 20

	// objects types for ddl operations
	obj_database,
	obj_relations,
	obj_views,
	obj_procedures,
	obj_functions,		// 25
	obj_packages,
	obj_generators,
	obj_domains,
	obj_exceptions,
	obj_roles,			// 30
	obj_charsets,
	obj_collations,
	obj_filters,

	// Add new codes here if they are used in RDB$DEPENDENCIES or RDB$USER_PRIVILEGES or stored in backup
	// Codes for DDL operations add in isDdlObject function as well (find it below).
	// etc. obj_tablespaces,

	obj_type_MAX,

	// used in the parser only / no relation with obj_type_MAX (should be greater)
	obj_user_or_role= 100,
	obj_parameter,
	obj_column,

	obj_any = 255
};


inline bool isDdlObject(ObjectType object_type)
{
	switch (object_type)
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
			return true;
		default:
			return false;
	}
}


inline const char* getSecurityClassName(ObjectType object_type)
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
		default:
			fb_assert(false);
			return "<unknown object type>";
	}
}


#endif // JRD_OBJ_H
