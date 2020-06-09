/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		12.batch_isc.cpp
 *	DESCRIPTION:	A sample of using Batch interface.
 *
 *					Example for the following interfaces:
 *					IUtil - get IStatement/ITransaction by handle
 *					IBatch - interface to work with FB batches
 *					IBatchCompletionState - contains result of batch execution
 *
 *	c++ 12.batch_isc.cpp -lfbclient
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
 *  Copyright (c) 2018 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "ifaceExamples.h"
#include <firebird/Message.h>

static IMaster* master = fb_get_master_interface();


// output error message to user

static void errPrint(IStatus* status)
{
	isc_print_status(status->getErrors());
}

static void raiseError(ThrowStatusWrapper& status, ISC_STATUS *vector)
{
	throw FbException(&status, vector);
}

// BatchCompletionState printer - prints all what we know about completed batch

static void print_cs(ThrowStatusWrapper& status, IBatchCompletionState* cs, IUtil* utl)
{
	unsigned p = 0;
	IStatus* s2 = NULL;
	bool pr1 = false, pr2 = false;

	// 1. Print per-message state info

	unsigned upcount = cs->getSize(&status);
	unsigned unk = 0, succ = 0;
	for (p = 0; p < upcount; ++p)
	{
		int s = cs->getState(&status, p);
		switch (s)
		{
		case IBatchCompletionState::EXECUTE_FAILED:
			if (!pr1)
			{
				printf("Message Status\n");
				pr1 = true;
			}
			printf("%5u   Execute failed\n", p);
			break;

		case IBatchCompletionState::SUCCESS_NO_INFO:
			++unk;
			break;

		default:
			if (!pr1)
			{
				printf("Message Status\n");
				pr1 = true;
			}
			printf("%5u   Updated %d record(s)\n", p, s);
			++succ;
			break;
		}
	}
	printf("Summary: total=%u success=%u success(but no update info)=%u\n", upcount, succ, unk);

	// 2. Print detailed errors (if exist) for messages 

	s2 = master->getStatus();
	for(p = 0; (p = cs->findError(&status, p)) != IBatchCompletionState::NO_MORE_ERRORS; ++p)
	{
		try
		{
			cs->getStatus(&status, s2, p);

			char text[1024];
			utl->formatStatus(text, sizeof(text) - 1, s2);
			text[sizeof(text) - 1] = 0;
			if (!pr2)
			{
				printf("\nDetailed errors status:\n");
				pr2 = true;
			}
			printf("Message %u: %s\n", p, text);
		}
		catch (const FbException& error)
		{
			// handle error
			fprintf(stderr, "\nError describing message %u\n", p);
			errPrint(error.getStatus());
			fprintf(stderr, "\n");
		}
	}

	if (s2)
		s2->dispose();
}

