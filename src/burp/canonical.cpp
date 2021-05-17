/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		canonical.cpp
 *	DESCRIPTION:
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
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix/MIPS" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#ifdef HP11
#include <arpa/inet.h>
#endif
#include "../burp/burp.h"
#include "../jrd/align.h"
#include "../common/sdl.h"
#include "../burp/canon_proto.h"
#include "../common/sdl_proto.h"
#include "../common/xdr_proto.h"
#include "../common/gdsassert.h"
#include "../common/StatusHolder.h"
#include "../common/status.h"
#include "fb_types.h"


using Firebird::FbLocalStatus;

struct BurpXdr : public xdr_t
{
	virtual bool_t x_getbytes(SCHAR *, unsigned);		// get some bytes from "
	virtual bool_t x_putbytes(const SCHAR*, unsigned);	// put some bytes to "

	BurpXdr()
		: x_public(nullptr)
	{ }

	lstring* x_public;
};
static bool_t expand_buffer(BurpXdr*);
static int xdr_init(BurpXdr*, lstring*, enum xdr_op);
static bool_t xdr_slice(BurpXdr*, lstring*, /*USHORT,*/ const UCHAR*);

const unsigned increment = 1024;


ULONG CAN_encode_decode(burp_rel* relation, lstring* buffer, UCHAR* data, bool direction, bool useMissingOffset)
{
/**************************************
 *
 *	C A N _ e n c o d e _ d e c o d e
 *
 **************************************
 *
 * Functional description
 *	encode and decode canonical backup.
 *
 **************************************/
	const burp_fld* field;
	SSHORT n;

	BurpXdr xdr;
	BurpXdr* xdrs = &xdr;

	xdr_init(xdrs, buffer, direction ? XDR_ENCODE : XDR_DECODE);

	RCRD_OFFSET offset = 0;
	for (field = relation->rel_fields; field; field = field->fld_next)
	{
		if (field->fld_flags & FLD_computed)
			continue;
		UCHAR* p = data + field->fld_offset;
		const bool array_fld = ((field->fld_flags & FLD_array) != 0);
		const FLD_LENGTH length = array_fld ? 8 : field->fld_length;
		if (field->fld_offset >= offset)
			offset = field->fld_offset + length;
		if (field->fld_type == blr_varying && !array_fld)
			offset += sizeof(SSHORT);
		SSHORT dtype;
		if (field->fld_type == blr_blob || array_fld)
			dtype = dtype_blob;
		else
			dtype = (SSHORT) gds_cvt_blr_dtype[field->fld_type];
		switch (dtype)
		{
		case dtype_text:
			if (!xdr_opaque(xdrs, reinterpret_cast<char*>(p), length))
			{
				return FALSE;
			}
			break;

		case dtype_varying:
			{
				vary* pVary = reinterpret_cast<vary*>(p);
				if (!xdr_short(xdrs, reinterpret_cast<SSHORT*>(&pVary->vary_length)))
				{
					return FALSE;
				}
				if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(pVary->vary_string),
								MIN(pVary->vary_length, length)))
				{
				  return FALSE;
				}
			}
			break;

		case dtype_cstring:
			if (xdrs->x_op == XDR_ENCODE)
				n = static_cast<SSHORT>(MIN(strlen(reinterpret_cast<const char*>(p)), length));
			if (!xdr_short(xdrs, &n))
				return FALSE;
			if (!xdr_opaque(xdrs, reinterpret_cast<SCHAR*>(p), n))
				  return FALSE;
			if (xdrs->x_op == XDR_DECODE)
				p[n] = 0;
			break;

		case dtype_short:
			if (!xdr_short(xdrs, (SSHORT*) p))
				return FALSE;
			break;

		case dtype_long:
		case dtype_sql_time:
		case dtype_sql_date:
			if (!xdr_long(xdrs, (SLONG*) p))
				return FALSE;
			break;

		case dtype_sql_time_tz:
			if (!xdr_long(xdrs, (SLONG*) p))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG))))
				return FALSE;
			break;

		case dtype_ex_time_tz:
			if (!xdr_long(xdrs, (SLONG*) p))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG))))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG) + sizeof(SSHORT))))
				return FALSE;
			break;

		case dtype_real:
			if (!xdr_float(xdrs, (float*) p))
				return FALSE;
			break;

		case dtype_double:
			if (!xdr_double(xdrs, (double*) p))
				return FALSE;
			break;

		case dtype_dec64:
			if (!xdr_dec64(xdrs, (Firebird::Decimal64*) p))
				return FALSE;
			break;

		case dtype_dec128:
			if (!xdr_dec128(xdrs, (Firebird::Decimal128*) p))
				return FALSE;
			break;

		case dtype_int128:
			if (!xdr_int128(xdrs, (Firebird::Int128*) p))
				return FALSE;
			break;

		case dtype_timestamp:
			if (!xdr_long(xdrs, &((SLONG*) p)[0]))
				return FALSE;
			if (!xdr_long(xdrs, &((SLONG*) p)[1]))
				return FALSE;
			break;

		case dtype_timestamp_tz:
			if (!xdr_long(xdrs, (SLONG*) p))
				return FALSE;
			if (!xdr_long(xdrs, &((SLONG*) p)[1]))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG) + sizeof(SLONG))))
				return FALSE;
			break;

		case dtype_ex_timestamp_tz:
			if (!xdr_long(xdrs, (SLONG*) p))
				return FALSE;
			if (!xdr_long(xdrs, &((SLONG*) p)[1]))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG) + sizeof(SLONG))))
				return FALSE;
			if (!xdr_short(xdrs, (SSHORT*) (p + sizeof(SLONG) + sizeof(SLONG) + sizeof(SSHORT))))
				return FALSE;
			break;

		case dtype_quad:
		case dtype_blob:
			if (!xdr_quad(xdrs, (SQUAD*) p))
				return FALSE;
			break;

		case dtype_int64:
			if (!xdr_hyper(xdrs, (SINT64*) p))
				return FALSE;
			break;

		case dtype_boolean:
			if (!xdr_opaque(xdrs, (SCHAR*) p, length))
				return FALSE;
			break;

		default:
			fb_assert(FALSE);
			return FALSE;
		}
	}

	// Next, get null flags

	for (field = relation->rel_fields; field; field = field->fld_next)
	{
		if (field->fld_flags & FLD_computed)
			continue;
		UCHAR* p = data + field->fld_missing_offset;
		if (!useMissingOffset)
		{
			offset = FB_ALIGN(offset, sizeof(SSHORT));
			p = data + offset;
			offset += sizeof(SSHORT);
		}
		if (!xdr_short(xdrs, (SSHORT*) p))
			return FALSE;
	}
	return (xdrs->x_private - xdrs->x_base);
}


