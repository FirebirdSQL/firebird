/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		mov.cpp
 *	DESCRIPTION:	Data mover and converter and comparator, etc.
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
 * 2002.08.21 Dmitry Yemanov: fixed bug with a buffer overrun,
 *                            which at least caused invalid dependencies
 *                            to be stored (DB$xxx, for example)
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../common/classes/VaryStr.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/intl.h"
#include "../jrd/blb_proto.h"
#include "../jrd/blb.h"
#include "../jrd/cvt_proto.h"
#include "../common/cvt.h"
#include "../jrd/cvt2_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/mov_proto.h"

using namespace Firebird;

int MOV_compare(Jrd::thread_db* tdbb, const dsc* arg1, const dsc* arg2)
{
/**************************************
 *
 *	M O V _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/
	fb_assert(!(arg1->dsc_flags & DSC_null));
	fb_assert(!(arg2->dsc_flags & DSC_null));

	return CVT2_compare(arg1, arg2, tdbb->getAttachment()->att_dec_status);
}


double MOV_date_to_double(const dsc* desc)
{
/**************************************
 *
 *	M O V _ d a t e _ t o _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *    Convert a date to double precision for
 *    date arithmetic routines.
 *
 **************************************/

	return CVT_date_to_double(desc);
}

void MOV_double_to_date(double real, SLONG fixed[2])
{
/**************************************
 *
 *	M O V _ d o u b l e _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert a double precision representation of a date
 *	to a fixed point representation.   Double is used for
 *      date arithmetic.
 *
 **************************************/

	CVT_double_to_date(real, fixed);
}


// Get the value of a boolean descriptor.
bool MOV_get_boolean(const dsc* desc)
{
	return CVT_get_boolean(desc, ERR_post);
}


double MOV_get_double(Jrd::thread_db* tdbb, const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a double precision number
 *
 **************************************/

	return CVT_get_double(desc, tdbb->getAttachment()->att_dec_status, ERR_post);
}


SLONG MOV_get_long(Jrd::thread_db* tdbb, const dsc* desc, SSHORT scale)
{
/**************************************
 *
 *	M O V _ g e t _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a long (32 bit) integer of given
 *	scale.
 *
 **************************************/

	return CVT_get_long(desc, scale, tdbb->getAttachment()->att_dec_status, ERR_post);
}


SINT64 MOV_get_int64(Jrd::thread_db* tdbb, const dsc* desc, SSHORT scale)
{
/**************************************
 *
 *	M O V _ g e t _ i n t 6 4
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a 64 bit integer of given
 *	scale.
 *
 **************************************/

	return CVT_get_int64(desc, scale, tdbb->getAttachment()->att_dec_status, ERR_post);
}


void MOV_get_metaname(Jrd::thread_db* tdbb, const dsc* desc, Jrd::MetaName& name)
{
/**************************************
 *
 *	M O V _ g e t _ m e t a n a m e
 *
 **************************************
 *
 * Functional description
 *	Copy string, which will afterward's
 *	be treated as a metadata name value,
 *	to the user-supplied object.
 *
 **************************************/

	VaryStr<MAX_SQL_IDENTIFIER_SIZE> temp;
	const char* str = NULL;
	const auto len = MOV_make_string(tdbb, desc, ttype_metadata, &str,
									 &temp, MAX_SQL_IDENTIFIER_SIZE);
	name.assign(str, len);
}


SQUAD MOV_get_quad(Jrd::thread_db* tdbb, const dsc* desc, SSHORT scale)
{
/**************************************
 *
 *	M O V _ g e t _ q u a d
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a quad
 *	Note: a quad is NOT the same as a 64 bit integer
 *
 **************************************/

	return CVT_get_quad(desc, scale, tdbb->getAttachment()->att_dec_status, ERR_post);
}


