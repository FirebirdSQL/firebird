/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		constants.h
 *	DESCRIPTION:	Misc system constants
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
 *
 * 2001.10.08 Claudio Valderrama: fb_sysflag enum with numbering
 *   for automatically created triggers that aren't system triggers.
 */

#ifndef JRD_CONSTANTS_H
#define JRD_CONSTANTS_H

// BLOb Subtype definitions

/* Subtypes < 0  are user defined
 * Subtype  0    means "untyped"
 * Subtypes > 0  are Firebird defined
 */

// BRS 29-Apr-2004
// replace those constants with public defined ones isc_blob_*
//
//const int BLOB_untyped	= 0;
//
//const int BLOB_text		= 1;
//const int BLOB_blr		= 2;
//const int BLOB_acl		= 3;
//const int BLOB_ranges	= 4;
//const int BLOB_summary	= 5;
//const int BLOB_format	= 6;
//const int BLOB_tra		= 7;
//const int BLOB_extfile	= 8;
//const int BLOB_max_predefined_subtype = 9;
//

// Column Limits (in bytes)

inline constexpr ULONG MAX_COLUMN_SIZE = 32767;
inline constexpr ULONG MAX_VARY_COLUMN_SIZE = MAX_COLUMN_SIZE - sizeof(USHORT);

inline constexpr ULONG MAX_STR_SIZE = 65535;

inline constexpr int TEMP_STR_LENGTH = 128;

// Metadata constants

inline constexpr unsigned METADATA_IDENTIFIER_CHAR_LEN	= 63;
inline constexpr unsigned METADATA_BYTES_PER_CHAR		= 4;

// Misc constant values

inline constexpr unsigned int USERNAME_LENGTH = METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR;

inline constexpr FB_SIZE_T MAX_SQL_IDENTIFIER_LEN = METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR;
inline constexpr FB_SIZE_T MAX_SQL_IDENTIFIER_SIZE = MAX_SQL_IDENTIFIER_LEN + 1;
inline constexpr FB_SIZE_T MAX_CONFIG_NAME_LEN = 63;

// Every character of the name may be a double-quote needed to be escaped (with another double-quote).
// The name would also need to be enclosed in double-quotes.
// There is two names, separated by a dot.
inline constexpr FB_SIZE_T MAX_QUALIFIED_NAME_TO_STRING_LEN =
	((METADATA_IDENTIFIER_CHAR_LEN * 2 + 2) * 2 + 1) * METADATA_BYTES_PER_CHAR;

inline constexpr ULONG MAX_SQL_LENGTH = 10 * 1024 * 1024; // 10 MB - just a safety check

inline constexpr const char* DB_KEY_NAME = "DB_KEY";
inline constexpr const char* RDB_DB_KEY_NAME = "RDB$DB_KEY";
inline constexpr const char* RDB_RECORD_VERSION_NAME = "RDB$RECORD_VERSION";

inline constexpr const char* NULL_STRING_MARK = "*** null ***";
inline constexpr const char* UNKNOWN_STRING_MARK = "*** unknown ***";

inline constexpr const char* ISC_USER = "ISC_USER";
inline constexpr const char* ISC_PASSWORD = "ISC_PASSWORD";

inline constexpr const char* NULL_ROLE = "NONE";
#define ADMIN_ROLE "RDB$ADMIN"		// It's used in C-string concatenations

// User name assigned to any user granted USR_locksmith rights.
// If this name is changed, modify also the trigger in
// jrd/grant.gdl (which turns into jrd/trig.h.
inline constexpr const char* DBA_USER_NAME = "SYSDBA";

inline constexpr const char* PRIMARY_KEY		= "PRIMARY KEY";
inline constexpr const char* FOREIGN_KEY		= "FOREIGN KEY";
inline constexpr const char* UNIQUE_CNSTRT		= "UNIQUE";
inline constexpr const char* CHECK_CNSTRT		= "CHECK";
inline constexpr const char* NOT_NULL_CNSTRT	= "NOT NULL";

