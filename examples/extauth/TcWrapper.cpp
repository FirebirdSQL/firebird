/*
 * Tomcrypt library <= firebird : c++ wrapper.
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

#include "TcWrapper.h"

namespace {
// LTC hack
class GInit
{
public:
	GInit()
	{
		ltc_mp = ltm_desc;
	}
};
GInit gInit;
}

void error(ThrowStatusWrapper* status, const char* text)
{
	if (! status)
		throw text;

	ISC_STATUS_ARRAY v;
	v[0] = isc_arg_gds;
	v[1] = isc_random;
	v[2] = isc_arg_string;
	v[3] = (ISC_STATUS) text;
	v[4] = isc_arg_end;

	throw FbException(status, v);
}

void check(ThrowStatusWrapper* status, int err, const char* text)
{
	if (err == CRYPT_OK)
		return;

	char buf[256];
	snprintf(buf, sizeof(buf), "%s: %s", text, error_to_string(err));
	error(status, buf);
}

unsigned readHexKey(ThrowStatusWrapper* status, const char* hex, unsigned char* key, unsigned bufSize)
{
	unsigned char* k = key;
	const char* const end = hex + strlen(hex) - 1;
	for (const char* s = hex; s < end; s += 2)
	{
		if (k - key >= bufSize)
			break;

		// FF
		char ss[3];
		ss[0] = s[0];
		ss[1] = s[1];
		ss[2] = 0;
		unsigned c = strtoul(ss, NULL, 16);
		if (c > 255)
			error(status, "Key format error");
		*k++ = static_cast<unsigned char>(c);
	}
	return k - key;
}

void PseudoRandom::init(ThrowStatusWrapper* status)
{
	// LTC hack
	ltc_mp = ltm_desc;

	// register yarrow
	index = register_prng(&yarrow_desc);
	if (index == -1)
		error(status, "Error registering PRNG yarrow");

	// setup the PRNG
	check(status, yarrow_start(&state), "Error starting PRNG yarrow");
	check(status, rng_make_prng(64, index, &state, NULL), "Error setting up PRNG yarrow");
}

void PseudoRandom::fini()
{
	yarrow_done(&state);
}

const PseudoRandom::PrngDescriptor* PseudoRandom::getDsc()
{
	return &yarrow_desc;
}

void Hash::init(ThrowStatusWrapper* status, const ltc_hash_descriptor* desc)
{
	// LTC hack
	ltc_mp = ltm_desc;

	/* register SHA256 */
	index = register_hash(desc);
	if (index == -1)
		error(status, "Error registering SHA256");
}

