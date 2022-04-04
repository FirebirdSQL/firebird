/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		LegacyServer.cpp
 *	DESCRIPTION:	User information database access
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
 *
 * 2003.02.02 Dmitry Yemanov: Implemented cached security database connection
 */

#include "firebird.h"

#include "ibase.h"
#include "iberror.h"
#include "firebird/Interface.h"

#include "../auth/SecurityDatabase/LegacyServer.h"
#include "../auth/SecurityDatabase/LegacyHash.h"
#include "../auth/SecDbCache.h"
#include "../remote/remot_proto.h"
#include "../jrd/constants.h"
#include "../common/enc_proto.h"
#include "../common/status.h"
#include "../common/classes/init.h"
#include "../common/classes/ClumpletWriter.h"

#include <string.h>

#define PLUG_MODULE 1

using namespace Firebird;

namespace {

// BLR to search database for user name record

const UCHAR PWD_REQUEST[] =
{
	blr_version5,
	blr_begin,
	blr_message, 1, 4, 0,
	blr_long, 0,
	blr_long, 0,
	blr_short, 0,
	blr_text, BLR_WORD(Auth::MAX_LEGACY_PASSWORD_LENGTH + 2),
	blr_message, 0, 1, 0,
	blr_cstring, 129, 0,
	blr_receive, 0,
	blr_begin,
	blr_for,
	blr_rse, 1,
	blr_relation, 9, 'P', 'L', 'G', '$', 'U', 'S', 'E', 'R', 'S', 0,
	blr_first,
	blr_literal, blr_short, 0, 1, 0,
	blr_boolean,
	blr_eql,
	blr_field, 0, 13, 'P', 'L', 'G', '$', 'U', 'S', 'E', 'R', '_', 'N', 'A', 'M', 'E',
	blr_parameter, 0, 0, 0,
	blr_end,
	blr_send, 1,
	blr_begin,
	blr_assignment,
	blr_field, 0, 7, 'P', 'L', 'G', '$', 'G', 'I', 'D',
	blr_parameter, 1, 0, 0,
	blr_assignment,
	blr_field, 0, 7, 'P', 'L', 'G', '$', 'U', 'I', 'D',
	blr_parameter, 1, 1, 0,
	blr_assignment,
	blr_literal, blr_short, 0, 1, 0,
	blr_parameter, 1, 2, 0,
	blr_assignment,
	blr_field, 0, 10, 'P', 'L', 'G', '$', 'P', 'A', 'S', 'S', 'W', 'D',
	blr_parameter, 1, 3, 0,
	blr_end,
	blr_send, 1,
	blr_assignment,
	blr_literal, blr_short, 0, 0, 0,
	blr_parameter, 1, 2, 0,
	blr_end,
	blr_end,
	blr_eoc
};

// Returns data in the following format

struct user_record
{
	SLONG gid;
	SLONG uid;
	SSHORT flag;
	SCHAR password[Auth::MAX_LEGACY_PASSWORD_LENGTH + 2];
};

typedef char user_name[129];

// Transaction parameter buffer

const UCHAR TPB[4] =
{
	isc_tpb_version1,
	isc_tpb_read,
	isc_tpb_concurrency,
	isc_tpb_wait
};

} // anonymous namespace

namespace Auth {

GlobalPtr<PluginDatabases> instances;


class SecurityDatabaseServer final :
	public StdPlugin<IServerImpl<SecurityDatabaseServer, CheckStatusWrapper> >
{
public:
	explicit SecurityDatabaseServer(IPluginConfig* p)
		: iParameter(p)
	{ }

	// IServer implementation
	int authenticate(CheckStatusWrapper* status, IServerBlock* sBlock,
		IWriter* writerInterface);
	void setDbCryptCallback(CheckStatusWrapper*, ICryptKeyCallback*) { }	// ignore

private:
	RefPtr<IPluginConfig> iParameter;
};


class SecurityDatabase : public VSecDb
{
public:
	bool lookup(void* inMsg, void* outMsg) override;

