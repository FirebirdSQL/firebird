/*
 *	PROGRAM:	Firebird utilities
 *	MODULE:		nbkswi.h
 *	DESCRIPTION:	nbackup switches
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
 *  Copyright (c) 2008 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef NBACKUP_NBKSWI_H
#define NBACKUP_NBKSWI_H

#include "../jrd/constants.h"

// Switch handling constants

const int IN_SW_NBK_0				= 0;
const int IN_SW_NBK_LOCK			= 1;
const int IN_SW_NBK_UNLOCK			= 2;
const int IN_SW_NBK_FIXUP			= 3;
const int IN_SW_NBK_BACKUP			= 4;
const int IN_SW_NBK_RESTORE			= 5;
const int IN_SW_NBK_NODBTRIG		= 6;
const int IN_SW_NBK_USER_NAME		= 7;
const int IN_SW_NBK_PASSWORD		= 8;
const int IN_SW_NBK_SIZE			= 9;
const int IN_SW_NBK_FETCH			= 10;
const int IN_SW_NBK_VERSION			= 11;
const int IN_SW_NBK_HELP			= 12;
const int IN_SW_NBK_DIRECT			= 13;
const int IN_SW_NBK_DECOMPRESS		= 14;
const int IN_SW_NBK_ROLE			= 15;
const int IN_SW_NBK_INPLACE			= 16;
const int IN_SW_NBK_SEQUENCE		= 17;
const int IN_SW_NBK_CLEAN_HISTORY	= 18;
const int IN_SW_NBK_KEEP			= 19;


static const struct Switches::in_sw_tab_t nbackup_in_sw_table [] =
{
	{IN_SW_NBK_NODBTRIG,	isc_spb_nbk_no_triggers,	"T",		0, 0, 0, false,	true,	0,	1, NULL},
	{IN_SW_NBK_DIRECT,		isc_spb_nbk_direct,			"DIRECT",	0, 0, 0, false, false,	0,	1, NULL},
	{IN_SW_NBK_INPLACE,		isc_spb_nbk_inplace,		"INPLACE",	0, 0, 0, false, true,	0,	1, NULL},
	{IN_SW_NBK_SEQUENCE,	isc_spb_nbk_sequence,		"SEQUENCE",	0, 0, 0, false, true,	0,	3, NULL},
	{IN_SW_NBK_0,			0,							NULL,		0, 0, 0, false, false,	0,	0, NULL}	// End of List
};

enum NbakOptionType { nboGeneral, nboSpecial, nboExclusive };

static const struct Switches::in_sw_tab_t nbackup_action_in_sw_table [] =
{
	{IN_SW_NBK_LOCK,		0,						"LOCK",				0, 0, 0, false, false,	8,	1,	NULL, nboExclusive},
	{IN_SW_NBK_UNLOCK,		0,						"N",				0, 0, 0, false, false,	0,	1,	NULL, nboExclusive},
	{IN_SW_NBK_UNLOCK,		0,						"UNLOCK",			0, 0, 0, false, false,	9,	2,	NULL, nboExclusive},
	{IN_SW_NBK_FIXUP,		isc_action_svc_nfix,	"FIXUP",			0, 0, 0, false, false,	10,	1,	NULL, nboExclusive},
	{IN_SW_NBK_BACKUP,		isc_action_svc_nbak,	"BACKUP",			0, 0, 0, false, false,	11,	1,	NULL, nboExclusive},
	{IN_SW_NBK_RESTORE,		isc_action_svc_nrest,	"RESTORE",			0, 0, 0, false, false,	12,	1,	NULL, nboExclusive},
	{IN_SW_NBK_DIRECT,		0,						"DIRECT",			0, 0, 0, false, false,	70,	1,	NULL, nboSpecial},
	{IN_SW_NBK_INPLACE,		0,						"INPLACE",			0, 0, 0, false, false,	78, 1,	NULL, nboSpecial},
	{IN_SW_NBK_SIZE,		0,						"SIZE",				0, 0, 0, false, false,	17,	1,	NULL, nboSpecial},
	{IN_SW_NBK_DECOMPRESS,	0,						"DECOMPRESS",		0, 0, 0, false, false,	74,	2,	NULL, nboSpecial},
	{IN_SW_NBK_SEQUENCE,	0,						"SEQUENCE",			0, 0, 0, false, false,	80, 3,	NULL, nboSpecial},
	{IN_SW_NBK_CLEAN_HISTORY, isc_spb_nbk_clean_history, "CLEAN_HISTORY",	0, 0, 0, false, false,	82, 10,	NULL, nboSpecial},
	{IN_SW_NBK_KEEP,		0,						"KEEP",				0, 0, 0, false, false,	83, 1,	NULL, nboSpecial},
	{IN_SW_NBK_NODBTRIG,	0,						"T",				0, 0, 0, false, false,	0,	1,	NULL, nboGeneral},
	{IN_SW_NBK_NODBTRIG,	0,						"NODBTRIGGERS",		0, 0, 0, false, false,	16,	3,	NULL, nboGeneral},
	{IN_SW_NBK_USER_NAME,	0,						"USER",				0, 0, 0, false, false,	13,	1,	NULL, nboGeneral},
	{IN_SW_NBK_ROLE,		0,						"ROLE",				0, 0, 0, false, false,	76,	2,	NULL, nboGeneral},
	{IN_SW_NBK_PASSWORD,	0,						"PASSWORD",			0, 0, 0, false, false,	14,	1,	NULL, nboGeneral},
	{IN_SW_NBK_FETCH,		0,						"FETCH_PASSWORD",	0, 0, 0, false, false,	15,	2,	NULL, nboGeneral},
	{IN_SW_NBK_VERSION, 	0,						"Z",				0, 0, 0, false, false,	18,	1,	NULL, nboGeneral},
	{IN_SW_NBK_HELP,		0,						"?",				0, 0, 0, false, false,	0,	1,	NULL, 0},
	{IN_SW_NBK_0,			0,						NULL,				0, 0, 0, false, false,	0,	0,	NULL, 0}	// End of List
};
#endif // NBACKUP_NBKSWI_H