inline constexpr const char* REL_SCOPE_PERSISTENT	= "persistent table %s";
inline constexpr const char* REL_SCOPE_GTT_PRESERVE	= "global temporary table %s of type ON COMMIT PRESERVE ROWS";
inline constexpr const char* REL_SCOPE_GTT_DELETE	= "global temporary table %s of type ON COMMIT DELETE ROWS";
inline constexpr const char* REL_SCOPE_EXTERNAL		= "external table %s";
inline constexpr const char* REL_SCOPE_VIEW			= "view %s";
inline constexpr const char* REL_SCOPE_VIRTUAL		= "virtual table %s";

// literal strings in rdb$ref_constraints to be used to identify
// the cascade actions for referential constraints. Used
// by isql/show and isql/extract for now.

inline constexpr const char* RI_ACTION_CASCADE = "CASCADE";
inline constexpr const char* RI_ACTION_NULL    = "SET NULL";
inline constexpr const char* RI_ACTION_DEFAULT = "SET DEFAULT";
inline constexpr const char* RI_ACTION_NONE    = "NO ACTION";
inline constexpr const char* RI_RESTRICT       = "RESTRICT";

// Automatically created domains for fields with direct data type.
// Also, automatically created indices that are unique or non-unique, but not PK.
inline constexpr const char* IMPLICIT_DOMAIN_PREFIX = "RDB$";
inline constexpr int IMPLICIT_DOMAIN_PREFIX_LEN = 4;

// Automatically created indices for PKs.
inline constexpr const char* IMPLICIT_PK_PREFIX = "RDB$PRIMARY";
inline constexpr int IMPLICIT_PK_PREFIX_LEN = 11;

// The invisible "id zero" generator.
inline constexpr const char* MASTER_GENERATOR = ""; //Was "RDB$GENERATORS";

inline constexpr const char* SYSTEM_SCHEMA = "SYSTEM";
inline constexpr const char* PUBLIC_SCHEMA = "PUBLIC";
inline constexpr const char* PLG_LEGACY_SEC_SCHEMA = "PLG$LEGACY_SEC";

// Automatically created security classes for SQL objects.
// Keep in sync with trig.h
inline constexpr const char* DEFAULT_CLASS				= "SQL$DEFAULT";
inline constexpr const char* SQL_SECCLASS_GENERATOR		= "RDB$SECURITY_CLASS";
inline constexpr const char* SQL_SECCLASS_PREFIX		= "SQL$";
inline constexpr int SQL_SECCLASS_PREFIX_LEN			= 4;
inline constexpr const char* SQL_FLD_SECCLASS_PREFIX	= "SQL$GRANT";
inline constexpr int SQL_FLD_SECCLASS_PREFIX_LEN		= 9;

inline constexpr const char* SQL_DDL_SECCLASS_FORMAT	= "SQL$D%02d%s";
inline constexpr int SQL_DDL_SECCLASS_PREFIX_LEN		= 7;

inline constexpr const char* GEN_SECCLASS_PREFIX		= "GEN$";
inline constexpr int GEN_SECCLASS_PREFIX_LEN			= 4;

inline constexpr const char* PROCEDURES_GENERATOR = "RDB$PROCEDURES";
inline constexpr const char* FUNCTIONS_GENERATOR = "RDB$FUNCTIONS";
inline constexpr const char* const CONSTANTS_GENERATOR = "RDB$CONSTANTS";

// Automatically created check constraints for unnamed PRIMARY and UNIQUE declarations.
inline constexpr const char* IMPLICIT_INTEGRITY_PREFIX = "INTEG_";
inline constexpr int IMPLICIT_INTEGRITY_PREFIX_LEN = 6;

// Default publication name
inline constexpr const char* DEFAULT_PUBLICATION = "RDB$DEFAULT";

//*****************************************
// System flag meaning - mainly Firebird.
//*****************************************

