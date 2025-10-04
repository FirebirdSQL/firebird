/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn.h
 *	DESCRIPTION:	Dynamic data definition local data
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

#ifndef	JRD_DYN_H
#define JRD_DYN_H

#include "../common/classes/MsgPrint.h"
#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/dsc.h"
#include "firebird/impl/msg_helper.h"

inline constexpr const char* ALL_PRIVILEGES = "SIUDR";	// all applicable grant/revoke privileges
inline constexpr const char* EXEC_PRIVILEGES = "X";		// execute privilege
inline constexpr const char* USAGE_PRIVILEGES = "G";	// usage privilege
inline constexpr const char* ALL_DDL_PRIVILEGES = "CLO";

inline constexpr int DYN_MSG_FAC = FB_IMPL_MSG_FACILITY_DYN;


namespace Jrd {

class jrd_tra;
class thread_db;

class dyn_fld
{
public:
	dsc dyn_dsc;
	bool dyn_null_flag = false;
	USHORT dyn_dtype = 0;
	USHORT dyn_precision = 0;
	USHORT dyn_charlen = 0;
	SSHORT dyn_collation = 0;
	SSHORT dyn_charset = 0;
	SSHORT dyn_sub_type = 0;
	QualifiedName dyn_fld_source;
	QualifiedName dyn_rel_name;
	QualifiedName dyn_fld_name;
    USHORT dyn_charbytelen = 0; // Used to check modify operations on string types.
    const UCHAR* dyn_default_src = nullptr;
    const UCHAR* dyn_default_val = nullptr;
    bool dyn_drop_default = false;
    const UCHAR* dyn_computed_src = nullptr;
    const UCHAR* dyn_computed_val = nullptr;
    bool dyn_drop_computed = false;
public:
	explicit dyn_fld(MemoryPool& p)
		: dyn_fld_source(p), dyn_rel_name(p), dyn_fld_name(p)
	{ }

	dyn_fld() noexcept
	{ }
};

} // namespace Jrd

#endif // JRD_DYN_H
