/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		protocol.cpp
 *	DESCRIPTION:	Protocol data structure mapper
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
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "IMP" port
 *
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix/MIPS" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "SGI" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "../remote/remote.h"
#include "iberror.h"
#include "../common/sdl.h"
#include "../common/gdsassert.h"
#include "../remote/parse_proto.h"
#include "../remote/proto_proto.h"
#include "../remote/remot_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/sdl_proto.h"
#include "../common/StatusHolder.h"
#include "../common/classes/stack.h"
#include "../common/classes/BatchCompletionState.h"
#include "../common/utils_proto.h"
#include "../dsql/DsqlBatch.h"

using namespace Firebird;

#ifdef DEBUG_XDR_MEMORY
inline bool_t P_TRUE(RemoteXdr* xdrs, PACKET* p)
{
	return xdr_debug_packet(xdrs, XDR_FREE, p);
}
inline bool_t P_FALSE(RemoteXdr* xdrs, PACKET* p)
{
	return !xdr_debug_packet(xdrs, XDR_FREE, p);
}
inline void DEBUG_XDR_PACKET(RemoteXdr* xdrs, PACKET* p)
{
	xdr_debug_packet(xdrs, XDR_DECODE, p);
}
inline void DEBUG_XDR_ALLOC(RemoteXdr* xdrs, const void* xdrvar, const void* addr, ULONG len)
{
	xdr_debug_memory(xdrs, XDR_DECODE, xdrvar, addr, len);
}
inline void DEBUG_XDR_FREE(RemoteXdr* xdrs, const void* xdrvar, const void* addr, ULONG len)
{
	xdr_debug_memory(xdrs, XDR_DECODE, xdrvar, addr, len);
}
#else
inline bool_t P_TRUE(RemoteXdr*, PACKET*)
{
	return TRUE;
}
inline bool_t P_FALSE(RemoteXdr* xdrs, PACKET*)
{
	return FALSE;
}
inline void DEBUG_XDR_PACKET(RemoteXdr*, PACKET*)
{
}
inline void DEBUG_XDR_ALLOC(RemoteXdr*, const void*, const void*, ULONG)
{
}
inline void DEBUG_XDR_FREE(RemoteXdr*, const void*, const void*, ULONG)
{
}
#endif // DEBUG_XDR_MEMORY

#define P_CHECK(xdr, p, st) if (st.getState() & IStatus::STATE_ERRORS) return P_FALSE(xdr, p)

#define MAP(routine, ptr)	if (!routine (xdrs, &ptr)) return P_FALSE(xdrs, p);
const ULONG MAX_OPAQUE		= 32768;

enum SQL_STMT_TYPE
{
	TYPE_IMMEDIATE,
	TYPE_PREPARED
};

static bool alloc_cstring(RemoteXdr*, CSTRING*);
static void free_cstring(RemoteXdr*, CSTRING*);
static void reset_statement(RemoteXdr*, SSHORT);
static bool_t xdr_cstring(RemoteXdr*, CSTRING*);
static bool_t xdr_response(RemoteXdr*, CSTRING*);
static bool_t xdr_cstring_with_limit(RemoteXdr*, CSTRING*, ULONG);
static inline bool_t xdr_cstring_const(RemoteXdr*, CSTRING_CONST*);
#ifdef DEBUG_XDR_MEMORY
static bool_t xdr_debug_packet(RemoteXdr*, enum xdr_op, PACKET*);
#endif
static bool_t xdr_longs(RemoteXdr*, CSTRING*);
static bool_t xdr_message(RemoteXdr*, RMessage*, const rem_fmt*);
static bool_t xdr_packed_message(RemoteXdr*, RMessage*, const rem_fmt*);
static bool_t xdr_request(RemoteXdr*, USHORT, USHORT, USHORT);
static bool_t xdr_slice(RemoteXdr*, lstring*, /*USHORT,*/ const UCHAR*);
static bool_t xdr_status_vector(RemoteXdr*, DynamicStatusVector*&);
static bool_t xdr_sql_blr(RemoteXdr*, SLONG, CSTRING*, bool, SQL_STMT_TYPE);
static bool_t xdr_sql_message(RemoteXdr*, SLONG);
static bool_t xdr_trrq_blr(RemoteXdr*, CSTRING*);
static bool_t xdr_trrq_message(RemoteXdr*, USHORT);
static bool_t xdr_bytes(RemoteXdr*, void*, ULONG);
static bool_t xdr_blob_stream(RemoteXdr*, SSHORT, CSTRING*);
static Rsr* getStatement(RemoteXdr*, USHORT);


inline void fixupLength(const RemoteXdr* xdrs, ULONG& length)
{
	// If the short (16-bit) value >= 32KB is being transmitted,
	// it gets expanded to long (32-bit) with a sign bit propagated.
	// In order to avoid troubles when reading such a value as long,
	// let's detect and fix unexpected overflows. Here we assume
	// that real longs will never have the highest 16 bits set.

	if (xdrs->x_op == XDR_DECODE && length >> 16 == (ULONG) 0xFFFF)
		length &= (ULONG) 0xFFFF;
}


#ifdef DEBUG
static ULONG xdr_save_size = 0;
inline void DEBUG_PRINTSIZE(RemoteXdr* xdrs, P_OP p)
{
	fprintf (stderr, "xdr_protocol: %s op %d size %lu\n",
		((xdrs->x_op == XDR_FREE)   ? "free" :
		 	(xdrs->x_op == XDR_ENCODE) ? "enc " : (xdrs->x_op == XDR_DECODE) ? "dec " : "othr"),
		p,
		((xdrs->x_op == XDR_ENCODE) ?
			(xdrs->x_handy - xdr_save_size) : (xdr_save_size - xdrs->x_handy)));
}
#else
inline void DEBUG_PRINTSIZE(RemoteXdr*, P_OP)
{
}
#endif


#ifdef DEBUG_XDR_MEMORY
void xdr_debug_memory(RemoteXdr* xdrs,
					  enum xdr_op xop,
					  const void* xdrvar, const void* address, ULONG length)
{
/**************************************
 *
 *	x d r _ d e b u g _ m e m o r y
 *
 **************************************
 *
 * Functional description
 *	Track memory allocation patterns of RemoteXdr aggregate
 *	types (i.e. xdr_cstring, xdr_string, etc.) to
 *	validate that memory is not leaked by overwriting
 *	RemoteXdr aggregate pointers and that freeing a packet
 *	with REMOTE_free_packet() does not miss anything.
 *
 *	All memory allocations due to marshalling RemoteXdr
 *	variables are recorded in a debug memory alloca-
 *	tion table stored at the front of a packet.
 *
 *	Once a packet is being tracked it is an assertion
 *	error if a memory allocation can not be recorded
 *	due to space limitations or if a previous memory
 *	allocation being freed cannot be found. At most
 *	P_MALLOC_SIZE entries can be stored in the memory
 *	allocation table. A rough estimate of the number
 *	of RemoteXdr aggregates that can hang off a packet can
 *	be obtained by examining the subpackets defined
 *	in <remote/protocol.h>: A guestimate of 36 at this
 *	time includes 10 strings used to decode an xdr
 *	status vector.
 *
 **************************************/
	rem_port* port = xdrs->x_public;
	fb_assert(port != 0);
	fb_assert(port->port_header.blk_type == type_port);

	// Compare the RemoteXdr variable address with the lower and upper bounds
	// of each packet to determine which packet contains it. Record or
	// delete an entry in that packet's memory allocation table.

	rem_vec* vector = port->port_packet_vector;
	if (!vector)	// Not tracking port's protocol
		return;

	ULONG i;
	for (i = 0; i < vector->vec_count; i++)
	{
		PACKET* packet = (PACKET*) vector->vec_object[i];
		if (packet)
		{
			fb_assert(packet->p_operation > op_void && packet->p_operation < op_max);

			if ((SCHAR*) xdrvar >= (SCHAR*) packet &&
				(SCHAR*) xdrvar < (SCHAR*) packet + sizeof(PACKET))
			{
				ULONG j;
				for (j = 0; j < P_MALLOC_SIZE; j++)
				{
					if (xop == XDR_FREE)
					{
						if ((SCHAR*) packet->p_malloc[j].p_address == (SCHAR*) address)
						{
							packet->p_malloc[j].p_operation = op_void;
							packet->p_malloc[j].p_allocated = NULL;
							packet->p_malloc[j].p_address = 0;
							return;
						}
					}
					else
					{
						// XDR_ENCODE or XDR_DECODE

						fb_assert(xop == XDR_ENCODE || xop == XDR_DECODE);
						if (packet->p_malloc[j].p_operation == op_void) {
							packet->p_malloc[j].p_operation = packet->p_operation;
							packet->p_malloc[j].p_allocated = length;
							packet->p_malloc[j].p_address = address;
							return;
						}
					}
				}
				// Assertion failure if not enough entries to record every xdr
				// memory allocation or an entry to be freed can't be found.

				fb_assert(j < P_MALLOC_SIZE);	// Increase P_MALLOC_SIZE if necessary
			}
		}
	}
	fb_assert(i < vector->vec_count);	// Couldn't find packet for this xdr arg
}
#endif


