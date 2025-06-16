/*
 *	PROGRAM:	JRD access method
 *	MODULE:		val.h
 *	DESCRIPTION:	Definitions associated with value handling
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
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef JRD_VAL_H
#define JRD_VAL_H

#include "../include/fb_blk.h"
#include "../common/classes/array.h"
#include "../jrd/intl_classes.h"
#include "../jrd/MetaName.h"
#include "../jrd/RecordNumber.h"
#include "../common/dsc.h"

#define FLAG_BYTES(n)	(((n + BITS_PER_LONG) & ~((ULONG)BITS_PER_LONG - 1)) >> 3)

// Random string block -- as long as impure areas don't have
// constructors and destructors, the need this varying string

class VaryingString : public pool_alloc_rpt<SCHAR, type_str>
{
public:
	USHORT str_length;
	UCHAR str_data[2];			// one byte for ALLOC and one for the NULL
};

const ULONG MAX_RECORD_SIZE	= 65535;

namespace Jrd {

class ArrayField;
class blb;
class Request;
class jrd_req;
class jrd_tra;
class thread_db;
class ValueExprNode;
class ValueListNode;

struct SortValueItem
{
	SortValueItem(const ValueExprNode* val, const dsc* d)
		: value(val), desc(d)
	{}

	static int compare(const dsc* desc1, const dsc* desc2);

	bool operator==(const SortValueItem& other) const
	{
		return (compare(desc, other.desc) == 0);
	}

	bool operator!=(const SortValueItem& other) const
	{
		return (compare(desc, other.desc) != 0);
	}

	bool operator>(const SortValueItem& other) const
	{
		return (compare(desc, other.desc) > 0);
	}

	const ValueExprNode* value;
	const dsc* desc;
};

typedef Firebird::SortedArray<SortValueItem> SortedValueList;

class LookupValueList
{
public:
	LookupValueList(MemoryPool& pool, ValueListNode* values, ULONG impure);

	ULONG getCount() const { return m_values.getCount(); }
	ValueExprNode** begin() { return m_values.begin(); }
	ValueExprNode** end() { return m_values.end(); }

	const SortedValueList* init(thread_db* tdbb, Request* request) const;

	bool find(thread_db* tdbb, Request* request,
			  const ValueExprNode* value, const dsc* desc) const;

private:
	Firebird::HalfStaticArray<ValueExprNode*, 4> m_values;
	const ULONG m_impureOffset;
};

// Various structures in the impure area

struct impure_state
{
	ULONG sta_state;
};

struct impure_value
{
	struct PatternMatcherCache : pool_alloc_rpt<UCHAR>
	{
		PatternMatcherCache(ULONG aKeySize)
			: keySize(aKeySize)
		{
		}

		ULONG keySize;
		USHORT ttype;
		USHORT patternLen;
		Firebird::AutoPtr<Jrd::PatternMatcher> matcher;
		USHORT escapeLen;
		UCHAR key[1];
	};

	dsc vlu_desc;
	USHORT vlu_flags; // Computed/invariant flags
	VaryingString* vlu_string;

	union
	{
		UCHAR vlu_uchar;
		SSHORT vlu_short;
		SLONG vlu_long;
		SINT64 vlu_int64;
		SQUAD vlu_quad;
		SLONG vlu_dbkey[2];
		float vlu_float;
		double vlu_double;
		Firebird::Decimal64 vlu_dec64;
		Firebird::Decimal128 vlu_dec128;
		Firebird::Int128 vlu_int128;
		GDS_TIMESTAMP vlu_timestamp;
		ISC_TIMESTAMP_TZ vlu_timestamp_tz;
		GDS_TIME vlu_sql_time;
		ISC_TIME_TZ vlu_sql_time_tz;
		GDS_DATE vlu_sql_date;
		bid vlu_bid;

		// Pre-compiled invariant object for pattern matcher functions
		Jrd::PatternMatcher* vlu_invariant;
		PatternMatcherCache* vlu_patternMatcherCache;
		SortedValueList* vlu_sortedList;
	} vlu_misc;

	void make_long(const SLONG val, const signed char scale = 0);
	void make_int64(const SINT64 val, const signed char scale = 0);
	void make_double(const double val);
	void make_decimal128(const Firebird::Decimal128 val);
	void make_decimal_fixed(const Firebird::Int128 val, const signed char scale);
};

// Do not use these methods where dsc_sub_type is not explicitly set to zero.
inline void impure_value::make_long(const SLONG val, const signed char scale)
{
	this->vlu_misc.vlu_long = val;
	this->vlu_desc.dsc_dtype = dtype_long;
	this->vlu_desc.dsc_length = sizeof(SLONG);
	this->vlu_desc.dsc_scale = scale;
	this->vlu_desc.dsc_sub_type = 0;
	this->vlu_desc.dsc_address = reinterpret_cast<UCHAR*>(&this->vlu_misc.vlu_long);
}

inline void impure_value::make_int64(const SINT64 val, const signed char scale)
{
	this->vlu_misc.vlu_int64 = val;
	this->vlu_desc.dsc_dtype = dtype_int64;
	this->vlu_desc.dsc_length = sizeof(SINT64);
	this->vlu_desc.dsc_scale = scale;
	this->vlu_desc.dsc_sub_type = 0;
	this->vlu_desc.dsc_address = reinterpret_cast<UCHAR*>(&this->vlu_misc.vlu_int64);
}

inline void impure_value::make_double(const double val)
{
	this->vlu_misc.vlu_double = val;
	this->vlu_desc.dsc_dtype = DEFAULT_DOUBLE;
	this->vlu_desc.dsc_length = sizeof(double);
	this->vlu_desc.dsc_scale = 0;
	this->vlu_desc.dsc_sub_type = 0;
	this->vlu_desc.dsc_address = reinterpret_cast<UCHAR*>(&this->vlu_misc.vlu_double);
}

inline void impure_value::make_decimal128(const Firebird::Decimal128 val)
{
	this->vlu_misc.vlu_dec128 = val;
	this->vlu_desc.dsc_dtype = dtype_dec128;
	this->vlu_desc.dsc_length = sizeof(Firebird::Decimal128);
	this->vlu_desc.dsc_scale = 0;
	this->vlu_desc.dsc_sub_type = 0;
	this->vlu_desc.dsc_address = reinterpret_cast<UCHAR*>(&this->vlu_misc.vlu_dec128);
}

inline void impure_value::make_decimal_fixed(const Firebird::Int128 val, const signed char scale)
{
	this->vlu_misc.vlu_int128 = val;
	this->vlu_desc.dsc_dtype = dtype_int128;
	this->vlu_desc.dsc_length = sizeof(Firebird::Int128);
	this->vlu_desc.dsc_scale = scale;
	this->vlu_desc.dsc_sub_type = 0;
	this->vlu_desc.dsc_address = reinterpret_cast<UCHAR*>(&this->vlu_misc.vlu_int128);
}

struct impure_value_ex : public impure_value
{
	SINT64 vlux_count;
	blb* vlu_blob;
};

const int VLU_computed		= 1;	// An invariant sub-query has been computed
const int VLU_null			= 2;	// An invariant sub-query computed to null
const int VLU_checked		= 4;	// Constraint already checked in first read or assignment to argument/variable
const int VLU_initialized	= 8;	// Variable initialized


class Format : public pool_alloc<type_fmt>
{
public:
	Format(MemoryPool& p, int len)
		: fmt_length(0), fmt_count(len), fmt_version(0),
		  fmt_desc(p, fmt_count), fmt_defaults(p, fmt_count)
	{
		fmt_desc.resize(fmt_count);
		fmt_defaults.resize(fmt_count);

		for (fmt_defaults_iterator impure = fmt_defaults.begin();
			 impure != fmt_defaults.end(); ++impure)
		{
			memset(&*impure, 0, sizeof(*impure));
		}
	}

	~Format()
	{
		for (fmt_defaults_iterator impure = fmt_defaults.begin();
			 impure != fmt_defaults.end(); ++impure)
		{
			delete impure->vlu_string;
		}
	}

	static Format* newFormat(MemoryPool& p, int len = 0)
	{
		return FB_NEW_POOL(p) Format(p, len);
	}

	ULONG fmt_length;
	USHORT fmt_count;
	USHORT fmt_version;
	Firebird::Array<dsc> fmt_desc;
	Firebird::Array<impure_value> fmt_defaults;

	typedef Firebird::Array<dsc>::iterator fmt_desc_iterator;
	typedef Firebird::Array<dsc>::const_iterator fmt_desc_const_iterator;

	typedef Firebird::Array<impure_value>::iterator fmt_defaults_iterator;
};


// Blob passing structure
// CVC: Moved to fun.epp where it belongs.

// Scalar array descriptor, "external side" seen by UDF's

struct scalar_array_desc
{
	DSC sad_desc;
	SLONG sad_dimensions;
	struct sad_repeat
	{
		SLONG sad_lower;
		SLONG sad_upper;
	} sad_rpt[1];
};

// Sorry for the clumsy name, but in blk.h this is referred as array description.
// Since we already have Scalar Array Descriptor and Array Description [Slice],
// it was too confusing. Therefore, it was renamed ArrayField, since ultimately,
// it represents an array field the user can manipulate.
// There was also confusion for the casual reader due to the presence of
// the structure "slice" in sdl.h that was renamed array_slice.

class ArrayField : public pool_alloc_rpt<Ods::InternalArrayDesc::iad_repeat, type_arr>
{
public:
	UCHAR*				arr_data;			// Data block, if allocated
	blb*				arr_blob;			// Blob for data access
	jrd_tra*			arr_transaction;	// Parent transaction block
	ArrayField*			arr_next;			// Next array in transaction
	Request*			arr_request;		// request
	SLONG				arr_effective_length;	// Length of array instance
	USHORT				arr_desc_length;	// Length of array descriptor
	ULONG				arr_temp_id;		// Temporary ID for open array inside the transaction

	// Keep this field last as it is C-style open array !
	Ods::InternalArrayDesc	arr_desc;		// Array descriptor. !
};

// Parameter passing mechanism for UDFs.
// Also used for returning values, except for scalar_array.

enum FUN_T
{
	FUN_value,
	FUN_reference,
	FUN_descriptor,
	FUN_blob_struct,
	FUN_scalar_array,
	FUN_ref_with_null
};

// Blob passing structure

struct udf_blob
{
	SSHORT (*blob_get_segment) (blb*, UCHAR*, USHORT, USHORT*);
	void* blob_handle;
	SLONG blob_number_segments;
	SLONG blob_max_segment;
	SLONG blob_total_length;
	void (*blob_put_segment) (blb*, const UCHAR*, USHORT);
	SLONG (*blob_seek) (blb*, USHORT, SLONG);
};

} //namespace Jrd

#endif // JRD_VAL_H
