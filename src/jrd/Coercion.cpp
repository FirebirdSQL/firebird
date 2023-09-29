/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Coercion.cpp
 *      DESCRIPTION:    Automatically coercing user datatypes
 *
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2019 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../jrd/Coercion.h"
#include "../jrd/cvt_proto.h"

#include "../dsql/dsql.h"
#include "../dsql/make_proto.h"
#include "../jrd/align.h"
#include "../jrd/DataTypeUtil.h"

using namespace Jrd;
using namespace Firebird;

static const USHORT FROM_MASK = FLD_has_len | FLD_has_chset | FLD_has_scale |
	FLD_has_sub | FLD_has_prec;
static const USHORT TO_MASK = FLD_has_len | FLD_has_chset | FLD_has_scale |
	FLD_legacy | FLD_native | FLD_has_sub | FLD_has_prec | FLD_extended;

bool CoercionArray::coerce(thread_db* tdbb, dsc* d, unsigned startItem) const
{
	// move down through array to ensure correct order: newer rule overrides older one
	for (unsigned n = getCount(); n-- > startItem; )
	{
		if (getElement(n).coerce(tdbb, d))
			return true;
	}

	return false;
}

void CoercionArray::setRule(const TypeClause* from, const TypeClause *to)
{
	CoercionRule newRule;
	newRule.setRule(from, to);

	for (unsigned n = 0; n < getCount(); ++n)
	{
		if (getElement(n) == newRule)
		{
			remove(n);
			break;
		}
	}

	add(newRule);
}

void CoercionRule::raiseError()
{
	// Do not use ERR_post here - old error should be overwritten
	(Arg::Gds(isc_bind_convert) << fromDsc.typeToText() << toDsc.typeToText()).raise();
}

void CoercionRule::setRule(const TypeClause* from, const TypeClause *to)
{
	fromMask = from->flags & FROM_MASK;
	DsqlDescMaker::fromField(&fromDsc, from);

	toMask = to->flags & TO_MASK;
	DsqlDescMaker::fromField(&toDsc, to);

	// Check for datatype compatibility

	// No checks for special case
	if (toMask & (FLD_native | FLD_legacy))
		return;

	// Extending timezone info
	if (toMask & FLD_extended)
	{
		if (fromDsc.isDateTimeTz())
			return;
		raiseError();
	}

	// Exceptions - enable blob2blob & blob2string casts
	if ((toDsc.dsc_dtype == dtype_blob && fromDsc.isText()) ||
		(fromDsc.dsc_dtype == dtype_blob && toDsc.isText()) ||
		(toDsc.isBlob() && fromDsc.isBlob()))
	{
		return;
	}

	// Disable the rest of casts with blobs
	if (toDsc.isBlob() || fromDsc.isBlob())
		raiseError();

	// Generic check
	class BufSize
	{
	public:
		static constexpr unsigned makeSize(unsigned len)
		{
			return (len + FB_ALIGNMENT) * 2;
		}
	};

	const unsigned DATASIZE = 256;
	HalfStaticArray<UCHAR, BufSize::makeSize(DATASIZE)> buffer;

	unsigned datasize = MAX(toMask & FLD_has_len ? toDsc.dsc_length : 0,
		fromMask & FLD_has_len ? fromDsc.dsc_length : 0);
	datasize = MAX(DATASIZE, datasize);
	UCHAR* buf = buffer.getBuffer(BufSize::makeSize(datasize));
	memset(buf, 0, buffer.getCount());

	toDsc.dsc_address = FB_ALIGN(buf, FB_ALIGNMENT);
	if (! (toMask & FLD_has_len))
	{
		toDsc.dsc_length = datasize - 2;
	}
	fromDsc.dsc_address = FB_ALIGN(toDsc.dsc_address + datasize, FB_ALIGNMENT);

	try
	{
		CVT_move(&fromDsc, &toDsc, 0);
	}
	catch(const Exception&)
	{
		raiseError();
	}
}

dsc* CoercionRule::makeLegacy(USHORT mask)
{
	toMask = FLD_legacy;
	fromMask = mask;
	return &fromDsc;
}

bool CoercionRule::operator==(const CoercionRule& rule) const
{
	if (fromMask != rule.fromMask)
		return false;
	return match(&rule.fromDsc);
}

bool CoercionRule::match(const dsc* d) const
{
	// check for exact match (taking flags into an account)
	if ((d->dsc_dtype == fromDsc.dsc_dtype) &&
		((d->dsc_length == fromDsc.dsc_length) || (!(fromMask & FLD_has_len))) &&
		((d->getCharSet() == fromDsc.getCharSet()) || (!(fromMask & FLD_has_chset))) &&
		((d->getSubType() == fromDsc.getSubType()) || (!(fromMask & FLD_has_sub))) &&
		((d->dsc_scale == fromDsc.dsc_scale) || (!(fromMask & FLD_has_scale))))
	{
		return true;
	}

	// check for inexact datatype match when FLD_has_prec is not set
	if (!(fromMask & FLD_has_prec))
	{
		switch(fromDsc.dsc_dtype)
		{
		case dtype_dec64:
		case dtype_dec128:
			if (DTYPE_IS_DECFLOAT(d->dsc_dtype))
				return true;
			break;

		case dtype_short:
		case dtype_long:
		case dtype_int64:
		case dtype_int128:
			if (d->isExact() && (fromMask & FLD_has_sub) && (d->dsc_sub_type == fromDsc.dsc_sub_type))
				return true;
			break;

		case dtype_timestamp_tz:
		case dtype_sql_time_tz:
			if (d->isDateTimeTz())
				return true;
			break;
		}
	}

	return false;
}

static const USHORT COMPATIBLE_TEXT = 1;
static const USHORT COMPATIBLE_INT = 2;