bool_t xdr_protocol(RemoteXdr* xdrs, PACKET* p)
{
/**************************************
 *
 *	x d r _ p r o t o c o l
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free a protocol packet.
 *
 **************************************/
	p_cnct::p_cnct_repeat* tail;
	P_ACPT *accept;
	P_ACPD *accept_with_data;
	P_ATCH *attach;
	P_RESP *response;
	P_CMPL *compile;
	P_STTR *transaction;
	P_DATA *data;
	P_RLSE *release;
	P_BLOB *blob;
	P_SGMT *segment;
	P_INFO *info;
	P_PREP *prepare;
	P_REQ *request;
	P_SLC *slice;
	P_SLR *slice_response;
	P_SEEK *seek;
	P_SQLFREE *free_stmt;
	P_SQLCUR *sqlcur;
	P_SQLST *prep_stmt;
	P_SQLDATA *sqldata;
	P_TRRQ *trrq;
#ifdef DEBUG
	xdr_save_size = xdrs->x_handy;
#endif

	DEBUG_XDR_PACKET(xdrs, p);

	if (!xdr_enum(xdrs, reinterpret_cast<xdr_op*>(&p->p_operation)))
		return P_FALSE(xdrs, p);

#if COMPRESS_DEBUG > 1
	if (xdrs->x_op != XDR_FREE)
	{
		fprintf(stderr, "operation=%d %c\n", p->p_operation,
			xdrs->x_op == XDR_ENCODE ? 'E' : xdrs->x_op == XDR_DECODE ? 'D' : xdrs->x_op == XDR_FREE ? 'F' : 'U');
	}
#endif

	const auto port = xdrs->x_public;

	switch (p->p_operation)
	{
	case op_reject:
	case op_disconnect:
	case op_dummy:
	case op_ping:
	case op_abort_aux_connection:
		return P_TRUE(xdrs, p);

	case op_connect:
		{
			P_CNCT* connect = &p->p_cnct;
			MAP(xdr_enum, reinterpret_cast<xdr_op&>(connect->p_cnct_operation));
			MAP(xdr_short, reinterpret_cast<SSHORT&>(connect->p_cnct_cversion));
			MAP(xdr_enum, reinterpret_cast<xdr_op&>(connect->p_cnct_client));
			MAP(xdr_cstring_const, connect->p_cnct_file);
			MAP(xdr_short, reinterpret_cast<SSHORT&>(connect->p_cnct_count));

			MAP(xdr_cstring_const, connect->p_cnct_user_id);

			const size_t CNCT_VERSIONS = FB_NELEM(connect->p_cnct_versions);
			tail = connect->p_cnct_versions;
			for (USHORT i = 0; i < connect->p_cnct_count; i++, tail++)
			{
				// ignore the rest of protocols in case of too many suggested versions
				p_cnct::p_cnct_repeat dummy;
				if (i >= CNCT_VERSIONS)
				{
					tail = &dummy;
				}

				MAP(xdr_short, reinterpret_cast<SSHORT&>(tail->p_cnct_version));
				MAP(xdr_enum, reinterpret_cast<xdr_op&>(tail->p_cnct_architecture));
				MAP(xdr_u_short, tail->p_cnct_min_type);
				MAP(xdr_u_short, tail->p_cnct_max_type);
				MAP(xdr_short, reinterpret_cast<SSHORT&>(tail->p_cnct_weight));
			}

			// ignore the rest of protocols in case of too many suggested versions
			if (connect->p_cnct_count > CNCT_VERSIONS)
			{
				connect->p_cnct_count = CNCT_VERSIONS;
			}

			DEBUG_PRINTSIZE(xdrs, p->p_operation);
			return P_TRUE(xdrs, p);
		}

	case op_accept:
		accept = &p->p_acpt;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(accept->p_acpt_version));
		MAP(xdr_enum, reinterpret_cast<xdr_op&>(accept->p_acpt_architecture));
		MAP(xdr_u_short, accept->p_acpt_type);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_accept_data:
	case op_cond_accept:
		accept_with_data = &p->p_acpd;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(accept_with_data->p_acpt_version));
		MAP(xdr_enum, reinterpret_cast<xdr_op&>(accept_with_data->p_acpt_architecture));
		MAP(xdr_u_short, accept_with_data->p_acpt_type);
		MAP(xdr_cstring, accept_with_data->p_acpt_data);
		MAP(xdr_cstring, accept_with_data->p_acpt_plugin);
		MAP(xdr_u_short, accept_with_data->p_acpt_authenticated);
		MAP(xdr_cstring, accept_with_data->p_acpt_keys);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_connect_request:
	case op_aux_connect:
		request = &p->p_req;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(request->p_req_type));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(request->p_req_object));
		MAP(xdr_long, reinterpret_cast<SLONG&>(request->p_req_partner));
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_attach:
	case op_create:
	case op_service_attach:
		attach = &p->p_atch;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(attach->p_atch_database));
		MAP(xdr_cstring_const, attach->p_atch_file);
		MAP(xdr_cstring_const, attach->p_atch_dpb);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_compile:
		compile = &p->p_cmpl;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(compile->p_cmpl_database));
		MAP(xdr_cstring_const, compile->p_cmpl_blr);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_receive:
	case op_start:
	case op_start_and_receive:
		data = &p->p_data;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_request));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_incarnation));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_transaction));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_message_number));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_messages));
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_send:
	case op_start_and_send:
	case op_start_send_and_receive:
		data = &p->p_data;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_request));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_incarnation));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_transaction));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_message_number));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_messages));

		// Changes to this op's protocol must mirror in xdr_protocol_overhead

		return xdr_request(xdrs, data->p_data_request,
						   data->p_data_message_number,
						   data->p_data_incarnation) ? P_TRUE(xdrs, p) : P_FALSE(xdrs, p);

	case op_response:
	case op_response_piggyback:

		// Changes to this op's protocol must be mirrored
		// in xdr_protocol_overhead

		response = &p->p_resp;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(response->p_resp_object));
		MAP(xdr_quad, response->p_resp_blob_id);
		MAP(xdr_response, response->p_resp_data);
		return xdr_status_vector(xdrs, response->p_resp_status_vector) ?
								 	P_TRUE(xdrs, p) : P_FALSE(xdrs, p);

	case op_transact:
		trrq = &p->p_trrq;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(trrq->p_trrq_database));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(trrq->p_trrq_transaction));
		xdr_trrq_blr(xdrs, &trrq->p_trrq_blr);
		MAP(xdr_cstring, trrq->p_trrq_blr);
		MAP(xdr_short, reinterpret_cast<SSHORT&>(trrq->p_trrq_messages));
		if (trrq->p_trrq_messages)
			return xdr_trrq_message(xdrs, 0) ? P_TRUE(xdrs, p) : P_FALSE(xdrs, p);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_transact_response:
		data = &p->p_data;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(data->p_data_messages));
		if (data->p_data_messages)
			return xdr_trrq_message(xdrs, 1) ? P_TRUE(xdrs, p) : P_FALSE(xdrs, p);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_open_blob2:
	case op_create_blob2:
		blob = &p->p_blob;
		MAP(xdr_cstring_const, blob->p_blob_bpb);
		// fall into:

	case op_open_blob:
	case op_create_blob:
		blob = &p->p_blob;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(blob->p_blob_transaction));
		MAP(xdr_quad, blob->p_blob_id);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_get_segment:
	case op_put_segment:
	case op_batch_segments:
		segment = &p->p_sgmt;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(segment->p_sgmt_blob));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(segment->p_sgmt_length));
		MAP(xdr_cstring_const, segment->p_sgmt_segment);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_seek_blob:
		seek = &p->p_seek;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(seek->p_seek_blob));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(seek->p_seek_mode));
		MAP(xdr_long, seek->p_seek_offset);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_reconnect:
	case op_transaction:
		transaction = &p->p_sttr;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(transaction->p_sttr_database));
		MAP(xdr_cstring_const, transaction->p_sttr_tpb);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_info_blob:
	case op_info_database:
	case op_info_request:
	case op_info_transaction:
	case op_service_info:
	case op_info_sql:
	case op_info_batch:
		info = &p->p_info;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(info->p_info_object));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(info->p_info_incarnation));
		MAP(xdr_cstring_const, info->p_info_items);
		if (p->p_operation == op_service_info)
			MAP(xdr_cstring_const, info->p_info_recv_items);
		MAP(xdr_long, reinterpret_cast<SLONG&>(info->p_info_buffer_length));
		// p_info_buffer_length was USHORT in older versions
		fixupLength(xdrs, info->p_info_buffer_length);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_service_start:
		info = &p->p_info;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(info->p_info_object));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(info->p_info_incarnation));
		MAP(xdr_cstring_const, info->p_info_items);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_commit:
	case op_prepare:
	case op_rollback:
	case op_unwind:
	case op_release:
	case op_close_blob:
	case op_cancel_blob:
	case op_detach:
	case op_drop_database:
	case op_service_detach:
	case op_commit_retaining:
	case op_rollback_retaining:
	case op_allocate_statement:
		release = &p->p_rlse;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(release->p_rlse_object));
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_prepare2:
		prepare = &p->p_prep;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prepare->p_prep_transaction));
		MAP(xdr_cstring_const, prepare->p_prep_data);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_que_events:
	case op_event:
		{
			P_EVENT* event = &p->p_event;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(event->p_event_database));
			MAP(xdr_cstring_const, event->p_event_items);

			// Nickolay Samofatov: these values are parsed, but are ignored by the client.
			// Values are useful only for debugging, anyway since upper words of pointers
			// are trimmed for 64-bit clients
			MAP(xdr_long, reinterpret_cast<SLONG&>(event->p_event_ast));
			MAP(xdr_long, event->p_event_arg);

			MAP(xdr_long, event->p_event_rid);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);
			return P_TRUE(xdrs, p);
		}

	case op_cancel_events:
		{
			P_EVENT* event = &p->p_event;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(event->p_event_database));
			MAP(xdr_long, event->p_event_rid);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);
			return P_TRUE(xdrs, p);
		}

	case op_ddl:
		{
			P_DDL* ddl = &p->p_ddl;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(ddl->p_ddl_database));
			MAP(xdr_short, reinterpret_cast<SSHORT&>(ddl->p_ddl_transaction));
			MAP(xdr_cstring_const, ddl->p_ddl_blr);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);
			return P_TRUE(xdrs, p);
		}

	case op_get_slice:
	case op_put_slice:
		slice = &p->p_slc;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(slice->p_slc_transaction));
		MAP(xdr_quad, slice->p_slc_id);
		MAP(xdr_long, reinterpret_cast<SLONG&>(slice->p_slc_length));
		MAP(xdr_cstring, slice->p_slc_sdl);
		MAP(xdr_longs, slice->p_slc_parameters);
		slice_response = &p->p_slr;
		if (slice_response->p_slr_sdl)
		{
			if (!xdr_slice(xdrs, &slice->p_slc_slice, //slice_response->p_slr_sdl_length,
						   slice_response->p_slr_sdl))
			{
				return P_FALSE(xdrs, p);
			}
		}
		else
			if (!xdr_slice(xdrs, &slice->p_slc_slice, //slice->p_slc_sdl.cstr_length,
						   slice->p_slc_sdl.cstr_address))
			{
				return P_FALSE(xdrs, p);
			}
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_slice:
		slice_response = &p->p_slr;
		MAP(xdr_long, reinterpret_cast<SLONG&>(slice_response->p_slr_length));
		if (!xdr_slice (xdrs, &slice_response->p_slr_slice, //slice_response->p_slr_sdl_length,
			 slice_response->p_slr_sdl))
		{
			return P_FALSE(xdrs, p);
		}
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_execute:
	case op_execute2:
		sqldata = &p->p_sqldata;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_statement));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_transaction));
		if (xdrs->x_op == XDR_DECODE)
		{
			// the statement should be reset for each execution so that
			// all prefetched information from a prior execute is properly
			// cleared out.  This should be done before fetching any message
			// information (for example: blr info)

			reset_statement(xdrs, sqldata->p_sqldata_statement);
		}

		if (!xdr_sql_blr(xdrs, (SLONG) sqldata->p_sqldata_statement,
						 &sqldata->p_sqldata_blr, false, TYPE_PREPARED))
		{
			return P_FALSE(xdrs, p);
		}
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_message_number));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_messages));
		if (sqldata->p_sqldata_messages)
		{
			if (!xdr_sql_message(xdrs, (SLONG) sqldata->p_sqldata_statement))
				return P_FALSE(xdrs, p);
		}
		if (p->p_operation == op_execute2)
		{
			if (!xdr_sql_blr(xdrs, (SLONG) - 1, &sqldata->p_sqldata_out_blr, true, TYPE_PREPARED))
			{
				return P_FALSE(xdrs, p);
			}
			MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_out_message_number));
		}
		if (port->port_protocol >= PROTOCOL_STMT_TOUT)
			MAP(xdr_u_long, sqldata->p_sqldata_timeout);
		if (port->port_protocol >= PROTOCOL_FETCH_SCROLL)
			MAP(xdr_u_long, sqldata->p_sqldata_cursor_flags);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_exec_immediate2:
		prep_stmt = &p->p_sqlst;
		if (!xdr_sql_blr(xdrs, (SLONG) - 1, &prep_stmt->p_sqlst_blr, false, TYPE_IMMEDIATE))
		{
			return P_FALSE(xdrs, p);
		}
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_message_number));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_messages));
		if (prep_stmt->p_sqlst_messages)
		{
			if (!xdr_sql_message(xdrs, (SLONG) - 1))
				return P_FALSE(xdrs, p);
		}
		if (!xdr_sql_blr(xdrs, (SLONG) - 1, &prep_stmt->p_sqlst_out_blr, true, TYPE_IMMEDIATE))
		{
			return P_FALSE(xdrs, p);
		}
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_out_message_number));
		// Fall into ...

	case op_exec_immediate:
	case op_prepare_statement:
		prep_stmt = &p->p_sqlst;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_transaction));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_statement));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(prep_stmt->p_sqlst_SQL_dialect));
		MAP(xdr_cstring_const, prep_stmt->p_sqlst_SQL_str);
		MAP(xdr_cstring_const, prep_stmt->p_sqlst_items);
		MAP(xdr_long, reinterpret_cast<SLONG&>(prep_stmt->p_sqlst_buffer_length));
		// p_sqlst_buffer_length was USHORT in older versions
		fixupLength(xdrs, prep_stmt->p_sqlst_buffer_length);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_fetch:
	case op_fetch_scroll:
		sqldata = &p->p_sqldata;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_statement));
		if (!xdr_sql_blr(xdrs, (SLONG) sqldata->p_sqldata_statement,
						 &sqldata->p_sqldata_blr, true, TYPE_PREPARED))
		{
			return P_FALSE(xdrs, p);
		}
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_message_number));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_messages));
		if (p->p_operation == op_fetch_scroll)
		{
			MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_fetch_op));
			MAP(xdr_long, sqldata->p_sqldata_fetch_pos);
		}
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_fetch_response:
		sqldata = &p->p_sqldata;
		MAP(xdr_long, reinterpret_cast<SLONG&>(sqldata->p_sqldata_status));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_messages));

		// Changes to this op's protocol must mirror in xdr_protocol_overhead

		if (sqldata->p_sqldata_messages)
		{
			return xdr_sql_message(xdrs, (SLONG)sqldata->p_sqldata_statement) ?
				P_TRUE(xdrs, p) : P_FALSE(xdrs, p);
		}
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_free_statement:
		free_stmt = &p->p_sqlfree;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(free_stmt->p_sqlfree_statement));
		MAP(xdr_short, reinterpret_cast<SSHORT&>(free_stmt->p_sqlfree_option));
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_set_cursor:
		sqlcur = &p->p_sqlcur;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqlcur->p_sqlcur_statement));
		MAP(xdr_cstring_const, sqlcur->p_sqlcur_cursor_name);
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqlcur->p_sqlcur_type));
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	case op_sql_response:
		sqldata = &p->p_sqldata;
		MAP(xdr_short, reinterpret_cast<SSHORT&>(sqldata->p_sqldata_messages));
		if (sqldata->p_sqldata_messages)
			return xdr_sql_message(xdrs, (SLONG) -1) ? P_TRUE(xdrs, p) : P_FALSE(xdrs, p);
		DEBUG_PRINTSIZE(xdrs, p->p_operation);
		return P_TRUE(xdrs, p);

	// the following added to have formal vulcan compatibility
	case op_update_account_info:
		{
			p_update_account* stuff = &p->p_account_update;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(stuff->p_account_database));
			MAP(xdr_cstring_const, stuff->p_account_apb);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_authenticate_user:
		{
			p_authenticate* stuff = &p->p_authenticate_user;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(stuff->p_auth_database));
			MAP(xdr_cstring_const, stuff->p_auth_dpb);
			MAP(xdr_cstring, stuff->p_auth_items);
			MAP(xdr_short, reinterpret_cast<SSHORT&>(stuff->p_auth_buffer_length));
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_trusted_auth:
		{
			P_TRAU* trau = &p->p_trau;
			MAP(xdr_cstring, trau->p_trau_data);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_cont_auth:
		{
			P_AUTH_CONT* auth = &p->p_auth_cont;
			MAP(xdr_cstring, auth->p_data);
			MAP(xdr_cstring, auth->p_name);
			MAP(xdr_cstring, auth->p_list);
			MAP(xdr_cstring, auth->p_keys);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_cancel:
		{
			P_CANCEL_OP* cancel_op = &p->p_cancel_op;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(cancel_op->p_co_kind));
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_crypt:
		{
			P_CRYPT* crypt = &p->p_crypt;
			MAP(xdr_cstring, crypt->p_plugin);
			MAP(xdr_cstring, crypt->p_key);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_crypt_key_callback:
		{
			P_CRYPT_CALLBACK* cc = &p->p_cc;
			MAP(xdr_cstring, cc->p_cc_data);

			// If the protocol is 0 we are in the process of establishing a connection.
			// crypt_key_callback at this phaze means server protocol is at least P15
			if (port->port_protocol >= PROTOCOL_VERSION14 || port->port_protocol == 0)
				MAP(xdr_short, reinterpret_cast<SSHORT&>(cc->p_cc_reply));

			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_batch_create:
		{
			P_BATCH_CREATE* b = &p->p_batch_create;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_cstring_const, b->p_batch_blr);
			MAP(xdr_u_long, b->p_batch_msglen);
			MAP(xdr_cstring_const, b->p_batch_pb);

			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_batch_msg:
		{
			P_BATCH_MSG* b = &p->p_batch_msg;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_u_long, b->p_batch_messages);

			if (xdrs->x_op == XDR_FREE)
			{
				MAP(xdr_cstring, b->p_batch_data);
				return P_TRUE(xdrs, p);
			}

			SSHORT statement_id = b->p_batch_statement;
			Rsr* statement;
			if (statement_id >= 0)
			{
				if (static_cast<ULONG>(statement_id) >= port->port_objects.getCount())
					return P_FALSE(xdrs, p);

				try
				{
					statement = port->port_objects[statement_id];
				}
				catch (const status_exception&)
				{
					return P_FALSE(xdrs, p);
				}
			}
			else
			{
				statement = port->port_statement;
			}

			if (!statement)
				return P_FALSE(xdrs, p);

			ULONG count = b->p_batch_messages;
			ULONG size = statement->rsr_batch_size;
			if (!size)
				statement->rsr_batch_size = size = FB_ALIGN(statement->rsr_format->fmt_length, FB_ALIGNMENT);
			if (xdrs->x_op == XDR_DECODE)
			{
				b->p_batch_data.cstr_length = (count ? count : 1) * size;
				alloc_cstring(xdrs, &b->p_batch_data);
			}

			RMessage* message = statement->rsr_buffer;
			if (!message)
				return P_FALSE(xdrs, p);
			statement->rsr_buffer = message->msg_next;
			message->msg_address = b->p_batch_data.cstr_address;

			while (count--)
			{
				DEB_RBATCH(fprintf(stderr, "BatRem: xdr packed msg\n"));
				if (!xdr_packed_message(xdrs, message, statement->rsr_format))
					return P_FALSE(xdrs, p);
				message->msg_address += statement->rsr_batch_size;
			}

			message->msg_address = nullptr;
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	case op_batch_exec:
		{
			P_BATCH_EXEC* b = &p->p_batch_exec;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_transaction));

			if (xdrs->x_op != XDR_FREE)
				DEB_RBATCH(fprintf(stderr, "BatRem: xdr execute\n"));

			return P_TRUE(xdrs, p);
		}

	case op_batch_cs:
		{
			P_BATCH_CS* b = &p->p_batch_cs;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_u_long, b->p_batch_reccount);
			MAP(xdr_u_long, b->p_batch_updates);
			MAP(xdr_u_long, b->p_batch_vectors);
			MAP(xdr_u_long, b->p_batch_errors);

			if (xdrs->x_op == XDR_FREE)
				return P_TRUE(xdrs, p);

			SSHORT statement_id = b->p_batch_statement;
			DEB_RBATCH(fprintf(stderr, "BatRem: xdr CS %d\n", statement_id));
			Rsr* statement;

			if (statement_id >= 0)
			{
				if (static_cast<ULONG>(statement_id) >= port->port_objects.getCount())
					return P_FALSE(xdrs, p);

				try
				{
					statement = port->port_objects[statement_id];
				}
				catch (const status_exception&)
				{
					return P_FALSE(xdrs, p);
				}
			}
			else
			{
				statement = port->port_statement;
			}

			if (!statement)
				return P_FALSE(xdrs, p);

			LocalStatus ls;
			CheckStatusWrapper status_vector(&ls);

			if ((xdrs->x_op == XDR_DECODE) && (!b->p_batch_updates))
			{
				DEB_RBATCH(fprintf(stderr, "BatRem: xdr reccount=%d\n", b->p_batch_reccount));
				statement->rsr_batch_cs->regSize(b->p_batch_reccount);
			}

			// Process update counters
			DEB_RBATCH(fprintf(stderr, "BatRem: xdr up %d\n", b->p_batch_updates));
			for (unsigned i = 0; i < b->p_batch_updates; ++i)
			{
				SLONG v;

				if (xdrs->x_op == XDR_ENCODE)
				{
					v = statement->rsr_batch_ics->getState(&status_vector, i);
					P_CHECK(xdrs, p, status_vector);
				}

				MAP(xdr_long, v);

				if (xdrs->x_op == XDR_DECODE)
				{
					statement->rsr_batch_cs->regUpdate(v);
				}
			}

			// Process status vectors
			ULONG pos = 0u;
			DEB_RBATCH(fprintf(stderr, "BatRem: xdr sv %d\n", b->p_batch_vectors));

			for (unsigned i = 0; i < b->p_batch_vectors; ++pos)
			{
				DynamicStatusVector s;
				DynamicStatusVector* ptr = NULL;

				if (xdrs->x_op == XDR_ENCODE)
				{
					pos = statement->rsr_batch_ics->findError(&status_vector, pos);
					P_CHECK(xdrs, p, status_vector);
					if (pos == IBatchCompletionState::NO_MORE_ERRORS)
						return P_FALSE(xdrs, p);

					LocalStatus to;
					statement->rsr_batch_ics->getStatus(&status_vector, &to, pos);
					if (status_vector.getState() & IStatus::STATE_ERRORS)
						continue;

					s.load(&to);
					ptr = &s;
				}

				MAP(xdr_u_long, pos);

				if (!xdr_status_vector(xdrs, ptr))
					return P_FALSE(xdrs, p);

				if (xdrs->x_op == XDR_DECODE)
				{
					Firebird::Arg::StatusVector sv(ptr->value());
					LocalStatus to;
					sv.copyTo(&to);
					delete ptr;
					statement->rsr_batch_cs->regErrorAt(pos, &to);
				}
				++i;
			}

			// Process status-less errors
			pos = 0u;
			DEB_RBATCH(fprintf(stderr, "BatRem: xdr err %d\n", b->p_batch_errors));

			for (unsigned i = 0; i < b->p_batch_errors; ++pos)
			{
				if (xdrs->x_op == XDR_ENCODE)
				{
					pos = statement->rsr_batch_ics->findError(&status_vector, pos);
					P_CHECK(xdrs, p, status_vector);
					if (pos == IBatchCompletionState::NO_MORE_ERRORS)
						return P_FALSE(xdrs, p);

					LocalStatus to;
					statement->rsr_batch_ics->getStatus(&status_vector, &to, pos);
					if (!(status_vector.getState() & IStatus::STATE_ERRORS))
						continue;
				}

				MAP(xdr_u_long, pos);

				if (xdrs->x_op == XDR_DECODE)
				{
					statement->rsr_batch_cs->regErrorAt(pos, nullptr);
				}
				++i;
			}

			return P_TRUE(xdrs, p);
		}

	case op_batch_rls:
	case op_batch_cancel:
		{
			P_BATCH_FREE_CANCEL* b = &p->p_batch_free_cancel;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));

			if (xdrs->x_op != XDR_FREE)
				DEB_RBATCH(fprintf(stderr, "BatRem: xdr release/cancel %d\n", p->p_operation));

			return P_TRUE(xdrs, p);
		}

	case op_batch_sync:
		{
			return P_TRUE(xdrs, p);
		}

	case op_batch_set_bpb:
		{
			P_BATCH_SETBPB* b = &p->p_batch_setbpb;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_cstring_const, b->p_batch_blob_bpb);

			Rsr* statement = getStatement(xdrs, b->p_batch_statement);
			if (!statement)
				return P_FALSE(xdrs, p);
			if (fb_utils::isBpbSegmented(b->p_batch_blob_bpb.cstr_length, b->p_batch_blob_bpb.cstr_address))
				statement->rsr_batch_flags |= (1 << Jrd::DsqlBatch::FLAG_DEFAULT_SEGMENTED);
			else
				statement->rsr_batch_flags &= ~(1 << Jrd::DsqlBatch::FLAG_DEFAULT_SEGMENTED);

			return P_TRUE(xdrs, p);
		}

	case op_batch_regblob:
		{
			P_BATCH_REGBLOB* b = &p->p_batch_regblob;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			MAP(xdr_quad, b->p_batch_exist_id);
			MAP(xdr_quad, b->p_batch_blob_id);

			return P_TRUE(xdrs, p);
		}

	case op_batch_blob_stream:
		{
			P_BATCH_BLOB* b = &p->p_batch_blob;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(b->p_batch_statement));
			if (!xdr_blob_stream(xdrs, b->p_batch_statement, &b->p_batch_blob_data))
				return P_FALSE(xdrs, p);

			return P_TRUE(xdrs, p);
		}

	case op_repl_data:
		{
			P_REPLICATE* repl = &p->p_replicate;
			MAP(xdr_short, reinterpret_cast<SSHORT&>(repl->p_repl_database));
			MAP(xdr_cstring_const, repl->p_repl_data);
			DEBUG_PRINTSIZE(xdrs, p->p_operation);

			return P_TRUE(xdrs, p);
		}

	///case op_insert:
	default:
