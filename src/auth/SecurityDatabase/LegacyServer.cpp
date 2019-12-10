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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ibase.h"
#include "../auth/SecurityDatabase/LegacyServer.h"
#include "../auth/SecurityDatabase/LegacyHash.h"
#include "../common/enc_proto.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/scl.h"
#include "../common/config/config.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/init.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/ParsedList.h"
#include "firebird/Interface.h"

#include "../remote/remot_proto.h"

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

class SecurityDatabaseServer FB_FINAL :
	public StdPlugin<IServerImpl<SecurityDatabaseServer, CheckStatusWrapper> >
{
public:
	explicit SecurityDatabaseServer(Firebird::IPluginConfig* p)
		: iParameter(p)
	{ }

	// IServer implementation
	int authenticate(Firebird::CheckStatusWrapper* status, Firebird::IServerBlock* sBlock,
		Firebird::IWriter* writerInterface);
	void setDbCryptCallback(Firebird::CheckStatusWrapper*, Firebird::ICryptKeyCallback*) { }	// ignore
	int release();

private:
	Firebird::RefPtr<Firebird::IPluginConfig> iParameter;
};

class SecurityDatabase FB_FINAL : public RefCntIface<ITimerImpl<SecurityDatabase, CheckStatusWrapper> >
{
public:
	int verify(IWriter* authBlock, IServerBlock* sBlock);

	// This 2 are needed to satisfy temporarily different calling requirements
	static int shutdown(const int, const int, void*)
	{
		return shutdown();
	}
	static void cleanup()
	{
		shutdown();
	}

	static int shutdown();

	char secureDbName[MAXPATHLEN];

	SecurityDatabase()
		: lookup_db(0), lookup_req(0)
	{
	}

	// ITimer implementation
	void handler();

	int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}

		return 1;
	}

private:
	Firebird::Mutex mutex;

	ISC_STATUS_ARRAY status;

	isc_db_handle lookup_db;
	isc_req_handle lookup_req;

	~SecurityDatabase();

	bool lookup_user(const char*, char*);
	void prepare();
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

bool SecurityDatabase::lookup_user(const char* user_name, char* pwd)
{
	bool found = false;		// user found flag
	char uname[129];		// user name buffer
	user_record user;		// user record

	// Start by clearing the output data

	if (pwd)
		*pwd = '\0';

	fb_utils::copy_terminate(uname, user_name, sizeof uname);

	MutexLockGuard guard(mutex, FB_FUNCTION);

	// Attach database and compile request

	prepare();

	// Lookup

	isc_tr_handle lookup_trans = 0;

	isc_start_transaction(status, &lookup_trans, 1, &lookup_db, sizeof(TPB), TPB);
	checkStatus("isc_start_transaction", isc_psw_start_trans);

	isc_start_and_send(status, &lookup_req, &lookup_trans, 0, sizeof(uname), uname, 0);
	checkStatus("isc_start_and_send");

	while (true)
	{
		isc_receive(status, &lookup_req, 1, sizeof(user), &user, 0);
		checkStatus("isc_receive");

		if (!user.flag || status[1])
			break;

		found = true;

		if (pwd)
		{
			fb_utils::copy_terminate(pwd, user.password, MAX_LEGACY_PASSWORD_LENGTH + 1);
		}
	}

	isc_rollback_transaction(status, &lookup_trans);
	checkStatus("isc_rollback_transaction");

	return found;
}

void SecurityDatabase::prepare()
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
	ClumpletWriter dpb(ClumpletReader::Tagged, MAX_DPB_SIZE, isc_dpb_version1);

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

/******************************************************************************
 *
 *	Public interface
 */

int SecurityDatabase::verify(IWriter* authBlock, IServerBlock* sBlock)
{
	const char* user = sBlock->getLogin();
	string login(user ? user : "");

	unsigned length;
	const unsigned char* data = sBlock->getData(&length);
	string passwordEnc;
	if (data)
	{
		passwordEnc.assign(data, length);
	}

	if (!login.hasData())
	{
		return IAuth::AUTH_CONTINUE;
	}
	if (!passwordEnc.hasData())
	{
		return IAuth::AUTH_MORE_DATA;
	}

	// Look up the user name in the userinfo database and use the parameters
	// found there. This means that another database must be accessed, and
	// that means the current context must be saved and restored.

	char pw1[MAX_LEGACY_PASSWORD_LENGTH + 1];
	if (!lookup_user(login.c_str(), pw1))
	{
		return IAuth::AUTH_CONTINUE;
	}
	pw1[MAX_LEGACY_PASSWORD_LENGTH] = 0;
	string storedHash(pw1, MAX_LEGACY_PASSWORD_LENGTH);
	storedHash.rtrim();
	storedHash.recalculate_length();

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
			return IAuth::AUTH_CONTINUE;
		}
	}

	LocalStatus ls;
	CheckStatusWrapper s(&ls);
	authBlock->add(&s, login.c_str());
	check(&s);
	authBlock->setDb(&s, secureDbName);
	check(&s);
	return IAuth::AUTH_SUCCESS;
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