enum fb_sysflag {
	fb_sysflag_user = 0,
	fb_sysflag_system = 1,
	fb_sysflag_qli = 2,
	fb_sysflag_check_constraint = 3,
	fb_sysflag_referential_constraint = 4,
	fb_sysflag_view_check = 5,
	fb_sysflag_identity_generator = 6
};

enum ViewContextType {
	VCT_TABLE,
	VCT_VIEW,
	VCT_PROCEDURE
};

enum IdentityType {
	IDENT_TYPE_ALWAYS,
	IDENT_TYPE_BY_DEFAULT
};

enum SubRoutineType
{
	SUB_ROUTINE_TYPE_PSQL
};

// UDF Arguments are numbered from 0 to MAX_UDF_ARGUMENTS --
// argument 0 is reserved for the return-type of the UDF

inline constexpr unsigned MAX_UDF_ARGUMENTS	= 15;

// Maximum length of single line returned from pretty printer
inline constexpr int PRETTY_BUFFER_SIZE = 1024;

inline constexpr int MAX_INDEX_SEGMENTS = 16;

// Maximum index key length (must be in sync with MAX_PAGE_SIZE in ods.h)
inline constexpr ULONG MAX_KEY = 8192; // Maximum page size possible divide by 4 (MAX_PAGE_SIZE / 4)

inline constexpr USHORT SQL_MATCH_1_CHAR	= '_';
inline constexpr USHORT SQL_MATCH_ANY_CHARS	= '%';

inline constexpr size_t MAX_CONTEXT_VARS = 1000; // Maximum number of context variables allowed for a single object

// Time precision limits and defaults for TIME/TIMESTAMP values.
// Currently they're applied to CURRENT_TIME[STAMP] expressions only.

// Should be more than 6 as per SQL spec, but we don't support more than 3 yet
inline constexpr size_t MAX_TIME_PRECISION			= 3;
// Consistent with the SQL spec
inline constexpr size_t DEFAULT_TIME_PRECISION		= 0;
// Should be 6 as per SQL spec
inline constexpr size_t DEFAULT_TIMESTAMP_PRECISION	= 3;

// SQL spec requires an implementation-specific default (6.1 <data type>, syntax rules 6 (VARBINARY) and 7 (VARCHAR))
inline constexpr size_t DEFAULT_VARCHAR_LENGTH = 255;
inline constexpr size_t DEFAULT_VARBINARY_LENGTH = 255;

// SQL spec requires a default length of 1 (6.1 <data type>, syntax rule 5)
inline constexpr size_t DEFAULT_CHAR_LENGTH = 1;
inline constexpr size_t DEFAULT_BINARY_LENGTH = 1;

inline constexpr size_t MAX_ARRAY_DIMENSIONS = 16;

inline constexpr size_t MAX_SORT_ITEMS = 255; // ORDER BY f1,...,f255

inline constexpr size_t MAX_DB_PER_TRANS = 256; // A multi-db txn can span up to 256 dbs

// relation types

enum rel_t {
	rel_persistent = 0,
	rel_view = 1,
	rel_external = 2,
	rel_virtual = 3,
	rel_temp_preserve = 4,
	rel_temp_delete = 5
};

// procedure types

enum prc_t {
	prc_legacy = 0,
	prc_selectable = 1,
	prc_executable = 2
};

// procedure parameter mechanism

enum prm_mech_t {
	prm_mech_normal = 0,
	prm_mech_type_of = 1
};

// states

enum mon_state_t {
	mon_state_idle = 0,
	mon_state_active = 1,
	mon_state_stalled = 2
};

// shutdown modes (match hdr_nbak_* in ods.h)

enum shut_mode_t {
	shut_mode_online = 0,
	shut_mode_multi = 1,
	shut_mode_single = 2,
	shut_mode_full = 3
};

// backup states (match hdr_backup_* in ods.h)

enum backup_state_t {
	backup_state_unknown = -1,
	backup_state_normal = 0,
	backup_state_stalled = 1,
	backup_state_merge = 2
};

// transaction isolation levels

enum tra_iso_mode_t {
	iso_mode_consistency = 0,
	iso_mode_concurrency = 1,
	iso_mode_rc_version = 2,
	iso_mode_rc_no_version = 3,
	iso_mode_rc_read_consistency = 4
};

