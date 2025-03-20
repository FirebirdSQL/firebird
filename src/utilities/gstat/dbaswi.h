/*
 *	PROGRAM:	Database analysis tool
 *	MODULE:		dbaswi.h
 *	DESCRIPTION:	dba switches
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

#ifndef DBA_DBASWI_H
#define DBA_DBASWI_H

#include "../common/classes/Switches.h"
#include "../jrd/constants.h"

// Switch handling constants.  Note that the first IN_SW_DATA_ITEMS
// switch constants refer to data items.  The remaining switch constants
// refer to actual switches.

const int IN_SW_DBA_0				= 0;	// not a known switch
const int IN_SW_DBA_SYSTEM			= 1;	// analyze system relations
const int IN_SW_DBA_DATA			= 2;	// analyze data pages
const int IN_SW_DBA_INDEX			= 3;	// analyze index leaf pages
const int IN_SW_DBA_VERSION			= 4;	// display version number
const int IN_SW_DBA_HEADER			= 5;	// analyze header page
//const int IN_SW_DBA_LOG			= 6;	// analze log pages
const int IN_SW_DBA_DATAIDX			= 7;	// analyze data and index pages
const int IN_SW_DBA_USERNAME		= 8;	// username
const int IN_SW_DBA_PASSWORD		= 9;	// password
const int IN_SW_DBA_RECORD			= 10;	// analyze record versions
const int IN_SW_DBA_RELATION		= 11;	// analyze specific relations
const int IN_SW_DBA_NOCREATION		= 12;	// don't print creation date
#ifdef TRUSTED_AUTH
const int IN_SW_DBA_TRUSTEDAUTH		= 13;	// trusted user name
#endif
const int IN_SW_DBA_FETCH_PASS		= 14;	// fetch password from file
const int IN_SW_DBA_ENCRYPTION		= 15;	// analyze pages encryption
const int IN_SW_DBA_HELP			= 16;	// show help
const int IN_SW_DBA_ROLE			= 17;	// SQL role
const int IN_SW_DBA_SCHEMA			= 18;	// analyze specific schemas

const static struct Switches::in_sw_tab_t dba_in_sw_table[] =
{
    {IN_SW_DBA_DATAIDX,			0,							"ALL",		0,0,0,	false,	false,	22,	1, NULL},	// msg 22: -a      analyze data and index pages
    {IN_SW_DBA_DATA,			isc_spb_sts_data_pages,		"DATA",		0,0,0,	false,	true,	23,	1, NULL},	// msg 23: -d      analyze data pages
    {IN_SW_DBA_ENCRYPTION,		isc_spb_sts_encryption,	  "ENCRYPTION",	0,0,0,	false,	true,	51,	1, NULL},	// msg 51: -e      analyze database encryption
    {IN_SW_DBA_HEADER,			isc_spb_sts_hdr_pages,		"HEADER",	0,0,0,	false,	true,	24,	1, NULL},	// msg 24: -h      analyze header page
    {IN_SW_DBA_INDEX,			isc_spb_sts_idx_pages,		"INDEX",	0,0,0,	false,	true,	25,	1, NULL},	// msg 25: -i      analyze index leaf pages
//  {IN_SW_DBA_LOG,				isc_spb_sts_db_log,			"LOG",		0,0,0,	false,	true,	26,	1, NULL},	// msg 26: -l      analyze log page
    {IN_SW_DBA_SYSTEM,			isc_spb_sts_sys_relations,	"SYSTEM",	0,0,0,	false,	true,	27,	1, NULL},	// msg 27: -s      analyze system relations
    {IN_SW_DBA_USERNAME,		0,							"USERNAME",	0,0,0,	false,	false,	32,	1, NULL},	// msg 32: -u      username
    {IN_SW_DBA_PASSWORD,		0,							"PASSWORD",	0,0,0,	false,	false,	33,	1, NULL},	// msg 33: -p      password
    {IN_SW_DBA_FETCH_PASS,		0,					"FETCH_PASSWORD",	0,0,0,	false,	false,	37,	2, NULL},	// msg 37: -fetch  fetch password from file
    {IN_SW_DBA_RECORD,			isc_spb_sts_record_versions,"RECORD",	0,0,0,	false,	true,	34,	1, NULL},	// msg 34: -r      analyze average record and version length
    {IN_SW_DBA_SCHEMA,			isc_spb_sts_schema,			"SCHEMA",	0,0,0,	false,	false,	65,	1, NULL},	// msg 65: -sch    schemaname (case sensitive)
    {IN_SW_DBA_RELATION,		isc_spb_sts_table,			"TABLE",	0,0,0,	false,	false,	35,	1, NULL},	// msg 35: -t      tablename
    {IN_SW_DBA_RELATION,		isc_spb_sts_table,			"TABLE",	0,0,0,	false,	true,	0,	1, NULL},	// no msg: let run old buggy code
    {IN_SW_DBA_ROLE,			0,							"ROLE",		0,0,0,	false,	false,	57,	1, NULL},	// msg 57: -role   SQL role name
	// special switch to avoid including creation date, only for tests (no message)
    {IN_SW_DBA_NOCREATION,		isc_spb_sts_nocreation,	"NOCREATION",	0,0,0,	false,	true,	0,	1, NULL},	// msg (ignored) -n suppress creation date
#ifdef TRUSTED_AUTH
    {IN_SW_DBA_TRUSTEDAUTH,		0,							"TRUSTED",	0,0,0,	false,	false,	36,	2, NULL},	// msg 36: -tr     use trusted authentication
#endif
    {IN_SW_DBA_VERSION,			0,							"Z",		0,0,0,	false,	false,	28,	1, NULL},	// msg 28: -z      display version number
    {IN_SW_DBA_HELP,			0,							"?",		0,0,0,	false,	false,	0,	1, NULL},	// Help
    {IN_SW_DBA_0,				0,							NULL,		0,0,0,	false,	false,	0,	0, NULL}	// End of List
};
#endif // DBA_DBASWI_H