typedef HalfStaticArray<SecurityDatabase*, 4> InstancesArray;
GlobalPtr<InstancesArray> instances;
GlobalPtr<Mutex> instancesMutex;

void SecurityDatabase::handler()
{
	try
	{
		MutexLockGuard g(instancesMutex, FB_FUNCTION);

		InstancesArray& curInstances(instances);
		for (unsigned int i = 0; i < curInstances.getCount(); ++i)
		{
			if (curInstances[i] == this)
			{
				curInstances.remove(i);
				release();
				break;
			}
		}
	}
	catch (Exception &ex)
	{
 		StaticStatusVector st;
 		ex.stuffException(st);
		const ISC_STATUS* status = st.begin();
		if (status[0] == 1 && status[1] != isc_att_shutdown)
		{
			iscLogStatus("Legacy security database timer handler", status);
		}
	}
}

int SecurityDatabase::shutdown()
{
	try
	{
		MutexLockGuard g(instancesMutex, FB_FUNCTION);
		InstancesArray& curInstances(instances);
		for (unsigned int i = 0; i < curInstances.getCount(); ++i)
		{
			if (curInstances[i])
			{
				LocalStatus ls;
				CheckStatusWrapper s(&ls);
				TimerInterfacePtr()->stop(&s, curInstances[i]);
				check(&s);
				curInstances[i]->release();
				curInstances[i] = NULL;
			}
		}
		curInstances.clear();
	}
	catch (Exception &ex)
	{
 		StaticStatusVector st;
 		ex.stuffException(st);
		const ISC_STATUS* status = st.begin();
		if (status[0] == 1 && status[1] != isc_att_shutdown)
		{
			iscLogStatus("Legacy security database shutdown", status);
		}

		return FB_FAILURE;
	}

	return FB_SUCCESS;
}

static Firebird::GlobalPtr<Firebird::ConfigKeys> keys;

int SecurityDatabaseServer::authenticate(Firebird::CheckStatusWrapper* status, IServerBlock* sBlock,
	IWriter* writerInterface)
{
	status->init();

	try
	{
		PathName secDbName;
		{ // config scope
			LocalStatus ls;
			CheckStatusWrapper s(&ls);
			RefPtr<IFirebirdConf> config(REF_NO_INCR, iParameter->getFirebirdConf(&s));
			check(&s);

			unsigned int secDbKey = keys->getKey(config, "SecurityDatabase");
			const char* tmp = config->asString(secDbKey);
			if (!tmp)
			{
				Arg::Gds(isc_secdb_name).raise();
			}

			secDbName = tmp;
		}

		RefPtr<SecurityDatabase> instance;

		{ // guard scope
			MutexLockGuard g(instancesMutex, FB_FUNCTION);
			InstancesArray& curInstances(instances);
			for (unsigned int i = 0; i < curInstances.getCount(); ++i)
			{
				if (secDbName == curInstances[i]->secureDbName)
				{
					instance = curInstances[i];
					break;
				}
			}

			if (!instance)
			{
				instance = FB_NEW SecurityDatabase;
				instance->addRef();
				secDbName.copyTo(instance->secureDbName, sizeof(instance->secureDbName));
				curInstances.add(instance);
			}
		}

		int rc = instance->verify(writerInterface, sBlock);
#define USE_ATT_RQ_CACHE
#ifdef USE_ATT_RQ_CACHE
		LocalStatus ls;
		CheckStatusWrapper s(&ls);
		TimerInterfacePtr()->start(&s, instance, 10 * 1000 * 1000);
		if (s.getState() & IStatus::STATE_ERRORS)
			instance->handler();
#else
		instance->handler();
#endif
		HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer: verify=%d\n", rc));
		return rc;
	}
	catch (const Firebird::Exception& ex)
	{
		ex.stuffException(status);
		HANDSHAKE_DEBUG(fprintf(stderr, "LegacyServer: exception status:\n"));
		HANDSHAKE_DEBUG(isc_print_status(status->getErrors()));
		HANDSHAKE_DEBUG(isc_print_status(status->getWarnings()));
		return IAuth::AUTH_FAILED;
	}
}

int SecurityDatabaseServer::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}

	return 1;
}

namespace {
	Firebird::SimpleFactory<SecurityDatabaseServer> factory;
}

void registerLegacyServer(Firebird::IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(Firebird::IPluginManager::TYPE_AUTH_SERVER,
		"Legacy_Auth", &factory);
}

} // namespace Auth


#ifdef PLUG_MODULE

extern "C" void FB_EXPORTED FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	CachedMasterInterface::set(master);

	getUnloadDetector()->setCleanup(Auth::SecurityDatabase::cleanup);
	Auth::registerLegacyServer(PluginManagerInterfacePtr());
	getUnloadDetector()->registerMe();
}

#endif // PLUG_MODULE
