/*
 *	PROGRAM:		Firebird samples.
 *	MODULE:			DbCrypt.cpp
 *	DESCRIPTION:	Sample of how diskcrypt may be written.
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

using namespace Firebird;

namespace
{

class PluginModule : public IPluginModuleImpl<PluginModule, CheckStatusWrapper>
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

	void threadDetach() {}

private:
	IPluginManager* pluginManager;
};

class DbCrypt : public IDbCryptPluginImpl<DbCrypt, CheckStatusWrapper>
{
public:
	explicit DbCrypt(IPluginConfig* cnf) throw()
		: config(cnf), key(0), refCounter(0), owner(NULL)
	{
		config->addRef();
	}

	~DbCrypt()
	{
		config->release();
	}

	// ICryptPlugin implementation
	void encrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to);
	void decrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to);
	void setKey(CheckStatusWrapper* status, unsigned int length, IKeyHolderPlugin** sources,
		const char* keyName);

	// One is free to ignore passed info when not needed
	void setInfo(CheckStatusWrapper* status, IDbCryptInfo* info)
	{
		// You may uncomment next line in a case of embedded connection
		// fprintf(stderr, "DbInfo: name is %s\n", info->getDatabaseFullPath(status));
	}

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
	IPluginConfig* config;
	char savedKeyName[32];
	ISC_UCHAR key;

	FbSampleAtomic refCounter;
	IReferenceCounted* owner;

	void noKeyError(CheckStatusWrapper* status);
};

void DbCrypt::noKeyError(CheckStatusWrapper* status)
{
	char msg[100];
	strcpy(msg, "Crypt key ");
	if (savedKeyName[0])
	{
		strcat(msg, savedKeyName);
		strcat(msg, " ");
	}
	strcat(msg, "not set");

	ISC_STATUS_ARRAY vector;
	vector[0] = isc_arg_gds;
	vector[1] = isc_random;
	vector[2] = isc_arg_string;
	vector[3] = (ISC_STATUS) msg;
	vector[4] = isc_arg_end;
	status->setErrors(vector);
}

void DbCrypt::encrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
{
	status->init();

	if (!key)
	{
		noKeyError(status);
		return;
	}

	const ISC_UCHAR* f = static_cast<const ISC_UCHAR*>(from);
	ISC_UCHAR* t = static_cast<ISC_UCHAR*>(to);

	while (length--)
	{
		*t++ = (*f++) ^ key;
	}
}

void DbCrypt::decrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
{
	status->init();

	if (!key)
	{
		noKeyError(status);
		return;
	}

	const ISC_UCHAR* f = static_cast<const ISC_UCHAR*>(from);
	ISC_UCHAR* t = static_cast<ISC_UCHAR*>(to);

	while (length--)
	{
		*t++ = (*f++) ^ key;
	}
}

void DbCrypt::setKey(CheckStatusWrapper* status, unsigned int length, IKeyHolderPlugin** sources,
	const char* keyName)
{
	status->init();

	if (key != 0)
		return;

	strncpy(savedKeyName, (keyName ? keyName : ""), sizeof(savedKeyName));
	savedKeyName[sizeof(savedKeyName) - 1] = 0;

	IConfig* def = config->getDefaultConfig(status);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return;

	IConfigEntry* confEntry = def->find(status, "Auto");
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
	{
		def->release();
		return;
	}

	if (confEntry)
	{
		char v = *(confEntry->getValue());
		confEntry->release();
		if (v == '1' || v == 'y' || v == 'Y' || v == 't' || v == 'T')
		{
			confEntry = def->find(status, "Value");
			def->release();
			if (confEntry)
			{
				v = confEntry->getIntValue();
				confEntry->release();
				if (v)
				{
					key = v;
					return;
				}
			}
			key = 0x5a;
			return;
		}
		def->release();
	}

	for (unsigned n = 0; n < length; ++n)
	{
		ICryptKeyCallback* callback = sources[n]->keyHandle(status, savedKeyName);
		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
			return;

		if (callback && callback->callback(0, NULL, 1, &key) == 1)
			return;
	}

	key = 0;
	noKeyError(status);
}

class Factory : public IPluginFactoryImpl<Factory, CheckStatusWrapper>
{
public:
	IPluginBase* createPlugin(CheckStatusWrapper* status, IPluginConfig* factoryParameter)
	{
/*
// ***	Uncomment this 2 lines to see how plugin creation errors are handled
	    const ISC_STATUS_ARRAY vector = {isc_arg_gds, isc_virmemexh, isc_arg_end};
		throw FbException(status, vector);
 */
		DbCrypt* p = new DbCrypt(factoryParameter);
		p->addRef();
		return p;
	}
};

PluginModule module;
Factory factory;

} // anonymous namespace

extern "C" void FB_DLL_EXPORT FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	IPluginManager* pluginManager = master->getPluginManager();

	module.registerMe(pluginManager);
	pluginManager->registerPluginFactory(IPluginManager::TYPE_DB_CRYPT, "fbSampleDbCrypt", &factory);
}
