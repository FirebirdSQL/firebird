/*
 * RSA key generate.
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

int main(int ac, char** av)
{
	try
	{
		int len = ac > 1 ? atoi(av[1]) : 2048;

		PseudoRandom pseudoRand;
		pseudoRand.init(NULL);

		rsa_key key;
		check(NULL, rsa_make_key(&pseudoRand.state, pseudoRand.index, len / 8, 65537, &key),
			"Error making RSA key");

		unsigned char outbuf[4096];
		unsigned long outlen = sizeof outbuf;
		check(NULL, rsa_export(outbuf, &outlen, PK_PRIVATE, &key),
			"Error exporting private RSA key");

		const char* const file = "fbSampleExtAuth.conf";
		FILE* conf = fopen(file, "w");
		if (!conf)
		{
			perror(file);
			return 1;
		}

		fprintf(conf, "Key = ");
		for (unsigned i = 0; i < outlen; ++i)
			fprintf(conf, "%02x", outbuf[i]);
		fprintf(conf, "\n#IgnoreLogin = No\n#IgnorePassword = No\n");

		if (fclose(conf) != 0)
		{
			perror(file);
			return 1;
		}
		printf("Wrote configuration file %s\n", file);
	}
	catch (const char* message)
	{
		fprintf(stderr, "%s\n", message);
		return 1;
	}

	return 0;
}

