/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		03.select.cpp
 *	DESCRIPTION:	A sample of running SELECT statement without parameters.
 *					Prints string fields in a table, coercing VARCHAR to CHAR.
 *					Learns how to coerce output data in prepared statement
 *					and execute it.
 *
 *					Example for the following interfaces:
 *
 *					IStatement - SQL statement execution
 *					IMessageMetadata - describe input and output data format
 *					IResultSet - fetch data returned by statement after execution
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
 *  Copyright (c) 2013 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "ifaceExamples.h"

static IMaster* master = fb_get_master_interface();

int main()
{
	int rc = 0;

	// set default password if none specified in environment
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	// status vector and main dispatcher
	ThrowStatusWrapper status(master->getStatus());
	IProvider* prov = master->getDispatcher();
	IUtil* utl = master->getUtilInterface();

	// declare pointers to required interfaces
	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	IStatement* stmt = NULL;
	IMessageMetadata* meta = NULL;
	IMetadataBuilder* builder = NULL;
	IXpbBuilder* tpb = NULL;

	// Interface provides access to data returned by SELECT statement
	IResultSet* curs = NULL;

	try
	{
		// attach employee db
		att = prov->attachDatabase(&status, "employee", 0, NULL);

		// start read only transaction
		tpb = utl->getXpbBuilder(&status, IXpbBuilder::TPB, NULL, 0);
		tpb->insertTag(&status, isc_tpb_read_committed);
		tpb->insertTag(&status, isc_tpb_no_rec_version);
		tpb->insertTag(&status, isc_tpb_wait);
		tpb->insertTag(&status, isc_tpb_read);
		tra = att->startTransaction(&status, tpb->getBufferLength(&status), tpb->getBuffer(&status));

		// prepare statement
		stmt = att->prepare(&status, tra, 0, "select last_name, first_name, phone_ext from phone_list "
										"where location = 'Monterey' order by last_name, first_name",
			SAMPLES_DIALECT, IStatement::PREPARE_PREFETCH_METADATA);

		// get list of columns
		meta = stmt->getOutputMetadata(&status);
		builder = meta->getBuilder(&status);
		unsigned cols = meta->getCount(&status);

		// struct to cache received metadata
		struct MyField
		{
			const char* name;
			unsigned length, offset;
		};
		MyField* fields = new MyField[cols];
		memset(fields, 0, sizeof(MyField) * cols);

		// parse columns list & coerce datatype(s)
		for (unsigned j = 0; j < cols; ++j)
		{
			unsigned t = meta->getType(&status, j);

			if (t == SQL_VARYING || t == SQL_TEXT)
			{
				builder->setType(&status, j, SQL_TEXT);
				fields[j].name = meta->getField(&status, j);
			}
		}

		// release automatically created metadata
		// metadata is not database object, therefore no specific call to close it
		meta->release();

		// get metadata with coerced datatypes
		meta = builder->getMetadata(&status);

		// builder not needed any more
		builder->release();
		builder = NULL;

		// now we may also get offsets info
		for (unsigned j = 0; j < cols; ++j)
		{
			if (fields[j].name)
			{
				fields[j].length = meta->getLength(&status, j);
				fields[j].offset = meta->getOffset(&status, j);
			}
		}

		// open cursor
		curs = stmt->openCursor(&status, tra, NULL, NULL, meta, 0);

		// allocate output buffer
		unsigned l = meta->getMessageLength(&status);
		unsigned char* buffer = new unsigned char[l];

		// fetch records from cursor and print them
		for (int line = 0; curs->fetchNext(&status, buffer) == IStatus::RESULT_OK; ++line)
		{
			if (line % 10 == 0)
			{
				printf("\n");
				for (unsigned j = 0; j < cols; ++j)
				{
					if (fields[j].name)
					{
						printf("%-*.*s ", fields[j].length, fields[j].length, fields[j].name);
					}
				}
				printf("\n");
			}

			for (unsigned j = 0; j < cols; ++j)
			{
				if (fields[j].name)
				{
					printf("%*.*s ", fields[j].length, fields[j].length, buffer + fields[j].offset);
				}
			}
			printf("\n");
		}
		printf("\n");

		// close interfaces
		curs->close(&status);
		curs = NULL;

		stmt->free(&status);
		stmt = NULL;

		meta->release();
		meta = NULL;

		tra->commit(&status);
		tra = NULL;

		att->detach(&status);
		att = NULL;
	}
	catch (const FbException& error)
	{
		// handle error
		rc = 1;

		char buf[256];
		master->getUtilInterface()->formatStatus(buf, sizeof(buf), error.getStatus());
		fprintf(stderr, "%s\n", buf);
	}

	// release interfaces after error caught
	if (meta)
		meta->release();
	if (builder)
		builder->release();
	if (curs)
		curs->release();
	if (stmt)
		stmt->release();
	if (tra)
		tra->release();
	if (att)
		att->release();
	if (tpb)
		tpb->dispose();

	prov->release();
	status.dispose();

	return rc;
}