int main()
{
	int rc = 0;

	// set default password if none specified in environment
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	// With ThrowStatusWrapper passed as status interface FbException will be thrown on error
	ThrowStatusWrapper status(master->getStatus());

	ISC_STATUS_ARRAY st;
	isc_db_handle db = 0;
	isc_tr_handle tr = 0;
	isc_stmt_handle stmt = 0;
	isc_blob_handle blb = 0;

	// Declare pointers to required interfaces
	/*IProvider* prov = master->getDispatcher();
	IAttachment* att = NULL;*/
	IUtil* utl = master->getUtilInterface();
	IStatement* statemt = NULL;
	ITransaction* tra = NULL;
	IBatch* batch = NULL;
	IBatchCompletionState* cs = NULL;
	IXpbBuilder* pb = NULL;

	unsigned char streamBuf[10240];		// big enough for demo
	unsigned char* stream = NULL;

	try
	{
		// attach employee db
		if (isc_attach_database(st, 0, "employee", &db, 0, NULL))
			raiseError(status, st);

		isc_tr_handle tr = 0;
		if (isc_start_transaction(st, &tr, 1, &db, 0, NULL))
			raiseError(status, st);

		// cleanup
		const char* cleanSql = "delete from project where proj_id like 'BAT%'";
		if (isc_dsql_execute_immediate(st, &db, &tr, 0, cleanSql, 3, NULL))
			raiseError(status, st);

		if (isc_dsql_allocate_statement(st, &db, &stmt))
			raiseError(status, st);

		// get transaction interface
		if (fb_get_transaction_interface(st, &tra, &tr))
			raiseError(status, st);

		//
		printf("\nPart 1. BLOB created using IBlob interface.\n");
		//

		// prepare statement
		const char* sqlStmt1 = "insert into project(proj_id, proj_name) values(?, ?)";
		if (isc_dsql_prepare(st, &tr, &stmt, 0, sqlStmt1, 3, NULL))
			raiseError(status, st);
		// and get it's interface
		if (fb_get_statement_interface(st, &statemt, &stmt))
			raiseError(status, st);

		// Message to store in a table
		FB_MESSAGE(Msg1, ThrowStatusWrapper,
			(FB_VARCHAR(5), id)
			(FB_VARCHAR(10), name)
		) project1(&status, master);
		project1.clear();
		IMessageMetadata* meta = project1.getMetadata();

		// set batch parameters
		pb = utl->getXpbBuilder(&status, IXpbBuilder::BATCH, NULL, 0);
		// collect per-message statistics
		pb->insertInt(&status, IBatch::TAG_RECORD_COUNTS, 1);

		// create batch
		batch = statemt->createBatch(&status, meta,
			pb->getBufferLength(&status), pb->getBuffer(&status));

		// fill batch with data record by record
		project1->id.set("BAT11");
		project1->name.set("SNGL_REC");
		batch->add(&status, 1, project1.getData());

		project1->id.set("BAT12");
		project1->name.set("SNGL_REC2");
		batch->add(&status, 1, project1.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// close batch
		batch->release();
		batch = NULL;

		// unprepare statement
		statemt->release();
		statemt = NULL;
		if (isc_dsql_free_statement(st, &stmt, DSQL_unprepare))
			raiseError(status, st);

		//
		printf("\nPart 2. BLOB created using isc_create_blob.\n");
		//

		// prepare statement
		const char* sqlStmt2 = "insert into project(proj_id, proj_name, proj_desc) values(?, ?, ?)";
		if (isc_dsql_prepare(st, &tr, &stmt, 0, sqlStmt2, 3, NULL))
			raiseError(status, st);
		// and get it's interface
		if (fb_get_statement_interface(st, &statemt, &stmt))
			raiseError(status, st);

		// Message to store in a table
		FB_MESSAGE(Msg2, ThrowStatusWrapper,
			(FB_VARCHAR(5), id)
			(FB_VARCHAR(10), name)
			(FB_BLOB, desc)
		) project2(&status, master);
		project2.clear();
		meta = project2.getMetadata();

		// set batch parameters
		pb->clear(&status);
		// enable blobs processing - IDs generated by firebird engine
		pb->insertInt(&status, IBatch::TAG_BLOB_POLICY, IBatch::BLOB_ID_ENGINE);

		// create batch
		batch = statemt->createBatch(&status, meta,
			pb->getBufferLength(&status), pb->getBuffer(&status));

		// create blob
		ISC_QUAD realId;
		if (isc_create_blob(st, &db, &tr, &blb, &realId))
			raiseError(status, st);
		const char* text = "Blob created using traditional API";
		if (isc_put_segment(st, &blb, strlen(text), text))
			raiseError(status, st);
		if (isc_close_blob(st, &blb))
			raiseError(status, st);

		// add message
		project2->id.set("BAT38");
		project2->name.set("FRGN_BLB");
		batch->registerBlob(&status, &realId, &project2->desc);
		batch->add(&status, 1, project2.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// close batch
		batch->release();
		batch = NULL;

		// unprepare statement
		statemt->release();
		statemt = NULL;
		if (isc_dsql_free_statement(st, &stmt, DSQL_drop))
			raiseError(status, st);

		// cleanup
		tra->release();
		tra = NULL;

	    if (isc_commit_transaction (st, &tr))
	        raiseError(status, st);

	    if (isc_detach_database(st, &db))
	        raiseError(status, st);
	}
	catch (const FbException& error)
	{
		// handle error
		rc = 1;
		errPrint(error.getStatus());
	}

	// release interfaces after error caught
	if (cs)
		cs->dispose();
	if (batch)
		batch->release();
	if (tra)
		tra->release();
	if (statemt)
		statemt->release();

	// close handles if not closed
	if (blb)
		isc_cancel_blob(st, &blb);
	if (stmt)
		isc_dsql_free_statement(st, &stmt, DSQL_close);
	if (tr)
	    isc_rollback_transaction (st, &tr);
	if (db)
    	isc_detach_database(st, &db);

	// cleanup
	if (pb)
		pb->dispose();
	status.dispose();

	return rc;
}
