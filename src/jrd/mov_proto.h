/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		mov_proto.h
 *	DESCRIPTION:	Prototype header file for mov.cpp
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

#ifndef JRD_MOV_PROTO_H
#define JRD_MOV_PROTO_H

#include "../common/dsc.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"

struct dsc;
struct vary;

int		MOV_compare(Jrd::thread_db*, const dsc*, const dsc*);
double	MOV_date_to_double(const dsc*);
void	MOV_double_to_date(double, SLONG[2]);
bool	MOV_get_boolean(const dsc*);
double	MOV_get_double(Jrd::thread_db*, const dsc*);
SLONG	MOV_get_long(Jrd::thread_db*, const dsc*, SSHORT);
void	MOV_get_metaname(Jrd::thread_db*, const dsc*, Firebird::MetaName&);
SQUAD	MOV_get_quad(Jrd::thread_db*, const dsc*, SSHORT);
SINT64	MOV_get_int64(Jrd::thread_db*, const dsc*, SSHORT);
int		MOV_get_string_ptr(Jrd::thread_db*, const dsc*, USHORT*, UCHAR**, vary*, USHORT);
int		MOV_get_string(Jrd::thread_db*, const dsc*, UCHAR**, vary*, USHORT);
GDS_DATE	MOV_get_sql_date(const dsc*);
GDS_TIME	MOV_get_sql_time(const dsc*);
ISC_TIME_TZ	MOV_get_sql_time_tz(const dsc*);
GDS_TIMESTAMP	MOV_get_timestamp(const dsc*);
ISC_TIMESTAMP_TZ MOV_get_timestamp_tz(const dsc*);
int		MOV_make_string(Jrd::thread_db*, const dsc*, USHORT, const char**, vary*, USHORT);
int		MOV_make_string2(Jrd::thread_db*, const dsc*, USHORT, UCHAR**, Jrd::MoveBuffer&, bool = true);
Firebird::string MOV_make_string2(Jrd::thread_db* tdbb, const dsc* desc, USHORT ttype,
	bool limit = true);
void	MOV_move(Jrd::thread_db*, /*const*/ dsc*, dsc*);
void	MOV_move_ext(Jrd::thread_db* tdbb, /*const*/ dsc* from, dsc* to, bool toExtern);
Firebird::Decimal64 MOV_get_dec64(Jrd::thread_db*, const dsc*);
Firebird::Decimal128 MOV_get_dec128(Jrd::thread_db*, const dsc*);
Firebird::DecimalFixed MOV_get_dec_fixed(Jrd::thread_db*, const dsc*, SSHORT);

namespace Jrd
{

class DescPrinter
{
public:
	DescPrinter(thread_db* tdbb, const dsc* desc, FB_SIZE_T mLen);

	const Firebird::string& get() const
	{
		return value;
	}

private:
	Firebird::string value;
	FB_SIZE_T maxLen;
};

}	// namespace Jrd

#endif // JRD_MOV_PROTO_H
