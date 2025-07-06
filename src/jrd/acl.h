/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		acl.h
 *	DESCRIPTION:	Access control list definitions
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

#ifndef JRD_ACL_H
#define JRD_ACL_H

// CVC: The correct type for these ACL_ and privileges seems to be UCHAR instead
// of int, based on usage, but they aren't coherent either with scl.epp's
// P_NAMES.p_names_acl that's USHORT.

inline constexpr int ACL_version	= 2;

inline constexpr int ACL_end		= 0;
inline constexpr int ACL_id_list	= 1;
inline constexpr int ACL_priv_list	= 2;

// Privileges to be granted

inline constexpr int priv_end			= 0;
inline constexpr int priv_control		= 1;		// Control over ACL
inline constexpr int priv_grant			= 2;		// Unused
inline constexpr int priv_drop			= 3;		// Drop object
inline constexpr int priv_select		= 4;		// SELECT
inline constexpr int priv_write			= 5;		// Unused
inline constexpr int priv_alter			= 6;		// Alter object
inline constexpr int priv_insert		= 7;		// INSERT
inline constexpr int priv_delete		= 8;		// DELETE
inline constexpr int priv_update		= 9;		// UPDATE
inline constexpr int priv_references	= 10;		// REFERENCES for foreign key
inline constexpr int priv_execute		= 11;		// EXECUTE (procedure, function, package)
// New in FB3
inline constexpr int priv_usage			= 12;		// USAGE (domain, exception, sequence, collation)
inline constexpr int priv_create		= 13;		// Create object
inline constexpr int priv_alter_any		= 14;		// Alter any object
inline constexpr int priv_drop_any		= 15;		// Drop any object
inline constexpr int priv_max			= 16;

// Identification criterias

inline constexpr int id_end				= 0;
inline constexpr int id_group			= 1;		// UNIX group id
inline constexpr int id_user			= 2;		// UNIX user
inline constexpr int id_person			= 3;		// User name
inline constexpr int id_project			= 4;		// Project name
inline constexpr int id_organization	= 5;		// Organization name
inline constexpr int id_node			= 6;		// Node id
inline constexpr int id_view			= 7;		// View name
inline constexpr int id_views			= 8;		// All views
inline constexpr int id_trigger			= 9;		// Trigger name
inline constexpr int id_procedure		= 10;		// Procedure name
inline constexpr int id_sql_role		= 11;		// SQL role
// New in FB3
inline constexpr int id_package			= 12;		// Package name
inline constexpr int id_function		= 13;		// Function name
inline constexpr int id_filter			= 14;		// Filter name
// New in FB4
inline constexpr int id_privilege		= 15;		// System privilege
inline constexpr int id_max				= 16;

/* Format of access control list:

	acl		:=	<ACL_version> [ <acl_element> ]... <0>
	acl_element	:=	<ACL_id_list> <id_list> <ACL_priv_list> <priv_list>
	id_list		:=	[ <id_item> ]... <id_end>
	id_item		:=	<id_criteria> <length> [<name>]
	priv_list	:=	[ <privilege> ]... <priv_end>

*/


// Transaction Description Record
// CVC: This information should match enum tdr_vals in alice.h

inline constexpr int TDR_VERSION		= 1;
inline constexpr int TDR_HOST_SITE		= 1;
inline constexpr int TDR_DATABASE_PATH	= 2;
inline constexpr int TDR_TRANSACTION_ID	= 3;
inline constexpr int TDR_REMOTE_SITE	= 4;

#endif // JRD_ACL_H