// statistics groups

enum stat_group_t {
	stat_database = 0,
	stat_attachment = 1,
	stat_transaction = 2,
	stat_statement = 3,
	stat_call = 4,
	stat_cmp_statement = 5
};

enum InfoType
{
	INFO_TYPE_CONNECTION_ID = 1,
	INFO_TYPE_TRANSACTION_ID = 2,
	INFO_TYPE_GDSCODE = 3,
	INFO_TYPE_SQLCODE = 4,
	INFO_TYPE_ROWS_AFFECTED = 5,
	INFO_TYPE_TRIGGER_ACTION = 6,
	INFO_TYPE_SQLSTATE = 7,
	INFO_TYPE_EXCEPTION = 8,
	INFO_TYPE_ERROR_MSG = 9,
	INFO_TYPE_SESSION_RESETTING = 10,
	MAX_INFO_TYPE
};

// Replica modes (match hdr_replica_* in ods.h)

enum ReplicaMode
{
	REPLICA_NONE = 0,
	REPLICA_READ_ONLY = 1,
	REPLICA_READ_WRITE = 2
};

enum TriggerType
{
	PRE_STORE_TRIGGER = 1,
	POST_STORE_TRIGGER = 2,
	PRE_MODIFY_TRIGGER = 3,
	POST_MODIFY_TRIGGER = 4,
	PRE_ERASE_TRIGGER = 5,
	POST_ERASE_TRIGGER = 6
};

enum TriggerAction
{
	// Order should be maintained because the numbers are stored in BLR
	// and should be in sync with IExternalTrigger::ACTION_* .
	TRIGGER_INSERT = 1,
	TRIGGER_UPDATE = 2,
	TRIGGER_DELETE = 3,
	TRIGGER_CONNECT = 4,
	TRIGGER_DISCONNECT = 5,
	TRIGGER_TRANS_START = 6,
	TRIGGER_TRANS_COMMIT = 7,
	TRIGGER_TRANS_ROLLBACK = 8,
	TRIGGER_DDL = 9
};

inline constexpr unsigned TRIGGER_TYPE_SHIFT		= 13;
inline constexpr FB_UINT64 TRIGGER_TYPE_MASK		= (QUADCONST(3) << TRIGGER_TYPE_SHIFT);

inline constexpr FB_UINT64 TRIGGER_TYPE_DML			= (QUADCONST(0) << TRIGGER_TYPE_SHIFT);
inline constexpr FB_UINT64 TRIGGER_TYPE_DB			= (QUADCONST(1) << TRIGGER_TYPE_SHIFT);
inline constexpr FB_UINT64 TRIGGER_TYPE_DDL			= (QUADCONST(2) << TRIGGER_TYPE_SHIFT);

inline constexpr unsigned DB_TRIGGER_CONNECT		= 0;
inline constexpr unsigned DB_TRIGGER_DISCONNECT		= 1;
inline constexpr unsigned DB_TRIGGER_TRANS_START	= 2;
inline constexpr unsigned DB_TRIGGER_TRANS_COMMIT	= 3;
inline constexpr unsigned DB_TRIGGER_TRANS_ROLLBACK	= 4;
inline constexpr unsigned DB_TRIGGER_DDL			= 5;
inline constexpr unsigned DB_TRIGGERS_COUNT			= 6;

