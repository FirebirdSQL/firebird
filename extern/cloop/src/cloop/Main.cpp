/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes.
 *
 *  Copyright (c) 2014 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "Lexer.h"
#include "Parser.h"
#include "Expr.h"
#include "Generator.h"
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

using std::unique_ptr;
using std::cerr;
using std::endl;
using std::exception;
using std::string;
using std::runtime_error;


//--------------------------------------


static void run(int argc, const char* argv[])
{
	if (argc < 4)
		throw runtime_error("Invalid command line parameters.");

	string inFilename(argv[1]);
	string outFormat(argv[2]);
	string outFilename(argv[3]);

	Lexer lexer(inFilename);

	Parser parser(&lexer);
	parser.parse();

	unique_ptr<Generator> generator;

	if (outFormat == "c++")
	{
		if (argc < 7)
			throw runtime_error("Invalid command line parameters for C++ output.");

		string headerGuard(argv[4]);
		string className(argv[5]);
		string prefix(argv[6]);

		generator.reset(new CppGenerator(outFilename, prefix, &parser, headerGuard, className));
	}
	else if (outFormat == "c-header")
	{
		if (argc < 6)
			throw runtime_error("Invalid command line parameters for C header output.");

		string headerGuard(argv[4]);
		string prefix(argv[5]);

		generator.reset(new CHeaderGenerator(outFilename, prefix, &parser, headerGuard));
	}
	else if (outFormat == "c-impl")
	{
		if (argc < 6)
			throw runtime_error("Invalid command line parameters for C implementation output.");

		string includeFilename(argv[4]);
		string prefix(argv[5]);

		generator.reset(new CImplGenerator(outFilename, prefix, &parser, includeFilename));
	}
	else if (outFormat == "pascal")
	{
		if (argc < 5)
			throw runtime_error("Invalid command line parameters for Pascal output.");

		string unitName(argv[4]);

		struct pascalSwitch
		{
			const char* sw;
			string val;
		};
		pascalSwitch sw[] = {
			{"--uses", ""}, {"--interfaceFile", ""}, {"--implementationFile", ""},
			{"--exceptionClass", ""}, {"--prefix", ""}, {"--functionsFile", ""},
			{NULL, ""} };

		argv += 5;
		argc -= 5;
		for (; argc >= 2; argc -= 2, argv += 2)
		{
			string key = argv[0];
			bool found = false;
			for (pascalSwitch* cur = sw; cur->sw; ++cur)
			{
				if (cur->sw == key)
				{
					if (!cur->val.empty())
						throw runtime_error("Repeated switch " + key);
					found = true;
					cur->val = argv[1];
					break;
				}
			}
			if (!found)
				throw runtime_error("Unknown switch " + key);
		}

		generator.reset(new PascalGenerator(outFilename, sw[4].val/*prefix*/, &parser, unitName,
			sw[0].val/*additionalUses*/, sw[1].val/*interfaceFile*/,
			sw[2].val/*implementationFile*/, sw[3].val/*exceptionClass*/,
			sw[5].val/*functionsFile*/));
	}
	else
		throw runtime_error("Invalid output format.");

	generator->generate();
}


int main(int argc, const char* argv[])
{
	try
	{
		run(argc, argv);
		return 0;
	}
	catch (exception& e)
	{
		cerr << e.what() << endl;
		return 1;
	}
}
