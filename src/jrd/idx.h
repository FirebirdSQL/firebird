/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		idx
 *	DESCRIPTION:	Declarations for index initialization
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
 * 2002.08.26 Dmitry Yemanov: new indices on system tables
 * 2002.09.16 Nickolay Samofatov: one more system index
 */

#ifndef JRD_IDX_H
#define JRD_IDX_H

/* Indices to be created */

/* Maxinum number of segments in any existing system index */
const int  INI_IDX_MAX_SEGMENTS		= 4;

struct ini_idx_t
{
	UCHAR ini_idx_index_id;
	UCHAR ini_idx_relid;
	UCHAR ini_idx_flags;
	UCHAR ini_idx_segment_count;
	USHORT ini_idx_ods;
	struct ini_idx_segment_t
	{
		UCHAR ini_idx_rfld_id;
		UCHAR ini_idx_type;
	} ini_idx_segment[INI_IDX_MAX_SEGMENTS];
};

/* Encoded descriptions of system indices */

using Jrd::idx_unique;
using Jrd::idx_metadata;
using Jrd::idx_numeric;
using Jrd::idx_string;
using Jrd::idx_descending;
using Jrd::idx_timestamp_tz;

#define INDEX(id, rel, unique, count, ods) {(id), (UCHAR) (rel), (unique), (count), (ods), {
#define SEGMENT(fld, type) {(fld), (type)}