	bool test() override
	{
		return fb_ping(status, &lookup_db) == FB_SUCCESS;
	}

	// This 2 are needed to satisfy temporarily different calling requirements
	static int shutdown(const int, const int, void*)
	{
		return instances->shutdown();
	}
	static void cleanup()
	{
		instances->shutdown();
	}

	SecurityDatabase(const char* secDbName)
		: lookup_db(0), lookup_req(0)
	{
		prepare(secDbName);
	}

private:
	ISC_STATUS_ARRAY status;

	isc_db_handle lookup_db;
	isc_req_handle lookup_req;

	~SecurityDatabase();

	void prepare(const char* secDbName);
	void checkStatus(const char* callName, ISC_STATUS userError = isc_psw_db_error);
};

/******************************************************************************
 *
 *	Private interface
 */

SecurityDatabase::~SecurityDatabase()
{
	// One can get 'invalid object' errors here cause provider
	// may get unloaded before authentication plugin

	if (lookup_req)
	{
		isc_release_request(status, &lookup_req);
		if (status[1] != isc_bad_req_handle)
			checkStatus("isc_release_request", 0);
	}

	if (lookup_db)
	{
		isc_detach_database(status, &lookup_db);
		if (status[1] != isc_bad_db_handle)
			checkStatus("isc_detach_database", 0);
	}
}


void SecurityDatabase::prepare(const char* secureDbName)
{
	if (lookup_db)
	{
		return;
	}

#ifndef PLUG_MODULE
	fb_shutdown_callback(status, shutdown, fb_shut_preproviders, 0);
#endif

	lookup_db = lookup_req = 0;

	// Perhaps build up a dpb
	ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);

	// Attachment is for the security database
	dpb.insertByte(isc_dpb_sec_attach, TRUE);

	// Attach as SYSDBA
	dpb.insertString(isc_dpb_trusted_auth, DBA_USER_NAME, fb_strlen(DBA_USER_NAME));

	// Do not use loopback provider
	dpb.insertString(isc_dpb_config, ParsedList::getNonLoopbackProviders(secureDbName));

	isc_db_handle tempHandle = 0;
	isc_attach_database(status, 0, secureDbName, &tempHandle,
		dpb.getBufferLength(), reinterpret_cast<const char*>(dpb.getBuffer()));
	checkStatus("isc_attach_database", isc_psw_attach);
	lookup_db = tempHandle;

	isc_compile_request(status, &lookup_db, &lookup_req, sizeof(PWD_REQUEST),
		reinterpret_cast<const char*>(PWD_REQUEST));
	if (status[1])
	{
		ISC_STATUS_ARRAY localStatus;
		// ignore status returned in order to keep first error
		isc_detach_database(localStatus, &lookup_db);
	}

	checkStatus("isc_compile_request", isc_psw_attach);
}

void SecurityDatabase::checkStatus(const char* callName, ISC_STATUS userError)
{
	if (status[1] == 0)
		return;

	// suppress throwing errors from destructor which passes userError == 0
	if (!userError)
		return;

	Arg::Gds secDbError(userError);

	string message;
	message.printf("Error in %s() API call when working with legacy security database", callName);
	secDbError << Arg::Gds(isc_random) << message;

	secDbError << Arg::StatusVector(status);
	secDbError.raise();
}

bool SecurityDatabase::lookup(void* inMsg, void* outMsg)
{
	isc_tr_handle lookup_trans = 0;

	isc_start_transaction(status, &lookup_trans, 1, &lookup_db, sizeof(TPB), TPB);
	checkStatus("isc_start_transaction", isc_psw_start_trans);

	isc_start_and_send(status, &lookup_req, &lookup_trans, 0, sizeof(user_name), inMsg, 0);
	checkStatus("isc_start_and_send");

	bool found = false;
	while (true)
	{
		user_record* user = static_cast<user_record*>(outMsg);
		isc_receive(status, &lookup_req, 1, sizeof(user_record), user, 0);
		checkStatus("isc_receive");

		if (!user->flag || status[1])
			break;

		found = true;
	}

	isc_rollback_transaction(status, &lookup_trans);
	checkStatus("isc_rollback_transaction");

	return found;
}