static inline constexpr const char* DDL_TRIGGER_ACTION_NAMES[][2] =
{
	{NULL, NULL},
	{"CREATE", "TABLE"},
	{"ALTER", "TABLE"},
	{"DROP", "TABLE"},
	{"CREATE", "PROCEDURE"},
	{"ALTER", "PROCEDURE"},
	{"DROP", "PROCEDURE"},
	{"CREATE", "FUNCTION"},
	{"ALTER", "FUNCTION"},
	{"DROP", "FUNCTION"},
	{"CREATE", "TRIGGER"},
	{"ALTER", "TRIGGER"},
	{"DROP", "TRIGGER"},
	{"", ""}, {"", ""}, {"", ""},	// gap for TRIGGER_TYPE_MASK - 3 bits
	{"CREATE", "EXCEPTION"},
	{"ALTER", "EXCEPTION"},
	{"DROP", "EXCEPTION"},
	{"CREATE", "VIEW"},
	{"ALTER", "VIEW"},
	{"DROP", "VIEW"},
	{"CREATE", "DOMAIN"},
	{"ALTER", "DOMAIN"},
	{"DROP", "DOMAIN"},
	{"CREATE", "ROLE"},
	{"ALTER", "ROLE"},
	{"DROP", "ROLE"},
	{"CREATE", "INDEX"},
	{"ALTER", "INDEX"},
	{"DROP", "INDEX"},
	{"CREATE", "SEQUENCE"},
	{"ALTER", "SEQUENCE"},
	{"DROP", "SEQUENCE"},
	{"CREATE", "USER"},
	{"ALTER", "USER"},
	{"DROP", "USER"},
	{"CREATE", "COLLATION"},
	{"DROP", "COLLATION"},
	{"ALTER", "CHARACTER SET"},
	{"CREATE", "PACKAGE"},
	{"ALTER", "PACKAGE"},
	{"DROP", "PACKAGE"},
	{"CREATE", "PACKAGE BODY"},
	{"DROP", "PACKAGE BODY"},
	{"CREATE", "MAPPING"},
	{"ALTER", "MAPPING"},
	{"DROP", "MAPPING"},
	{"CREATE", "SCHEMA"},
	{"ALTER", "SCHEMA"},
	{"DROP", "SCHEMA"}
};

inline constexpr int DDL_TRIGGER_BEFORE	= 0;
inline constexpr int DDL_TRIGGER_AFTER	= 1;

inline constexpr FB_UINT64 DDL_TRIGGER_ANY				= 0x7FFFFFFFFFFFFFFFULL & ~(FB_UINT64) TRIGGER_TYPE_MASK & ~1ULL;