#ifdef DEV_BUILD
		if (xdrs->x_op != XDR_FREE)
		{
			gds__log("xdr_packet: operation %d not recognized\n", p->p_operation);
		}
#endif
		return P_FALSE(xdrs, p);
	}
}


static bool_t xdr_bytes(RemoteXdr* xdrs, void* bytes, ULONG size)
{
	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		if (!xdrs->x_putbytes(reinterpret_cast<const SCHAR*>(bytes), size))
			return FALSE;
		break;

	case XDR_DECODE:
		if (!xdrs->x_getbytes(reinterpret_cast<SCHAR*>(bytes), size))
			return FALSE;
		break;
	}

	return TRUE;
}


ULONG xdr_protocol_overhead(P_OP op)
{
/**************************************
 *
 *	x d r _ p r o t o c o l _ o v e r h e a d
 *
 **************************************
 *
 * Functional description
 *	Report the overhead size of a particular packet.
 *	NOTE: This is not the same as the actual size to
 *	send the packet - as this figure discounts any data
 *	to be sent with the packet.  It's purpose is to figure
 *	overhead for deciding on a batching window count.
 *
 *	A better version of this routine would use xdr_sizeof - but
 *	it is unknown how portable that Solaris call is to other
 *	OS.
 *
 **************************************/
	ULONG size = 4; // xdr_sizeof (xdr_enum, p->p_operation)

	switch (op)
	{
	case op_fetch_response:
		size += 4				// xdr_sizeof (xdr_long, sqldata->p_sqldata_status)
			+ 4;				// xdr_sizeof (xdr_short, sqldata->p_sqldata_messages)
		break;

	case op_send:
	case op_start_and_send:
	case op_start_send_and_receive:
		size += 4				// xdr_sizeof (xdr_short, data->p_data_request)
			+ 4					// xdr_sizeof (xdr_short, data->p_data_incarnation)
			+ 4					// xdr_sizeof (xdr_short, data->p_data_transaction)
			+ 4					// xdr_sizeof (xdr_short, data->p_data_message_number)
			+ 4;				// xdr_sizeof (xdr_short, data->p_data_messages)
		break;

	case op_response:
	case op_response_piggyback:
		// Note: minimal amounts are used for cstring & status_vector
		size += 4				// xdr_sizeof (xdr_short, response->p_resp_object)
			+ 8					// xdr_sizeof (xdr_quad, response->p_resp_blob_id)
			+ 4					// xdr_sizeof (xdr_cstring, response->p_resp_data)
			+
			3 *
			4;					// xdr_sizeof (xdr_status_vector (xdrs, response->p_resp_status_vector
		break;

	default:
		fb_assert(FALSE);		// Not supported operation
		return 0;
	}
	return size;
}


