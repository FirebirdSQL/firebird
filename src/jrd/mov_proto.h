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

namespace Firebird::Jrd
{


int		MOV_compare(thread_db*, const dsc*, const dsc*);
double	MOV_date_to_double(const dsc*);
void	MOV_double_to_date(double, SLONG[2]);
bool	MOV_get_boolean(const dsc*);
double	MOV_get_double(thread_db*, const dsc*);
SLONG	MOV_get_long(thread_db*, const dsc*, SSHORT);
void	MOV_get_metaname(thread_db*, const dsc*, MetaName&);
SQUAD	MOV_get_quad(thread_db*, const dsc*, SSHORT);
SINT64	MOV_get_int64(thread_db*, const dsc*, SSHORT);
int		MOV_get_string_ptr(thread_db*, const dsc*, TTypeId*, UCHAR**, vary*, USHORT);
int		MOV_get_string(thread_db*, const dsc*, UCHAR**, vary*, USHORT);
void	MOV_get_string(thread_db* tdbb, const dsc* desc, string& str);
GDS_DATE	MOV_get_sql_date(const dsc*);
GDS_TIME	MOV_get_sql_time(const dsc*);
ISC_TIME_TZ	MOV_get_sql_time_tz(const dsc*);
GDS_TIMESTAMP	MOV_get_timestamp(const dsc*);
ISC_TIMESTAMP_TZ MOV_get_timestamp_tz(const dsc*);
USHORT	MOV_make_string(thread_db*, const dsc*, TTypeId, const char**, vary*, USHORT);
ULONG	MOV_make_string2(thread_db*, const dsc*, TTypeId, UCHAR**, MoveBuffer&, bool = true);
string MOV_make_string2(thread_db* tdbb, const dsc* desc, TTypeId ttype,
	bool limit = true);
void	MOV_move(thread_db*, /*const*/ dsc*, dsc*, bool trustedSource = false);
Decimal64 MOV_get_dec64(thread_db*, const dsc*);
Decimal128 MOV_get_dec128(thread_db*, const dsc*);
Int128 MOV_get_int128(thread_db*, const dsc*, SSHORT);

class DescPrinter
{
public:
	DescPrinter(thread_db* tdbb, const dsc* desc, FB_SIZE_T mLen, TTypeId charSetId);

	const string& get() const
	{
		return value;
	}

private:
	string value;
	FB_SIZE_T maxLen;
};


}	// namespace Firebird::Jrd

#endif // JRD_MOV_PROTO_H
