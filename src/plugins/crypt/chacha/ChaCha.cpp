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

void tomCheck(int err, const char* text, int specErr = CRYPT_OK,  const char* specText = nullptr)
{
	if (err == CRYPT_OK)
		return;

	string buf;
	if (specText && (err == specErr))
		buf = specText;
	else
		buf.printf("TomCrypt library error %s: %s", text, error_to_string(err));
	(Arg::Gds(isc_random) << buf).raise();
}


class Cipher : public GlobalStorage
{
public:
	Cipher(const unsigned char* key, unsigned int ivlen, const unsigned char* iv)
	{
		tomCheck(chacha_setup(&chacha, key, 32, 20), "initializing CHACHA#20");

		unsigned ctr = 0;
		switch (ivlen)
		{
		case 16:
			ctr = (iv[12] << 24) + (iv[13] << 16) + (iv[14] << 8) + iv[15];
			// fall down...
		case 12:
			tomCheck(chacha_ivctr32(&chacha, iv, 12, ctr), "setting IV for CHACHA#20");
			break;
		case 8:
			tomCheck(chacha_ivctr64(&chacha, iv, 8, 0), "setting IV for CHACHA#20");
			break;
		default:
			(Arg::Gds(isc_random) << "Wrong IV length, need 8, 12 or 16").raise();
			break;
		}
	}

	void transform(unsigned int length, const void* from, void* to)
	{
		unsigned char* t = static_cast<unsigned char*>(to);
		const unsigned char* f = static_cast<const unsigned char*>(from);
		tomCheck(chacha_crypt(&chacha, f, length, t), "processing CHACHA#20",
			CRYPT_OVERFLOW, "Connection broken - internal chacha overflow. Reattach to server to proceed.");
	}

private:
	chacha_state chacha;
};


template <unsigned IV_SIZE>
class ChaCha FB_FINAL : public StdPlugin<IWireCryptPluginImpl<ChaCha<IV_SIZE>, CheckStatusWrapper> >
{
public:
	explicit ChaCha(IPluginConfig*)
		: en(NULL), de(NULL), iv(this->getPool())
	{
		if (IV_SIZE == 16)
		{
			GenerateRandomBytes(iv.getBuffer(16), 12);
			iv[12] = iv[13] = iv[14] = iv[15] = 0;
		}
		else
			GenerateRandomBytes(iv.getBuffer(8), 8);
	}

	// ICryptPlugin implementation
	const char* getKnownTypes(CheckStatusWrapper* status)
	{
		return "Symmetric";
	}

	void setKey(CheckStatusWrapper* status, ICryptKey* key)
	{
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

	void encrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
	{
		try
		{
			en->transform(length, from, to);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void decrypt(CheckStatusWrapper* status, unsigned int length, const void* from, void* to)
	{
		try
		{
			de->transform(length, from, to);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	const unsigned char* getSpecificData(CheckStatusWrapper* status, const char* type, unsigned* len)
	{
		*len = IV_SIZE;

		//WIRECRYPT_DEBUG(fprintf(stderr, "getSpecificData %d\n", *len));
		return iv.begin();
	}

	void setSpecificData(CheckStatusWrapper* status, const char* type, unsigned len, const unsigned char* data)
	{
		//WIRECRYPT_DEBUG(fprintf(stderr, "setSpecificData %d\n", len));
		memcpy(iv.getBuffer(len), data, len);
	}

private:
	Cipher* createCypher(unsigned int l, const void* key)
	{
		if (l < 16)
			(Arg::Gds(isc_random) << "Key too short").raise();

		hash_state md;
		tomCheck(sha256_init(&md), "initializing sha256");
		tomCheck(sha256_process(&md, static_cast<const unsigned char*>(key), l), "processing original key in sha256");
		unsigned char stretched[32];
		tomCheck(sha256_done(&md, stretched), "getting stretched key from sha256");

		return FB_NEW Cipher(stretched, iv.getCount(), iv.begin());
	}

	AutoPtr<Cipher> en, de;
	UCharBuffer iv;
};

SimpleFactory<ChaCha<16> > factory;
SimpleFactory<ChaCha<8> > factory64;

} // anonymous namespace

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(Firebird::IMaster* master)
{
	CachedMasterInterface::set(master);
	PluginManagerInterfacePtr()->registerPluginFactory(IPluginManager::TYPE_WIRE_CRYPT, "ChaCha", &factory);
	PluginManagerInterfacePtr()->registerPluginFactory(IPluginManager::TYPE_WIRE_CRYPT, "ChaCha64", &factory64);
	getUnloadDetector()->registerMe();
}