inline constexpr int DDL_TRIGGER_CREATE_TABLE			= 1;
inline constexpr int DDL_TRIGGER_ALTER_TABLE			= 2;
inline constexpr int DDL_TRIGGER_DROP_TABLE				= 3;
inline constexpr int DDL_TRIGGER_CREATE_PROCEDURE		= 4;
inline constexpr int DDL_TRIGGER_ALTER_PROCEDURE		= 5;
inline constexpr int DDL_TRIGGER_DROP_PROCEDURE			= 6;
inline constexpr int DDL_TRIGGER_CREATE_FUNCTION		= 7;
inline constexpr int DDL_TRIGGER_ALTER_FUNCTION			= 8;
inline constexpr int DDL_TRIGGER_DROP_FUNCTION			= 9;
inline constexpr int DDL_TRIGGER_CREATE_TRIGGER			= 10;
inline constexpr int DDL_TRIGGER_ALTER_TRIGGER			= 11;
inline constexpr int DDL_TRIGGER_DROP_TRIGGER			= 12;
// gap for TRIGGER_TYPE_MASK - 3 bits
inline constexpr int DDL_TRIGGER_CREATE_EXCEPTION		= 16;
inline constexpr int DDL_TRIGGER_ALTER_EXCEPTION		= 17;
inline constexpr int DDL_TRIGGER_DROP_EXCEPTION			= 18;
inline constexpr int DDL_TRIGGER_CREATE_VIEW			= 19;
inline constexpr int DDL_TRIGGER_ALTER_VIEW				= 20;
inline constexpr int DDL_TRIGGER_DROP_VIEW				= 21;
inline constexpr int DDL_TRIGGER_CREATE_DOMAIN			= 22;
inline constexpr int DDL_TRIGGER_ALTER_DOMAIN			= 23;
inline constexpr int DDL_TRIGGER_DROP_DOMAIN			= 24;
inline constexpr int DDL_TRIGGER_CREATE_ROLE			= 25;
inline constexpr int DDL_TRIGGER_ALTER_ROLE				= 26;
inline constexpr int DDL_TRIGGER_DROP_ROLE				= 27;
inline constexpr int DDL_TRIGGER_CREATE_INDEX			= 28;
inline constexpr int DDL_TRIGGER_ALTER_INDEX			= 29;
inline constexpr int DDL_TRIGGER_DROP_INDEX				= 30;
inline constexpr int DDL_TRIGGER_CREATE_SEQUENCE		= 31;
inline constexpr int DDL_TRIGGER_ALTER_SEQUENCE			= 32;
inline constexpr int DDL_TRIGGER_DROP_SEQUENCE			= 33;
inline constexpr int DDL_TRIGGER_CREATE_USER			= 34;
inline constexpr int DDL_TRIGGER_ALTER_USER				= 35;
inline constexpr int DDL_TRIGGER_DROP_USER				= 36;
inline constexpr int DDL_TRIGGER_CREATE_COLLATION		= 37;
inline constexpr int DDL_TRIGGER_DROP_COLLATION			= 38;
inline constexpr int DDL_TRIGGER_ALTER_CHARACTER_SET	= 39;
inline constexpr int DDL_TRIGGER_CREATE_PACKAGE			= 40;
inline constexpr int DDL_TRIGGER_ALTER_PACKAGE			= 41;
inline constexpr int DDL_TRIGGER_DROP_PACKAGE			= 42;
inline constexpr int DDL_TRIGGER_CREATE_PACKAGE_BODY	= 43;
inline constexpr int DDL_TRIGGER_DROP_PACKAGE_BODY		= 44;
inline constexpr int DDL_TRIGGER_CREATE_MAPPING			= 45;
inline constexpr int DDL_TRIGGER_ALTER_MAPPING			= 46;
inline constexpr int DDL_TRIGGER_DROP_MAPPING			= 47;
inline constexpr int DDL_TRIGGER_CREATE_SCHEMA			= 48;
inline constexpr int DDL_TRIGGER_ALTER_SCHEMA			= 49;
inline constexpr int DDL_TRIGGER_DROP_SCHEMA			= 50;

// that's how database trigger action types are encoded
//    (TRIGGER_TYPE_DB | type)

// that's how DDL trigger action types are encoded
//    (TRIGGER_TYPE_DDL | DDL_TRIGGER_{AFTER | BEFORE} [ | DDL_TRIGGER_??? ...])

// switches for username and password used when an username and/or password
// is specified by the client application
#define USERNAME_SWITCH "USER"
#define PASSWORD_SWITCH "PASSWORD"

// The highest transaction number possible
inline constexpr TraNumber MAX_TRA_NUMBER = 0x0000FFFFFFFFFFFF; // ~2.8 * 10^14

// Number of streams, conjuncts, indices that will be statically allocated
// in various arrays. Larger numbers will have to be allocated dynamically
inline constexpr unsigned OPT_STATIC_ITEMS = 16;
inline constexpr unsigned OPT_STATIC_STREAMS = 64;

#define CURRENT_ENGINE "Engine14"
#define EMBEDDED_PROVIDERS "Providers=" CURRENT_ENGINE

// Features set for current version of engine provider
#define ENGINE_FEATURES {fb_feature_multi_statements, \
						 fb_feature_multi_transactions, \
						 fb_feature_session_reset, \
						 fb_feature_read_consistency, \
						 fb_feature_statement_timeout, \
						 fb_feature_statement_long_life}

inline constexpr int WITH_GRANT_OPTION = 1;
inline constexpr int WITH_ADMIN_OPTION = 2;

// Max length of the string returned by ERROR_TEXT context variable
inline constexpr USHORT MAX_ERROR_MSG_LENGTH = 1024 * METADATA_BYTES_PER_CHAR; // 1024 UTF-8 characters

// Prefix of index that's getting dropped
inline constexpr const char* TEMP_DEPEND = "RDB$TEMP_DEPEND";

#endif // JRD_CONSTANTS_H
