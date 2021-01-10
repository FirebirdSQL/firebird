/*
 *	Simple shared-key based authentication plugin
 *	Each node (firebird server) contains same key which is used to authenticate cross-server
 *	connections. Each connection coming from one node to another has on target same
 *	login as it was on source node.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <memory>
#include <atomic>

#include "TcWrapper.h"

#define HANDSHAKE_DEBUG(A)

const unsigned LOGINSIZE = 128u;
const unsigned RANDSIZE = 32u;
const unsigned SALTLEN = 8u;

typedef unsigned int ULong;

using namespace std;

namespace {

IMaster* master = NULL;

class PluginModule : public IPluginModuleImpl<PluginModule, ThrowStatusWrapper>
{
public:
	PluginModule()
		: pluginManager(NULL)
	{ }

	~PluginModule()
	{
		if (pluginManager)
		{
			pluginManager->unregisterModule(this);
			doClean();
		}
	}

	void registerMe(IPluginManager* m)
	{
		pluginManager = m;
		pluginManager->registerModule(this);
	}

	void doClean()
	{
		pluginManager = NULL;
	}

	void threadDetach()
	{ }

private:
	IPluginManager* pluginManager;
};


template <class P>
class Factory : public IPluginFactoryImpl<Factory<P>, ThrowStatusWrapper>
{
public:
	// IPluginFactory implementation
	IPluginBase* createPlugin(ThrowStatusWrapper* status, IPluginConfig* factoryParameter)
	{
		IPluginBase* p = new P(status, factoryParameter);
		p->addRef();
		return p;
	}
};

//
// Common RSA helper
//

class PluginData
{
public:
	PluginData(ThrowStatusWrapper* status, IPluginConfig* cnf)
		: refCounter(0), owner(NULL), iniLvl(0)
	{
		hash.init(status);
		iniLvl = 1;
		pseudoRand.init(status);
		iniLvl = 2;

		AutoRelease<IConfig> conf(cnf->getDefaultConfig(status));
		if (!conf)
			return;
		AutoRelease<IConfigEntry> ce(conf->find(status, "Key"));
		if (!ce)
			return;

		// import a key
		unsigned char key[4096];
		unsigned keySize = readHexKey(status, ce->getValue(), key, sizeof(key));
		check(status, rsa_import(key, keySize, &privateKey),
			"ExtAuth plugin failed to initialize - error importing private RSA key");
		iniLvl = 3;
	}

	~PluginData()
	{
		if (iniLvl >= 3)
			rsa_free(&privateKey);
		if (iniLvl >= 2)
			pseudoRand.fini();
		if (iniLvl >= 1)
			hash.fini();
	}

protected:
	atomic<int> refCounter;
	IReferenceCounted* owner;

	PseudoRandom pseudoRand;
	HashSha256 hash;
	rsa_key privateKey;
	int iniLvl;
};


//
// Client plugin
//

class ExtAuthClient : public IClientImpl<ExtAuthClient, ThrowStatusWrapper>, public PluginData
{
public:
	ExtAuthClient(ThrowStatusWrapper* status, IPluginConfig* cnf)
		: PluginData(status, cnf),
		  ignorePassword(false),
		  ignoreLogin(false)
	{
		AutoRelease<IConfig> conf(cnf->getDefaultConfig(status));
		if (conf)
		{
			AutoRelease<IConfigEntry> igPass(conf->find(status, "IgnorePassword"));
			if (igPass)
				ignorePassword = igPass->getBoolValue();
			AutoRelease<IConfigEntry> igLgn(conf->find(status, "IgnoreLogin"));
			if (igLgn)
				ignoreLogin = igLgn->getBoolValue();
		}
	}

	// IClient implementation
	int authenticate(ThrowStatusWrapper* status, IClientBlock* cBlock);

	int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

	void addRef()
	{
		++refCounter;
	}

	void setOwner(IReferenceCounted* o)
	{
		owner = o;
	}

	IReferenceCounted* getOwner()
	{
		return owner;
	}

private:
	bool ignorePassword, ignoreLogin;
};

int ExtAuthClient::authenticate(ThrowStatusWrapper* status, IClientBlock* cBlock)
{
	try
	{
		// did we initialize correctly?
		if (iniLvl < 3)
			return AUTH_CONTINUE;

		// check for missing login from the user
		if ((!ignoreLogin) && cBlock->getLogin())
			return AUTH_CONTINUE;

		// check for missing password from the user
		if ((!ignorePassword) && cBlock->getPassword())
			return AUTH_CONTINUE;

		// check for presence of authenticatiion block
		IAuthBlock* authBlock = cBlock->getAuthBlock(status);
		if (!authBlock)
			return AUTH_CONTINUE;
		if (!authBlock->first(status))
			return AUTH_CONTINUE;

		// and for presence of user name in that authenticatiion block
		const char* login = NULL;
		do
		{
			const char* type = authBlock->getType();
			if (type && (strcmp(type, "USER") == 0))
			{
				login = authBlock->getName();
				if (login)
					break;
			}
		} while(authBlock->next(status));
		if (!login)
			return AUTH_CONTINUE;

		// check if server started to talk to us
		unsigned dl = 0;
		const unsigned char* data = cBlock->getData(&dl);
		if (dl == 0 || !data)
			return AUTH_MORE_DATA;

		// decrypt message
		unsigned char bytes[RANDSIZE + LOGINSIZE + 1];
		unsigned long outlen = RANDSIZE;
		int result = 0;
		check(status, rsa_decrypt_key(data, dl, bytes, &outlen, NULL, 0, hash.index, &result, &privateKey),
			"Error decrypting message");
		if (outlen < RANDSIZE)
			error(status, "Malformed data from server - missing random block");

		// next append login to random block
		unsigned len = strlen(login);
		if (len > LOGINSIZE)
			len = LOGINSIZE;
		memcpy(&bytes[RANDSIZE], login, len);

		// calc hash for whole block
		hash_state state;
		sha256_init(&state);
		check(status, sha256_process(&state, bytes, RANDSIZE + len), "Error hashing message");
		unsigned char digest[256 / 8];
		check(status, sha256_done(&state, digest), "Error extracting hash");

		// build message
		unsigned char msg[4096];

		// put login to it
		memcpy(msg, login, len);
		msg[len++] = 0;

		// append sign of hash to it
		unsigned long signLen = sizeof(msg) - len;
		unsigned char* sign = &msg[len];
		check(status, rsa_sign_hash(digest, sizeof digest, sign, &signLen, 	&pseudoRand.state,
			pseudoRand.index, hash.index, SALTLEN, &privateKey), "Error signing message hash");

		// send message
		cBlock->putData(status, len + signLen, msg);

		// output the wire crypt key
		ICryptKey* cKey = cBlock->newKey(status);
		cKey->setSymmetric(status, "Symmetric", RANDSIZE, bytes);
		HANDSHAKE_DEBUG( fprintf(stderr, "Key ="); for (unsigned n = 0; n < RANDSIZE; ++n)
				fprintf(stderr, " %02u", bytes[n]); fprintf(stderr, "\n"); )

		return AUTH_SUCCESS;
	}
	catch(const FbException& ex)
	{
		status->setErrors(ex.getStatus()->getErrors());
		return AUTH_FAILED;
	}
}


//
// Server plugin
//

class ExtAuthServer : public IServerImpl<ExtAuthServer, ThrowStatusWrapper>, public PluginData
{
public:
	ExtAuthServer(ThrowStatusWrapper* status, IPluginConfig* cnf)
		: PluginData(status, cnf), sentData(false)
	{ }

	// IServer implementation
	int authenticate(ThrowStatusWrapper* status, IServerBlock* sBlock, IWriter* writerInterface);

	void setDbCryptCallback(ThrowStatusWrapper* status, ICryptKeyCallback* cryptCallback)
	{ }

	int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

	void addRef()
	{
		++refCounter;
	}

	void setOwner(IReferenceCounted* o)
	{
		owner = o;
	}

	IReferenceCounted* getOwner()
	{
		return owner;
	}

private:
	unsigned char msg[RANDSIZE + LOGINSIZE];
	bool sentData;
};


int ExtAuthServer::authenticate(ThrowStatusWrapper* status, IServerBlock* sBlock, IWriter* writerInterface)
{
	try
	{
		// did we initialize correctly?
		if (iniLvl < 3)
			return AUTH_CONTINUE;

		unsigned dl = 0;
		const unsigned char* data = sBlock->getData(&dl);
		if (!sentData)
		{
			// fbassert(dl == 0 && !data);

			// build message: first of all get some randomness
			pseudoRand.getDsc()->read(msg, RANDSIZE, &pseudoRand.state);

			// now encrypt that random block
			unsigned char encrypted[4096];
			unsigned long encLen = sizeof encrypted;
			check(status, rsa_encrypt_key(msg, RANDSIZE, encrypted, &encLen, NULL, 0,
				&pseudoRand.state, pseudoRand.index, hash.index, &privateKey), "Error encrypting message");

			// send message
			sBlock->putData(status, encLen, encrypted);
			sentData = true;

			return AUTH_MORE_DATA;
		}

		// decompose message
		const char* login = reinterpret_cast<const char*>(data);
		unsigned len = strnlen(login, dl);
		if (len == dl)
			error(status, "Wrong data from client - no signature in a message");
		if (len == 0)
			error(status, "Wrong data from client - empty login");
		if (len > LOGINSIZE)
			error(status, "Wrong data from client - login too long");
		memcpy(&msg[RANDSIZE], data, len);
		const unsigned char* sign = &data[len + 1];
		unsigned long signLen = dl - (len + 1);

		// calc hash for message
		hash_state state;
		sha256_init(&state);
		check(status, sha256_process(&state, msg, RANDSIZE + len), "Error hashing message");
		unsigned char digest[256 / 8];
		check(status, sha256_done(&state, digest), "Error extracting hash");

		// validate signature
		int result = 0;
		int err = rsa_verify_hash(sign, signLen, digest, sizeof digest, hash.index, SALTLEN, &result, &privateKey);
		if (err != CRYPT_INVALID_PACKET)
			check(status, err, "Error verifying digital signature");
		else
			result = 0;
		if (!result)
			error(status, "Malformed data from client - invalid digital signature");

		// output the wire crypt key
		ICryptKey* cKey = sBlock->newKey(status);
		cKey->setSymmetric(status, "Symmetric", RANDSIZE, msg);
		HANDSHAKE_DEBUG( fprintf(stderr, "Key ="); for (unsigned n = 0; n < RANDSIZE; ++n)
				fprintf(stderr, " %02x", msg[n]); fprintf(stderr, "\n"); )

		// store received login name in auth block
		writerInterface->add(status, login);

		return AUTH_SUCCESS;
	}
	catch(const FbException& ex)
	{
		status->setErrors(ex.getStatus()->getErrors());
	}
	return AUTH_FAILED;
}


//
// Static variables
//

PluginModule module;
Factory<ExtAuthClient> clientFactory;
Factory<ExtAuthServer> serverFactory;

} // anonymous namespace


#if defined(_WIN32)
#define FB_DLL_EXPORT __declspec(dllexport)
#else
#define FB_DLL_EXPORT
#endif

extern "C" void FB_DLL_EXPORT FB_PLUGIN_ENTRY_POINT(IMaster* m)
{
	master = m;
	IPluginManager* pluginManager = master->getPluginManager();

	module.registerMe(pluginManager);
	const char* plName = "fbSampleExtAuth";
	pluginManager->registerPluginFactory(IPluginManager::TYPE_AUTH_CLIENT, plName, &clientFactory);
	pluginManager->registerPluginFactory(IPluginManager::TYPE_AUTH_SERVER, plName, &serverFactory);
}