static bool alloc_cstring(RemoteXdr* xdrs, CSTRING* cstring)
{
/**************************************
 *
 *	a l l o c _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Handle allocation for cstring.
 *
 **************************************/

	if (!cstring->cstr_length)
	{
		if (cstring->cstr_allocated)
			*cstring->cstr_address = '\0';
		else
			cstring->cstr_address = NULL;

		return true;
	}

	if (cstring->cstr_length > cstring->cstr_allocated && cstring->cstr_allocated)
	{
		free_cstring(xdrs, cstring);
	}

	if (!cstring->cstr_address)
	{
		// fb_assert(!cstring->cstr_allocated);
		try {
			cstring->cstr_address = FB_NEW_POOL(*getDefaultMemoryPool()) UCHAR[cstring->cstr_length];
		}
		catch (const BadAlloc&) {
			return false;
		}

		cstring->cstr_allocated = cstring->cstr_length;
		DEBUG_XDR_ALLOC(xdrs, cstring, cstring->cstr_address, cstring->cstr_allocated);
	}

	return true;
}


static void free_cstring( RemoteXdr* xdrs, CSTRING* cstring)
{
/**************************************
 *
 *	f r e e _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Free any memory allocated for a cstring.
 *
 **************************************/

	if (cstring->cstr_allocated)
	{
		delete[] cstring->cstr_address;
		DEBUG_XDR_FREE(xdrs, cstring, cstring->cstr_address, cstring->cstr_allocated);
	}

	cstring->cstr_address = NULL;
	cstring->cstr_allocated = 0;
}