static const USHORT subTypeCompatibility[DTYPE_TYPE_MAX] =
{
	0,							// dtype_unknown
	COMPATIBLE_TEXT,			// dtype_text
	0,							// dtype_cstring
	COMPATIBLE_TEXT,			// dtype_varying
	0,
	0,
	0,							// dtype_packed
	0,							// dtype_byte
	COMPATIBLE_INT,				// dtype_short      -32768
	COMPATIBLE_INT,				// dtype_long       -2147483648
	0,							// dtype_quad       -9223372036854775808
	0,							// dtype_real       -1.23456789e+12
	0,							// dtype_double     -1.2345678901234567e+123
	0,							// dtype_d_float (believed to have this range)  -1.2345678901234567e+123
	0,							// dtype_sql_date   YYYY-MM-DD
	0,							// dtype_sql_time   HH:MM:SS.MMMM
	0,							// dtype_timestamp  YYYY-MM-DD HH:MM:SS.MMMM
	0,							// dtype_blob       FFFF:FFFF
	0,							// dtype_array      FFFF:FFFF
	COMPATIBLE_INT,				// dtype_int64      -9223372036854775808
	0,							// dtype_dbkey
	0,							// dtype_boolean
	0,							// dtype_dec64		1 + 1 + 1 + 1 + 16(34) + 3(4)
	0,							// dtype_dec128		+-  .   e   +-  coeff  + exp
	COMPATIBLE_INT,				// dtype_int128
	0,							// dtype_sql_time_tz      HH:MM:SS.MMMM +NN:NN
	0,							// dtype_timestamp_tz     YYYY-MM-DD HH:MM:SS.MMMM +NN:NN
	0,							// dtype_ex_time_tz       HH:MM:SS.MMMM +NN:NN
	0,							// dtype_ex_timestamp_tz  YYYY-MM-DD HH:MM:SS.MMMM +NN:NN
};


bool CoercionRule::coerce(thread_db* tdbb, dsc* d) const
{
	// check does descriptor match FROM clause
	if (! match(d))
		return false;

	// native binding - do not touch descriptor at all
	if (toMask & FLD_native)
		return true;

	// process legacy case
	if (toMask & FLD_legacy)
	{
		bool found = true;

		switch(d->dsc_dtype)
		{
		case dtype_dec64:
		case dtype_dec128:
			d->dsc_dtype = dtype_double;
			d->dsc_length = sizeof(double);
			break;
		case dtype_sql_time_tz:
			d->dsc_dtype = dtype_sql_time;
			d->dsc_length = sizeof(ISC_TIME);
			break;
		case dtype_timestamp_tz:
			d->dsc_dtype = dtype_timestamp;
			d->dsc_length = sizeof(ISC_TIMESTAMP);
			break;
		case dtype_int128:
			d->dsc_dtype = dtype_int64;
			d->dsc_length = sizeof(SINT64);
			break;
		case dtype_boolean:
			d->dsc_dtype = dtype_text;
			d->dsc_length = 5;
			break;
		default:
			found = false;
			break;
		}

		return found;
	}

	// extending time zone
	if (toMask & FLD_extended)
	{
		bool found = true;

		switch(d->dsc_dtype)
		{
		case dtype_timestamp_tz:
			d->dsc_dtype = dtype_ex_timestamp_tz;
			d->dsc_length = sizeof(ISC_TIMESTAMP_TZ_EX);
			break;
		case dtype_sql_time_tz:
			d->dsc_dtype = dtype_ex_time_tz;
			d->dsc_length = sizeof(ISC_TIME_TZ_EX);
			break;
		default:
			found = false;
			break;
		}

		return found;
	}

	// final pass - order is important

	const auto srcCharSet = d->getCharSet();

	// scale
	if (toMask & FLD_has_scale)
		d->dsc_scale = toDsc.dsc_scale;
	else if (!(DTYPE_IS_EXACT(d->dsc_dtype) && DTYPE_IS_EXACT(toDsc.dsc_dtype)))
		d->dsc_scale = 0;

	// subtype
	if (toMask & FLD_has_sub ||
		d->dsc_dtype >= DTYPE_TYPE_MAX || toDsc.dsc_dtype >= DTYPE_TYPE_MAX ||
		subTypeCompatibility[d->dsc_dtype] == 0 ||
		subTypeCompatibility[d->dsc_dtype] != subTypeCompatibility[toDsc.dsc_dtype])
	{
		d->dsc_sub_type = toDsc.dsc_sub_type;
	}

	// length
	if (toMask & FLD_has_len)
		d->dsc_length = toDsc.dsc_length;

	// type
	if (toMask & FLD_has_prec ||
		subTypeCompatibility[d->dsc_dtype] != COMPATIBLE_INT ||
		subTypeCompatibility[toDsc.dsc_dtype] != COMPATIBLE_INT)
	{
		if (!(toMask & FLD_has_len))
		{
			if (!type_lengths[toDsc.dsc_dtype])
			{
				fb_assert(toDsc.isText());
				d->dsc_length = d->getStringLength();
			}
			else
				d->dsc_length = type_lengths[toDsc.dsc_dtype];
		}

		d->dsc_dtype = toDsc.dsc_dtype;
	}

	// charset
	if (toMask & FLD_has_chset)
		d->setTextType(toDsc.getTextType());

	if (d->isText())
		d->dsc_length = DataTypeUtil(tdbb).convertLength(d->dsc_length, srcCharSet, toDsc.getCharSet());

	// varchar length
	if (d->dsc_dtype == dtype_varying && !(toMask & FLD_has_len))
		d->dsc_length += sizeof(USHORT);

	// subtype - special processing for BLOBs
	if (toMask & FLD_has_sub)
		d->setBlobSubType(toDsc.getBlobSubType());

	return true;
}

