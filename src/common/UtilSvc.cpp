/*
 *	PROGRAM:		Firebird utilities interface
 *	MODULE:			UtilSvc.cpp
 *	DESCRIPTION:	Interface making it possible to use same code
 *					as both utility or service
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
 *  Copyright (c) 2007 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "../common/UtilSvc.h"
#include "../common/classes/alloc.h"
#include "../common/StatusArg.h"
#include "iberror.h"

#include <string.h>
#include <stdarg.h>


namespace Firebird {

namespace {
	void outputFile(FILE* f, const void* text, size_t len)
	{
		if (::fwrite(text, 1, len, f) != len)
		{
			// ASF: If the console is configured to UTF-8 (chcp 65001) with TrueType font, the MSVC
			// runtime returns the number of characters (instead of bytes) written and make
			// ferror(stdout) return true. So lets not check for errors here.
#ifndef WIN_NT
			Firebird::system_call_failed::raise("StandaloneUtilityInterface::output()/fwrite()");
#endif
		}
	}

	void outputFile(FILE* std, const char* text)
	{
		outputFile(std, text, strlen(text));
		fflush(std);
	}
}

class StandaloneUtilityInterface : public UtilSvc
{
public:
	StandaloneUtilityInterface(int ac, char** av)
	{
		while (ac--)
		{
			argv.push(*av++);
		}
	}

	void outputVerbose(const char* text) override
	{
		outputFile(usvcDataMode ? stderr : stdout, text);
  	}

	void outputError(const char* text) override
	{
		outputFile(stderr, text);
	}

	void outputData(const void* data, FB_SIZE_T size) override
	{
		fb_assert(usvcDataMode);
		outputFile(stdout, data, size);
	}

	void printf(bool err, const SCHAR* format, ...) override
	{
		va_list arglist;
		va_start(arglist, format);
		int rc = ::vfprintf((usvcDataMode || err) ? stderr : stdout, format, arglist);
		va_end(arglist);

		if (rc < 0)
		{
			system_call_failed::raise("StandaloneUtilityInterface::printf()/vfprintf()");
		}
	}

	void hidePasswd(ArgvType& argv, int pos) override
	{
		const size_t l = strlen(argv[pos]);
		char* data = FB_NEW_POOL(getPool()) char[l + 1];
		memcpy(data, argv[pos], l);
		data[l] = 0;

		// here const-correctness is violated to make the rest 99.9%
		// places of code much more clear
		char* hide = const_cast<char*>(argv[pos]);
		argv[pos] = data;
		memset(hide, '*', l);
	}

    bool isService() override
	{
		return false;
	}

	void checkService() override
	{
		status_exception::raise(Arg::Gds(isc_utl_trusted_switch));
	}

	unsigned int getAuthBlock(const unsigned char** bytes) override
	{
		// Utility has no auth block
		*bytes = NULL;
		return 0;
	}

	// do nothing for non-service
	void started() override { }
	void putLine(char, const char*) override { }
	void putSLong(char, SLONG) override { }
	void putSInt64(char, SINT64) override { }
	void putChar(char, char) override { }
	void putBytes(const UCHAR*, FB_SIZE_T) override { }
	ULONG getBytes(UCHAR*, ULONG) override { return 0; }
	void setServiceStatus(const ISC_STATUS*) override { }
	void setServiceStatus(const USHORT, const USHORT, const MsgFormat::SafeArg&) override { }
    StatusAccessor getStatusAccessor() override { return StatusAccessor(); }
	void fillDpb(ClumpletWriter&) override { }
	bool finished() override { return false; }
	bool utf8FileNames() override { return false; }
	Firebird::ICryptKeyCallback* getCryptCallback() override { return NULL; }
	int getParallelWorkers() override { return 0; };
};


UtilSvc* UtilSvc::createStandalone(int ac, char** av)
{
	return FB_NEW StandaloneUtilityInterface(ac, av);
}

} // namespace Firebird
