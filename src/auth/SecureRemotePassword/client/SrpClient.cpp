/*
 *	PROGRAM:		Firebird authentication.
 *	MODULE:			SrpClient.cpp
 *	DESCRIPTION:	SPR authentication plugin.
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
 *  Copyright (c) 2011 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../auth/SecureRemotePassword/client/SrpClient.h"
#include "../auth/SecureRemotePassword/srp.h"
#include "../common/classes/ImplementHelper.h"

using namespace Firebird;

namespace Auth {

class SrpClient : public StdPlugin<IClientImpl<SrpClient, CheckStatusWrapper> >
{
public:
	explicit SrpClient(IPluginConfig*)
		: client(NULL), data(getPool()),
		  sessionKey(getPool())
	{ }

	~SrpClient()
	{
		delete client;
	}

	// IClient implementation
	int authenticate(CheckStatusWrapper*, IClientBlock* cb);
    int release();

private:
	RemotePassword* client;
	string data;
	UCharBuffer sessionKey;

protected:
    virtual RemotePassword* remotePasswordFactory() = 0;
};

template <class SHA> class SrpClientImpl FB_FINAL : public SrpClient
{
public:
	explicit SrpClientImpl<SHA>(IPluginConfig* ipc)
		: SrpClient(ipc)
	{}

protected:
    RemotePassword* remotePasswordFactory()
    {
		return FB_NEW RemotePasswordImpl<SHA>;
	}
};

int SrpClient::authenticate(CheckStatusWrapper* status, IClientBlock* cb)
{
	try
	{
		if (sessionKey.hasData())
		{
			// Why are we called when auth is completed?
			(Arg::Gds(isc_random) << "Auth sync failure - SRP's authenticate called more times than supported").raise();
		}

		if (!client)
		{
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: SRP phase1: login=%s password=%s\n",
				cb->getLogin(), cb->getPassword()));
			if (!(cb->getLogin() && cb->getPassword()))
			{
				return AUTH_CONTINUE;
			}

			client = remotePasswordFactory();
			client->genClientKey(data);
			dumpIt("Clnt: clientPubKey", data);
			cb->putData(status, data.length(), data.begin());

			if (status->getState() & IStatus::STATE_ERRORS)
				return AUTH_FAILED;

			return AUTH_MORE_DATA;
		}

		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: SRP phase2\n"));
		unsigned length;
		const unsigned char* saltAndKey = cb->getData(&length);
		if (!saltAndKey || length == 0)
		{
			Arg::Gds(isc_auth_data).raise();
		}
		const unsigned expectedLength =
			(RemotePassword::SRP_SALT_SIZE + RemotePassword::SRP_KEY_SIZE + 2) * 2;
		if (length > expectedLength)
		{
			(Arg::Gds(isc_auth_datalength) << Arg::Num(length) <<
				Arg::Num(expectedLength) << "data").raise();
		}

		string salt, key;
		unsigned charSize = *saltAndKey++;
		charSize += ((unsigned) *saltAndKey++) << 8;
		if (charSize > RemotePassword::SRP_SALT_SIZE * 2)
		{
			(Arg::Gds(isc_auth_datalength) << Arg::Num(charSize) <<
				Arg::Num(RemotePassword::SRP_SALT_SIZE * 2) << "salt").raise();
		}
		salt.assign(saltAndKey, charSize);
		dumpIt("Clnt: salt", salt);
		saltAndKey += charSize;
		length -= (charSize + 2);

		charSize = *saltAndKey++;
		charSize += ((unsigned) *saltAndKey++) << 8;
		if (charSize != length - 2)
		{
			(Arg::Gds(isc_auth_datalength) << Arg::Num(charSize) <<
				Arg::Num(length - 2) << "key").raise();
		}
		key.assign(saltAndKey, charSize);
		dumpIt("Clnt: key(srvPub)", key);

		dumpIt("Clnt: login", string(cb->getLogin()));
		dumpIt("Clnt: pass", string(cb->getPassword()));
		client->clientSessionKey(sessionKey, cb->getLogin(), salt.c_str(), cb->getPassword(), key.c_str());
		dumpIt("Clnt: sessionKey", sessionKey);

		BigInteger cProof = client->clientProof(cb->getLogin(), salt.c_str(), sessionKey);
		cProof.getText(data);
		dumpIt("Clnt: Client Proof",cProof);

		cb->putData(status, data.length(), data.c_str());
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return AUTH_FAILED;
		}

		// output the key
		ICryptKey* cKey = cb->newKey(status);
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return AUTH_FAILED;
		}
		cKey->setSymmetric(status, "Symmetric", sessionKey.getCount(), sessionKey.begin());
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return AUTH_FAILED;
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return AUTH_FAILED;
	}

	return AUTH_SUCCESS;
}

int SrpClient::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}
	return 1;
}

namespace
{
	SimpleFactory<SrpClientImpl<Sha1> > factory_sha1;
	SimpleFactory<SrpClientImpl<sha224> > factory_sha224;
	SimpleFactory<SrpClientImpl<sha256> > factory_sha256;
	SimpleFactory<SrpClientImpl<sha384> > factory_sha384;
	SimpleFactory<SrpClientImpl<sha512> > factory_sha512;
}

void registerSrpClient(IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, RemotePassword::plugName, &factory_sha1);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, RemotePassword::pluginName(224).c_str(), &factory_sha224);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, RemotePassword::pluginName(256).c_str(), &factory_sha256);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, RemotePassword::pluginName(384).c_str(), &factory_sha384);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, RemotePassword::pluginName(512).c_str(), &factory_sha512);
}

} // namespace Auth
