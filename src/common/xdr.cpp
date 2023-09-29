/*
 *	PROGRAM:	External Data Representation
 *	MODULE:		xdr.cpp
 *	DESCRIPTION:	GDS version of Sun's register XDR Package.
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
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "firebird.h"
#include <string.h>
#include "../common/xdr.h"
#include "../common/xdr_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/gdsassert.h"
#include "../common/DecFloat.h"
#include "../common/Int128.h"

typedef struct xdr_t xdr_t;

inline UCHAR* XDR_ALLOC(ULONG size)
{
	return (UCHAR*) gds__alloc((SLONG) size);
}
inline void XDR_FREEA(void* block)
{
	gds__free(block);
}

#ifdef DEBUG_XDR_MEMORY
inline void DEBUG_XDR_ALLOC(xdr_t* xdrs, const void* xdrvar, const void* addr, ULONG len)
{
	xdr_debug_memory(xdrs, XDR_DECODE, xdrvar, addr, len)
}
inline void DEBUG_XDR_FREE(xdr_t* xdrs, const void* xdrvar, const void* addr, ULONG len)
{
	xdr_debug_memory (xdrs, XDR_FREE, xdrvar, addr, (ULONG) len);
}
#else
inline void DEBUG_XDR_ALLOC(xdr_t*, const void*, const void*, ULONG)
{
}
inline void DEBUG_XDR_FREE(xdr_t*, const void*, const void*, ULONG)
{
}
#endif // DEBUG_XDR_MEMORY

// Sun's XDR documentation says this should be "MAXUNSIGNED", but
// for Firebird purposes, limiting strings to 65K is more than
// sufficient.
// This setting may be related to our max DSQL statement size.

const unsigned MAXSTRING_FOR_WRAPSTRING	= 65535;


#define GETBYTES	 xdrs->x_getbytes
#define PUTBYTES	 xdrs->x_putbytes

inline bool_t GETLONG(xdr_t* xdrs, SLONG* lp)
{
	SLONG l;

	if (!xdrs->x_getbytes(reinterpret_cast<char*>(&l), 4))
		return FALSE;

	*lp = xdrs->x_local ? l : ntohl(l);

	return TRUE;
}

inline bool_t PUTLONG(xdr_t* xdrs, const SLONG* lp)
{
	const SLONG l = xdrs->x_local ? *lp : htonl(*lp);
	return xdrs->x_putbytes(reinterpret_cast<const char*>(&l), 4);
}

static SCHAR zeros[4] = { 0, 0, 0, 0 };


bool_t xdr_hyper( xdr_t* xdrs, void* pi64)
{
/**************************************
 *
 *	x d r _ h y p e r
 *
 **************************************
 *
 * Functional description
 *	Map a 64-bit Integer from external to internal representation
 *      (or vice versa).
 *
 *      Handles "swapping" of the 2 long's to be "Endian" sensitive.
 *
 **************************************/
	SLONG temp_long[2];

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		memcpy(temp_long, pi64, sizeof temp_long);
#ifndef WORDS_BIGENDIAN
		if (PUTLONG(xdrs, &temp_long[1]) &&
			PUTLONG(xdrs, &temp_long[0]))
		{
			return TRUE;
		}
#else
		if (PUTLONG(xdrs, &temp_long[0]) &&
			PUTLONG(xdrs, &temp_long[1]))
		{
			return TRUE;
		}
#endif
		return FALSE;

	case XDR_DECODE:
#ifndef WORDS_BIGENDIAN
		if (!GETLONG(xdrs, &temp_long[1]) ||
			!GETLONG(xdrs, &temp_long[0]))
		{
			return FALSE;
		}
#else
		if (!GETLONG(xdrs, &temp_long[0]) ||
			!GETLONG(xdrs, &temp_long[1]))
		{
			return FALSE;
		}
#endif
		memcpy(pi64, temp_long, sizeof temp_long);
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}
	// TMN: added compiler silencier return FALSE.
	return FALSE;
}


