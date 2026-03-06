#include "../common/BigInteger.h"
#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../common/sha.h"
#include "../common/sha2/sha2.h"

#ifndef AUTH_SRP_SRP_H
#define AUTH_SRP_SRP_H

#define SRP_DEBUG 0		// >0 - prints some debug info
						// >1 - uses consts instead randoms, NEVER use in PRODUCTION!

// for HANDSHAKE_DEBUG
#include "../remote/remot_proto.h"

#include <functional>

namespace Firebird::Auth
{


/*
 * Order of battle for SRP handshake:
 *
 * 													0.  At account creation, the server generates
 * 														a random salt and computes a password
 * 														verifier from the account name, password,
 * 														and salt.
*
 * 		1. Client generates random number
 * 		   as private key, computes public
 * 		   key.
 *
 * 		2. Client sends server the account
 * 		   name and its public key.
 * 													3.  Server receives account name, looks up
 * 														salt and password verifier.  Server
 * 														generates random number as private key.
 * 														Server computes public key from private
 * 														key, account name, verifier, and salt.
 *
 * 													4.  Server sends client public key and salt
 *
 * 		3. Client receives server public
 * 		   key and computes session key
 * 		   from server key, salt, account
 * 		   name, and password.
  * 												5.  Server computes session key from client
 * 														public key, client name, and verifier
 *
 * 		For full details, see http://www.ietf.org/rfc/rfc5054.txt
 *
 */

class RemoteGroup;

template <class SHA> class SecureHash : public SHA
{
public:
	void getInt(BigInteger& hash)
	{
		UCharBuffer tmp;
		SHA::getHash(tmp);
		hash.assign(tmp.getCount(), tmp.begin());
	}

	void processInt(const BigInteger& data)
	{
		UCharBuffer bytes;
		data.getBytes(bytes);
		SHA::process(bytes);
	}

	void processStrippedInt(const BigInteger& data)
	{
		UCharBuffer bytes;
		data.getBytes(bytes);
		if (bytes.getCount())
		{
			unsigned int n = (bytes[0] == 0) ? 1u : 0;
			SHA::process(bytes.getCount() - n, bytes.begin() + n);
		}
	}
};


class RemotePassword : public GlobalStorage
{
private:
	const RemoteGroup*		group;
	Auth::SecureHash<Sha1>	hash;
	BigInteger	privateKey;
	BigInteger	scramble;

protected:
    virtual BigInteger makeProof(const BigInteger n1, const BigInteger n2,
                const char* salt, const UCharBuffer& sessionKey) = 0;

public:
	BigInteger	clientPublicKey;
	BigInteger	serverPublicKey;

public:
	RemotePassword();
	virtual ~RemotePassword();

	static const char* plugName;
	static constexpr unsigned SRP_KEY_SIZE = 128;
	static constexpr unsigned SRP_VERIFIER_SIZE = SRP_KEY_SIZE;
	static constexpr unsigned SRP_SALT_SIZE = 32;

	static string pluginName(unsigned bits);

	BigInteger getUserHash(const char* account,
									 const char* salt,
									 const char* password);
	BigInteger computeVerifier(const string& account,
										 const string& salt,
										 const string& password);
	void genClientKey(string& clientPubKey);
	void genServerKey(string& serverPubKey, const UCharBuffer& verifier);
	void computeScramble();
	void clientSessionKey(UCharBuffer& sessionKey, const char* account,
						  const char* salt, const char* password,
						  const char* serverPubKey);
	void serverSessionKey(UCharBuffer& sessionKey,
						  const char* clientPubKey,
						  const UCharBuffer& verifier);
	BigInteger clientProof(const char* account,
									 const char* salt,
									 const UCharBuffer& sessionKey);
};

template <class SHA> class RemotePasswordImpl : public RemotePassword
{
protected:
	BigInteger makeProof(const BigInteger n1, const BigInteger n2,
                const char* salt, const UCharBuffer& sessionKey)
    {
		Auth::SecureHash<SHA> digest;
		digest.processInt(n1);				// H(prime) ^ H(g)
		digest.processInt(n2);				// H(I)
		digest.process(salt);				// s
		digest.processInt(clientPublicKey);	// A
		digest.processInt(serverPublicKey);	// B
		digest.process(sessionKey);			// K

		BigInteger rc;
		digest.getInt(rc);
		return rc;
	}
};



#if SRP_DEBUG > 0
void dumpIt(const char* name, const BigInteger& bi);
void dumpIt(const char* name, const UCharBuffer& data);
void dumpIt(const char* name, const string& str);
void dumpBin(const char* name, const string& str);
#else
void static inline dumpIt(const char* /*name*/, const BigInteger& /*bi*/) { }
void static inline dumpIt(const char* /*name*/, const UCharBuffer& /*data*/) { }
void static inline dumpIt(const char* /*name*/, const string& /*str*/) { }
void static inline dumpBin(const char* /*name*/, const string& /*str*/) { }
#endif

void checkStatusVectorForMissingTable(const ISC_STATUS* v, std::function<void ()> cleanup = nullptr);


} // namespace Firebird::Auth

#endif // AUTH_SRP_SRP_H
