/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		cvt_proto.h
 *	DESCRIPTION:	Prototype header file for cvt.cpp
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

#ifndef JRD_CVT_PROTO_H
#define JRD_CVT_PROTO_H

#include "../common/classes/init.h"
#include "../common/cvt.h"
#include "../jrd/err_proto.h"

double		CVT_date_to_double(const dsc*);
void		CVT_double_to_date(double, SLONG[2]);
UCHAR		CVT_get_numeric(const UCHAR*, const USHORT, SSHORT*, void*);
GDS_DATE	CVT_get_sql_date(const dsc*);
GDS_TIME	CVT_get_sql_time(const dsc*);
ISC_TIME_TZ	CVT_get_sql_time_tz(const dsc*);
GDS_TIMESTAMP CVT_get_timestamp(const dsc*);
ISC_TIMESTAMP_TZ CVT_get_timestamp_tz(const dsc*);

namespace Jrd
{
	class EngineCallbacks : public Firebird::Callbacks
	{
	public:
		explicit EngineCallbacks(ErrorFunction aErr)
			: Callbacks(aErr)
		{
		}

		explicit EngineCallbacks(Firebird::MemoryPool&)
			: Callbacks(ERR_post)
		{
		}

	public:
		virtual bool transliterate(const dsc* from, dsc* to, CHARSET_ID&);
		virtual CHARSET_ID getChid(const dsc* d);
		virtual CharSet* getToCharset(CHARSET_ID charset2);
		virtual void validateData(CharSet* toCharset, SLONG length, const UCHAR* q);
		virtual ULONG validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
			const USHORT size);
		virtual SLONG getLocalDate();
		virtual ISC_TIMESTAMP getCurrentGmtTimeStamp();
		virtual USHORT getSessionTimeZone();
		virtual void isVersion4(bool& v4);

	public:
		static Firebird::GlobalPtr<EngineCallbacks> instance;
	};

	class TruncateCallbacks : public EngineCallbacks
	{
	public:
		explicit TruncateCallbacks(ISC_STATUS tr)
			: EngineCallbacks(ERR_post), truncateReason(tr)
		{
		}

		virtual ULONG validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
			const USHORT size);

	private:
		const ISC_STATUS truncateReason;
	};
}

inline void CVT_move(const dsc* from, dsc* to, Firebird::DecimalStatus decSt)
{
	CVT_move_common(from, to, decSt, &Jrd::EngineCallbacks::instance);
}

inline USHORT CVT_get_string_ptr(const dsc* desc, USHORT* ttype, UCHAR** address,
                                 vary* temp, USHORT length, Firebird::DecimalStatus decSt)
{
	return CVT_get_string_ptr_common(desc, ttype, address, temp, length, decSt,
									 &Jrd::EngineCallbacks::instance);
}

#endif // JRD_CVT_PROTO_H
