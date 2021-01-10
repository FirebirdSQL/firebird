/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		merge.cpp
 *	DESCRIPTION:	Merge database/server information
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

#include "firebird.h"
#include <string.h>
#include "ibase.h"
#include "../remote/remote.h"
#include "../remote/merge_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/classes/DbImplementation.h"

inline void PUT_WORD(UCHAR*& ptr, USHORT value)
{
	*ptr++ = static_cast<UCHAR>(value);
	*ptr++ = static_cast<UCHAR>(value >> 8);
}

#define PUT(ptr, value)		*(ptr)++ = value;

static ISC_STATUS merge_setup(const Firebird::ClumpletReader&, UCHAR**, const UCHAR* const, FB_SIZE_T);


USHORT MERGE_database_info(const UCHAR* const in,
							UCHAR* out,
							USHORT buf_length,
							USHORT impl,
							USHORT class_,
							USHORT base_level,
							const UCHAR* version,
							const UCHAR* id)
{
/**************************************
 *
 *	M E R G E _ d a t a b a s e _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Merge server / remote interface / Y-valve information into
 *	database block.  Return the actual length of the packet.
 *	See also jrd/utl.cpp for decoding of this block.
 *
 **************************************/
	SSHORT l;
	const UCHAR* p;

	UCHAR* start = out;
	const UCHAR* const end = out + buf_length;

	UCHAR mergeLevel = 0;
	Firebird::ClumpletReader input(Firebird::ClumpletReader::InfoResponse, in, buf_length);
	while (!input.isEof())
	{
		bool flStop = true;
		switch(input.getClumpTag())
		{
		case isc_info_implementation:
			mergeLevel = input.getBytes()[0];
			break;

		case isc_info_end:
		case isc_info_truncated:
			break;

		default:
			flStop = false;
			break;
		}

		if (flStop)
			break;
		input.moveNext();
	}

	for (input.rewind(); !input.isEof(); input.moveNext())
	{
		*out++ = input.getClumpTag();
		switch (input.getClumpTag())
		{
		case isc_info_end:
		case isc_info_truncated:
			return out - start;

		case isc_info_firebird_version:
			l = static_cast<SSHORT>(strlen((char *) (p = version)));
			if (l > MAX_UCHAR)
			    l = MAX_UCHAR;
			if (merge_setup(input, &out, end, l + 1))
				return 0;
			for (*out++ = (UCHAR) l; l; --l)
				*out++ = *p++;
			break;

		case isc_info_db_id:
			l = static_cast<SSHORT>(strlen((SCHAR *) (p = id)));
			if (l > MAX_UCHAR)
				l = MAX_UCHAR;
			if (merge_setup(input, &out, end, l + 1))
				return 0;
			for (*out++ = (UCHAR) l; l; --l)
				*out++ = *p++;
			break;

		case isc_info_implementation:
			if (merge_setup(input, &out, end, 2))
				return 0;
			PUT(out, (UCHAR) impl);
			PUT(out, (UCHAR) class_);
			break;

		case fb_info_implementation:
			if (merge_setup(input, &out, end, 6))
				return 0;
			Firebird::DbImplementation::current.stuff(&out);
			PUT(out, (UCHAR) class_);
			PUT(out, mergeLevel);
			break;

		case isc_info_base_level:
			if (merge_setup(input, &out, end, 1))
				return 0;
			PUT(out, (UCHAR) base_level);
			break;

		default:
			{
				USHORT length = input.getClumpLength();
				if (out + length + 2 >= end)
				{
					out[-1] = isc_info_truncated;
					return 0;
				}
				PUT_WORD(out, length);
				memcpy(out, input.getBytes(), length);
				out += length;
			}
			break;
		}
	}

	return 0;	// error - missing isc_info_end item
}

static ISC_STATUS merge_setup(const Firebird::ClumpletReader& input, UCHAR** out, const UCHAR* const end,
							  FB_SIZE_T delta_length)
{
/**************************************
 *
 *	m e r g e _ s e t u p
 *
 **************************************
 *
 * Functional description
 *	Get ready to toss new stuff onto an info packet.  This involves
 *	picking up and bumping the "count" field and copying what is
 *	already there.
 *
 **************************************/
	FB_SIZE_T length = input.getClumpLength();
	const FB_SIZE_T new_length = length + delta_length;

	if (new_length > MAX_USHORT || *out + new_length + 2 >= end)
	{
		(*out)[-1] = isc_info_truncated;
		return FB_FAILURE;
	}

	const USHORT count = 1 + *(input.getBytes());
	PUT_WORD(*out, new_length);
	PUT(*out, (UCHAR) count);

	// Copy data portion of information sans original count

	if (--length)
	{
		memcpy(*out, input.getBytes() + 1, length);
		*out += length;
	}

	return FB_SUCCESS;
}
