/*
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
#include "firebird.h"
#include "gen/iberror.h"

#define entry(r)	r(int* status) {return error (status); }

entry(gds__attach_database)
	entry(gds__blob_info)
	entry(gds__cancel_blob)
	entry(gds__close_blob)
	entry(gds__commit_transaction)
	entry(gds__compile_request)
	entry(gds__create_blob)
	entry(gds__create_database)
	entry(gds__database_cleanup)
	entry(gds__database_info)
	entry(gds__detach_database)
	entry(gds__get_segment)
	entry(gds__open_blob)
	entry(gds__prepare_transaction)
	entry(gds__put_segment)
	entry(gds__reconnect_transaction)
	entry(gds__receive)
	entry(gds__release_request)
	entry(gds__request_info)
	entry(gds__rollback_transaction)
	entry(gds__seek_blob)
	entry(gds__send)
	entry(gds__start_and_send)
	entry(gds__start_request)
	entry(gds__start_multiple)
	entry(gds__start_transaction)
	entry(gds__transaction_info)
	entry(gds__unwind_request)
	entry(gds__decode_date)
	entry(gds__encode_date)
	entry(gds__print_status)
	entry(gds__sqlcode)
	entry(gds__ftof)
	entry(gds__qtoq)
	entry(gds__vtof)
	entry(gds__vtov)
	entry(gds__alloc)
	entry(gds__free)
	entry(perf_format)
	entry(perf_get_info)
	entry(perf_report)
	entry(gds__temp_file)
	entry(gds__vax_integer)
	entry(gds__encode)
	entry(gds__decode)
	entry(gds__interprete)
	entry(gds__interprete_a)
	entry(gds__edit)
	entry(blob_close)
	entry(blob__display)
	entry(blob_display)
	entry(blob__dump)
	entry(blob_dump)
	entry(blob__edit)
	entry(blob_edit)
	entry(blob_get)
	entry(blob__load)
	entry(blob_load)
	entry(bopen)
	entry(blob_open)
	entry(blob_put)
	entry(unlink)
	entry(gds__compile_request2)
	entry(gds__print_blr)
	entry(gds__version)
	entry(gds__ddl)
	entry(gds__close)
	entry(gds__declare)
	entry(gds__describe)
	entry(gds__execute)
	entry(gds__execute_immediate)
	entry(gds__fetch)
	entry(gds__open)
	entry(gds__prepare)
	entry(gds__to_sqlda)
	entry(gds__put_error)
	entry(gds__blob_size)
	entry(gds__attach_database_d)
	entry(gds__execute_immediate_d)
	entry(gds__prepare_d)
	entry(gds__fetch_a)
	entry(gds__dsql_finish)
	entry(gds__cancel_events)
	entry(gds__commit_retaining)
	entry(gds__create_blob2)
	entry(gds__enable_subsystem)
	entry(gds__msg_format)
	entry(gds__open_blob2)
	entry(gds__prepare_transaction2)
	entry(gds__que_events)
	entry(gds__register_cleanup)
	entry(gds__unregister_cleanup)
	entry(gds__set_debug)
	entry(gds__event_block)
	entry(gds__event_counts)
	entry(gds__msg_lookup)
	entry(gds__msg_open)
	entry(gds__msg_close)
	entry(gds__msg_put)
	entry(gds__parse_bpb)
	entry(gds__get_slice)
	entry(gds__put_slice)
	entry(gds__prefix)
	entry(gds__validate_lib_path)
	entry(gds__event_wait)
	entry(gds__thread_start)
	entry(gds__thread_wait)
	entry(gds__thread_enter)
	entry(gds__thread_exit)
	entry(gds__thread_enable)
	entry(gds__describe_bind)
	entry(gds__log)
	entry(gds__completion_ast)
	entry(gds__ast_active)
	entry(gds__wake_init)
	entry(gds__disable_subsystem)
	entry(gds__map_blobs)
	entry(isc_expand_dpb)
	entry(gds__log_status)

	entry(isc_attach_database)
	entry(isc_blob_info)
	entry(isc_cancel_blob)
	entry(isc_cancel_events)
	entry(isc_close_blob)
	entry(isc_commit_retaining)
	entry(isc_commit_transaction)
	entry(isc_compile_request)
	entry(isc_compile_request2)
	entry(isc_create_blob)
	entry(isc_create_blob2)
	entry(isc_create_database)
	entry(isc_database_cleanup)
	entry(isc_database_info)
	entry(isc_ddl)
	entry(isc_decode_date)
	entry(isc_detach_database)
	entry(isc_drop_database)
	entry(isc_encode_date)
	entry(isc_event_block)
	entry(isc_event_counts)
	entry(isc_ftof)
	entry(ISC_get_prefix)
	entry(isc_get_segment)
	entry(isc_get_slice)
	entry(isc_interprete)
	entry(fb_interpret)
	entry(isc_open_blob)
	entry(isc_open_blob2)
	entry(ISC_prefix)
	entry(ISC_prefix_lock)
	entry(ISC_prefix_msg)
	entry(isc_prepare_transaction)
	entry(isc_prepare_transaction2)
	entry(isc_print_blr)
	entry(isc_print_status)
	entry(isc_put_segment)
	entry(isc_put_slice)
	entry(isc_qtoq)
	entry(isc_que_events)
	entry(isc_receive)
	entry(isc_reconnect_transaction)
	entry(isc_release_request)
	entry(isc_request_info)
	entry(isc_rollback_transaction)
	entry(isc_seek_blob)
	entry(isc_send)
	entry(isc_service_attach)
	entry(isc_service_detach)
	entry(isc_service_query)
	entry(isc_service_start)
	entry(isc_set_debug)
	entry(isc_sqlcode)
	entry(isc_start_and_send)
	entry(isc_start_multiple)
	entry(isc_start_transaction)
	entry(isc_start_request)
	entry(isc_transact_request)
	entry(isc_transaction_info)
	entry(isc_unwind_request)
	entry(isc_version)
	entry(isc_vtof)
	entry(isc_vtov)
	entry(isc_wait_for_event)

	entry(isc_close)
	entry(isc_declare)
	entry(isc_describe)
	entry(isc_describe_bind)
	entry(isc_dsql_finish)
	entry(isc_dsql_release)
	entry(isc_execute)
	entry(isc_execute_immediate)
	entry(isc_fetch)
	entry(isc_fetch_a)
	entry(isc_open)
	entry(isc_prepare)
	entry(isc_to_sqlda)
	entry(isc_array_lookup_desc)
	entry(isc_array_lookup_bounds)
	entry(isc_array_set_desc)
	entry(isc_array_get_slice)
	entry(isc_array_put_slice)

	entry(isc_baddress)
	entry(isc_execute_immediate_d)
	entry(isc_prepare_d)
	entry(isc_print_sqlerror)
	entry(isc_sql_interprete)
	entry(isc_vax_integer)
	entry(isc_rollback_retaining)

	entry(gds__event_block_a)

	entry(isc_dsql_allocate_statement)
	entry(isc_dsql_alloc_statement2)
	entry(isc_dsql_execute)
	entry(isc_dsql_execute2)
	entry(isc_dsql_execute_m)
	entry(isc_dsql_execute2_m)
	entry(isc_dsql_execute_immediate)
	entry(isc_dsql_execute_immediate_m)
	entry(isc_dsql_execute_immediate_d)
	entry(isc_dsql_exec_immed2)
	entry(isc_dsql_exec_immed2_m)
	entry(isc_dsql_fetch)
#ifdef SCROLLABLE_CURSORS
	entry(isc_dsql_fetch2)
#endif
	entry(isc_dsql_fetch_a)
#ifdef SCROLLABLE_CURSORS
	entry(isc_dsql_fetch2_a)
#endif
	entry(isc_dsql_fetch_m)
#ifdef SCROLLABLE_CURSORS
	entry(isc_dsql_fetch2_m)
#endif
	entry(isc_dsql_free_statement)
	entry(isc_dsql_insert)
	entry(isc_dsql_insert_m)
	entry(isc_dsql_prepare)
	entry(isc_dsql_prepare_m)
	entry(isc_dsql_prepare_d)
	entry(isc_dsql_set_cursor_name)
	entry(isc_dsql_sql_info)
	entry(isc_dsql_describe)
	entry(isc_dsql_describe_bind)
	entry(isc_embed_dsql_close)
	entry(isc_embed_dsql_declare)
	entry(isc_embed_dsql_execute)
	entry(isc_embed_dsql_execute2)
	entry(isc_embed_dsql_execute_immed)
	entry(isc_embed_dsql_execute_immed_d)
	entry(isc_embed_dsql_exec_immed2)
	entry(isc_embed_dsql_exec_immed2_d)
	entry(isc_embed_dsql_fetch)
#ifdef SCROLLABLE_CURSORS
	entry(isc_embed_dsql_fetch2)
#endif
	entry(isc_embed_dsql_fetch_a)
#ifdef SCROLLABLE_CURSORS
	entry(isc_embed_dsql_fetch2_a)
#endif
	entry(isc_embed_dsql_insert)
	entry(isc_embed_dsql_open)
	entry(isc_embed_dsql_open2)
	entry(isc_embed_dsql_prepare)
	entry(isc_embed_dsql_prepare_d)
	entry(isc_embed_dsql_release)
	entry(isc_embed_dsql_describe)
	entry(isc_embed_dsql_describe_bind)

	entry(SCH_current_thread)
	entry(SCH_enter)
	entry(SCH_exit)
	entry(SCH_hiber)
	entry(SCH_schedule)
	entry(SCH_wake)

static error(int* status)
{
	*status++ = isc_arg_gds;
	*status++ = isc_unavailable;
	*status++ = isc_arg_end;

	return isc_unavailable;
}

