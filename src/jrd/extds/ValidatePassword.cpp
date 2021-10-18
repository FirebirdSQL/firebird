/*
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2016 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "fb_exception.h"
#include "iberror.h"

#include "../jrd.h"
#include "../common/isc_f_proto.h"
#include "../common/db_alias.h"
#include "../common/isc_proto.h"
#include "../common/Auth.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/ParsedList.h"


using namespace Jrd;
using namespace Firebird;


namespace {

class DummyCryptKey FB_FINAL :
    public Firebird::AutoIface<Firebird::ICryptKeyImpl<DummyCryptKey, Firebird::CheckStatusWrapper> >
{
public:
	// ICryptKey implementation
	void setSymmetric(Firebird::CheckStatusWrapper* status, const char* type, unsigned keyLength, const void* key)
	{ }

	void setAsymmetric(Firebird::CheckStatusWrapper* status, const char* type, unsigned encryptKeyLength,
		const void* encryptKey, unsigned decryptKeyLength, const void* decryptKey)
	{ }

	const void* getEncryptKey(unsigned* length)
	{
		*length = 0;
		return NULL;
	}

	const void* getDecryptKey(unsigned* length)
	{
		*length = 0;
		return NULL;
	}
};

class SBlock;

class CBlock FB_FINAL : public RefCntIface<IClientBlockImpl<CBlock, CheckStatusWrapper> >
{
public:
	CBlock(const string& p_login, const string& p_password)
		: login(p_login), password(p_password)
	{ }

	// Here we are going to place reference counted object on a stack.
	// Release will never be called, but let's better have safe implementation for it.
	int release() override
	{
		return 1;
	}

	// Firebird::IClientBlock implementation
	const char* getLogin() override
	{
		return login.c_str();
	}

	const char* getPassword() override
	{
		return password.c_str();
	}

	const unsigned char* getData(unsigned int* length) override
	{
		*length = data.getCount();
		return data.begin();
	}

	void putData(CheckStatusWrapper* status, unsigned int length, const void* d) override;

	Firebird::ICryptKey* newKey(Firebird::CheckStatusWrapper* status) override
	{
		return &dummyCryptKey;
	}

	Firebird::IAuthBlock* getAuthBlock(Firebird::CheckStatusWrapper*) override
	{
		return nullptr;
	}

private:
	const string& login;
	const string& password;
	DummyCryptKey dummyCryptKey;
	UCharBuffer data;

	friend class SBlock;
	SBlock* sBlock;
};

class SBlock FB_FINAL : public AutoIface<IServerBlockImpl<SBlock, CheckStatusWrapper> >
{
public:
	explicit SBlock(CBlock* par)
		: cBlock(par)
	{
		cBlock->sBlock = this;
	}

	// Firebird::IServerBlock implementation
	const char* getLogin()
	{
		return cBlock->getLogin();
	}

	const unsigned char* getData(unsigned int* length)
	{
		*length = data.getCount();
		return data.begin();
	}

	void putData(CheckStatusWrapper* status, unsigned int length, const void* d);

	Firebird::ICryptKey* newKey(Firebird::CheckStatusWrapper* status)
	{
		return &dummyCryptKey;
	}

private:
	DummyCryptKey dummyCryptKey;
	UCharBuffer data;

	friend class CBlock;
	CBlock* cBlock;
};

void CBlock::putData(CheckStatusWrapper* status, unsigned int length, const void* d)
{
	void* to = sBlock->data.getBuffer(length);
	memcpy(to, d, length);
}

void SBlock::putData(CheckStatusWrapper* status, unsigned int length, const void* d)
{
	void* to = cBlock->data.getBuffer(length);
	memcpy(to, d, length);
}

} // anonymous namespace


namespace EDS {

void validatePassword(thread_db* tdbb, const PathName& file, ClumpletWriter& dpb)
{
	// Preliminary checks - should we really validate the password ourselves
	if (!dpb.find(isc_dpb_user_name))		// check for user name presence
		return;
	if (ISC_check_if_remote(file, false))	// check for remote connection
		return;
	UserId* usr = tdbb->getAttachment()->att_user;
	if (!usr)
		return;
	if (!usr->usr_auth_block.hasData())		// check for embedded attachment
		return;

	Arg::Gds loginError(isc_login_error);

	// Build list of client/server plugins
	RefPtr<const Config> config;
	PathName list;
	expandDatabaseName(file, list /* unused value */, &config);
	PathName serverList = config->getPlugins(IPluginManager::TYPE_AUTH_SERVER);
	PathName clientList = config->getPlugins(IPluginManager::TYPE_AUTH_CLIENT);
	ParsedList::mergeLists(list, serverList, clientList);

	if (!list.hasData())
	{
		Arg::Gds noPlugins(isc_vld_plugins);
		iscLogStatus(NULL, noPlugins.value());

#ifdef DEV_BUILD
		loginError << noPlugins;
#endif
		loginError.raise();
	}

	// Analyze DPB
	string login;
	if (dpb.find(isc_dpb_user_name))
	{
		dpb.getString(login);
		fb_utils::dpbItemUpper(login);
	}
	string password;
	if (dpb.find(isc_dpb_password))
		dpb.getString(password);

	// Try each plugin from the merged list
	for (GetPlugins<IClient> client(IPluginManager::TYPE_AUTH_CLIENT, config, list.c_str());
		client.hasData(); client.next())
	{
		GetPlugins<IServer> server(IPluginManager::TYPE_AUTH_SERVER, config, client.name());
		if (!server.hasData())
			continue;

		CBlock cBlock(login, password);
		SBlock sBlock(&cBlock);
		Auth::WriterImplementation writer;
		writer.setPlugin(client.name());

		const int MAXLIMIT = 100;
		for (int limit = 0; limit < MAXLIMIT; ++limit)		// avoid endless AUTH_MORE_DATA loop
		{
			LocalStatus ls;
			CheckStatusWrapper s(&ls);
			ISC_STATUS code = isc_login;

			switch (client.plugin()->authenticate(&s, &cBlock))
			{
			case IAuth::AUTH_SUCCESS:
			case IAuth::AUTH_MORE_DATA:
				break;

			case IAuth::AUTH_CONTINUE:
				limit = MAXLIMIT;
				continue;

			case IAuth::AUTH_FAILED:
				if (s.getState() & Firebird::IStatus::STATE_ERRORS)
				{
					iscLogStatus("Authentication failed, client plugin:", &s);
					code = isc_login_error;
				}
				(Arg::Gds(code)
#ifdef DEV_BUILD
								 << Arg::StatusVector(&s)
#endif
								 ).raise();
				break;	// compiler silencer
			}

			switch (server.plugin()->authenticate(&s, &sBlock, &writer))
			{
			case IAuth::AUTH_SUCCESS:
				dpb.deleteWithTag(isc_dpb_user_name);
				dpb.deleteWithTag(isc_dpb_password);
				writer.store(&dpb, isc_dpb_auth_block);
				return;

			case IAuth::AUTH_CONTINUE:
				limit = MAXLIMIT;
				continue;

			case IAuth::AUTH_MORE_DATA:
				break;

			case IAuth::AUTH_FAILED:
				if (s.getState() & Firebird::IStatus::STATE_ERRORS)
				{
					iscLogStatus("Authentication faled, server plugin:", &s);
					code = isc_login_error;
				}
				(Arg::Gds(code)
#ifdef DEV_BUILD
								 << Arg::StatusVector(&s)
#endif
								 ).raise();
				break;	// compiler silencer
			}
		}
	}

	Arg::Gds(isc_login).raise();
}

} // namespace EDS
