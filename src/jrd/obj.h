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

// Object types used in RDB$DEPENDENCIES and RDB$USER_PRIVILEGES
// Note: some values are hard coded in grant.gdl

const int obj_relation			= 0;
const int obj_view				= 1;
const int obj_trigger			= 2;
const int obj_computed			= 3;
const int obj_validation		= 4;
const int obj_procedure			= 5;
const int obj_expression_index	= 6;
const int obj_exception			= 7;
const int obj_user				= 8;
const int obj_field				= 9;
const int obj_index				= 10;
const int obj_charset			= 11;
const int obj_user_group		= 12;
const int obj_sql_role			= 13;
const int obj_generator			= 14;
const int obj_udf				= 15;
const int obj_blob_filter		= 16;
const int obj_collation			= 17;
const int obj_package_header	= 18;
const int obj_package_body		= 19;
const int obj_privilege			= 20;
const int obj_tablespace		= 21;

const int obj_last_non_ddl		= 21;	// keep in sync!!!

// objects types for ddl operations
const int obj_database			= obj_last_non_ddl + 1;
const int obj_relations			= obj_last_non_ddl + 2;
const int obj_views				= obj_last_non_ddl + 3;
const int obj_procedures		= obj_last_non_ddl + 4;
const int obj_functions			= obj_last_non_ddl + 5;
const int obj_packages			= obj_last_non_ddl + 6;
const int obj_generators		= obj_last_non_ddl + 7;
const int obj_domains			= obj_last_non_ddl + 8;
const int obj_exceptions		= obj_last_non_ddl + 9;
const int obj_roles				= obj_last_non_ddl + 10;
const int obj_charsets			= obj_last_non_ddl + 11;
const int obj_collations		= obj_last_non_ddl + 12;
const int obj_filters			= obj_last_non_ddl + 13;
const int obj_tablespaces		= obj_last_non_ddl + 14;

const int obj_type_MAX			= obj_last_non_ddl + 15;	// keep this last!

// used in the parser only / no relation with obj_type_MAX (should be greater)
const int obj_user_or_role		= 100;
const int obj_schema			= 101;
const int obj_parameter			= 102;

inline const char* get_object_name(int object_type)
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
		case obj_tablespaces:
			return "SQL$TABLESPACES";
		default:
			return "";
	}
}

#endif // JRD_OBJ_H