bool_t xdr_datum( xdr_t* xdrs, const dsc* desc, UCHAR* buffer)
{
/**************************************
 *
 *	x d r _ d a t u m
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *	Handle a data item by relative descriptor and buffer.
 *
 **************************************/
	BLOB_PTR* p = buffer + (IPTR) desc->dsc_address;

	switch (desc->dsc_dtype)
	{
	case dtype_dbkey:
		fb_assert(false);	// dbkey should not get outside jrd,
		// but in case it happenned in production server treat it as text
		// Fall through ...

	case dtype_text:
	case dtype_boolean:
		if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(p), desc->dsc_length))
			return FALSE;
		break;

	case dtype_varying:
		{
			fb_assert(desc->dsc_length >= sizeof(USHORT));
			vary* v = reinterpret_cast<vary*>(p);
			if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(&v->vary_length)))
			{
				return FALSE;
			}
			if (!xdr_opaque(xdrs, v->vary_string,
							MIN((USHORT) (desc->dsc_length - 2), v->vary_length)))
			{
				return FALSE;
			}
			if (xdrs->x_op == XDR_DECODE && desc->dsc_length - 2 > v->vary_length)
			{
				memset(v->vary_string + v->vary_length, 0, desc->dsc_length - 2 - v->vary_length);
			}
		}
		break;

	case dtype_cstring:
	    {
			//SSHORT n;
			USHORT n;
			if (xdrs->x_op == XDR_ENCODE)
			{
				n = MIN(static_cast<ULONG>(strlen(reinterpret_cast<char*>(p))), (ULONG)(desc->dsc_length - 1));
			}
			if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(&n)))
				return FALSE;
			if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(p), n))
				return FALSE;
			if (xdrs->x_op == XDR_DECODE)
				p[n] = 0;
		}
		break;

	case dtype_short:
		fb_assert(desc->dsc_length >= sizeof(SSHORT));
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p)))
			return FALSE;
		break;

	case dtype_sql_time:
	case dtype_sql_date:
	case dtype_long:
		fb_assert(desc->dsc_length >= sizeof(SLONG));
		if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(p)))
			return FALSE;
		break;

	case dtype_sql_time_tz:
		fb_assert(desc->dsc_length >= sizeof(SLONG) + sizeof(SSHORT));
		if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(p)))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + sizeof(SLONG))))
			return FALSE;
		break;

	case dtype_ex_time_tz:
		fb_assert(desc->dsc_length >= sizeof(SLONG) + 2 * sizeof(SSHORT));
		if (!xdr_long(xdrs, reinterpret_cast<SLONG*>(p)))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + sizeof(SLONG))))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + sizeof(SLONG) + sizeof(SSHORT))))
			return FALSE;
		break;

	case dtype_real:
		fb_assert(desc->dsc_length >= sizeof(float));
		if (!xdr_float(xdrs, reinterpret_cast<float*>(p)))
			return FALSE;
		break;

	case dtype_double:
		fb_assert(desc->dsc_length >= sizeof(double));
		if (!xdr_double(xdrs, reinterpret_cast<double*>(p)))
			return FALSE;
		break;

	case dtype_dec64:
		fb_assert(desc->dsc_length >= sizeof(Firebird::Decimal64));
		if (!xdr_dec64(xdrs, reinterpret_cast<Firebird::Decimal64*>(p)))
			return FALSE;
		break;

	case dtype_dec128:
		fb_assert(desc->dsc_length >= sizeof(Firebird::Decimal128));
		if (!xdr_dec128(xdrs, reinterpret_cast<Firebird::Decimal128*>(p)))
			return FALSE;
		break;

	case dtype_int128:
		fb_assert(desc->dsc_length >= sizeof(Firebird::Int128));
		if (!xdr_int128(xdrs, reinterpret_cast<Firebird::Int128*>(p)))
			return FALSE;
		break;

	case dtype_timestamp:
		fb_assert(desc->dsc_length >= 2 * sizeof(SLONG));
		if (!xdr_long(xdrs, &((SLONG*) p)[0]))
			return FALSE;
		if (!xdr_long(xdrs, &((SLONG*) p)[1]))
			return FALSE;
		break;

	case dtype_timestamp_tz:
		fb_assert(desc->dsc_length >= 2 * sizeof(SLONG) + sizeof(SSHORT));
		if (!xdr_long(xdrs, &((SLONG*) p)[0]))
			return FALSE;
		if (!xdr_long(xdrs, &((SLONG*) p)[1]))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + 2 * sizeof(SLONG))))
			return FALSE;
		break;

	case dtype_ex_timestamp_tz:
		fb_assert(desc->dsc_length >= 2 * sizeof(SLONG) + 2 * sizeof(SSHORT));
		if (!xdr_long(xdrs, &((SLONG*) p)[0]))
			return FALSE;
		if (!xdr_long(xdrs, &((SLONG*) p)[1]))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + 2 * sizeof(SLONG))))
			return FALSE;
		if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(p + 2 * sizeof(SLONG) + sizeof(SSHORT))))
			return FALSE;
		break;

	case dtype_int64:
		fb_assert(desc->dsc_length >= sizeof(SINT64));
		if (!xdr_hyper(xdrs, reinterpret_cast<SINT64*>(p)))
			return FALSE;
		break;

	case dtype_array:
	case dtype_quad:
	case dtype_blob:
		fb_assert(desc->dsc_length >= sizeof(SQUAD));
		if (!xdr_quad(xdrs, reinterpret_cast<SQUAD*>(p)))
			return FALSE;
		break;

	default:
		fb_assert(FALSE);
		return FALSE;
	}

	return TRUE;
}