int MOV_get_string_ptr(Jrd::thread_db* tdbb,
					   const dsc* desc,
					   USHORT* ttype,
					   UCHAR** address, vary* temp, USHORT length)
{
/**************************************
 *
 *	M O V _ g e t _ s t r i n g _ p t r
 *
 **************************************
 *
 * Functional description
 *	Get address and length of string, converting the value to
 *	string, if necessary.  The caller must provide a sufficiently
 *	large temporary.  The address of the resultant string is returned
 *	by reference.  Get_string returns the length of the string.
 *
 *	Note: If the descriptor is known to be a string type, the third
 *	argument (temp buffer) may be omitted.
 *
 **************************************/

	return CVT_get_string_ptr(desc, ttype, address, temp, length, tdbb->getAttachment()->att_dec_status);
}


int MOV_get_string(Jrd::thread_db* tdbb, const dsc* desc, UCHAR** address, vary* temp, USHORT length)
{
/**************************************
 *
 *	M O V _ g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	USHORT ttype;

	return MOV_get_string_ptr(tdbb, desc, &ttype, address, temp, length);
}


GDS_DATE MOV_get_sql_date(const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ s q l _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a SQL date
 *
 **************************************/

	return CVT_get_sql_date(desc);
}


GDS_TIME MOV_get_sql_time(const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ s q l _ t i m e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a SQL time
 *
 **************************************/

	return CVT_get_sql_time(desc);
}


ISC_TIME_TZ MOV_get_sql_time_tz(const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ s q l _ t i m e _ t z
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a SQL time with time zone
 *
 **************************************/

	return CVT_get_sql_time_tz(desc);
}


GDS_TIMESTAMP MOV_get_timestamp(const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ t i m e s t a m p
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a timestamp
 *
 **************************************/

	return CVT_get_timestamp(desc);
}


ISC_TIMESTAMP_TZ MOV_get_timestamp_tz(const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ t i m e s t a m p _ t z
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a timestamp with time zone
 *
 **************************************/

	return CVT_get_timestamp_tz(desc);
}


int MOV_make_string(Jrd::thread_db* tdbb,
					const dsc*	 desc,
					USHORT	     ttype,
					const char** address,
					vary*	     temp,
					USHORT	     length)
{
/**************************************
 *
 *	M O V _ m a k e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Make a string, in a specified text type, out of a descriptor.
 *	The caller must provide a sufficiently
 *	large temporary.  The address of the resultant string is returned
 *	by reference.
 *	MOV_make_string returns the length of the string in bytes.
 *
 *	Note: If the descriptor is known to be a string type in the
 *	given ttype the argument (temp buffer) may be omitted.
 *	But this would be a bad idea in general.
 *
 **************************************/

	return CVT_make_string(desc, ttype, address, temp, length, tdbb->getAttachment()->att_dec_status, ERR_post);
}


int MOV_make_string2(Jrd::thread_db* tdbb,
					 const dsc* desc,
					 USHORT ttype,
					 UCHAR** address,
					 Jrd::MoveBuffer& buffer,
					 bool limit)
{
/**************************************
 *
 *	M O V _ m a k e _ s t r i n g 2
 *
 **************************************
 *
 * Functional description
 *	Make a string, in a specified text type, out of a descriptor.
 *	The address of the resultant string is returned by reference.
 *	MOV_make_string2 returns the length of the string in bytes.
 *
 **************************************/

	if (desc->isBlob())
	{
		// fake descriptor
		dsc temp;
		temp.dsc_dtype = dtype_text;
		temp.setTextType(ttype);

		Firebird::UCharBuffer bpb;
		BLB_gen_bpb_from_descs(desc, &temp, bpb);

		Jrd::blb* blob = Jrd::blb::open2(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<Jrd::bid*>(desc->dsc_address), bpb.getCount(), bpb.begin());

		ULONG size;

		if (temp.getCharSet() == desc->getCharSet())
		{
			size = blob->blb_length;
		}
		else
		{
			size = (blob->blb_length / INTL_charset_lookup(tdbb, desc->getCharSet())->minBytesPerChar()) *
				INTL_charset_lookup(tdbb, temp.getCharSet())->maxBytesPerChar();
		}

		*address = buffer.getBuffer(size);

		size = blob->BLB_get_data(tdbb, *address, size, true);

		if (limit && size > MAX_COLUMN_SIZE)
		{
			ERR_post(Arg::Gds(isc_arith_except) <<
					 Arg::Gds(isc_blob_truncation));
		}

		return size;
	}

	return CVT2_make_string2(desc, ttype, address, buffer, tdbb->getAttachment()->att_dec_status);
}


