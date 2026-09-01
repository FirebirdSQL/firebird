#include "../auth/SecureRemotePassword/srp.h"

using namespace Firebird;
using namespace Firebird::Auth;

template<class SHA>void runTest(int argc, char** argv)
{
	string salt;
#if SRP_DEBUG > 1
	BigInteger s("02E268803000000079A478A700000002D1A6979000000026E1601C000000054F");
#else
	BigInteger s;
	s.random(128);
#endif
	s.getText(salt);

	RemotePassword* server = FB_NEW RemotePasswordImpl<SHA>();
	RemotePassword* client = FB_NEW RemotePasswordImpl<SHA>();

	const char* account = "SYSDBA";
	const char* password = "masterkey";

	UCharBuffer verifier;
	dumpIt("salt", salt);
#if SRP_DEBUG > 0
	fprintf(stderr, "%s %s\n", account, password);
#endif
	server->computeVerifier(account, salt, password).getBytes(verifier);
	dumpIt("verifier", verifier);

	string clientPubKey, serverPubKey;
	client->genClientKey(clientPubKey);
	fprintf(stderr, "C Pub %d\n", clientPubKey.length());
	server->genServerKey(serverPubKey, verifier);
	fprintf(stderr, "S Pub %d\n", serverPubKey.length());

	UCharBuffer key1, key2;
	client->clientSessionKey(key1, account, salt.c_str(), argc > 1 ? argv[1] : password, serverPubKey.c_str());
	server->serverSessionKey(key2, clientPubKey.c_str(), verifier);

	BigInteger cProof = client->clientProof(account, salt.c_str(), key1);
	BigInteger sProof = server->clientProof(account, salt.c_str(), key2);

	printf("Proof length = %d\n",cProof.length());
	printf("%s\n", cProof == sProof ? "OK" : "differ");

}

int main(int argc, char** argv)
{
	runTest<Sha1>(argc,argv);
	runTest<sha224>(argc,argv);
	runTest<sha256>(argc,argv);
	runTest<sha384>(argc,argv);
	runTest<sha512>(argc,argv);
}

