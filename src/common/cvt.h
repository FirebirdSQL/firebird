/*
 *	PROGRAM:		Common part of dsc converters.
 *	MODULE:			cvt.h
 *	DESCRIPTION:	Common part of dsc converters.
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
 *  Copyright (c) 2008 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef COMMON_CVT_H
#define COMMON_CVT_H

#include "../common/DecFloat.h"

namespace Jrd {

class CharSet;

}

struct dsc;

namespace Firebird {

class Callbacks
{
public:
	explicit Callbacks(ErrorFunction aErr)
		: err(aErr)
	{
	}

	virtual ~Callbacks()
	{
	}

public:
	virtual bool transliterate(const dsc* from, dsc* to, CHARSET_ID&) = 0;
	virtual CHARSET_ID getChid(const dsc* d) = 0;
	virtual Jrd::CharSet* getToCharset(CHARSET_ID charset2) = 0;
	virtual void validateData(Jrd::CharSet* toCharset, SLONG length, const UCHAR* q) = 0;
	virtual ULONG validateLength(Jrd::CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
		const USHORT size) = 0;
	virtual SLONG getLocalDate() = 0;
	virtual ISC_TIMESTAMP getCurrentGmtTimeStamp() = 0;
	virtual USHORT getSessionTimeZone() = 0;
	virtual void isVersion4(bool& v4) = 0;

public:
	const ErrorFunction err;
};

extern Callbacks* CVT_commonCallbacks;

enum EXPECT_DATETIME
{
	expect_timestamp,
	expect_timestamp_tz,
	expect_sql_date,
	expect_sql_time,
	expect_sql_time_tz
};

class Int128;

} // namespace Firebird


void CVT_conversion_error(const dsc*, ErrorFunction, const Firebird::Exception* = nullptr);
double CVT_power_of_ten(const int);
SLONG CVT_get_long(const dsc*, SSHORT, Firebird::DecimalStatus, ErrorFunction);
bool CVT_get_boolean(const dsc*, ErrorFunction);
double CVT_get_double(const dsc*, Firebird::DecimalStatus, ErrorFunction, bool* getNumericOverflow = nullptr);
Firebird::Decimal64 CVT_get_dec64(const dsc*, Firebird::DecimalStatus, ErrorFunction);
Firebird::Decimal128 CVT_get_dec128(const dsc*, Firebird::DecimalStatus, ErrorFunction);
Firebird::Int128 CVT_get_int128(const dsc*, SSHORT, Firebird::DecimalStatus, ErrorFunction);
Firebird::Int128 CVT_hex_to_int128(const char* str, USHORT len);
USHORT CVT_make_string(const dsc*, USHORT, const char**, vary*, USHORT, Firebird::DecimalStatus, ErrorFunction);
void CVT_move_common(const dsc*, dsc*, Firebird::DecimalStatus, Firebird::Callbacks*);
void CVT_move(const dsc*, dsc*, Firebird::DecimalStatus, ErrorFunction);
SSHORT CVT_decompose(const char*, USHORT, SSHORT*, ErrorFunction);
SSHORT CVT_decompose(const char*, USHORT, SLONG*, ErrorFunction);
SSHORT CVT_decompose(const char*, USHORT, SINT64*, ErrorFunction);
SSHORT CVT_decompose(const char*, USHORT, Firebird::Int128*, ErrorFunction);
USHORT CVT_get_string_ptr(const dsc*, USHORT*, UCHAR**, vary*, USHORT, Firebird::DecimalStatus, ErrorFunction);
USHORT CVT_get_string_ptr_common(const dsc*, USHORT*, UCHAR**, vary*, USHORT, Firebird::DecimalStatus, Firebird::Callbacks*);
SINT64 CVT_get_int64(const dsc*, SSHORT, Firebird::DecimalStatus, ErrorFunction);
SQUAD CVT_get_quad(const dsc*, SSHORT, Firebird::DecimalStatus, ErrorFunction);
void CVT_string_to_datetime(const dsc*, ISC_TIMESTAMP_TZ*, bool*, const Firebird::EXPECT_DATETIME,
	bool, Firebird::Callbacks*);
const UCHAR* CVT_get_bytes(const dsc*, unsigned&);

#endif //COMMON_CVT_H
