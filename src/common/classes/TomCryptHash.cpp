/*
 *	Hashing using libtomcrypt library.
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2017 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/Hash.h"
#include "../common/dsc.h"
#include "../intl/charsets.h"

#if !defined(__GNUC__) || defined(__clang__)
#define LTC_NO_ASM	// disable ASM in tomcrypt headers
#endif
#include <tomcrypt.h>

using namespace Firebird;


struct LibTomCryptHashContext::Descriptor
{
	const ltc_hash_descriptor* tcDesc;
};

struct LibTomCryptHashContext::State
{
	hash_state tcState;
};


LibTomCryptHashContext::LibTomCryptHashContext(MemoryPool& pool, const Descriptor* aDescriptor)
	: descriptor(aDescriptor),
	  statePtr(FB_NEW_POOL(pool) State),
	  buffer(pool)
{
	descriptor->tcDesc->init(&statePtr->tcState);
}

LibTomCryptHashContext::~LibTomCryptHashContext()
{
	delete statePtr;
}

void LibTomCryptHashContext::update(const void* data, FB_SIZE_T length)
{
	descriptor->tcDesc->process(&statePtr->tcState, static_cast<const UCHAR*>(data), length);
}

void LibTomCryptHashContext::finish(dsc& result)
{
	unsigned char* hashResult = buffer.getBuffer(descriptor->tcDesc->hashsize);
	descriptor->tcDesc->done(&statePtr->tcState, hashResult);
	result.makeText(descriptor->tcDesc->hashsize, CS_BINARY, hashResult);
}


static LibTomCryptHashContext::Descriptor md5Descriptor{&md5_desc};

Md5HashContext::Md5HashContext(MemoryPool& pool)
	: LibTomCryptHashContext(pool, &md5Descriptor)
{
}


static LibTomCryptHashContext::Descriptor sha1Descriptor{&sha1_desc};

Sha1HashContext::Sha1HashContext(MemoryPool& pool)
	: LibTomCryptHashContext(pool, &sha1Descriptor)
{
}


static LibTomCryptHashContext::Descriptor sha256Descriptor{&sha256_desc};

Sha256HashContext::Sha256HashContext(MemoryPool& pool)
	: LibTomCryptHashContext(pool, &sha256Descriptor)
{
}


static LibTomCryptHashContext::Descriptor sha512Descriptor{&sha512_desc};

Sha512HashContext::Sha512HashContext(MemoryPool& pool)
	: LibTomCryptHashContext(pool, &sha512Descriptor)
{
}


struct Crc32HashContext::State
{
	State()
	{
		crc32_init(&ctx);
	}

	crc32_state ctx;
};


Crc32HashContext::Crc32HashContext(MemoryPool& pool)
{
	statePtr = FB_NEW_POOL(pool) State();
}

Crc32HashContext::~Crc32HashContext()
{
	delete statePtr;
}

void Crc32HashContext::update(const void* data, FB_SIZE_T length)
{
	crc32_update(&statePtr->ctx, static_cast<const UCHAR*>(data), length);
}

void Crc32HashContext::finish(dsc& result)
{
	crc32_finish(&statePtr->ctx, &hash, sizeof hash);
	result.makeLong(0, &hash);
}