ULONG CAN_slice(lstring* buffer, lstring* slice, bool direction, UCHAR* sdl)
{
/**************************************
 *
 *	C A N _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	encode and decode canonical backup.
 *
 **************************************/
	BurpXdr xdr;
	BurpXdr* xdrs = &xdr;

	xdr_init(xdrs, buffer, direction ? XDR_ENCODE : XDR_DECODE);

	xdr_slice(xdrs, slice, /*sdl_length,*/ sdl);
	return (xdrs->x_private - xdrs->x_base);
}


bool_t BurpXdr::x_getbytes(SCHAR* buff, unsigned bytecount)
{
/**************************************
 *
 *	b u r p _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Fetch a bunch of bytes into a memory stream if it fits.
 *
 **************************************/

	if (bytecount && x_handy >= bytecount)
	{
		memcpy(buff, x_private, bytecount);
		x_private += bytecount;
		x_handy -= bytecount;

		return TRUE;
	}

	while (bytecount--)
	{
		if (x_handy == 0 && !expand_buffer(this))
			return FALSE;

		*buff++ = *x_private++;
		--x_handy;
	}

	return TRUE;
}


bool_t BurpXdr::x_putbytes(const SCHAR* buff, unsigned bytecount)
{
/**************************************
 *
 *	b u r p _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Fetch a bunch of bytes into a memory stream if it fits.
 *
 **************************************/

	if (bytecount && x_handy >= bytecount)
	{
		memcpy(x_private, buff, bytecount);
		x_private += bytecount;
		x_handy -= bytecount;

		return TRUE;
	}

	while (bytecount--)
	{
		if (x_handy == 0 && !expand_buffer(this))
			return FALSE;

		*x_private++ = *buff++;
		--x_handy;
	}

	return TRUE;
}


static bool_t expand_buffer(BurpXdr* xdrs)
{
/**************************************
 *
 *	e x p a n d _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Allocate a new, larger buffer, copy
 *	everything we've got, and release the
 *	old one.
 *
 **************************************/
	lstring* buffer = xdrs->x_public;
	const unsigned usedLength = xdrs->x_private - xdrs->x_base;
	const unsigned length = usedLength + xdrs->x_handy + increment;

	caddr_t new_buf = (caddr_t) BURP_alloc(length);

	buffer->lstr_allocated = buffer->lstr_length = length;
	buffer->lstr_address = (UCHAR *) new_buf;
	memcpy(new_buf, xdrs->x_base, usedLength);

	BURP_free(xdrs->x_base);

	xdrs->x_private = new_buf + usedLength;
	xdrs->x_base = new_buf;
	xdrs->x_handy += increment;

	return TRUE;
}


static int xdr_init(BurpXdr* xdrs, lstring* buffer, enum xdr_op x_op)
{
/**************************************
 *
 *	x d r _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize an XDR stream for Apollo mailboxes.
 *
 **************************************/

	xdrs->x_public = buffer;
	xdrs->create((caddr_t) buffer->lstr_address, buffer->lstr_length, x_op);

	return TRUE;
}


static bool_t xdr_slice(BurpXdr* xdrs, lstring* slice, /*USHORT sdl_length,*/ const UCHAR* sdl)
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
			BURP_free(slice->lstr_address);
			slice->lstr_address = NULL;
		}
		if (!slice->lstr_address)
		{
			slice->lstr_address = BURP_alloc((SLONG) slice->lstr_length);
			if (!slice->lstr_address) {
				return FALSE;
			}
			slice->lstr_allocated = slice->lstr_length;
		}
		break;

	case XDR_FREE:
		if (slice->lstr_allocated)
			BURP_free(slice->lstr_address);
		slice->lstr_address = NULL;
		slice->lstr_allocated = 0;
		return TRUE;

	default:
		fb_assert(FALSE);
		return FALSE;
	}

	// Get descriptor of array element

	sdl_info info;
	{
		FbLocalStatus s;
		if (SDL_info(&s, sdl, &info, 0))
			return FALSE;
	}

	dsc* desc = &info.sdl_info_element;
	const ULONG n = slice->lstr_length / desc->dsc_length;
	UCHAR* p = slice->lstr_address;

	for (UCHAR* const end = p + n * desc->dsc_length; p < end; p += desc->dsc_length)
	{
		if (!xdr_datum(xdrs, desc, p)) {
			return FALSE;
		}
	}

	return TRUE;
}
