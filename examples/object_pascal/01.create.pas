{
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		01.create.pas
 *	DESCRIPTION:	A sample of creating new database and new table in it.
 *					Run second time (when database already exists) to see
 *					how FbException is caught and handled by this code.
 *
 *					Example for the following interfaces:
 *					IMaster - main inteface to access all the rest
 *					Status - returns the status of executed command
 *					Provider - main interface to access DB / service
 *					Attachment - database attachment interface
 *					Transaction - transaction interface
 *					Util - helper calls here and there
 *					XpbBuilder - build various parameters blocks
 *
 *					Run something like this to build: fpc -Fu<path-to-Firebird.pas> -Mdelphi 01.create.pas
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
 *  Copyright (c) 2015 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________. }

Program create;

uses Sysutils, Firebird;

var
	// Declare pointers to required interfaces

	// Status is used to return wide error description to user
	st : IStatus;

	// This is main interface of firebird, and the only one
	// for getting which there is special function in our API
	master : IMaster;
	util : IUtil;

	// XpbBuilder helps to create various parameter blocks for API calls
	dpb : IXpbBuilder;

	// Provider is needed to start to work with database (or service)
	prov : IProvider;

	// Attachment and Transaction contain methods to work with
	// database attachment and transaction
	att : IAttachment;
	tra : ITransaction;

	procedure PrintError(s : IStatus);
	var
		maxMessage : Integer;
		outMessage : PAnsiChar;
	begin
		maxMessage := 256;
		outMessage := StrAlloc(maxMessage);
		util.formatStatus(outMessage, maxMessage, s);
		writeln(outMessage);
		StrDispose(outMessage);
	end;

begin
	// Here we get access to master interface and helper utility interface
	// no error return may happen - these functions always succeed
	master := fb_get_master_interface;
	util := master.getUtilInterface;

	// status vector and main dispatcher are returned by calls to IMaster functions
	// no error return may happen - these functions always succeed
	st := master.getStatus;
	prov := master.getDispatcher;

	try
		// create DPB
		dpb := util.getXpbBuilder(st, IXpbBuilder.DPB, nil, 0);
		dpb.insertInt(st, isc_dpb_page_size, 4 * 1024);
		dpb.insertString(st, isc_dpb_user_name, 'sysdba');
		dpb.insertString(st, isc_dpb_password, 'masterkey');

		// create empty database
		att := prov.createDatabase(st, 'fbtests.fdb', dpb.getBufferLength(st), dpb.getBuffer(st));
		writeln ('Database fbtests.fdb created');

		// detach from database
		att.detach(st);
		att := nil;

		// remove unneeded any more tag from DPB
		if dpb.findFirst(st, isc_dpb_page_size)
			then dpb.removeCurrent(st);

		// attach it once again
		att := prov.attachDatabase(st, 'fbtests.fdb', dpb.getBufferLength(st), dpb.getBuffer(st));
		writeln ('Re-attached database fbtests.fdb');

		// start transaction
		tra := att.startTransaction(st, 0, nil);

		// create table
		att.execute(st, tra, 0, 'create table dates_table (d1 date)', 3,
			nil, nil, nil, nil);	// Input parameters and output data not used

		// commit transaction retaining
		tra.commitRetaining(st);
		writeln ('Table dates_table created');

		// insert a record into dates_table
		att.execute(st, tra, 0, 'insert into dates_table values (CURRENT_DATE)', 3,
			nil, nil, nil, nil);	// Input parameters and output data not used

		// commit transaction (will close interface)
		tra.commit(st);
		tra := nil;

		writeln ('Record inserted into dates_table');

		// detach from database (will close interface)
		att.detach(st);
		att := nil;

		dpb.dispose;
		dpb := nil;
	except
		on e: FbException do PrintError(e.getStatus);
	end;

	prov.release;
end.