Firebird::string MOV_make_string2(Jrd::thread_db* tdbb, const dsc* desc, USHORT ttype, bool limit)
{
	Jrd::MoveBuffer buffer;
	UCHAR* ptr;
	int len = MOV_make_string2(tdbb, desc, ttype, &ptr, buffer, limit);

	return string((const char*) ptr, len);
}


void MOV_move(Jrd::thread_db* tdbb, /*const*/ dsc* from, dsc* to)
{
/**************************************
 *
 *	M O V _ m o v e
 *
 **************************************
 *
 * Functional description
 *	Move (and possible convert) something to something else.
 *
 **************************************/

	if (DTYPE_IS_BLOB_OR_QUAD(from->dsc_dtype) || DTYPE_IS_BLOB_OR_QUAD(to->dsc_dtype))
		Jrd::blb::move(tdbb, from, to);
	else
		CVT_move(from, to, tdbb->getAttachment()->att_dec_status);
}


Decimal64 MOV_get_dec64(Jrd::thread_db* tdbb, const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ d e c 6 4
 *
 **************************************/

	return CVT_get_dec64(desc, tdbb->getAttachment()->att_dec_status, ERR_post);
}


Decimal128 MOV_get_dec128(Jrd::thread_db* tdbb, const dsc* desc)
{
/**************************************
 *
 *	M O V _ g e t _ d e c 1 2 8
 *
 **************************************/

	return CVT_get_dec128(desc, tdbb->getAttachment()->att_dec_status, ERR_post);
}


Int128 MOV_get_int128(Jrd::thread_db* tdbb, const dsc* desc, SSHORT scale)
{
/**************************************
 *
 *	M O V _ g e t _ d e c _ f i x e d
 *
 **************************************/

	return CVT_get_int128(desc, scale, tdbb->getAttachment()->att_dec_status, ERR_post);
}


namespace Jrd
{

DescPrinter::DescPrinter(thread_db* tdbb, const dsc* desc, FB_SIZE_T mLen)
	: maxLen(mLen)
{
	const char* const NULL_KEY_STRING = "NULL";

	if (!desc)
	{
		value = NULL_KEY_STRING;
		return;
	}

	fb_assert(!desc->isBlob());

	const bool isBinary = (desc->isText() && desc->getTextType() == ttype_binary);
	value = MOV_make_string2(tdbb, desc, isBinary ? ttype_binary : ttype_dynamic);

	const char* const str = value.c_str();

	if (desc->isText() || desc->isDateTime())
	{
		if (desc->dsc_dtype == dtype_text)
		{
			const char* const pad = (desc->dsc_sub_type == ttype_binary) ? "\0" : " ";
			value.rtrim(pad);
		}

		if (isBinary)
		{
			string hex;

			FB_SIZE_T len = value.length();
			const bool cut = (len > (maxLen - 3) / 2);	// 3 is a length of enclosing symbols: x' and '
			if (cut)
				len = (maxLen - 5) / 2;					// 5 is a length of enclosing symbols: x' and ...

			char* s = hex.getBuffer(2 * len);			// each raw byte represented by 2 chars in string

			for (FB_SIZE_T i = 0; i < len; i++)
			{
				sprintf(s, "%02X", (int)(unsigned char) str[i]);
				s += 2;
			}
			value = "x'" + hex + (cut ? "..." : "'");
		}
		else
			value = "'" + value + "'";
	}

	if (value.length() > maxLen)
	{
		fb_assert(desc->isText());

		value.resize(maxLen);

		const CharSet* const cs = INTL_charset_lookup(tdbb, desc->getCharSet());

		while (value.hasData() && !cs->wellFormed(value.length(), (const UCHAR*) value.c_str()))
			value.resize(value.length() - 1);

		value += "...";
	}
}

}	// namespace Jrd
