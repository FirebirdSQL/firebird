/*
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
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef FB_API_PROTO_H
#define FB_API_PROTO_H

#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
struct FirebirdApiPointers
{
	decltype(&isc_attach_database) attach_database;
	decltype(&isc_array_gen_sdl) array_gen_sdl;
	decltype(&isc_array_get_slice) array_get_slice;
	decltype(&isc_array_lookup_bounds) array_lookup_bounds;
	decltype(&isc_array_lookup_desc) array_lookup_desc;
	decltype(&isc_array_set_desc) array_set_desc;
	decltype(&isc_array_put_slice) array_put_slice;
	decltype(&isc_blob_default_desc) blob_default_desc;
	decltype(&isc_blob_gen_bpb) blob_gen_bpb;
	decltype(&isc_blob_info) blob_info;
	decltype(&isc_blob_lookup_desc) blob_lookup_desc;
	decltype(&isc_blob_set_desc) blob_set_desc;
	decltype(&isc_cancel_blob) cancel_blob;
	decltype(&isc_cancel_events) cancel_events;
	decltype(&isc_close_blob) close_blob;
	decltype(&isc_commit_retaining) commit_retaining;
	decltype(&isc_commit_transaction) commit_transaction;
	decltype(&isc_create_blob) create_blob;
	decltype(&isc_create_blob2) create_blob2;
	decltype(&isc_create_database) create_database;
	decltype(&isc_database_info) database_info;
	decltype(&isc_decode_date) decode_date;
	decltype(&isc_decode_sql_date) decode_sql_date;
	decltype(&isc_decode_sql_time) decode_sql_time;
	decltype(&isc_decode_timestamp) decode_timestamp;
	decltype(&isc_detach_database) detach_database;
	decltype(&isc_drop_database) drop_database;
	decltype(&isc_dsql_allocate_statement) dsql_allocate_statement;
	decltype(&isc_dsql_alloc_statement2) dsql_alloc_statement2;
	decltype(&isc_dsql_describe) dsql_describe;
	decltype(&isc_dsql_describe_bind) dsql_describe_bind;
	decltype(&isc_dsql_exec_immed2) dsql_exec_immed2;
	decltype(&isc_dsql_execute) dsql_execute;
	decltype(&isc_dsql_execute2) dsql_execute2;
	decltype(&isc_dsql_execute_immediate) dsql_execute_immediate;
	decltype(&isc_dsql_fetch) dsql_fetch;
	decltype(&isc_dsql_finish) dsql_finish;
	decltype(&isc_dsql_free_statement) dsql_free_statement;
	decltype(&isc_dsql_insert) dsql_insert;
	decltype(&isc_dsql_prepare) dsql_prepare;
	decltype(&isc_dsql_set_cursor_name) dsql_set_cursor_name;
	decltype(&isc_dsql_sql_info) dsql_sql_info;
	decltype(&isc_encode_date) encode_date;
	decltype(&isc_encode_sql_date) encode_sql_date;
	decltype(&isc_encode_sql_time) encode_sql_time;
	decltype(&isc_encode_timestamp) encode_timestamp;
	decltype(&isc_event_block) event_block;
	decltype(&isc_event_counts) event_counts;
	decltype(&isc_expand_dpb) expand_dpb;
	decltype(&isc_modify_dpb) modify_dpb;
	decltype(&isc_free) free;
	decltype(&isc_get_segment) get_segment;
	decltype(&isc_get_slice) get_slice;
	decltype(&isc_open_blob) open_blob;
	decltype(&isc_open_blob2) open_blob2;
	decltype(&isc_prepare_transaction2) prepare_transaction2;
	decltype(&isc_print_sqlerror) print_sqlerror;
	decltype(&isc_print_status) print_status;
	decltype(&isc_put_segment) put_segment;
	decltype(&isc_put_slice) put_slice;
	decltype(&isc_que_events) que_events;
	decltype(&isc_rollback_retaining) rollback_retaining;
	decltype(&isc_rollback_transaction) rollback_transaction;
	decltype(&isc_start_multiple) start_multiple;
	decltype(&isc_start_transaction) start_transaction;
	decltype(&isc_reconnect_transaction) reconnect_transaction;
	decltype(&isc_sqlcode) sqlcode;
	decltype(&isc_sql_interprete) sql_interprete;
	decltype(&isc_transaction_info) transaction_info;
	decltype(&isc_transact_request) transact_request;
	decltype(&isc_vax_integer) vax_integer;
	decltype(&isc_seek_blob) seek_blob;
	decltype(&isc_service_attach) service_attach;
	decltype(&isc_service_detach) service_detach;
	decltype(&isc_service_query) service_query;
	decltype(&isc_service_start) service_start;
	decltype(&fb_interpret) interpret;
	decltype(&fb_cancel_operation) cancel_operation;
	decltype(&fb_database_crypt_callback) database_crypt_callback;
	decltype(&fb_dsql_set_timeout) dsql_set_timeout;
};
#if defined __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