static bool xdr_is_client(RemoteXdr* xdrs)
{
	const rem_port* port = xdrs->x_public;
	return !(port->port_flags & PORT_server);
}


// CVC: This function is a little stub to validate that indeed, bpb's aren't
// overwritten by accident. Even though xdr_string writes to cstr_address,
// an action we wanted to block, it first allocates a new buffer.
// The problem is that bpb's aren't copied, but referenced by address, so we
// don't want a const param being hijacked and its memory location overwritten.
// The same test has been applied to put_segment and batch_segments operations.
// The layout of CSTRING and CSTRING_CONST is exactly the same.
// Changing CSTRING to use cstr_address as const pointer would upset other
// places of the code, so only P_BLOB was changed to use CSTRING_CONST.
// The same function is being used to check P_SGMT & P_DDL.
static inline bool_t xdr_cstring_const(RemoteXdr* xdrs, CSTRING_CONST* cstring)
{
	if (xdr_is_client(xdrs) && xdrs->x_op == XDR_DECODE)
	{
		fb_assert(!(cstring->cstr_length <= cstring->cstr_allocated && cstring->cstr_allocated));

		if (!cstring->cstr_allocated)
		{
			// Normally we should not decode into such CSTRING_CONST at client side
			// May be op, normally never sent to client, was received
			cstring->cstr_address = nullptr;
			cstring->cstr_length = 0;
		}
	}
	return xdr_cstring(xdrs, reinterpret_cast<CSTRING*>(cstring));
}

static inline bool_t xdr_response(RemoteXdr* xdrs, CSTRING* cstring)
{
	if (xdr_is_client(xdrs) && xdrs->x_op == XDR_DECODE && cstring->cstr_allocated)
	{
		ULONG limit = cstring->cstr_allocated;
		cstring->cstr_allocated = 0;
		return xdr_cstring_with_limit(xdrs, cstring, limit);
	}

	return xdr_cstring(xdrs, cstring);
}

static bool_t xdr_cstring( RemoteXdr* xdrs, CSTRING* cstring)
{
	return xdr_cstring_with_limit(xdrs, cstring, 0);
}

static bool_t xdr_cstring_with_limit( RemoteXdr* xdrs, CSTRING* cstring, ULONG limit)
{
/**************************************
 *
 *	x d r _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Map a counted string structure.
 *
 **************************************/
	SLONG l;
	SCHAR trash[4];
	static const SCHAR filler[4] = { 0, 0, 0, 0 };

	if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(&cstring->cstr_length)))
	{
		return FALSE;
	}

	// string length was USHORT in older versions
	fixupLength(xdrs, cstring->cstr_length);

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		if (cstring->cstr_length &&
			!xdrs->x_putbytes(reinterpret_cast<const SCHAR*>(cstring->cstr_address), cstring->cstr_length))
		{
			return FALSE;
		}
		l = (4 - cstring->cstr_length) & 3;
		if (l)
			return xdrs->x_putbytes(filler, l);
		return TRUE;

	case XDR_DECODE:
		if (limit && cstring->cstr_length > limit)
			return FALSE;
		if (!alloc_cstring(xdrs, cstring))
			return FALSE;
		if (!xdrs->x_getbytes(reinterpret_cast<SCHAR*>(cstring->cstr_address), cstring->cstr_length))
			return FALSE;
		l = (4 - cstring->cstr_length) & 3;
		if (l)
			return xdrs->x_getbytes(trash, l);
		return TRUE;

	case XDR_FREE:
		free_cstring(xdrs, cstring);
		return TRUE;
	}

	return FALSE;
}


#ifdef DEBUG_XDR_MEMORY
static bool_t xdr_debug_packet( RemoteXdr* xdrs, enum xdr_op xop, PACKET* packet)
{
/**************************************
 *
 *	x d r _ d e b u g _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Start/stop monitoring a packet's memory allocations by
 *	entering/removing from a port's packet tracking vector.
 *
 **************************************/
	rem_port* port = xdrs->x_public;
	fb_assert(port != 0);
	fb_assert(port->port_header.blk_type == type_port);

	if (xop == XDR_FREE)
	{
		// Free a slot in the packet tracking vector

		rem_vec* vector = port->port_packet_vector;
		if (vector)
		{
			for (ULONG i = 0; i < vector->vec_count; i++)
			{
				if (vector->vec_object[i] == (BLK) packet) {
					vector->vec_object[i] = 0;
					return TRUE;
				}
			}
		}
	}
	else
	{
		// XDR_ENCODE or XDR_DECODE

		// Allocate an unused slot in the packet tracking vector
		// to start recording memory allocations for this packet.

		fb_assert(xop == XDR_ENCODE || xop == XDR_DECODE);
		rem_vec* vector = A L L R _vector(&port->port_packet_vector, 0);
		ULONG i;

		for (i = 0; i < vector->vec_count; i++)
		{
			if (vector->vec_object[i] == (BLK) packet)
				return TRUE;
		}

		for (i = 0; i < vector->vec_count; i++)
		{
			if (vector->vec_object[i] == 0)
				break;
		}

		if (i >= vector->vec_count)
			vector = A L L R _vector(&port->port_packet_vector, i);

		vector->vec_object[i] = (BLK) packet;
	}

	return TRUE;
}
#endif


