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

typedef SSHORT ObjectType;

const ObjectType obj_relation = 0;
const ObjectType obj_view = 1;
const ObjectType obj_trigger = 2;
const ObjectType obj_computed = 3;
const ObjectType obj_validation = 4;
const ObjectType obj_procedure = 5;
const ObjectType obj_index_expression = 6;
const ObjectType obj_exception = 7;
const ObjectType obj_user = 8;
const ObjectType obj_field = 9;
const ObjectType obj_index = 10;
const ObjectType obj_charset = 11;
const ObjectType obj_user_group = 12;
const ObjectType obj_sql_role = 13;
const ObjectType obj_generator = 14;
const ObjectType obj_udf = 15;
const ObjectType obj_blob_filter = 16;
const ObjectType obj_collation = 17;
const ObjectType obj_package_header = 18;
const ObjectType obj_package_body = 19;
const ObjectType obj_privilege = 20;

// objects types for ddl operations
const ObjectType obj_database = 21;
const ObjectType obj_relations = 22;
const ObjectType obj_views = 23;
const ObjectType obj_procedures = 24;
const ObjectType obj_functions = 25;
const ObjectType obj_packages = 26;
const ObjectType obj_generators = 27;
const ObjectType obj_domains = 28;
const ObjectType obj_exceptions = 29;
const ObjectType obj_roles = 30;
const ObjectType obj_charsets = 31;
const ObjectType obj_collations = 32;
const ObjectType obj_filters = 33;

// Add new codes here if they are used in RDB$DEPENDENCIES or RDB$USER_PRIVILEGES or stored in backup
// Codes for DDL operations add in isDdlObject function as well (find it below).
const ObjectType obj_jobs = 34;
const ObjectType obj_tablespace = 35;
const ObjectType obj_tablespaces = 36;
const ObjectType obj_index_condition = 37;

const ObjectType obj_type_MAX = 38;

// used in the parser only / no relation with obj_type_MAX (should be greater)
const ObjectType obj_user_or_role= 100;
const ObjectType obj_parameter = 101;
const ObjectType obj_column = 102;

const ObjectType obj_any = 255;


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
		case obj_jobs:
		case obj_tablespaces:
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
		case obj_tablespaces:
			return "SQL$TABLESPACES";
		case obj_jobs:
			return "SQL$JOBS";
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
		case obj_tablespaces:
			return "TABLESPACE";
		case obj_jobs:
			return "JOB";
		default:
			fb_assert(false);
			return "<unknown object type>";
	}
}


#endif // JRD_OBJ_H