bool_t xdr_double(xdr_t* xdrs, double* ip)
{
/**************************************
 *
 *	x d r _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	union {
		double temp_double;
		SLONG temp_long[2];
	} temp;

	fb_assert(sizeof(double) == sizeof(temp));

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp.temp_double = *ip;
		if (PUTLONG(xdrs, &temp.temp_long[FB_LONG_DOUBLE_FIRST]) &&
			PUTLONG(xdrs, &temp.temp_long[FB_LONG_DOUBLE_SECOND]))
		{
			return TRUE;
		}
		return FALSE;

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp.temp_long[FB_LONG_DOUBLE_FIRST]) ||
			!GETLONG(xdrs, &temp.temp_long[FB_LONG_DOUBLE_SECOND]))
		{
			return FALSE;
		}
		*ip = temp.temp_double;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}

/*
DecFloat (at least as implemented in IBM's library) has a kind of PDP-endian format:
Bytes in 4-byte words are in endianess dependent order
4-byte words - are in endianess independent order

Therefore need in special processing
*/

static bool_t xdr_decfloat_hyper(xdr_t* xdrs, void* dec)
{
	SLONG temp_long[2];

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		memcpy(temp_long, dec, sizeof temp_long);
		if (PUTLONG(xdrs, &temp_long[1]) &&
			PUTLONG(xdrs, &temp_long[0]))
		{
			return TRUE;
		}
		return FALSE;

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp_long[1]) ||
			!GETLONG(xdrs, &temp_long[0]))
		{
			return FALSE;
		}
		memcpy(dec, temp_long, sizeof temp_long);
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}
	// TMN: added compiler silencier return FALSE.
	return FALSE;
}


bool_t xdr_dec64(xdr_t* xdrs, Firebird::Decimal64* ip)
{
	return xdr_decfloat_hyper(xdrs, ip->getBytes());
}


bool_t xdr_dec128(xdr_t* xdrs, Firebird::Decimal128* ip)
{
	UCHAR* bytes = ip->getBytes();
	return xdr_decfloat_hyper(xdrs, &bytes[8]) && xdr_decfloat_hyper(xdrs, &bytes[0]);
}


