/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		06.fb_message.cpp
 *	DESCRIPTION:	A sample of using static messages.
 *					Prints user-defined tables with comments.
 *
 *					Example for the following interface:
 *					IDecFloat16 - support for IEEE-754 64-bit decimal float numbers
 *
 *					Example for the following macro:
 *
 *					FB_MESSAGE - defines static messages
 *					C++ specific sample!
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Adriano dos Santos Fernandes.
 * Portions created by the Initial Developer are Copyright (C) 2011 the Initial Developer.
 * All Rights Reserved.
 *
 * Contributor(s):
 *	Alexander Peshkov
 *
 */

#include "ifaceExamples.h"
#include <firebird/Message.h>

static IMaster* master = fb_get_master_interface();
static IDecFloat16* idf16 = NULL;
static IInt128* ii128 = NULL;

int main()
{
	int rc = 0;

	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	ThrowStatusWrapper status(master->getStatus());
	IProvider* prov = master->getDispatcher();

	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	IResultSet* rs = NULL;

	const char* dbName = "employee";

	try
	{
		idf16 = master->getUtilInterface()->getDecFloat16(&status);
		ii128 = master->getUtilInterface()->getInt128(&status);

		att = prov->attachDatabase(&status, dbName, 0, NULL);
		tra = att->startTransaction(&status, 0, NULL);

		// Comment some tables
		att->execute(&status, tra, 0, "comment on table employee is 'Employees'",
			SAMPLES_DIALECT, NULL, NULL, NULL, NULL);
		att->execute(&status, tra, 0, "comment on table customer is 'Customers'",
			SAMPLES_DIALECT, NULL, NULL, NULL, NULL);
		att->execute(&status, tra, 0, "comment on table country is 'Countries and national currencies'",
			SAMPLES_DIALECT, NULL, NULL, NULL, NULL);
		tra->commitRetaining(&status);

		// Print tables list
		FB_MESSAGE(Input, ThrowStatusWrapper,
			(FB_INTEGER, systemFlag)
		) input(&status, master);

		FB_MESSAGE(Output, ThrowStatusWrapper,
			(FB_SMALLINT, relationId)
			(FB_VARCHAR(31), relationName)
			(FB_VARCHAR(100), description)
			(FB_DECFLOAT16, df16)
			(FB_INT128, iHuge)
		) output(&status, master);

		input.clear();
		input->systemFlag = 0;

		rs = att->openCursor(&status, tra, 0,
			"select rdb$relation_id, rdb$relation_name, rdb$description,"
			"    cast (rdb$relation_id as decfloat(16)) * 0.05 as df16,"
			"	 cast (rdb$relation_id as int128) * 212778764464767 as iHuge"
			"  from rdb$relations"
			"  where rdb$system_flag = ?"
			"  order by rdb$relation_id",
			SAMPLES_DIALECT, input.getMetadata(), input.getData(), output.getMetadata(), NULL, 0);

		printf("  ID Name                            datatype-tests (perform some arithmetics)   /comment\n");
		while (rs->fetchNext(&status, output.getData()) == IStatus::RESULT_OK)
		{
			unsigned lRelName = output->relationNameNull ? 0 : output->relationName.length;
			unsigned lDesc = output->descriptionNull ? 0 : output->description.length;
			char t16[IDecFloat16::STRING_SIZE];
			idf16->toString(&status, &output->df16, sizeof(t16), t16);
			char huge[IInt128::STRING_SIZE];
			ii128->toString(&status, &output->iHuge, -3, sizeof(huge), huge);

			printf("%4d %*.*s [Decfloat16:%s Int128:%s] %c%*.*s\n", output->relationId,
				lRelName, lRelName, output->relationName.str,
				t16, huge,
				lDesc ? '/' : ' ',
				lDesc, lDesc, output->description.str);
		}

		rs->close(&status);
		rs = NULL;

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
	if (rs)
		rs->release();
	if (tra)
		tra->release();
	if (att)
		att->release();

	// generic cleanup
	prov->release();
	status.dispose();
}
