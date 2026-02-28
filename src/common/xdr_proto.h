/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		xdr_proto.h
 *	DESCRIPTION:	Prototype Header file for xdr.cpp
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

#ifndef REMOTE_XDR_PROTO_H
#define REMOTE_XDR_PROTO_H

#include "../common/dsc.h"
#include "../common/xdr.h"

bool_t	xdr_datum(xdr_t*, const dsc*, UCHAR*);
bool_t	xdr_double(xdr_t*, double*);
bool_t	xdr_dec64(xdr_t*, Firebird::Decimal64*);
bool_t	xdr_dec128(xdr_t*, Firebird::Decimal128*);
bool_t	xdr_float(xdr_t*, float*);
bool_t	xdr_int(xdr_t*, int*);
bool_t	xdr_int128(xdr_t*, Firebird::Int128*);
bool_t	xdr_long(xdr_t*, SLONG*);

// Template xdr_enum for type-safe enum serialization.
// Accepts any enum type, uses static_cast to avoid UB from reinterpret_cast.
template<typename EnumT>
inline bool_t xdr_enum(xdr_t* xdrs, EnumT* ip)
{
	SLONG temp;
	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = static_cast<SLONG>(*ip);
		return xdr_long(xdrs, &temp);
	case XDR_DECODE:
		if (!xdr_long(xdrs, &temp))
			return FALSE;
		*ip = static_cast<EnumT>(temp);
		return TRUE;
	case XDR_FREE:
		return TRUE;
	}
	return FALSE;
}
bool_t	xdr_opaque(xdr_t*, SCHAR*, unsigned);
bool_t	xdr_quad(xdr_t*, SQUAD*);
bool_t	xdr_short(xdr_t*, SSHORT*);
bool_t	xdr_string(xdr_t*, SCHAR**, unsigned);
bool_t	xdr_u_int(xdr_t*, unsigned*);
bool_t	xdr_u_long(xdr_t*, ULONG*);
bool_t	xdr_u_short(xdr_t*, u_short*);
bool_t	xdr_wrapstring(xdr_t*, SCHAR**);
bool_t	xdr_hyper(xdr_t*, void*);
SLONG	xdr_peek_long(const xdr_t*, const void* data, size_t size);

#endif	// REMOTE_XDR_PROTO_H