/******************************************************************************
 *
 *	Public interface
 */

int SecurityDatabaseServer::authenticate(CheckStatusWrapper* status, IServerBlock* sBlock,
	IWriter* authBlock)
{
	status->init();

	try
	{
		const char* user = sBlock->getLogin();
		if (!user)
		{
			HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer (nologin) %d\n", IAuth::AUTH_CONTINUE));
			return IAuth::AUTH_CONTINUE;
		}
		string login(user);

		unsigned length;
		const unsigned char* data = sBlock->getData(&length);
		if (!(data && length))
		{
			HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer (nopw) %d\n", IAuth::AUTH_MORE_DATA));
			return IAuth::AUTH_MORE_DATA;
		}

		bool found = false;
		char pw1[MAX_LEGACY_PASSWORD_LENGTH + 1];
		PathName secureDbName;
		{ // instance scope
			// Get database block from cache
			CachedSecurityDatabase::Instance instance;
			instances->getInstance(iParameter, instance);

			secureDbName = instance->secureDbName;
			if (!instance->secDb)
				instance->secDb = FB_NEW SecurityDatabase(instance->secureDbName);

			user_name uname;		// user name buffer
			login.copyTo(uname, sizeof uname);
			user_record user_block;		// user record
			found = instance->secDb->lookup(uname, &user_block);
			fb_utils::copy_terminate(pw1, user_block.password, MAX_LEGACY_PASSWORD_LENGTH + 1);
		}
		if (!found)
		{
			HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer (badlogin) %d\n", IAuth::AUTH_CONTINUE));
			return IAuth::AUTH_CONTINUE;
		}

		string storedHash(pw1, MAX_LEGACY_PASSWORD_LENGTH);
		storedHash.rtrim();
		storedHash.recalculate_length();

		string passwordEnc;
		passwordEnc.assign(data, length);

		string newHash;
		LegacyHash::hash(newHash, login, passwordEnc, storedHash);
		if (newHash != storedHash)
		{
			bool legacyHash = Config::getLegacyHash();
			if (legacyHash)
			{
				newHash.resize(MAX_LEGACY_PASSWORD_LENGTH + 2);
				ENC_crypt(newHash.begin(), newHash.length(), passwordEnc.c_str(), LEGACY_PASSWORD_SALT);
				newHash.recalculate_length();
				newHash.erase(0, 2);
				legacyHash = newHash == storedHash;
			}
			if (!legacyHash)
			{
				HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer (badpw) %d\n", IAuth::AUTH_CONTINUE));
				return IAuth::AUTH_CONTINUE;
			}
		}

		FbLocalStatus s;
		authBlock->add(&s, login.c_str());
		check(&s);
		authBlock->setDb(&s, secureDbName.c_str());
		check(&s);
		HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer (OK) %d\n", IAuth::AUTH_SUCCESS));
		return IAuth::AUTH_SUCCESS;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer: exception status:\n"));
		HANDSHAKE_DEBUG(isc_print_status(status->getErrors()));
		HANDSHAKE_DEBUG(isc_print_status(status->getWarnings()));
		return IAuth::AUTH_FAILED;
	}
}

namespace {
	SimpleFactory<SecurityDatabaseServer> factory;
}

void registerLegacyServer(IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER,
		"Legacy_Auth", &factory);
}

} // namespace Auth


#ifdef PLUG_MODULE

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	CachedMasterInterface::set(master);

	getUnloadDetector()->setCleanup(Auth::SecurityDatabase::cleanup);
	Auth::registerLegacyServer(PluginManagerInterfacePtr());
	getUnloadDetector()->registerMe();
}

#endif // PLUG_MODULE
