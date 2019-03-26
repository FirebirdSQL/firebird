/*
 *	PROGRAM:		Firebird access control.
 *	MODULE:			jrd/SystemPrivileges.h
 *	DESCRIPTION:	List of known system privileges.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2016 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#if defined(SYSTEM_PRIVILEGE) || (!defined(FB_JRD_SYSTEM_PRIVILEGES))

#ifndef SYSTEM_PRIVILEGE
#define SYSTEM_PRIVILEGE(p) p,
#define FB_JRD_SYSTEM_PRIVILEGES
#define FB_JRD_SYSTEM_PRIVILEGES_TMP

namespace Jrd {
enum SystemPrivilege
{
NULL_PRIVILEGE,
#endif

SYSTEM_PRIVILEGE(USER_MANAGEMENT)
SYSTEM_PRIVILEGE(READ_RAW_PAGES)
SYSTEM_PRIVILEGE(CREATE_USER_TYPES)
SYSTEM_PRIVILEGE(USE_NBACKUP_UTILITY)
SYSTEM_PRIVILEGE(CHANGE_SHUTDOWN_MODE)
SYSTEM_PRIVILEGE(TRACE_ANY_ATTACHMENT)
SYSTEM_PRIVILEGE(MONITOR_ANY_ATTACHMENT)
SYSTEM_PRIVILEGE(ACCESS_SHUTDOWN_DATABASE)
SYSTEM_PRIVILEGE(CREATE_DATABASE)
SYSTEM_PRIVILEGE(DROP_DATABASE)
SYSTEM_PRIVILEGE(USE_GBAK_UTILITY)
SYSTEM_PRIVILEGE(USE_GSTAT_UTILITY)
SYSTEM_PRIVILEGE(USE_GFIX_UTILITY)
SYSTEM_PRIVILEGE(IGNORE_DB_TRIGGERS)
SYSTEM_PRIVILEGE(CHANGE_HEADER_SETTINGS)
SYSTEM_PRIVILEGE(SELECT_ANY_OBJECT_IN_DATABASE)
SYSTEM_PRIVILEGE(ACCESS_ANY_OBJECT_IN_DATABASE)
SYSTEM_PRIVILEGE(MODIFY_ANY_OBJECT_IN_DATABASE)
SYSTEM_PRIVILEGE(CHANGE_MAPPING_RULES)
SYSTEM_PRIVILEGE(USE_GRANTED_BY_CLAUSE)
SYSTEM_PRIVILEGE(GRANT_REVOKE_ON_ANY_OBJECT)
SYSTEM_PRIVILEGE(GRANT_REVOKE_ANY_DDL_RIGHT)
SYSTEM_PRIVILEGE(CREATE_PRIVILEGED_ROLES)
SYSTEM_PRIVILEGE(GET_DBCRYPT_INFO)
SYSTEM_PRIVILEGE(MODIFY_EXT_CONN_POOL)
SYSTEM_PRIVILEGE(REPLICATE_INTO_DATABASE)

#ifdef FB_JRD_SYSTEM_PRIVILEGES_TMP
maxSystemPrivilege
};
} // namespace Jrd

#undef SYSTEM_PRIVILEGE
#undef FB_JRD_SYSTEM_PRIVILEGES_TMP
#endif

#endif
