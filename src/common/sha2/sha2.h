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

#include <string>
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

class sha2_base : public GlobalStorage {
#else
class sha2_base {
#endif
public:
	sha2_base() {}
	virtual ~sha2_base() {}

	virtual const unsigned int get_DigestSize() = 0;
	virtual const unsigned int get_BlockSize() = 0;

	typedef unsigned char uint8;
	typedef unsigned int  uint32;
	typedef unsigned long long uint64;

protected:
	virtual void sha_init() {}
	virtual void sha_update(const unsigned char* message, unsigned int len) = 0;
	virtual void sha_final(unsigned char* digest) = 0;

public:
	void reset() { sha_init(); }

	void process(size_t length, const void* bytes)
	{
		sha_update(static_cast<const unsigned char*>(bytes), length);
	}

	void process(size_t length, const unsigned char* message)
	{
		sha_update(message, length);
	}

	void process(const std::string& str)
	{
		process(str.length(), str.c_str());
	}

	void process(const char* str)
	{
		process(strlen(str), str);
	}

	void getHash(unsigned char* digest);

#ifndef NIST_COMPLIANCY_TESTS
	void process(const UCharBuffer& bytes)
	{
		process(bytes.getCount(), bytes.begin());
	}

	void getHash(UCharBuffer& h);
#endif
};

class sha256 : public sha2_base {
public:
	sha256();
	const unsigned int get_DigestSize() { return SHA256_DIGEST_SIZE; }
	const unsigned int get_BlockSize() { return SHA256_BLOCK_SIZE; }

protected:
	typedef struct {
		unsigned int tot_len;
		unsigned int len;
		unsigned char block[2 * SHA256_BLOCK_SIZE];
		uint32 h[8];
	} sha256_ctx;

private:
	void sha256_init(sha256_ctx* ctx);
	void sha256_update(sha256_ctx* ctx, const unsigned char* message,
                   unsigned int len);
	void sha256_final(sha256_ctx* ctx, unsigned char* digest);

protected:
	sha256_ctx ctx;

	void sha256_transf(sha256_ctx* ctx, const unsigned char* message,
                   unsigned int block_nb);
	void sha_init() { sha256_init(&ctx); }
	void sha_update(const unsigned char* message, unsigned int len) { sha256_update(&ctx,message,len); }
	void sha_final(unsigned char* digest) { sha256_final(&ctx,digest); }
};

class sha224 : public sha256 {
public:
    sha224();
	const unsigned int get_DigestSize() { return SHA224_DIGEST_SIZE; }
	const unsigned int get_BlockSize() { return SHA224_BLOCK_SIZE; }

private:
	typedef sha256_ctx sha224_ctx;
	void sha224_init(sha224_ctx* ctx);
	void sha224_update(sha224_ctx* ctx, const unsigned char* message,
                   unsigned int len);
	void sha224_final(sha224_ctx* ctx, unsigned char* digest);

protected:
	void sha_init() { sha224_init(&ctx); }
	void sha_update(const unsigned char* message, unsigned int len) { sha224_update(&ctx, message, len); }
	void sha_final(unsigned char* digest) { sha224_final(&ctx,digest); }
};

class sha512 : public sha2_base {
public:
	sha512();
	const unsigned int get_DigestSize() { return SHA512_DIGEST_SIZE; }
	const unsigned int get_BlockSize() { return SHA512_BLOCK_SIZE; }

protected:
	typedef struct {
		unsigned int tot_len;
		unsigned int len;
		unsigned char block[2 * SHA512_BLOCK_SIZE];
		uint64 h[8];
	} sha512_ctx;

private:
	void sha512_init(sha512_ctx* ctx);
	void sha512_update(sha512_ctx* ctx, const unsigned char* message,
                   unsigned int len);
	void sha512_final(sha512_ctx* ctx, unsigned char* digest);
protected:

	sha512_ctx ctx;

	void sha512_transf(sha512_ctx* ctx, const unsigned char* message,
                   unsigned int block_nb);
	void sha_init() { sha512_init(&ctx); }
	void sha_update(const unsigned char* message, unsigned int len) { sha512_update(&ctx, message, len); }
	void sha_final(unsigned char* digest) { sha512_final(&ctx, digest); }
};

class sha384 : public sha512 {
public:
	sha384();
	const unsigned int get_DigestSize() { return SHA384_DIGEST_SIZE; }
	const unsigned int get_BlockSize() { return SHA384_BLOCK_SIZE; }

private:
	typedef sha512_ctx sha384_ctx;
	void sha384_init(sha384_ctx* ctx);
	void sha384_update(sha384_ctx* ctx, const unsigned char* message,
                   unsigned int len);
	void sha384_final(sha384_ctx* ctx, unsigned char* digest);

protected:
	void sha_init() { sha384_init(&ctx); }
	void sha_update(const unsigned char* message, unsigned int len) { sha384_update(&ctx,message,len); }
	void sha_final(unsigned char* digest) { sha384_final(&ctx, digest); }
};

} //Firebird

#endif /* !_SHA2_H */