bool_t xdr_int128(xdr_t* xdrs, Firebird::Int128* ip)
{
	UCHAR* bytes = ip->getBytes();

#ifndef WORDS_BIGENDIAN
	return xdr_hyper(xdrs, &bytes[8]) && xdr_hyper(xdrs, &bytes[0]);
#else
	fb_assert(false);			// Dec64/128 XDR not tested on bigendians!
	return xdr_hyper(xdrs, &bytes[0]) && xdr_hyper(xdrs, &bytes[8]);
#endif
}


bool_t xdr_enum(xdr_t* xdrs, xdr_op* ip)
{
/**************************************
 *
 *	x d r _ e n u m
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	SLONG temp;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = (SLONG) *ip;
		return PUTLONG(xdrs, &temp);

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp))
			return FALSE;
		*ip = (xdr_op) temp;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_float(xdr_t* xdrs, float* ip)
{
/**************************************
 *
 *	x d r _ f l o a t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	fb_assert(sizeof(float) == sizeof(SLONG));

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		return PUTLONG(xdrs, reinterpret_cast<SLONG*>(ip));

	case XDR_DECODE:
		return GETLONG(xdrs, reinterpret_cast<SLONG*>(ip));

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_int(xdr_t* xdrs, int* ip)
{
/**************************************
 *
 *	x d r _ i n t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	SLONG temp;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = *ip;
		return PUTLONG(xdrs, &temp);

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp))
			return FALSE;
		*ip = (int) temp;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_long(xdr_t* xdrs, SLONG* ip)
{
/**************************************
 *
 *	x d r _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		return PUTLONG(xdrs, ip);

	case XDR_DECODE:
		return GETLONG(xdrs, ip);

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_opaque(xdr_t* xdrs, SCHAR* p, unsigned len)
{
/**************************************
 *
 *	x d r _ o p a q u e
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free an opaque object.
 *
 **************************************/
	SCHAR trash[4];
	static const SCHAR filler[4] = { 0, 0, 0, 0 };

	const SSHORT l = (4 - len) & 3;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		if (!PUTBYTES(p, len))
			return FALSE;
		if (l)
			return PUTBYTES(filler, l);
		return TRUE;

	case XDR_DECODE:
		if (!GETBYTES(p, len))
			return FALSE;
		if (l)
			return GETBYTES(trash, l);
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_quad( xdr_t* xdrs, SQUAD* ip)
{
/**************************************
 *
 *	x d r _ q u a d
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *	A "quad" is represented by two longs.
 *	Currently used only for blobs
 *
 **************************************/

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		if (PUTLONG(xdrs, reinterpret_cast<SLONG*>(&ip->gds_quad_high)) &&
			PUTLONG(xdrs, reinterpret_cast<SLONG*>(&ip->gds_quad_low)))
		{
			return TRUE;
		}
		return FALSE;

	case XDR_DECODE:
		if (!GETLONG(xdrs, reinterpret_cast<SLONG*>(&ip->gds_quad_high)))
		{
			return FALSE;
		}
		return GETLONG(xdrs, reinterpret_cast<SLONG*>(&ip->gds_quad_low));

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_short(xdr_t* xdrs, SSHORT* ip)
{
/**************************************
 *
 *	x d r _ s h o r t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	SLONG temp;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = *ip;
		return PUTLONG(xdrs, &temp);

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp))
			return FALSE;
		*ip = (SSHORT) temp;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_string(xdr_t* xdrs, SCHAR** sp, unsigned maxlength)
{
/**************************************
 *
 *	x d r _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free a string.
 *
 **************************************/
	SCHAR trash[4];
	static const SCHAR filler[4] = { 0, 0, 0, 0 };
	ULONG length;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		length = static_cast<ULONG>(strlen(*sp));
		if (length > maxlength ||
			!PUTLONG(xdrs, reinterpret_cast<SLONG*>(&length)) ||
			!PUTBYTES(*sp, length))
		{
			return FALSE;
		}
		if ((length = (4 - length) & 3) != 0)
			return PUTBYTES(filler, length);
		return TRUE;

	case XDR_DECODE:
		if (!*sp)
		{
			*sp = (SCHAR*) XDR_ALLOC((ULONG) (maxlength + 1));
			// FREE: via XDR_FREE call to this procedure
			if (!*sp)			// NOMEM: return error
				return FALSE;
			DEBUG_XDR_ALLOC(xdrs, sp, *sp, (maxlength + 1));
		}
		if (!GETLONG(xdrs, reinterpret_cast<SLONG*>(&length)) ||
			length > maxlength || !GETBYTES(*sp, length))
		{
			return FALSE;
		}
		(*sp)[length] = 0;
		if ((length = (4 - length) & 3) != 0)
			return GETBYTES(trash, length);
		return TRUE;

	case XDR_FREE:
		if (*sp)
		{
			XDR_FREEA(*sp);
			DEBUG_XDR_FREE(xdrs, sp, *sp, (maxlength + 1));
			*sp = NULL;
		}
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_u_int(xdr_t* xdrs, unsigned* ip)
{
/**************************************
 *
 *	x d r _ u _ i n t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	SLONG temp;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = *ip;
		return PUTLONG(xdrs, &temp);

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp))
			return FALSE;
		*ip = temp;
		return TRUE;

	case XDR_FREE:
		return TRUE;

	default:
		return FALSE;
	}
}


bool_t xdr_u_long(xdr_t* xdrs, ULONG* ip)
{
/**************************************
 *
 *	x d r _ u _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		return PUTLONG(xdrs, reinterpret_cast<SLONG*>(ip));

	case XDR_DECODE:
		if (!GETLONG(xdrs, reinterpret_cast<SLONG*>(ip)))
			  return FALSE;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_u_short(xdr_t* xdrs, u_short* ip)
{
/**************************************
 *
 *	x d r _ u _ s h o r t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
	SLONG temp;

	switch (xdrs->x_op)
	{
	case XDR_ENCODE:
		temp = *ip;
		return PUTLONG(xdrs, &temp);

	case XDR_DECODE:
		if (!GETLONG(xdrs, &temp))
			return FALSE;
		*ip = (unsigned) temp;
		return TRUE;

	case XDR_FREE:
		return TRUE;
	}

	return FALSE;
}


bool_t xdr_wrapstring(xdr_t* xdrs, SCHAR** strp)
{
/**************************************
 *
 *	x d r _ w r a p s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

	return xdr_string(xdrs, strp, MAXSTRING_FOR_WRAPSTRING);
}


int xdr_t::create(SCHAR* addr, unsigned len, xdr_op op)
{
/**************************************
 *
 *	x d r m e m _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Initialize an "in memory" register XDR stream.
 *
 **************************************/

	x_base = x_private = addr;
	x_handy = len;
	x_op = op;

	return TRUE;
}

bool_t xdr_t::x_getbytes(SCHAR* buff, unsigned bytecount)
{
/**************************************
 *
 *	m e m _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Get a bunch of bytes from a memory stream if it fits.
 *
 **************************************/
	if (x_handy < bytecount)
		return FALSE;

	if (bytecount)
	{
		memcpy(buff, x_private, bytecount);
		x_private += bytecount;
		x_handy -= bytecount;
	}

	return TRUE;
}


SLONG xdr_peek_long(const xdr_t* xdrs, const void* data, size_t size)
{
/**************************************
 *
 *	x d r _ p e e k _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch the first four bytes (supposedly, the operation code)
 *	from the given buffer and convert it into the host byte order.
 *
 **************************************/
	if (size < sizeof(SLONG))
		return 0;

	const SLONG* p = (SLONG*) data;
	return xdrs->x_local ? *p : ntohl(*p);
}


bool_t xdr_t::x_putbytes(const SCHAR* buff, unsigned bytecount)
{
/**************************************
 *
 *	m e m _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Put a bunch of bytes to a memory stream if it fits.
 *
 **************************************/
	if (x_handy < bytecount)
		return FALSE;

	if (bytecount)
	{
		memcpy(x_private, buff, bytecount);
		x_private += bytecount;
		x_handy -= bytecount;
	}

	return TRUE;
}

xdr_t::~xdr_t()
{ }