static bool_t xdr_longs( RemoteXdr* xdrs, CSTRING* cstring)
{
/**************************************
 *
 *	x d r _ l o n g s
 *
 **************************************
 *
 * Functional description
 *	Pass a vector of longs.
 *
 **************************************/
	if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(&cstring->cstr_length)))
	{
		return FALSE;
	}

	// string length was USHORT in older versions
	fixupLength(xdrs, cstring->cstr_length);

	// Handle operation specific stuff, particularly memory allocation/deallocation

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		break;

	case XDR_DECODE:
		if (!alloc_cstring(xdrs, cstring))
			return FALSE;
		break;

	case XDR_FREE:
		free_cstring(xdrs, cstring);
		return TRUE;
	}

	const size_t n = cstring->cstr_length / sizeof(SLONG);

	SLONG* next = (SLONG*) cstring->cstr_address;
	for (const SLONG* const end = next + n; next < end; next++)
	{
		if (!xdr_long(xdrs, next))
			return FALSE;
	}

	return TRUE;
}


static bool_t xdr_message( RemoteXdr* xdrs, RMessage* message, const rem_fmt* format)
{
/**************************************
 *
 *	x d r _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted message.
 *
 **************************************/
	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	rem_port* port = xdrs->x_public;

	if (!message || !format)
		return FALSE;

	// If we are running a symmetric version of the protocol, just slop
	// the bits and don't sweat the translations

	if (port->port_flags & PORT_symmetric)
		return xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(message->msg_address), format->fmt_length);

	const dsc* desc = format->fmt_desc.begin();
	for (const dsc* const end = format->fmt_desc.end(); desc < end; ++desc)
	{
		if (!xdr_datum(xdrs, desc, message->msg_address))
			return FALSE;
	}

	DEBUG_PRINTSIZE(xdrs, op_void);
	return TRUE;
}


static bool_t xdr_packed_message( RemoteXdr* xdrs, RMessage* message, const rem_fmt* format)
{
/**************************************
 *
 *	x d r _ p a c k e d _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted message.
 *
 **************************************/

	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	const rem_port* const port = xdrs->x_public;

	if (!message || !format)
		return FALSE;

	// If we are running a symmetric version of the protocol, just slop
	// the bits and don't sweat the translations

	if (port->port_flags & PORT_symmetric)
		return xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(message->msg_address), format->fmt_length);

	// Optimize the message by transforming NULL indicators into a bitmap
	// and then skipping the NULL items

	class NullBitmap : private HalfStaticArray<UCHAR, 4>
	{
	public:
		explicit NullBitmap(USHORT size)
		{
			resize(size);
		}

		void setNull(USHORT id)
		{
			data[id >> 3] |= (1 << (id & 7));
		}

		bool isNull(USHORT id) const
		{
			return data[id >> 3] & (1 << (id & 7));
		}

		UCHAR* getData()
		{
			return data;
		}
	};

	fb_assert(format->fmt_desc.getCount() % 2 == 0);
	const USHORT flagBytes = (format->fmt_desc.getCount() / 2 + 7) / 8;
	NullBitmap nulls(flagBytes);

	if (xdrs->x_op == XDR_ENCODE)
	{
		// First pass (odd elements): track NULL indicators

		const dsc* desc = format->fmt_desc.begin() + 1;
		for (const dsc* const end = format->fmt_desc.end(); desc < end; desc += 2)
		{
			fb_assert(desc->dsc_dtype == dtype_short);
			const USHORT index = (USHORT) (desc - format->fmt_desc.begin()) / 2;
			const SSHORT* const flag = (SSHORT*) (message->msg_address + (IPTR) desc->dsc_address);

			if (*flag)
				nulls.setNull(index);
		}

		// Send the NULL bitmap

		if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(nulls.getData()), flagBytes))
			return FALSE;

		// Second pass (even elements): process non-NULL items

		desc = format->fmt_desc.begin();
		for (const dsc* const end = format->fmt_desc.end(); desc < end; desc += 2)
		{
			const USHORT index = (USHORT) (desc - format->fmt_desc.begin()) / 2;

			if (!nulls.isNull(index))
			{
				if (!xdr_datum(xdrs, desc, message->msg_address))
					return FALSE;
			}
		}
	}
	else	// XDR_DECODE
	{
		// Zero-initialize the message

		memset(message->msg_address, 0, format->fmt_length);

		// Receive the NULL bitmap

		if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(nulls.getData()), flagBytes))
			return FALSE;

		// First pass (odd elements): initialize NULL indicators

		const dsc* desc = format->fmt_desc.begin() + 1;
		for (const dsc* const end = format->fmt_desc.end(); desc < end; desc += 2)
		{
			fb_assert(desc->dsc_dtype == dtype_short);
			const USHORT index = (USHORT) (desc - format->fmt_desc.begin()) / 2;
			SSHORT* const flag = (SSHORT*) (message->msg_address + (IPTR) desc->dsc_address);
			*flag = nulls.isNull(index) ? -1 : 0;
		}

		// Second pass (even elements): process non-NULL items

		desc = format->fmt_desc.begin();
		for (const dsc* const end = format->fmt_desc.end(); desc < end; desc += 2)
		{
			const USHORT index = (USHORT) (desc - format->fmt_desc.begin()) / 2;

			if (!nulls.isNull(index))
			{
				if (!xdr_datum(xdrs, desc, message->msg_address))
					return FALSE;
			}
		}
	}

	DEBUG_PRINTSIZE(xdrs, op_void);
	return TRUE;
}


static bool_t xdr_request(RemoteXdr* xdrs,
						  USHORT request_id,
						  USHORT message_number, USHORT incarnation)
{
/**************************************
 *
 *	x d r _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Map a formatted message.
 *
 **************************************/
	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	rem_port* port = xdrs->x_public;

	if (request_id >= port->port_objects.getCount())
		return FALSE;

	Rrq* request;

	try
	{
		request = port->port_objects[request_id];
	}
	catch (const status_exception&)
	{
		return FALSE;
	}

	if (incarnation && !(request = REMOTE_find_request(request, incarnation)))
		return FALSE;

	if (message_number > request->rrq_max_msg)
		return FALSE;

	Rrq::rrq_repeat* tail = &request->rrq_rpt[message_number];

	RMessage* message = tail->rrq_xdr;
	if (!message)
		return FALSE;

	tail->rrq_xdr = message->msg_next;
	const rem_fmt* format = tail->rrq_format;

	// Find the address of the record

	if (!message->msg_address)
		message->msg_address = message->msg_buffer;

	return xdr_message(xdrs, message, format);
}


// Maybe it's better to take sdl_length into account?
static bool_t xdr_slice(RemoteXdr* xdrs, lstring* slice, /*USHORT sdl_length,*/ const UCHAR* sdl)
{
/**************************************
 *
 *	x d r _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Move a slice of an array under
 *
 **************************************/
	if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(&slice->lstr_length)))
		return FALSE;

	// Handle operation specific stuff, particularly memory allocation/deallocation

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		break;

	case XDR_DECODE:
		if (!slice->lstr_length)
			return TRUE;
		if (slice->lstr_length > slice->lstr_allocated && slice->lstr_allocated)
		{
			delete[] slice->lstr_address;
			DEBUG_XDR_FREE(xdrs, slice, slice->lstr_address, slice->lstr_allocated);
			slice->lstr_address = NULL;
		}
		if (!slice->lstr_address)
		{
			try {
				slice->lstr_address = FB_NEW_POOL(*getDefaultMemoryPool()) UCHAR[slice->lstr_length];
			}
			catch (const BadAlloc&) {
				return false;
			}

			slice->lstr_allocated = slice->lstr_length;
			DEBUG_XDR_ALLOC(xdrs, slice, slice->lstr_address, slice->lstr_allocated);
		}
		break;

	case XDR_FREE:
		if (slice->lstr_allocated) {
			delete[] slice->lstr_address;
			DEBUG_XDR_FREE(xdrs, slice, slice->lstr_address, slice->lstr_allocated);
		}
		slice->lstr_address = NULL;
		slice->lstr_allocated = 0;
		return TRUE;
	}

	// Get descriptor of array element

	struct sdl_info info;
	{
		Firebird::LocalStatus ls;
		Firebird::CheckStatusWrapper s(&ls);
		if (SDL_info(&s, sdl, &info, 0))
			return FALSE;
	}

	const dsc* desc = &info.sdl_info_element;
	const rem_port* port = xdrs->x_public;
	BLOB_PTR* p = (BLOB_PTR*) slice->lstr_address;
	ULONG n;

	if (port->port_flags & PORT_symmetric)
	{
		for (n = slice->lstr_length; n > MAX_OPAQUE; n -= MAX_OPAQUE, p += (int) MAX_OPAQUE)
		{
			if (!xdr_opaque (xdrs, reinterpret_cast<SCHAR*>(p), MAX_OPAQUE))
				 return FALSE;
		}
		if (n)
			if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(p), n))
				return FALSE;
	}
	else
	{
		for (n = 0; n < slice->lstr_length / desc->dsc_length; n++)
		{
			if (!xdr_datum(xdrs, desc, p))
				return FALSE;
			p = p + (ULONG) desc->dsc_length;
		}
	}

	return TRUE;
}


