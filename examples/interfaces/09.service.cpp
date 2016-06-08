/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		09.service.cpp
 *	DESCRIPTION:	A sample of using service manager.
 *					Prints server version and employee's encryption statistics.
 *
 *					Example for the following interfaces:
 *					IService - interface to work with service manager.
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
 *  Copyright (c) 2016 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "ifaceExamples.h"

static IMaster* master = fb_get_master_interface();

// print information, returned by isc_svc_query() call

bool printLine(const unsigned char*& p)
{
	const ISC_USHORT length = (ISC_USHORT) isc_vax_integer((char*)p, sizeof(ISC_USHORT));
	p += sizeof(ISC_USHORT);
	if (length > 0)
		printf("%*.*s\n", length, length, p);
	p += length;
	return length > 0;
}

bool printInfo(const unsigned char* p, size_t pSize)
{
	bool ret = false;
	bool ignoreTruncation = false;
	const unsigned char* const end = p + pSize;

	while (p < end && *p != isc_info_end)
	{
		switch (*p++)
		{
		case isc_info_svc_server_version:
			printf ("Server version: ");
			if (!printLine(p))
				printf ("<no data>\n");
			break;

		case isc_info_svc_line:
			ret = printLine(p);
			break;

		case isc_info_truncated:
			if (!ignoreTruncation)
			{
				printf("\n<<< truncated >>>\n");
			}
			fflush(stdout);
			ret = true;
			break;

		case isc_info_svc_timeout:
		case isc_info_data_not_ready:
			ret = true;
			break;

		default:
			printf("Unknown item 0x%x in received buffer\n", p[-1]);
		}
	}

	fflush(stdout);
	return ret;
}


int main()
{
	int rc = 0;

	// Status wrapper
	ThrowStatusWrapper status(master->getStatus());

	// Declare pointers to required interfaces
	IProvider* prov = master->getDispatcher();
	IUtil* utl = master->getUtilInterface();
	IService* svc = NULL;
	IXpbBuilder* spb1 = NULL;
	IXpbBuilder* spb2 = NULL;

	try {
		printf("** Attaching to service manager...\n");

		// Prepare SPB to attach to service manager
		spb1 = utl->getXpbBuilder(&status, IXpbBuilder::SPB_ATTACH, NULL, 0);
		spb1->insertString(&status, isc_spb_user_name, "sysdba");
		spb1->insertString(&status, isc_spb_password, "masterkey");
		// In case when your program is expected to be used on a server
		// with multiple security database it's very good idea to specify
		// DB expected to be used by services (you anyway need separate
		// services connectons for databases with different user list location).
		spb1->insertString(&status, isc_spb_expected_db, "employee");

		// Attach to service manager
		svc = prov->attachServiceManager(&status, "service_mgr", spb1->getBufferLength(&status),
			spb1->getBuffer(&status));

		printf("** Demo of querying information about server version...\n");

		// In the simplest case sendItems parameter may be NULL
		// Building receiveItems is mostly trivial
		const unsigned char receiveItems[] = {isc_info_svc_server_version};

		// Output buffer
		unsigned char results[1024];

		// Query server version
		svc->query(&status, 0, NULL, sizeof(receiveItems), receiveItems, sizeof(results), results);
		printInfo(results, sizeof(results));

		printf("** Demo of running utility using service manager...\n");

		// Build service start SPB
		spb2 = utl->getXpbBuilder(&status, IXpbBuilder::SPB_START, NULL, 0);
		spb2->insertTag(&status, isc_action_svc_db_stats);
		spb2->insertString(&status, isc_spb_dbname, "employee");
		spb2->insertInt(&status, isc_spb_options, isc_spb_sts_encryption);

		// Start service
		svc->start(&status, spb2->getBufferLength(&status), spb2->getBuffer(&status));

		// Prepare receiveItems block
		const unsigned char receiveItems2[] = {isc_info_svc_line};

		// Query service output
		do
		{
			svc->query(&status, 0, NULL, sizeof(receiveItems2), receiveItems2, sizeof(results), results);
		} while (printInfo(results, sizeof(results)));

		printf("** Detaching from service manager...\n");

		// Detach from service manager
		svc->detach(&status);
		svc = NULL;

		printf("** Done.\n");
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
	if (svc)
		svc->release();

	// generic cleanup
	prov->release();
	status.dispose();
	if (spb1)
		spb1->dispose();
	if (spb2)
		spb2->dispose();

	return rc;
}
