/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		01.create.c
 *	DESCRIPTION:	Minimal sample of using interfaces from plain C.
 *
 *					Run something like this to build: cc 01.create.c -lfbclient
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2024 Alexander Peshkoff <alexander.peshkoff@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird/fb_c_api.h"
#include <stdio.h>

#define CHECK_STATUS(st) if (IStatus_getState(st) & IStatus_STATE_ERRORS) {char buf[256]; IUtil_formatStatus(utl, buf, sizeof buf, st); puts(buf); return 1;}

int main()
{
	// Here we get access to master interface. This is main interface of firebird,
	// and the only one for getting which there is special function in our API.
	struct IMaster* master = fb_get_master_interface();

	// Declare pointers to required interfaces
	// IStatus is used to return wide error description to user
	// IProvider is needed to start to work with database (or service)
	// Status vector, main dispatcher and utility interfaces are returned by IMaster functions
	// No error return may happen - these functions always succeed
	struct IStatus* st = IMaster_getStatus(master);
	struct IProvider* prov = IMaster_getDispatcher(master);
	struct IUtil* utl = IMaster_getUtilInterface(master);

	// IAttachment and ITransaction contain methods to work with database attachment
	// and transactions
	struct IAttachment* att = NULL;
	struct ITransaction* tra = NULL;

	// IXpbBuilder is used to access various parameters blocks used in API
	struct IXpbBuilder* dpb = NULL;

	// create DPB - use non-default page size 4Kb
	dpb = IUtil_getXpbBuilder(utl, st, IXpbBuilder_DPB, NULL, 0);
	CHECK_STATUS(st);
	IXpbBuilder_insertInt(dpb, st, isc_dpb_page_size, 4 * 1024);
	CHECK_STATUS(st);
	IXpbBuilder_insertString(dpb, st, isc_dpb_user_name, "sysdba");
	CHECK_STATUS(st);
	IXpbBuilder_insertString(dpb, st, isc_dpb_password, "masterkey");
	CHECK_STATUS(st);

	// create empty database
	att = IProvider_createDatabase(prov, st, "fbtests.fdb",
		IXpbBuilder_getBufferLength(dpb, st), IXpbBuilder_getBuffer(dpb, st));
	CHECK_STATUS(st);
	printf("Database fbtests.fdb created\n");

	// start transaction
	tra = IAttachment_startTransaction(att, st, 0, NULL);
	CHECK_STATUS(st);

	// create table
	IAttachment_execute(att, st, tra, 0, "create table dates_table (d1 date)", 3,
		NULL, NULL, NULL, NULL);	// Input parameters and output data not used
	CHECK_STATUS(st);

	// commit transaction retaining
	ITransaction_commitRetaining(tra, st);
	CHECK_STATUS(st);
	printf("Table dates_table created\n");

	// insert a record into dates_table
	IAttachment_execute(att, st, tra, 0, "insert into dates_table values (CURRENT_DATE)", 3,
		NULL, NULL, NULL, NULL);	// Input parameters and output data not used
	CHECK_STATUS(st);

	// commit transaction (will close interface)
	ITransaction_commit(tra, st);
	CHECK_STATUS(st);
	printf("Record inserted into dates_table\n");

	// detach from database (will close interface)
	IAttachment_detach(att, st);
	CHECK_STATUS(st);

	IStatus_dispose(st);
	IProvider_release(prov);

	return 0;
}
