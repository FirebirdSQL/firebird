/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ids.h
 *	DESCRIPTION:	System relation field numbers
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

#define RELATION(...) enum : USHORT {
#define FIELD(field_id, ...) field_id,
#define END_RELATION };

#include "relations.h"

#undef RELATION
#undef FIELD
#undef END_RELATION


// Because it is ODS-related header, an additional check
// to ensure compatibility: position of one field for each table is checked.
// This field don't have to be the last, any one is good.

	static_assert(f_pag_type == 3, "Wrong field id");
	static_assert(f_dat_sql_security == 5, "Wrong field id");
	static_assert(f_fld_owner == 29, "Wrong field id");
	static_assert(f_seg_statistics == 3, "Wrong field id");
	static_assert(f_idx_statistics == 12, "Wrong field id");
	static_assert(f_rfr_identity_type == 20, "Wrong field id");
	static_assert(f_rel_sql_security == 17, "Wrong field id");
	static_assert(f_vrl_pkg_name == 5, "Wrong field id");
	static_assert(f_fmt_desc == 2, "Wrong field id");
	static_assert(f_cls_desc == 2, "Wrong field id");
	static_assert(f_file_shad_num == 5, "Wrong field id");
	static_assert(f_typ_sys_flag == 4, "Wrong field id");
	static_assert(f_trg_sql_security == 14, "Wrong field id");
	static_assert(f_dpd_pkg_name == 5, "Wrong field id");
	static_assert(f_fun_sql_security == 20, "Wrong field id");
	static_assert(f_arg_desc == 21, "Wrong field id");
	static_assert(f_flt_owner == 8, "Wrong field id");
	static_assert(f_msg_msg == 2, "Wrong field id");
	static_assert(f_prv_o_type == 7, "Wrong field id");
	static_assert(f_trn_desc == 3, "Wrong field id");
	static_assert(f_gen_increment == 7, "Wrong field id");
	static_assert(f_dims_upper == 3, "Wrong field id");
	static_assert(f_rcon_iname == 5, "Wrong field id");
	static_assert(f_refc_del_rul == 4, "Wrong field id");
	static_assert(f_ccon_tname == 1, "Wrong field id");
	static_assert(f_log_flags == 5, "Wrong field id");
	static_assert(f_prc_sql_security == 18, "Wrong field id");
	static_assert(f_prm_pkg_name == 14, "Wrong field id");
	static_assert(f_cs_owner == 10, "Wrong field id");
	static_assert(f_coll_owner == 10, "Wrong field id");
	static_assert(f_xcp_owner == 6, "Wrong field id");
	static_assert(f_rol_sys_priv == 5, "Wrong field id");
	static_assert(f_backup_name == 5, "Wrong field id");
	static_assert(f_mon_db_crypt_state == 22, "Wrong field id");
	static_assert(f_mon_att_remote_crypt == 25, "Wrong field id");
	static_assert(f_mon_tra_stat_id == 12, "Wrong field id");
	static_assert(f_mon_stmt_timer == 9, "Wrong field id");
	static_assert(f_mon_call_pkg_name == 9, "Wrong field id");
	static_assert(f_mon_io_page_marks == 5, "Wrong field id");
	static_assert(f_mon_rec_imgc == 16, "Wrong field id");
	static_assert(f_mon_ctx_var_value == 3, "Wrong field id");
	static_assert(f_mon_mem_max_alloc == 5, "Wrong field id");
	static_assert(f_pkg_sql_security == 8, "Wrong field id");
	static_assert(f_sec_plugin == 7, "Wrong field id");
	static_assert(f_sec_attr_plugin == 3, "Wrong field id");
	static_assert(f_map_desc == 9, "Wrong field id");
	static_assert(f_sec_map_to == 7, "Wrong field id");
	static_assert(f_crt_u_type == 1, "Wrong field id");
	static_assert(f_sec_crt_u_type == 1, "Wrong field id");
	static_assert(f_mon_tab_rec_stat_id == 3, "Wrong field id");
	static_assert(f_tz_name == 1, "Wrong field id");
	static_assert(f_mon_ltt_type == 4, "Wrong field id");
	static_assert(f_const_name == 0, "Wrong field id");
	static_assert(f_const_id == 1, "Wrong field id");
	static_assert(f_const_package == 2, "Wrong field id");
	static_assert(f_const_field == 3, "Wrong field id");
	static_assert(f_const_private_flag == 4, "Wrong field id");
	static_assert(f_const_blr == 5, "Wrong field id");
	static_assert(f_const_source == 6, "Wrong field id");