static bool_t xdr_sql_blr(RemoteXdr* xdrs,
						  SLONG statement_id,
						  CSTRING* blr,
						  bool direction, SQL_STMT_TYPE stmt_type)
{
/**************************************
 *
 *	x d r _ s q l _ b l r
 *
 **************************************
 *
 * Functional description
 *	Map an sql blr string.  This work is necessary because
 *	we will use the blr to read data in the current packet.
 *
 **************************************/
	if (!xdr_cstring(xdrs, blr))
		return FALSE;

	// We care about all receives and sends from fetch

	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	rem_port* port = xdrs->x_public;

	Rsr* statement;

	if (statement_id >= 0)
	{
		if (static_cast<ULONG>(statement_id) >= port->port_objects.getCount())
			return FALSE;

		try
		{
			statement = port->port_objects[statement_id];
		}
		catch (const status_exception&)
		{
			return FALSE;
		}
	}
	else
	{
		if (!(statement = port->port_statement))
			statement = port->port_statement = FB_NEW Rsr;
	}

	if ((xdrs->x_op == XDR_ENCODE) && !direction)
	{
		if (statement->rsr_bind_format)
			statement->rsr_format = statement->rsr_bind_format;
		return TRUE;
	}

	// Parse the blr describing the message.

	rem_fmt** fmt_ptr = direction ? &statement->rsr_select_format : &statement->rsr_bind_format;

	if (xdrs->x_op == XDR_DECODE)
	{
		// For an immediate statement, flush out any previous format information
		// that might be hanging around from an earlier execution.
		// For all statements, if we have new blr, flush out the format information
		// for the old blr.

		if (*fmt_ptr && ((stmt_type == TYPE_IMMEDIATE) || blr->cstr_length != 0))
		{
			delete *fmt_ptr;
			*fmt_ptr = NULL;
		}

		// If we have BLR describing a new input/output message, get ready by
		// setting up a format

		if (blr->cstr_length)
			*fmt_ptr = PARSE_msg_format(blr->cstr_address, blr->cstr_length);
	}

	// If we know the length of the message, make sure there is a buffer
	// large enough to hold it.

	if (!(statement->rsr_format = *fmt_ptr))
		return TRUE;

	RMessage* message = statement->rsr_buffer;
	if (!message || statement->rsr_format->fmt_length > statement->rsr_fmt_length)
	{
		RMessage* const org_message = message;
		const ULONG org_length = message ? statement->rsr_fmt_length : 0;
		statement->rsr_fmt_length = statement->rsr_format->fmt_length;
		statement->rsr_buffer = message = FB_NEW RMessage(statement->rsr_fmt_length);
		statement->rsr_message = message;
		message->msg_next = message;
		if (org_length)
		{
			// dimitr:	the original buffer might have something useful inside
			//			(filled by a prior xdr_sql_message() call, for example),
			//			so its contents must be preserved (see CORE-3730)
			memcpy(message->msg_buffer, org_message->msg_buffer, org_length);
		}
		REMOTE_release_messages(org_message);
	}

	return TRUE;
}


static bool_t xdr_sql_message( RemoteXdr* xdrs, SLONG statement_id)
{
/**************************************
 *
 *	x d r _ s q l _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted sql message.
 *
 **************************************/
	Rsr* statement;

	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	rem_port* port = xdrs->x_public;

	if (statement_id >= 0)
	{
		if (static_cast<ULONG>(statement_id) >= port->port_objects.getCount())
			return FALSE;

		try
		{
			statement = port->port_objects[statement_id];
		}
		catch (const status_exception&)
		{
			return FALSE;
		}
	}
	else
	{
		statement = port->port_statement;
	}

	if (!statement)
		return FALSE;

	RMessage* message = statement->rsr_buffer;
	if (!message)
		return FALSE;

	statement->rsr_buffer = message->msg_next;
	if (!message->msg_address)
		message->msg_address = message->msg_buffer;

	return (port->port_protocol >= PROTOCOL_VERSION13) ?
		xdr_packed_message(xdrs, message, statement->rsr_format) :
		xdr_message(xdrs, message, statement->rsr_format);
}


static bool_t xdr_status_vector(RemoteXdr* xdrs, DynamicStatusVector*& vector)
{
/**************************************
 *
 *	x d r _ s t a t u s _ v e c t o r
 *
 **************************************
 *
 * Functional description
 *	Map a status vector.  This is tricky since the status vector
 *	may contain argument types, numbers, and strings.
 *
 **************************************/

	// If this is a free operation, release any allocated strings

	if (xdrs->x_op == XDR_FREE)
	{
		delete vector;
		vector = NULL;
		return TRUE;
	}

	if (!vector)
		vector = FB_NEW_POOL(*getDefaultMemoryPool()) DynamicStatusVector();

	StaticStatusVector vectorDecode;
	const ISC_STATUS* vectorEncode = vector->value();

	Stack<SCHAR*> space;
	bool rc = false;

	SLONG vec;

	while (true)
	{
		if (xdrs->x_op == XDR_ENCODE)
			vec = *vectorEncode++;
		if (!xdr_long(xdrs, &vec))
			goto brk;
		if (xdrs->x_op == XDR_DECODE)
			vectorDecode.push((ISC_STATUS) vec);

		switch (static_cast<ISC_STATUS>(vec))
		{
		case isc_arg_end:
			rc = true;
			goto brk;

		case isc_arg_interpreted:
		case isc_arg_string:
		case isc_arg_sql_state:
			if (xdrs->x_op == XDR_ENCODE)
			{
				if (!xdr_wrapstring(xdrs, (SCHAR**)(vectorEncode++)))
					goto brk;
			}
			else
			{
				SCHAR* sp = NULL;

				if (!xdr_wrapstring(xdrs, &sp))
					goto brk;
				vectorDecode.push((ISC_STATUS)(IPTR) sp);
				space.push(sp);
			}
			break;

		case isc_arg_number:
		default:
			if (xdrs->x_op == XDR_ENCODE)
				vec = *vectorEncode++;
			if (!xdr_long(xdrs, &vec))
				goto brk;
			if (xdrs->x_op == XDR_DECODE)
				vectorDecode.push((ISC_STATUS) vec);
			break;
		}
	}

brk:
	// If everything is OK, copy temp buffer to dynamic storage
	if (rc && xdrs->x_op == XDR_DECODE)
	{
		vector->save(vectorDecode.begin());
	}

	// Free memory allocated by xdr_wrapstring()
	while (space.hasData())
	{
		SCHAR* sp = space.pop();
		RemoteXdr freeXdrs;
		freeXdrs.x_public = xdrs->x_public;
		freeXdrs.x_op = XDR_FREE;
		if (!xdr_wrapstring(&freeXdrs, &sp))
		{
			fb_assert(false);	// Very interesting how could it happen
			return FALSE;
		}
	}

	return rc;
}


static bool_t xdr_trrq_blr(RemoteXdr* xdrs, CSTRING* blr)
{
/**************************************
 *
 *	x d r _ t r r q  _ b l r
 *
 **************************************
 *
 * Functional description
 *	Map a message blr string.  This work is necessary because
 *	we will use the blr to read data in the current packet.
 *
 **************************************/
	if (!xdr_cstring(xdrs, blr))
		return FALSE;

	// We care about all receives and sends from fetch

	if (xdrs->x_op == XDR_FREE || xdrs->x_op == XDR_ENCODE)
		return TRUE;

	rem_port* port = xdrs->x_public;
	Rpr* procedure = port->port_rpr;
	if (!procedure)
		procedure = port->port_rpr = FB_NEW Rpr;

	// Parse the blr describing the message.

	delete procedure->rpr_in_msg;
	procedure->rpr_in_msg = NULL;
	delete procedure->rpr_in_format;
	procedure->rpr_in_format = NULL;
	delete procedure->rpr_out_msg;
	procedure->rpr_out_msg = NULL;
	delete procedure->rpr_out_format;
	procedure->rpr_out_format = NULL;

	RMessage* message = PARSE_messages(blr->cstr_address, blr->cstr_length);
	while (message)
	{
		switch (message->msg_number)
		{
		case 0:
			procedure->rpr_in_msg = message;
			procedure->rpr_in_format = (rem_fmt*) message->msg_address;
			message->msg_address = message->msg_buffer;
			message = message->msg_next;
			procedure->rpr_in_msg->msg_next = NULL;
			break;
		case 1:
			procedure->rpr_out_msg = message;
			procedure->rpr_out_format = (rem_fmt*) message->msg_address;
			message->msg_address = message->msg_buffer;
			message = message->msg_next;
			procedure->rpr_out_msg->msg_next = NULL;
			break;
		default:
			{
				RMessage* temp = message;
				message = message->msg_next;
				delete temp;
			}
			break;
		}
	}

	return TRUE;
}


