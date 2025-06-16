/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		val_proto.h
 *	DESCRIPTION:	Prototype header file for val.cpp
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

#ifndef JRD_VAL_PROTO_H
#define JRD_VAL_PROTO_H

bool VAL_validate(Jrd::thread_db*, USHORT);
int VAL_service(Firebird::UtilSvc*);

const int IN_SW_VAL_TAB_INCL		= 1;
const int IN_SW_VAL_TAB_EXCL		= 2;
const int IN_SW_VAL_IDX_INCL		= 3;
const int IN_SW_VAL_IDX_EXCL		= 4;
const int IN_SW_VAL_LOCK_TIMEOUT	= 5;
const int IN_SW_VAL_DATABASE		= 6;
const int IN_SW_VAL_SCH_INCL		= 7;
const int IN_SW_VAL_SCH_EXCL		= 8;

static const Switches::in_sw_tab_t val_option_in_sw_table[] =
{
	{IN_SW_VAL_SCH_INCL,		isc_spb_val_sch_incl,		"SCH_INCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_SCH_EXCL,		isc_spb_val_sch_excl,		"SCH_EXCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_TAB_INCL,		isc_spb_val_tab_incl,		"TAB_INCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_TAB_EXCL,		isc_spb_val_tab_excl,		"TAB_EXCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_IDX_INCL,		isc_spb_val_idx_incl,		"IDX_INCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_IDX_EXCL,		isc_spb_val_idx_excl,		"IDX_EXCLUDE",	0, 0, 0, false,	false,	0,	5, NULL},
	{IN_SW_VAL_LOCK_TIMEOUT,	isc_spb_val_lock_timeout,	"WAIT", 		0, 0, 0, false,	false,	0,	1, NULL},
	{IN_SW_VAL_DATABASE,		isc_spb_dbname,				"DATABASE",		0, 0, 0, false,	false,	0,	1, NULL},
	{0, 0, NULL, 0, 0, 0, false, false,	0, 0, NULL}		// End of List
};

#endif // JRD_VAL_PROTO_H

