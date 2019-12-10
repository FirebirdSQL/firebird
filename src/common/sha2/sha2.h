/*
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 02/02/2007
 * Issue date:  04/30/2005
 * https://github.com/ouah/sha2
 *
 * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Updated for use in Firebird by Tony Whyman <tony@mwasoftware.co.uk>
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /*
  * This update is intended to make available the SHA-2 family of message
  * digests as C++ classes for use in Firebird. sha224, sha256, sha384 and
  * sha512 are each implemented as separate classes. The class methods are
  * intended to be as similar as possible to the legacy class sha1 in order
  * to facilitate straightforward replacement.
  *
  * This implementation also comes with a NIST compliancy test for each
  * digest. This is enabled by building with the NIST_COMPLIANCY_TESTS symbol
  * defined.
  */

#ifndef _SHA2_H
#define _SHA2_H

#include <string.h>
#ifndef NIST_COMPLIANCY_TESTS
#include "firebird.h"
#include "../../common/classes/alloc.h"
#include "../../common/classes/array.h"
#include "../../common/classes/fb_string.h"
#include "../../common/utils_proto.h"
#endif


#define SHA224_DIGEST_SIZE ( 224 / 8)
#define SHA256_DIGEST_SIZE ( 256 / 8)
#define SHA384_DIGEST_SIZE ( 384 / 8)
#define SHA512_DIGEST_SIZE ( 512 / 8)
#define SHA_MAX_DIGEST_SIZE SHA512_DIGEST_SIZE

#define SHA256_BLOCK_SIZE  ( 512 / 8)
#define SHA512_BLOCK_SIZE  (1024 / 8)
#define SHA384_BLOCK_SIZE  SHA512_BLOCK_SIZE
#define SHA224_BLOCK_SIZE  SHA256_BLOCK_SIZE

namespace Firebird {

/* This template function provides a simple one line means of computing a SHA-2
 * digest from an arbitrary length message.
 */

template<class SHA>void get_digest(const unsigned char* message, size_t len, unsigned char* digest)
{
	SHA sha;
	sha.process(len, message);
	sha.getHash(digest);
}


#ifndef NIST_COMPLIANCY_TESTS
/* This template class provides a simple one line means of computing a SHA-2
 * digest from an arbitrary length message, and encoding the result in BASE64.
 */
template<class SHA> void hashBased64(Firebird::string& hash, const Firebird::string& data)
{
	SHA digest;
	digest.process(data.length(), data.c_str());
	UCharBuffer b;
	digest.getHash(b);
	fb_utils::base64(hash, b);
}

/* The sha2_base class is an abstract class that is the ancestor for all
 * the SHA-2 classes. It defines all public methods for the classes and
 * a common model of use.
 *
 * When instatiated a SHA-2 class is already initialized for use. The message
 * for which a digest is required is then fed to the class using one of
 * the "process" methods, either as a single action or accumulatively.
 *
 * When the entire message has been input, the resulting digest is returned
 * by a "getHash" method. Calling "getHash" also clears the digest and
 * re-initializes the SHA-2 generator ready to compute a new digest.
 *
 * A SHA-2 generator can be cleared down and re-initialized at any time
 * by calling the "reset" method.
 */

class sha224_traits;
class sha256_traits;
class sha384_traits;
class sha512_traits;

class sha2_types
{
public:
	typedef unsigned char uint8;
	typedef unsigned int  uint32;
	typedef unsigned long long uint64;
};

template<class SHA_TRAITS>
class sha2_base : public GlobalStorage {
#else
class sha2_base {
#endif
private:
	sha2_base(const sha2_base&);
	sha2_base& operator = (const sha2_base&);

public:
	sha2_base()
	{
		SHA_TRAITS::sha_init(&m_ctx);
	}

	virtual ~sha2_base() {};

public:
	void reset()
	{
		SHA_TRAITS::sha_init(&m_ctx);
	}

	void process(size_t length, const void* bytes)
	{
		SHA_TRAITS::sha_update(&m_ctx, static_cast<const unsigned char*>(bytes), length);
	}

	void process(size_t length, const unsigned char* message)
	{
		SHA_TRAITS::sha_update(&m_ctx, message, length);
	}

	void process(const char* str)
	{
		SHA_TRAITS::sha_update(&m_ctx, reinterpret_cast<const unsigned char*>(str), strlen(str));
	}

	void getHash(unsigned char* digest)
	{
		SHA_TRAITS::sha_final(&m_ctx, digest);
		SHA_TRAITS::sha_init(&m_ctx);
	}

#ifndef NIST_COMPLIANCY_TESTS
	void process(const UCharBuffer& bytes)
	{
		SHA_TRAITS::sha_update(&m_ctx, bytes.begin(), bytes.getCount());
	}

	void getHash(UCharBuffer& h)
	{
		SHA_TRAITS::sha_final(&m_ctx, h.getBuffer(SHA_TRAITS::get_DigestSize()));
		SHA_TRAITS::sha_init(&m_ctx);
	}
#endif

private:
	typename SHA_TRAITS::sha_ctx m_ctx;
};

typedef sha2_base<sha224_traits> sha224;
typedef sha2_base<sha256_traits> sha256;
typedef sha2_base<sha384_traits> sha384;
typedef sha2_base<sha512_traits> sha512;

struct sha256_ctx {
	unsigned int tot_len;
	unsigned int len;
	unsigned char block[2 * SHA256_BLOCK_SIZE];
	sha2_types::uint32 h[8];

	void transf(const unsigned char* message, unsigned int block_nb);
};

class sha256_traits: private sha2_types {
public:
	typedef sha256_ctx sha_ctx;

public:
	static unsigned int get_DigestSize() {return SHA256_DIGEST_SIZE;};

	static void sha_init(sha_ctx* ctx);

	static void sha_update(sha_ctx* ctx, const unsigned char* message, unsigned int len);

	static void sha_final(sha_ctx *ctx, unsigned char* digest);
};

class sha224_traits: private sha2_types {
public:
	typedef sha256_ctx sha_ctx;

public:
	static unsigned int get_DigestSize() {return SHA224_DIGEST_SIZE;};

	static void sha_init(sha_ctx* ctx);

	static void sha_update(sha_ctx* ctx, const unsigned char* message, unsigned int len);

	static void sha_final(sha_ctx* ctx, unsigned char* digest);
};

struct sha512_ctx{
	unsigned int tot_len;
	unsigned int len;
	unsigned char block[2 * SHA512_BLOCK_SIZE];
	sha2_types::uint64 h[8];

	void transf(const unsigned char* message, unsigned int block_nb);

};

class sha512_traits: private sha2_types {
public:
	typedef sha512_ctx sha_ctx;

public:
	static unsigned int get_DigestSize() {return SHA512_DIGEST_SIZE;};

	static void sha_init(sha_ctx* ctx);

	static void sha_update(sha_ctx* ctx, const unsigned char* message, unsigned int len);

	static void sha_final(sha_ctx* ctx, unsigned char* digest);
};

class sha384_traits: private sha2_types {
public:
	typedef sha512_ctx sha_ctx;

public:
	static unsigned int get_DigestSize() {return SHA384_DIGEST_SIZE;};

	static void sha_init(sha_ctx* ctx);

	static void sha_update(sha_ctx* ctx, const unsigned char* message, unsigned int len);

	static void sha_final(sha_ctx* ctx, unsigned char* digest);
};

} //Firebird

#endif /* !_SHA2_H */
