/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		remot_proto.h
 *	DESCRIPTION:	Prototpe header file for remote.cpp
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

#ifndef REMOTE_REMOT_PROTO_H
#define REMOTE_REMOT_PROTO_H

#include "../common/classes/fb_string.h"
#include "../common/config/config.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/ParsedList.h"
#include "../common/xdr.h"
#include "../remote/protocol.h"

namespace Firebird
{
	class ClumpletReader;
}

struct rem_port;
struct rem_fmt;
struct Rdb;
typedef bool PacketReceive(rem_port*, UCHAR*, SSHORT, SSHORT*);
typedef bool PacketSend(rem_port*, const SCHAR*, SSHORT);
typedef bool ProtoWrite(XDR*);
enum LegacyPlugin {PLUGIN_NEW = 0, PLUGIN_LEGACY, PLUGIN_TRUSTED};

void		REMOTE_cleanup_transaction (struct Rtr *);
USHORT		REMOTE_compute_batch_size (rem_port*, USHORT, P_OP, const rem_fmt*);
void		REMOTE_get_timeout_params(rem_port* port, Firebird::ClumpletReader* pb);
struct Rrq*	REMOTE_find_request (struct Rrq *, USHORT);
void		REMOTE_free_packet (rem_port*, struct packet *, bool = false);
struct rem_str*	REMOTE_make_string (const SCHAR*);
void		REMOTE_release_messages (struct RMessage*);
void		REMOTE_release_request (struct Rrq *);
void		REMOTE_reset_request (struct Rrq *, struct RMessage*);
void		REMOTE_reset_statement (struct Rsr *);
bool_t		REMOTE_getbytes (XDR*, SCHAR*, unsigned);
LegacyPlugin REMOTE_legacy_auth(const char* nm, int protocol);
Firebird::RefPtr<const Firebird::Config> REMOTE_get_config(const Firebird::PathName* dbName,
	const Firebird::string* dpb_config = NULL);
void		REMOTE_check_response(Firebird::IStatus* warning, Rdb* rdb, PACKET* packet, bool checkKeys = false);
bool		REMOTE_inflate(rem_port*, PacketReceive*, UCHAR*, SSHORT, SSHORT*);
bool		REMOTE_deflate(XDR*, ProtoWrite*, PacketSend*, bool flash);

extern signed char wcCompatible[3][3];

#define HANDSHAKE_DEBUG(A)
#define WIRECRYPT_DEBUG(A)

#endif // REMOTE_REMOT_PROTO_H
