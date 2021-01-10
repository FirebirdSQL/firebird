/*
 *	PROGRAM:		Firebird samples.
 *	MODULE:			CryptApplication.cpp
 *	DESCRIPTION:	Sample of passing a key to crypt plugin
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
 *  Copyright (c) 2012 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "../interfaces/ifaceExamples.h"
#include <firebird/Message.h>

using namespace Firebird;

class CryptKey : public ICryptKeyCallbackImpl<CryptKey, CheckStatusWrapper>
{
public:
	unsigned int callback(unsigned int, const void*, unsigned int length, void* buffer)
	{
		if (length > 0 && buffer)
		{
			char k = 0x5a;
			memcpy(buffer, &k, 1);
			fprintf(stderr, "\nTransfered key to server\n");
		}
		return 1;
	}
};

class App
{
public:
	App() :
		master(fb_get_master_interface()),
		statusWrapper(master->getStatus()), status(&statusWrapper),
		p(NULL), att(NULL), tra(NULL)
	{ }

	~App()
	{
		if (tra)
		{
			tra->rollback(status);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				print("rollback");
				tra->release();
			}
		}
		if (att)
		{
			att->detach(status);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				print("detach");
				att->release();
			}
		}
		if (p)
		{
			p->release();
		}
		status->dispose();
	}

	enum Action {NONE, ENC, DEC, EX_LCL, EX_RMT};
	// Switches/actions have the following meanings:
	// ENC(-e) - encrypt database
	// DEC(-d) - decrypt database
	// EX_LCL(-l) - execute some predefined select command (demonstrates that database can respond to select request)
	// EX_RMT(-r) - execute select using execute statement in remote datasource (demonstrates that dbcrypt key is
	//				passed to target database when using execute statement)

	void execute(const char* dbName, const Action a)
	{
		status->init();

		p = master->getDispatcher();

		p->setDbCryptCallback(status, &key);
		if (status->getState() & IStatus::STATE_ERRORS)
			throw "setDbCryptCallback";

		char s[256];
		sprintf(s, "localhost:%s", dbName);
		att = p->attachDatabase(status, s, 0, NULL);
		if (status->getState() & IStatus::STATE_ERRORS)
			throw "attachDatabase";

		if (a != NONE)
		{
			tra = att->startTransaction(status, 0, NULL);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "startTransaction";
		}

		switch(a)
		{
		case ENC:
			att->execute(status, tra, 0,
				"ALTER DATABASE ENCRYPT WITH \"fbSampleDbCrypt\"", 3, NULL, NULL, NULL, NULL);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "execute";
			break;

		case DEC:
			att->execute(status, tra, 0, "ALTER DATABASE DECRYPT", 3, NULL, NULL, NULL, NULL);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "execute";
			break;

		case EX_LCL:
		case EX_RMT:
		  {
			FB_MESSAGE(Output, CheckStatusWrapper,
				(FB_VARCHAR(31), logon)
			) output(status, master);

			const char* sqlL = "select current_user from rdb$database";
			const char* sqlR = "execute block returns(logon varchar(31)) as begin "
				"execute statement 'select current_user from rdb$database' "
				"on external 'localhost:employee' as user 'test' password 'test' into :logon; "
				"suspend; end";
			const char* sql = a == EX_LCL ? sqlL : sqlR;

			curs = att->openCursor(status, tra, 0, sql, 3, NULL, NULL, output.getMetadata(), NULL, 0);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "openCursor";

			printf("\nExec SQL: %s\nReturns:\n", sql);
			while (curs->fetchNext(status, output.getData()) == IStatus::RESULT_OK)
			{
				unsigned l = output->logonNull ? 0 : output->logon.length;
				printf("%*.*s\n", l, l, output->logon.str);
			}
			printf("done.\n");
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "fetchNext";

			curs->close(status);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "close";
			curs = NULL;
			break;
		  }
		}

		if (tra)
		{
			tra->commit(status);
			if (status->getState() & IStatus::STATE_ERRORS)
				throw "commit";
			tra = NULL;
		}

		printf("\nProviding key for crypt plugin - press enter to continue ...");
		getchar();

		att->detach(status);
		if (status->getState() & IStatus::STATE_ERRORS)
			throw "detach";
		att = NULL;

		p->release();
		p = NULL;
	}

	void print(const char* where)
	{
		fprintf(stderr, "Error in %s: ", where);
		isc_print_status(status->getErrors());
	}

private:
	IMaster* master;
	CheckStatusWrapper statusWrapper;
	CheckStatusWrapper* status;
	IProvider* p;
	IAttachment* att;
	ITransaction* tra;
	IResultSet* curs;

	CryptKey key;
};

int usage()
{
	fprintf(stderr, "Usage: cryptAppSample [ -e | -d | -l | -r ] { db-name }\n");
	return 2;
}

int main(int ac, char** av)
{
	App::Action act = App::NONE;

	if (ac < 2 || ac > 3)
		return usage();

	if (ac == 3)
	{
		if (av[1][0] != '-')
			return usage();

		switch(av[1][1])
		{
		case 'e':
			act = App::ENC;
			break;
		case 'd':
			act = App::DEC;
			break;
		case 'l':
			act = App::EX_LCL;
			break;
		case 'r':
			act = App::EX_RMT;
			break;
		default:
			return usage();
		}
		av++;
	}

#ifdef WIN_NT
	_putenv_s("ISC_USER", "sysdba");
	_putenv_s("ISC_PASSWORD", "masterkey");
#else
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);
#endif

	App app;
	try
	{
		app.execute(av[1], act);
	}
	catch (const char* where)
	{
		app.print(where);
		return 1;
	}

	return 0;
}
