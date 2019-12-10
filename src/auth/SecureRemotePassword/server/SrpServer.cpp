/*
 *	PROGRAM:		Firebird authentication.
 *	MODULE:			SrpServer.cpp
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
#include "../common/classes/ClumpletWriter.h"
#include "../auth/SecureRemotePassword/Message.h"
#include "../common/classes/ParsedList.h"

#include "../jrd/constants.h"

using namespace Firebird;

namespace {

GlobalPtr<ConfigKeys> keys;

const unsigned int SZ_LOGIN = 31;

}


namespace Auth {

class SrpServer : public StdPlugin<IServerImpl<SrpServer, CheckStatusWrapper> >
{
public:
	explicit SrpServer(IPluginConfig* par)
		: server(NULL), data(getPool()), account(getPool()),
		  clientPubKey(getPool()), serverPubKey(getPool()),
		  verifier(getPool()), salt(getPool()), sessionKey(getPool()),
		  secDbName(NULL), cryptCallback(NULL)
	{
		LocalStatus ls;
		CheckStatusWrapper s(&ls);
		config.assignRefNoIncr(par->getFirebirdConf(&s));
		check(&s);
	}

	// IServer implementation
	int authenticate(CheckStatusWrapper* status, IServerBlock* sBlock, IWriter* writerInterface);
	void setDbCryptCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback);
    int release();

	~SrpServer()
	{
		delete server;
	}

private:
	RemotePassword* server;
	string data;
	string account;
	string clientPubKey, serverPubKey;
	UCharBuffer verifier;
	string salt;
	UCharBuffer sessionKey;
	RefPtr<IFirebirdConf> config;
	const char* secDbName;
	ICryptKeyCallback* cryptCallback;

protected:
    virtual RemotePassword* remotePasswordFactory() = 0;
};

template <class SHA> class SrpServerImpl FB_FINAL : public SrpServer
{
public:
	explicit SrpServerImpl<SHA>(IPluginConfig* ipc)
		: SrpServer(ipc)
	{}

protected:
    RemotePassword* remotePasswordFactory()
    {
		return FB_NEW RemotePasswordImpl<SHA>;
	}
};

int SrpServer::authenticate(CheckStatusWrapper* status, IServerBlock* sb, IWriter* writerInterface)
{
	try
	{
		if (!server)
		{
			HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP phase1\n"));

			if (!sb->getLogin())
			{
				return AUTH_CONTINUE;
			}

			account = sb->getLogin();

			unsigned int length;
			const unsigned char* val = sb->getData(&length);
			clientPubKey.assign(val, length);
			dumpBin("Srv: clientPubKey", clientPubKey);

			if (!clientPubKey.hasData())
			{
				HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP: empty pubkey AUTH_MORE_DATA\n"));
				return AUTH_MORE_DATA;
			}

			// read salt and verifier from database
			// obviously we need something like attachments cache here
			unsigned int secDbKey = keys->getKey(config, "SecurityDatabase");
			secDbName = config->asString(secDbKey);
			if (!(secDbName && secDbName[0]))
			{
				Arg::Gds(isc_secdb_name).raise();
			}

			DispatcherPtr p;
			IAttachment* att = NULL;
			ITransaction* tra = NULL;
			IStatement* stmt = NULL;

			try
			{
				if (cryptCallback)
				{
					p->setDbCryptCallback(status, cryptCallback);
					status->init();		// ignore possible errors like missing call in provider
				}

				ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);
				dpb.insertByte(isc_dpb_sec_attach, TRUE);
				dpb.insertString(isc_dpb_user_name, DBA_USER_NAME, fb_strlen(DBA_USER_NAME));
				dpb.insertString(isc_dpb_config, ParsedList::getNonLoopbackProviders(secDbName));
				att = p->attachDatabase(status, secDbName, dpb.getBufferLength(), dpb.getBuffer());
				check(status);
				HANDSHAKE_DEBUG(fprintf(stderr, "Srv SRP: attached sec db %s\n", secDbName));

				const UCHAR tpb[] =
				{
					isc_tpb_version1,
					isc_tpb_read,
					isc_tpb_read_committed,
					isc_tpb_rec_version,
					isc_tpb_wait
				};
				tra = att->startTransaction(status, sizeof(tpb), tpb);
				check(status);
				HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP1: started transaction\n"));

				const char* sql =
					"SELECT PLG$VERIFIER, PLG$SALT FROM PLG$SRP WHERE PLG$USER_NAME = ? AND PLG$ACTIVE";
				stmt = att->prepare(status, tra, 0, sql, 3, IStatement::PREPARE_PREFETCH_METADATA);
				if (status->getState() & IStatus::STATE_ERRORS)
				{
					checkStatusVectorForMissingTable(status->getErrors());
					status_exception::raise(status);
				}

				Meta im(stmt, false);
				Message par(im);
				Field<Varying> login(par);
				login = account.c_str();

				Meta om(stmt, true);
				Message dat(om);
				check(status);
				Field<Varying> verify(dat);
				Field<Varying> slt(dat);
				HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP1: Ready to run statement with login '%s'\n", account.c_str()));

				stmt->execute(status, tra, par.getMetadata(), par.getBuffer(),
					dat.getMetadata(), dat.getBuffer());
				check(status);
				HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP1: Executed statement\n"));

				verifier.assign(reinterpret_cast<const UCHAR*>(verify->data), verify->len);
				dumpIt("Srv: verifier", verifier);
				UCharBuffer s;
				s.assign(reinterpret_cast<const UCHAR*>(slt->data), slt->len);
				BigInteger(s).getText(salt);
				dumpIt("Srv: salt", salt);

				stmt->free(status);
				check(status);
				stmt = NULL;

				tra->rollback(status);
				check(status);
				tra = NULL;

				att->detach(status);
				check(status);
				att = NULL;
			}
			catch (const Exception&)
			{
				LocalStatus ls;
				CheckStatusWrapper s(&ls);

				if (stmt) stmt->free(&s);
				if (tra) tra->rollback(&s);
				if (att) att->detach(&s);

				throw;
			}

			server = remotePasswordFactory();
			server->genServerKey(serverPubKey, verifier);

			// Ready to prepare data for client and calculate session key
			data = "";
			fb_assert(salt.length() <= RemotePassword::SRP_SALT_SIZE * 2);
			data += char(salt.length());
			data += char(salt.length() >> 8);
			data.append(salt);
			fb_assert(serverPubKey.length() <= RemotePassword::SRP_KEY_SIZE * 2);
			data += char(serverPubKey.length());
			data += char(serverPubKey.length() >> 8);
			data.append(serverPubKey);
			dumpIt("Srv: serverPubKey", serverPubKey);
			dumpBin("Srv: data", data);
			sb->putData(status, data.length(), data.c_str());
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				return AUTH_FAILED;
			}

			server->serverSessionKey(sessionKey, clientPubKey.c_str(), verifier);
			dumpIt("Srv: sessionKey", sessionKey);
			return AUTH_MORE_DATA;
		}

		unsigned int length;
		const unsigned char* val = sb->getData(&length);
		HANDSHAKE_DEBUG(fprintf(stderr, "Srv: SRP: phase2, data length is %d\n", length));
		string proof;
		proof.assign(val, length);
		BigInteger clientProof(proof.c_str());
		BigInteger serverProof = server->clientProof(account.c_str(), salt.c_str(), sessionKey);
		HANDSHAKE_DEBUG(fprintf(stderr, "Client Proof Received, Length = %d\n", clientProof.length()));
		dumpIt("Srv: Client Proof", clientProof);
		dumpIt("Srv: Server Proof", serverProof);

		if (clientProof == serverProof)
		{
			// put the record into authentication block
			writerInterface->add(status, account.c_str());
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				return AUTH_FAILED;
			}
			writerInterface->setDb(status, secDbName);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				return AUTH_FAILED;
			}

			// output the key
			ICryptKey* cKey = sb->newKey(status);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				return AUTH_FAILED;
			}
			cKey->setSymmetric(status, "Symmetric", sessionKey.getCount(), sessionKey.begin());
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				return AUTH_FAILED;
			}

			return AUTH_SUCCESS;
		}
	}
	catch (const Exception& ex)
	{
		status->init();
		ex.stuffException(status);
		switch(status->getErrors()[1])
		{
		case isc_stream_eof:	// User name not found in security database
			break;
		default:
			return AUTH_FAILED;
		}
	}

	status->init();
	return AUTH_CONTINUE;
}

void SrpServer::setDbCryptCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback)
{
	cryptCallback = callback;
}

int SrpServer::release()
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
	SimpleFactory<SrpServerImpl<Sha1> > factory_sha1;
	SimpleFactory<SrpServerImpl<sha224> > factory_sha224;
	SimpleFactory<SrpServerImpl<sha256> > factory_sha256;
	SimpleFactory<SrpServerImpl<sha384> > factory_sha384;
	SimpleFactory<SrpServerImpl<sha512> > factory_sha512;
}

void registerSrpServer(IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, RemotePassword::plugName, &factory_sha1);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, RemotePassword::pluginName(224).c_str(), &factory_sha224);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, RemotePassword::pluginName(256).c_str(), &factory_sha256);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, RemotePassword::pluginName(384).c_str(), &factory_sha384);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, RemotePassword::pluginName(512).c_str(), &factory_sha512);
}

} // namespace Auth