static bool_t xdr_trrq_message( RemoteXdr* xdrs, USHORT msg_type)
{
/**************************************
 *
 *	x d r _ t r r q _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted transact request message.
 *
 **************************************/
	if (xdrs->x_op == XDR_FREE)
		return TRUE;

	rem_port* port = xdrs->x_public;
	Rpr* procedure = port->port_rpr;

	if (msg_type == 1)
		return xdr_message(xdrs, procedure->rpr_out_msg, procedure->rpr_out_format);

	return xdr_message(xdrs, procedure->rpr_in_msg, procedure->rpr_in_format);
}


static void reset_statement( RemoteXdr* xdrs, SSHORT statement_id)
{
/**************************************
 *
 *	r e s e t _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Resets the statement.
 *
 **************************************/

	Rsr* statement = NULL;
	rem_port* port = xdrs->x_public;

	// if the statement ID is -1, this seems to indicate that we are
	// re-executing the previous statement.  This is not a
	// well-understood area of the implementation.

	//if (statement_id == -1)
	//	statement = port->port_statement;
	//else

	fb_assert(statement_id >= -1);

	if (((ULONG) statement_id < port->port_objects.getCount()) && (statement_id >= 0))
	{
		try
		{
			statement = port->port_objects[statement_id];
			REMOTE_reset_statement(statement);
		}
		catch (const status_exception&)
		{} // no-op
	}
}

static Rsr* getStatement(RemoteXdr* xdrs, USHORT statement_id)
{
	rem_port* port = xdrs->x_public;

	if (statement_id >= 0)
	{
		if (statement_id >= port->port_objects.getCount())
			return nullptr;

		try
		{
			return port->port_objects[statement_id];
		}
		catch (const status_exception&)
		{
			return nullptr;
		}
	}

	return port->port_statement;
}

static bool_t xdr_blob_stream(RemoteXdr* xdrs, SSHORT statement_id, CSTRING* strmPortion)
{
	if (xdrs->x_op == XDR_FREE)
		return xdr_cstring(xdrs, strmPortion);

	Rsr* statement = getStatement(xdrs, statement_id);
	if (!statement)
		return FALSE;

	// create local copy - required in a case when packet is not complete and will be restarted
	Rsr::BatchStream localStrm(statement->rsr_batch_stream);

	struct BlobFlow
	{
		ULONG remains;
		UCHAR* streamPtr;
		ULONG& blobSize;
		ULONG& bpbSize;
		ULONG& segSize;

		BlobFlow(Rsr::BatchStream* bs)
			: remains(0), streamPtr(NULL),
			  blobSize(bs->blobRemaining), bpbSize(bs->bpbRemaining), segSize(bs->segRemaining)
		{ }

		void newBlob(ULONG totalSize, ULONG parSize)
		{
			blobSize = totalSize;
			bpbSize = parSize;
			segSize = 0;
		}

		void move(ULONG step)
		{
			move2(step);
			blobSize -= step;
		}

		void moveBpb(ULONG step)
		{
			move(step);
			bpbSize -= step;
		}

		void moveSeg(ULONG step)
		{
			move(step);
			segSize -= step;
		}

		bool align(ULONG alignment)
		{
			ULONG a = IPTR(streamPtr) % alignment;
			if (a)
			{
				a = alignment - a;
				move2(a);
				if (blobSize)
					blobSize -= a;
			}
			return a;
		}

private:
		void move2(ULONG step)
		{
			streamPtr += step;
			remains -= step;
		}
	};

	BlobFlow flow(&localStrm);

	if (xdrs->x_op == XDR_ENCODE)
	{
		flow.remains = strmPortion->cstr_length;
		strmPortion->cstr_length += localStrm.hdrPrevious;
	}
	if (!xdr_u_long(xdrs, &strmPortion->cstr_length))
		return FALSE;
	if (xdrs->x_op == XDR_DECODE)
		flow.remains = strmPortion->cstr_length;

	fb_assert(localStrm.alignment);
	if (flow.remains % localStrm.alignment)
		return FALSE;
	if (!flow.remains)
		return TRUE;

	if (xdrs->x_op == XDR_DECODE)
		alloc_cstring(xdrs, strmPortion);

	flow.streamPtr = strmPortion->cstr_address;
	if (IPTR(flow.streamPtr) % localStrm.alignment != 0)
		return FALSE;

	while (flow.remains)
	{
		if (!flow.blobSize)		// we should process next blob header
		{
			// align data stream
			if (flow.align(localStrm.alignment))
				continue;

			// check for partial header in the stream
			if (flow.remains + localStrm.hdrPrevious < Rsr::BatchStream::SIZEOF_BLOB_HEAD)
			{
				// On the receiver that means packet protocol processing is complete: actual
				// size of packet is sligtly less than passed in batch_blob_data.cstr_length.
				if (xdrs->x_op == XDR_DECODE)
					strmPortion->cstr_length -= flow.remains;
				// On transmitter reserve partial header for future use
				else
					localStrm.saveData(flow.streamPtr, flow.remains);

				// Done with packet
				break;
			}

			// parse blob header
			fb_assert(intptr_t(flow.streamPtr) % localStrm.alignment == 0);
			unsigned char* hdrPtr = flow.streamPtr;	// default is to use header in main buffer
			unsigned hdrOffset = Rsr::BatchStream::SIZEOF_BLOB_HEAD;
			if (localStrm.hdrPrevious)
			{
				// on transmitter reserved partial header may be used
				fb_assert(xdrs->x_op == XDR_ENCODE);
				hdrOffset -= localStrm.hdrPrevious;
				localStrm.saveData(flow.streamPtr, hdrOffset);
				hdrPtr = localStrm.hdr;
			}

			ISC_QUAD* batchBlobId = reinterpret_cast<ISC_QUAD*>(hdrPtr);
			ULONG* blobSize = reinterpret_cast<ULONG*>(hdrPtr + sizeof(ISC_QUAD));
			ULONG* bpbSize = reinterpret_cast<ULONG*>(hdrPtr + sizeof(ISC_QUAD) + sizeof(ULONG));
			if (!xdr_quad(xdrs, batchBlobId))
				return FALSE;
			if (!xdr_u_long(xdrs, blobSize))
				return FALSE;
			if (!xdr_u_long(xdrs, bpbSize))
				return FALSE;

			flow.move(hdrOffset);
			localStrm.hdrPrevious = 0;
			flow.newBlob(*blobSize, *bpbSize);
			localStrm.curBpb.clear();

			if (!flow.bpbSize)
				localStrm.segmented = statement->rsr_batch_flags & (1 << Jrd::DsqlBatch::FLAG_DEFAULT_SEGMENTED);

			continue;
		}

		// process BPB
		if (flow.bpbSize)
		{
			ULONG size = MIN(flow.bpbSize, flow.remains);
			if (!xdr_bytes(xdrs, flow.streamPtr, size))
				return FALSE;
			localStrm.curBpb.add(flow.streamPtr, size);
			flow.moveBpb(size);
			if (flow.bpbSize == 0)		// bpb is passed completely
			{
				try
				{
					localStrm.segmented = fb_utils::isBpbSegmented(localStrm.curBpb.getCount(),
						localStrm.curBpb.begin());
				}
				catch (const Exception&)
				{
					return FALSE;
				}
				localStrm.curBpb.clear();
			}

			continue;
		}

		// pass data
		ULONG dataSize = MIN(flow.blobSize, flow.remains);
		if (dataSize)
		{
			if (localStrm.segmented)
			{
				if (!flow.segSize)
				{
					if (flow.align(IBatch::BLOB_SEGHDR_ALIGN))
						continue;

					USHORT* segSize = reinterpret_cast<USHORT*>(flow.streamPtr);
					if (!xdr_u_short(xdrs, segSize))
						return FALSE;
					flow.segSize = *segSize;
					flow.move(sizeof(USHORT));

					if (flow.segSize > flow.blobSize)
						return FALSE;
				}

				dataSize = MIN(flow.segSize, flow.remains);
				if (!xdr_bytes(xdrs, flow.streamPtr, dataSize))
					return FALSE;
				flow.moveSeg(dataSize);
			}
			else
			{
				if (!xdr_bytes(xdrs, flow.streamPtr, dataSize))
					return FALSE;
				flow.move(dataSize);
			}
		}
	}

	// packet processed successfully - save stream data for next one
	statement->rsr_batch_stream = localStrm;

	return TRUE;
}