static const struct ini_idx_t indices[] =
{
	// define index RDB$INDEX_0 for RDB$RELATIONS unique RDB$SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(0, rel_relations, idx_unique, 2, ODS_14_0)
		SEGMENT(f_rel_schema, idx_metadata),	// schema name
		SEGMENT(f_rel_name, idx_metadata)		// relation name
	}},
	// define index RDB$INDEX_1 for RDB$RELATIONS RDB$RELATION_ID;
	INDEX(1, rel_relations, 0, 1, ODS_13_0)
		SEGMENT(f_rel_id, idx_numeric)			// relation id
	}},
	// define index RDB$INDEX_2 for RDB$FIELDS unique RDB$SCHEMA_NAME, RDB$FIELD_NAME;
	INDEX(2, rel_fields, idx_unique, 2, ODS_14_0)
		SEGMENT(f_fld_schema, idx_metadata),	// schema name
		SEGMENT(f_fld_name, idx_metadata)		// field name
	}},
	// define index RDB$INDEX_3 for RDB$RELATION_FIELDS RDB$FIELD_SOURCE_SCHEMA_NAME, RDB$FIELD_SOURCE;
	INDEX(3, rel_rfr, 0, 2, ODS_14_0)
		SEGMENT(f_rfr_field_source_schema, idx_metadata),	// field source schema name
		SEGMENT(f_rfr_sname, idx_metadata)		// field source name
	}},
	// define index RDB$INDEX_4 for RDB$RELATION_FIELDS RDB$SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(4, rel_rfr, 0, 2, ODS_14_0)
		SEGMENT(f_rfr_schema, idx_metadata),	// schema name
		SEGMENT(f_rfr_rname, idx_metadata)		// relation name in RFR
	}},
	// define index RDB$INDEX_5 for RDB$INDICES unique RDB$SCHEMA_NAME, RDB$INDEX_NAME;
	INDEX(5, rel_indices, idx_unique, 2, ODS_14_0)
		SEGMENT(f_idx_schema, idx_metadata),	// schema name
		SEGMENT(f_idx_name, idx_metadata)		// index name
	}},
	// define index RDB$INDEX_6 for RDB$INDEX_SEGMENTS RDB$SCHEMA_NAME, RDB$INDEX_NAME;
	INDEX(6, rel_segments, 0, 2, ODS_14_0)
		SEGMENT(f_seg_schema, idx_metadata),	// schema name
		SEGMENT(f_seg_name, idx_metadata)		// index name in seg
	}},
	// define index RDB$INDEX_7 for RDB$SECURITY_CLASSES unique RDB$SECURITY_CLASS;
	INDEX(7, rel_classes, idx_unique, 1, ODS_13_0)
		SEGMENT(f_cls_class, idx_metadata)		// security class
	}},
	// define index RDB$INDEX_8 for RDB$TRIGGERS unique RDB$SCHEMA_NAME, RDB$TRIGGER_NAME;
	INDEX(8, rel_triggers, idx_unique, 2, ODS_14_0)
		SEGMENT(f_trg_schema, idx_metadata),	// schema name
		SEGMENT(f_trg_name, idx_metadata)		// trigger name
	}},
	// define index RDB$INDEX_9 for RDB$FUNCTIONS unique RDB$SCHEMA_NAME, RDB$PACKAGE_NAME, RDB$FUNCTION_NAME;
	INDEX(9, rel_funs, idx_unique, 3, ODS_14_0)
		SEGMENT(f_fun_schema, idx_metadata),	// schema name
		SEGMENT(f_fun_pkg_name, idx_metadata),	// package name
		SEGMENT(f_fun_name, idx_metadata)		// function name
	}},
	// define index RDB$INDEX_10 for RDB$FUNCTION_ARGUMENTS RDB$SCHEMA_NAME, RDB$PACKAGE_NAME, RDB$FUNCTION_NAME;
	INDEX(10, rel_args, 0, 3, ODS_14_0)
		SEGMENT(f_arg_schema, idx_metadata),	// schema name
		SEGMENT(f_arg_pkg_name, idx_metadata),	// package name
		SEGMENT(f_arg_fun_name, idx_metadata)	// function name
	}},
	// define index RDB$INDEX_11 for RDB$GENERATORS unique RDB$SCHEMA_NAME, RDB$GENERATOR_NAME;
	INDEX(11, rel_gens, idx_unique, 2, ODS_14_0)
		SEGMENT(f_gen_schema, idx_metadata),	// schema name
		SEGMENT(f_gen_name, idx_metadata)		// Generator name
	}},
	// define index RDB$INDEX_12 for RDB$RELATION_CONSTRAINTS unique RDB$SCHEMA_NAME, RDB$CONSTRAINT_NAME;
	INDEX(12, rel_rcon, idx_unique, 2, ODS_14_0)
		SEGMENT(f_rcon_schema, idx_metadata),	// schema name
		SEGMENT(f_rcon_cname, idx_metadata)		// constraint name
	}},
	// define index RDB$INDEX_13 for RDB$REF_CONSTRAINTS unique RDB$SCHEMA_NAME, RDB$CONSTRAINT_NAME;
	INDEX(13, rel_refc, idx_unique, 2, ODS_14_0)
		SEGMENT(f_refc_schema, idx_metadata),	// schema name
		SEGMENT(f_refc_cname, idx_metadata)		// constraint name
	}},
	// define index RDB$INDEX_14 for RDB$CHECK_CONSTRAINTS RDB$SCHEMA_NAME, RDB$CONSTRAINT_NAME;
	INDEX(14, rel_ccon, 0, 2, ODS_14_0)
		SEGMENT(f_ccon_schema, idx_metadata),	// schema name
		SEGMENT(f_ccon_cname, idx_metadata)		// constraint name
	}},
	// define index RDB$INDEX_15 for RDB$RELATION_FIELDS unique RDB$FIELD_NAME, RDB$SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(15, rel_rfr, idx_unique, 3, ODS_14_0)
		SEGMENT(f_rfr_fname, idx_metadata),		// field name
		SEGMENT(f_rfr_schema, idx_metadata),	// schema name
		SEGMENT(f_rfr_rname, idx_metadata)		// relation name
	}},
	// define index RDB$INDEX_16 for RDB$FORMATS RDB$RELATION_ID, RDB$FORMAT;
	INDEX(16, rel_formats, 0, 2, ODS_13_0)
		SEGMENT(f_fmt_rid, idx_numeric),		// relation id
		SEGMENT(f_fmt_format, idx_numeric)		// format id
	}},
	// define index RDB$INDEX_17 for RDB$FILTERS RDB$INPUT_SUB_TYPE, RDB$OUTPUT_SUB_TYPE;
	INDEX(17, rel_filters, idx_unique, 2, ODS_13_0)
		SEGMENT(f_flt_input, idx_numeric),		// input subtype
		SEGMENT(f_flt_output, idx_numeric)		// output subtype
	}},
	// define index RDB$INDEX_18 for RDB$PROCEDURE_PARAMETERS unique RDB$SCHEMA_NAME, RDB$PACKAGE_NAME,
	// RDB$PROCEDURE_NAME, RDB$PARAMETER_NAME;
	INDEX(18, rel_prc_prms, idx_unique, 4, ODS_14_0)
		SEGMENT(f_prm_schema, idx_metadata),	// schema name
		SEGMENT(f_prm_pkg_name, idx_metadata),	// package name
		SEGMENT(f_prm_procedure, idx_metadata),	// procedure name
		SEGMENT(f_prm_name, idx_metadata)		// parameter name
	}},
	// define index RDB$INDEX_19 for RDB$CHARACTER_SETS unique RDB$SCHEMA_NAME, RDB$CHARACTER_SET_NAME;
	INDEX(19, rel_charsets, idx_unique, 2, ODS_14_0)
		SEGMENT(f_cs_schema, idx_metadata),	// schema name
		SEGMENT(f_cs_cs_name, idx_metadata)		// character set name
	}},
	// define index RDB$INDEX_20 for RDB$COLLATIONS unique RDB$SCHEMA_NAME, RDB$COLLATION_NAME;
	INDEX(20, rel_collations, idx_unique, 2, ODS_14_0)
		SEGMENT(f_coll_schema, idx_metadata),	// schema name
		SEGMENT(f_coll_name, idx_metadata)		// collation name
	}},
	// define index RDB$INDEX_21 for RDB$PROCEDURES unique RDB$SCHEMA_NAME, RDB$PACKAGE_NAME, RDB$PROCEDURE_NAME;
	INDEX(21, rel_procedures, idx_unique, 3, ODS_14_0)
		SEGMENT(f_prc_schema, idx_metadata),	// schema name
		SEGMENT(f_prc_pkg_name, idx_metadata),	// package name
		SEGMENT(f_prc_name, idx_metadata)		// procedure name
	}},
	// define index RDB$INDEX_22 for RDB$PROCEDURES unique RDB$PROCEDURE_ID;
	INDEX(22, rel_procedures, idx_unique, 1, ODS_13_0)
		SEGMENT(f_prc_id, idx_numeric)			// procedure id
	}},
	// define index RDB$INDEX_23 for RDB$EXCEPTIONS unique RDB$SCHEMA_NAME, RDB$EXCEPTION_NAME;
	INDEX(23, rel_exceptions, idx_unique, 2, ODS_14_0)
		SEGMENT(f_xcp_schema, idx_metadata),	// schema name
		SEGMENT(f_xcp_name, idx_metadata)		// exception name
	}},
	// define index RDB$INDEX_24 for RDB$EXCEPTIONS unique RDB$EXCEPTION_NUMBER;
	INDEX(24, rel_exceptions, idx_unique, 1, ODS_13_0)
		SEGMENT(f_xcp_number, idx_numeric)		// exception number
	}},
	// define index RDB$INDEX_25 for RDB$CHARACTER_SETS unique RDB$CHARACTER_SET_ID;
	INDEX(25, rel_charsets, idx_unique, 1, ODS_13_0)
		SEGMENT(f_cs_id, idx_numeric)			// character set id
	}},
	// define index RDB$INDEX_26 for RDB$COLLATIONS unique RDB$COLLATION_ID, RDB$CHARACTER_SET_ID;
	INDEX(26, rel_collations, idx_unique, 2, ODS_13_0)
		SEGMENT(f_coll_id, idx_numeric),		// collation id
		SEGMENT(f_coll_cs_id, idx_numeric)		// character set id
	}},
	// define index RDB$INDEX_27 for RDB$DEPENDENCIES RDB$DEPENDENT_SCHEMA_NAME, RDB$DEPENDENT_NAME, RDB$DEPENDENT_TYPE;
	INDEX(27, rel_dpds, 0, 3, ODS_14_0)
		SEGMENT(f_dpd_schema, idx_metadata),	// dependent schema name
		SEGMENT(f_dpd_name, idx_metadata),		// dependent name
		SEGMENT(f_dpd_type, idx_numeric)		// dependent type
	}},
	// define index RDB$INDEX_28 for RDB$DEPENDENCIES RDB$DEPENDED_ON_SCHEMA_NAME, RDB$DEPENDED_ON_NAME,
	// RDB$DEPENDED_ON_TYPE, RDB$FIELD_NAME;
	INDEX(28, rel_dpds, 0, 4, ODS_14_0)
		SEGMENT(f_dpd_o_schema, idx_metadata),	// dependent on schema name
		SEGMENT(f_dpd_o_name, idx_metadata),	// dependent on name
		SEGMENT(f_dpd_o_type, idx_numeric),		// dependent on type
		SEGMENT(f_dpd_f_name, idx_metadata)		// field name
	}},
	// define index RDB$INDEX_29 for RDB$USER_PRIVILEGES RDB$RELATION_SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(29, rel_priv, 0, 2, ODS_14_0)
		SEGMENT(f_prv_rel_schema, idx_metadata),	// relation schema name
		SEGMENT(f_prv_rname, idx_metadata)			// relation name
	}},
	// define index RDB$INDEX_30 for RDB$USER_PRIVILEGES RDB$USER_SCHEMA_NAME, RDB$USER;
	INDEX(30, rel_priv, 0, 2, ODS_14_0)
		SEGMENT(f_prv_user_schema, idx_metadata),	// granted user schema name
		SEGMENT(f_prv_user, idx_metadata)			// granted user
	}},
	// define index RDB$INDEX_31 for RDB$INDICES RDB$SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(31, rel_indices, 0, 2, ODS_14_0)
		SEGMENT(f_idx_schema, idx_metadata),	// schema name
		SEGMENT(f_idx_relation, idx_metadata)	// indexed relation
	}},
	// define index RDB$INDEX_32 for RDB$TRANSACTIONS unique RDB$TRANSACTION_ID;
	INDEX(32, rel_trans, idx_unique, 1, ODS_13_0)
		SEGMENT(f_trn_id, idx_numeric)			// transaction id
	}},
	// define index RDB$INDEX_33 for RDB$VIEW_RELATIONS RDB$SCHEMA_NAME, RDB$VIEW_NAME;
	INDEX(33, rel_vrel, 0, 2, ODS_14_0)
		SEGMENT(f_vrl_schema, idx_metadata),	// schema name
		SEGMENT(f_vrl_vname, idx_metadata)		// view name
	}},
	// define index RDB$INDEX_34 for RDB$VIEW_RELATIONS RDB$RELATION_SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(34, rel_vrel, 0, 2, ODS_14_0)
		SEGMENT(f_vrl_rname_schema, idx_metadata),	// schema name
		SEGMENT(f_vrl_rname, idx_metadata)		// base relation name
	}},
	// define index RDB$INDEX_35 for RDB$TRIGGER_MESSAGES RDB$SCHEMA_NAME, RDB$TRIGGER_NAME;
	INDEX(35, rel_msgs, 0, 2, ODS_14_0)
		SEGMENT(f_msg_schema, idx_metadata),	// schema name
		SEGMENT(f_msg_trigger, idx_metadata)	// trigger name
	}},
	// define index RDB$INDEX_36 for RDB$FIELD_DIMENSIONS RDB$SCHEMA_NAME, RDB$FIELD_NAME;
	INDEX(36, rel_dims, 0, 2, ODS_14_0)
		SEGMENT(f_dims_schema, idx_metadata),	// schema name
		SEGMENT(f_dims_fname, idx_metadata)		// array name
	}},
	// define index RDB$INDEX_37 for RDB$TYPES RDB$TYPE_NAME;
	INDEX(37, rel_types, 0, 1, ODS_13_0)
		SEGMENT(f_typ_name, idx_metadata)		// type name
	}},
	// define index RDB$INDEX_38 for RDB$TRIGGERS RDB$SCHEMA_NAME, RDB$RELATION_NAME;
	INDEX(38, rel_triggers, 0, 2, ODS_14_0)
		SEGMENT(f_trg_schema, idx_metadata),	// schema name
		SEGMENT(f_trg_rname, idx_metadata)		// triggered relation
	}},
	// define index RDB$INDEX_39 for RDB$ROLES unique RDB$ROLE_NAME;
	INDEX(39, rel_roles, idx_unique, 1, ODS_13_0)
		SEGMENT(f_rol_name, idx_metadata)		// role name
	}},
	// define index RDB$INDEX_40 for RDB$CHECK_CONSTRAINTS RDB$SCHEMA_NAME, RDB$TRIGGER_NAME;
	INDEX(40, rel_ccon, 0, 2, ODS_14_0)
		SEGMENT(f_ccon_schema, idx_metadata),	// schema name
		SEGMENT(f_ccon_tname, idx_metadata)		// trigger name
	}},
	// define index RDB$INDEX_41 for RDB$INDICES RDB$FOREIGN_KEY_SCHEMA_NAME, RDB$FOREIGN_KEY;
	INDEX(41, rel_indices, 0, 2, ODS_14_0)
		SEGMENT(f_idx_foreign_schema, idx_metadata),	// foreign key schema name
		SEGMENT(f_idx_foreign, idx_metadata)			// foreign key name
	}},
	// define index RDB$INDEX_42 for RDB$RELATION_CONSTRAINTS RDB$SCHEMA_NAME, RDB$RELATION_NAME, RDB$CONSTRAINT_TYPE;
	INDEX(42, rel_rcon, 0, 3, ODS_14_0)
		SEGMENT(f_rcon_schema, idx_metadata),	// schema name
		SEGMENT(f_rcon_rname, idx_metadata),	// relation name
		SEGMENT(f_rcon_ctype, idx_metadata)     // constraint type
	}},
	// define index RDB$INDEX_43 for RDB$RELATION_CONSTRAINTS RDB$SCHEMA_NAME, RDB$INDEX_NAME;
	INDEX(43, rel_rcon, 0, 2, ODS_14_0)
		SEGMENT(f_rcon_schema, idx_metadata),	// schema name
		SEGMENT(f_rcon_iname, idx_metadata),	// index name
	}},
	// define index RDB$INDEX_44 for RDB$BACKUP_HISTORY RDB$LEVEL, RDB$BACKUP_ID;
	INDEX(44, rel_backup_history, idx_unique | idx_descending, 2, ODS_13_0)
		SEGMENT(f_backup_level, idx_numeric),	// backup level
		SEGMENT(f_backup_id, idx_numeric)		// backup id
	}},
	// define index RDB$INDEX_45 for RDB$FILTERS RDB$FUNCTION_NAME;
	INDEX(45, rel_filters, idx_unique, 1, ODS_13_0)
		SEGMENT(f_flt_name, idx_metadata)		// function name
	}},
	//	define index RDB$INDEX_46 for RDB$GENERATORS unique RDB$GENERATOR_ID;
	INDEX(46, rel_gens, idx_unique, 1, ODS_13_0)
		SEGMENT(f_gen_id, idx_numeric)			// generator id
	}},
	// define index RDB$INDEX_47 for RDB$PACKAGES unique RDB$SCHEMA_NAME, RDB$PACKAGE_NAME;
	INDEX(47, rel_packages, idx_unique, 2, ODS_14_0)
		SEGMENT(f_pkg_schema, idx_metadata),	// schema name
		SEGMENT(f_pkg_name, idx_metadata)		// package name
	}},
	//	define index RDB$INDEX_48 for RDB$PROCEDURE_PARAMETERS RDB$FIELD_SOURCE_SCHEMA_NAME, RDB$FIELD_SOURCE;
	INDEX(48, rel_prc_prms, 0, 2, ODS_14_0)
		SEGMENT(f_prm_field_source_schema, idx_metadata),	// field source schema name
		SEGMENT(f_prm_sname, idx_metadata)					// field source name
	}},
	//	define index RDB$INDEX_49 for RDB$FUNCTION_ARGUMENTS RDB$FIELD_SOURCE_SCHEMA_NAME, RDB$FIELD_SOURCE;
	INDEX(49, rel_args, 0, 2, ODS_14_0)
		SEGMENT(f_arg_field_source_schema, idx_metadata),	// field source schema name
		SEGMENT(f_arg_sname, idx_metadata)					// field source name
	}},
	//	define index RDB$INDEX_50 for RDB$PROCEDURE_PARAMETERS RDB$SCHEMA_NAME, RDB$RELATION_NAME, RDB$FIELD_NAME;
	INDEX(50, rel_prc_prms, 0, 3, ODS_14_0)
		SEGMENT(f_prm_rel_schema, idx_metadata),	// relation schema name
		SEGMENT(f_prm_rname, idx_metadata),			// relation name
		SEGMENT(f_prm_fname, idx_metadata)			// field name
	}},
	//	define index RDB$INDEX_51 for RDB$FUNCTION_ARGUMENTS RDB$SCHEMA_NAME, RDB$RELATION_NAME, RDB$FIELD_NAME;
	INDEX(51, rel_args, 0, 3, ODS_14_0)
		SEGMENT(f_arg_rel_schema, idx_metadata),	// relation schema name
		SEGMENT(f_arg_rname, idx_metadata),			// relation name
		SEGMENT(f_arg_fname, idx_metadata)			// field name
	}},
	//	define index RDB$INDEX_52 for RDB$AUTH_MAPPING RDB$MAP_NAME;
	INDEX(52, rel_auth_mapping, 0, 1, ODS_13_0)
		SEGMENT(f_map_name, idx_metadata)		// mapping name
	}},
	// define index RDB$INDEX_53 for RDB$FUNCTIONS unique RDB$FUNCTION_ID;
	INDEX(53, rel_funs, idx_unique, 1, ODS_13_0)
		SEGMENT(f_fun_id, idx_numeric)			// function id
	}},
	// define index RDB$INDEX_54 for RDB$BACKUP_HISTORY RDB$GUID;
	INDEX(54, rel_backup_history, idx_unique, 1, ODS_13_0)
		SEGMENT(f_backup_guid, idx_string)		// backup guid
	}},
	// define index RDB$INDEX_55 for RDB$PUBLICATIONS unique RDB$PUBLICATION_NAME;
	INDEX(55, rel_pubs, idx_unique, 1, ODS_13_0)
		SEGMENT(f_pub_name, idx_string)		// publication name
	}},
	// define index RDB$INDEX_56 for RDB$PUBLICATION_TABLES unique RDB$TABLE_SCHEMA_NAME,
	// RDB$TABLE_NAME, RDB$PUBLICATION_NAME;
	INDEX(56, rel_pub_tables, idx_unique, 3, ODS_14_0)
		SEGMENT(f_pubtab_tab_schema, idx_metadata),	// table schema name
		SEGMENT(f_pubtab_tab_name, idx_metadata),	// table name
		SEGMENT(f_pubtab_pub_name, idx_metadata)	// publication name
	}},
	// define index RDB$INDEX_57 for RDB$BACKUP_HISTORY RDB$TIMESTAMP;
	INDEX(57, rel_backup_history, idx_descending, 1, ODS_13_1)
		SEGMENT(f_backup_time, idx_timestamp_tz)		// backup timestamp
	}},
	// define index RDB$INDEX_58 for RDB$SCHEMAS unique RDB$SCHEMA_NAME;
	INDEX(58, rel_schemas, idx_unique, 1, ODS_14_0)
		SEGMENT(f_sch_schema, idx_metadata)		// schema name
	}},
};

#define SYSTEM_INDEX_COUNT FB_NELEM(indices)

#endif /* JRD_IDX_H */
