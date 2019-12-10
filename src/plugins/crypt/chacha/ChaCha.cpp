/*
 *	PROGRAM:		Firebird authentication.
 *	MODULE:			ChaCha.cpp
 *	DESCRIPTION:	ChaCha wire crypt plugin.
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
 *  Copyright (c) 2018 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../common/classes/ImplementHelper.h"
#include "../common/classes/auto.h"
#include <tomcrypt.h>
#include <../common/os/guid.h>

using namespace Firebird;

namespace
{

void tomCheck(int err, const char* text)
{
	if (err == CRYPT_OK)
		return;

	string buf;
	buf.printf("LibTomCrypt error %s: %s", text, error_to_string(err));
	(Arg::Gds(isc_random) << buf).raise();
}


class Cipher : public GlobalStorage
{
public:
	Cipher(unsigned int l, const unsigned char* key, unsigned int ivlen, const unsigned char* iv) throw()
	{
		if (l < 16)
			(Arg::Gds(isc_random) << "Key too short").raise();
		else if (l < 32)
			l = 16;
		else if (l > 32)
			l = 32;

		if (ivlen != 16)
			(Arg::Gds(isc_random) << "Wrong IV length, need 16").raise();

		unsigned ctr = (iv[12] << 24) + (iv[13] << 16) + (iv[14] << 8) + iv[15];
		tomCheck(chacha_setup(&chacha, key, l, 20), "initializing CHACHA#20");
		tomCheck(chacha_ivctr32(&chacha, iv, 12, ctr), "setting IV for CHACHA#20");
	}

	void transform(unsigned int length, const void* from, void* to) throw()
	{
		unsigned char* t = static_cast<unsigned char*>(to);
		const unsigned char* f = static_cast<const unsigned char*>(from);
		tomCheck(chacha_crypt(&chacha, f, length, t), "processing CHACHA#20");
	}

private:
	chacha_state chacha;
};


class ChaCha FB_FINAL : public StdPlugin<IWireCryptPluginImpl<ChaCha, CheckStatusWrapper> >
{
public:
	explicit ChaCha(IPluginConfig*)
		: en(NULL), de(NULL), iv(getPool())
	{ }

	// ICryptPlugin implementation
	const char* getKnownTypes(CheckStatusWrapper* status);
	void setKey(CheckStatusWrapper* status, ICryptKey* key);
	void encrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to);
	void decrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to);
	const unsigned char* getSpecificData(CheckStatusWrapper* status, const char* type, unsigned* len);
	void setSpecificData(CheckStatusWrapper* status, const char* type, unsigned len, const unsigned char* data);
	int release();

private:
	Cipher* createCypher(unsigned int l, const void* key);
	AutoPtr<Cipher> en, de;
	UCharBuffer iv;
};

int ChaCha::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}
	return 1;
}

void ChaCha::setKey(CheckStatusWrapper* status, ICryptKey* key)
{
	status->init();
	try
	{
    	unsigned int l;
		const void* k = key->getEncryptKey(&l);
		en = createCypher(l, k);

	    k = key->getDecryptKey(&l);
		de = createCypher(l, k);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void ChaCha::encrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
{
	status->init();
	en->transform(length, from, to);
}

void ChaCha::decrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
{
	status->init();
	de->transform(length, from, to);
}

Cipher* ChaCha::createCypher(unsigned int l, const void* key)
{
	return FB_NEW Cipher(l, static_cast<const unsigned char*>(key), iv.getCount(), iv.begin());
}

const char* ChaCha::getKnownTypes(CheckStatusWrapper* status)
{
	status->init();
	return "Symmetric";
}

const unsigned char* ChaCha::getSpecificData(CheckStatusWrapper* status, const char*, unsigned* len)
{
	*len = 16;
	GenerateRandomBytes(iv.getBuffer(*len), *len);
	return iv.begin();
}

void ChaCha::setSpecificData(CheckStatusWrapper* status, const char*, unsigned len, const unsigned char* data)
{
	memcpy(iv.getBuffer(len), data, len);
}

SimpleFactory<ChaCha> factory;

} // anonymous namespace

extern "C" void FB_PLUGIN_ENTRY_POINT(Firebird::IMaster* master)
{
	CachedMasterInterface::set(master);
	PluginManagerInterfacePtr()->registerPluginFactory(IPluginManager::TYPE_WIRE_CRYPT, "ChaCha", &factory);
	getUnloadDetector()->registerMe();
}
