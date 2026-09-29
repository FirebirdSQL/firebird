/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		04.print_table.cpp
 *	DESCRIPTION:	Run SELECT statement without parameters.
 *					Use attachment method to open cursor.
 *					Print all fields for selected records in a table.
 *					Learns how to access blob data and use unprepared statement.
 *
 *					Example for the following interfaces:
 *
 *					IAttachment - database attachment
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

struct MyField
{
	const char* name;
	unsigned type, length, offset, null;

	void print(ThrowStatusWrapper* st, IAttachment* att, ITransaction* tra, unsigned char* buf);
};

int main()
{
	int rc = 0;
	char s[100];

	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	ThrowStatusWrapper status(master->getStatus());
	IProvider* prov = master->getDispatcher();

	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	IResultSet* curs = NULL;
	IMessageMetadata* meta = NULL;

	try
	{
		att = prov->attachDatabase(&status, "employee", 0, NULL);
		tra = att->startTransaction(&status, 0, NULL);

		// If we are not going to run same SELECT query many times we may do not prepare it,
		// opening cursor instead with single API call.
		// If statement has input parameters and we know them, appropriate IMetadata may be
		// constructed and passed to openCursor() together with data buffer.
		// We also may provide out format info and coerce data passing appropriate metadata
		// into API call.
		// In this sample we have no input parameters and do not coerce anything - just
		// print what we get from SQL query.
		const char* sql = "select * from rdb$relations where RDB$RELATION_ID < 3 "
														 "or RDB$VIEW_SOURCE is not null";

		// Do not use IStatement - just ask attachment to open cursor
		curs = att->openCursor(&status, tra, 0, sql, SAMPLES_DIALECT, NULL, NULL, NULL, NULL, 0);
		meta = curs->getMetadata(&status);
		unsigned cols = meta->getCount(&status);

		MyField* fields = new MyField[cols];
		memset(fields, 0, sizeof(MyField) * cols);

		unsigned f = 0;
		for (unsigned j = 0; j < cols; ++j)
		{
			unsigned t = meta->getType(&status, j) & ~1;
			unsigned sub = meta->getSubType(&status, j);

			switch (t)
			{
			case SQL_BLOB:
				if (sub != 1)
					continue;
				break;

			case SQL_TEXT:
			case SQL_VARYING:
			case SQL_SHORT:
			case SQL_DOUBLE:
				break;

			default:
				{
					snprintf(s, sizeof(s), "Unknown type %d for %s", t, meta->getField(&status, j));
					throw s;
				}
				continue;
			}

			// we can work with this field - cache metadata info for fast access
			fields[f].type = t;
			fields[f].name = meta->getField(&status, j);
			fields[f].length = meta->getLength(&status, j);
			fields[f].offset = meta->getOffset(&status, j);
			fields[f].null = meta->getNullOffset(&status, j);
			++f;
		}

		unsigned l = meta->getMessageLength(&status);
		unsigned char* buffer = new unsigned char[l];

		// fetch records from cursor
		while (curs->fetchNext(&status, buffer) == IStatus::RESULT_OK)
		{
			for (unsigned j = 0; j < f; ++j)
			{
				// call field's function to print it
				fields[j].print(&status, att, tra, buffer);
			}
			printf("\n");
		}

		curs->close(&status);
		curs = NULL;

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
		fflush(stdout);
		rc = 1;

		char buf[256];
		master->getUtilInterface()->formatStatus(buf, sizeof(buf), error.getStatus());
		fprintf(stderr, "%s\n", buf);
	}

	if (meta)
		meta->release();
	if (curs)
		curs->release();
	if (tra)
		tra->release();
	if (att)
		att->release();

	prov->release();
	status.dispose();

	return rc;
}

template <typename T>
T as(unsigned char* ptr)
{
	return *((T*) ptr);
}

void MyField::print(ThrowStatusWrapper* st, IAttachment* att, ITransaction* tra, unsigned char* buf)
{
	printf("%s: ", name);
	if (as<short>(buf + null))
	{
		printf("<Null>\n");
		return;
	}

	// IBlob makes it possible to read/write BLOB data
	IBlob* blob = NULL;
	switch (type)
	{
	// text fields
	case SQL_TEXT:
		printf("%*.*s\n", length, length, buf + offset);
		break;

	case SQL_VARYING:
		{
			unsigned l = as<short>(buf + offset);
			printf("%*.*s\n", l, l, buf + offset + sizeof(short));
		}
		break;

	// numeric fields
	case SQL_SHORT:
		printf("%d\n", as<short>(buf + offset));
		break;

	case SQL_DOUBLE:
		printf("%f\n", as<double>(buf + offset));
		break;

	// blob requires more handling in DB
	case SQL_BLOB:
		try
		{
			// use attachment's method to access BLOB object
			blob = att->openBlob(st, tra, (ISC_QUAD*) (buf + offset), 0, NULL);

			char segbuf[16];
			unsigned len;
			// read data segment by segment
			for (;;)
			{
				int cc = blob->getSegment(st, sizeof(segbuf), segbuf, &len);
				if (cc != IStatus::RESULT_OK && cc != IStatus::RESULT_SEGMENT)
					break;
				fwrite(segbuf, sizeof(char), len, stdout);
			}

			// close BLOB after receiving all data
			blob->close(st);
			blob = NULL;
			printf("\n");
		}
		catch (...)
		{
			if (blob)
				blob->release();
			throw;
		}
		break;

	// do not plan to have all types here
	default:
		throw "Unknown type in MyField::print()";
	}
}
