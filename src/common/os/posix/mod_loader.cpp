/*
 *	PROGRAM:		JRD Module Loader
 *	MODULE:			mod_loader.cpp
 *	DESCRIPTION:	POSIX-specific class for loadable modules.
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
 *  The Original Code was created by John Bellardo
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 John Bellardo <bellardo at cs.ucsd.edu>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../common/os/mod_loader.h"
#include "../common/os/os_utils.h"
#include "../common/os/path_utils.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <limits.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <link.h>
#include <dlfcn.h>

/// This is the POSIX (dlopen) implementation of the mod_loader abstraction.

class DlfcnModule : public ModuleLoader::Module
{
public:
	DlfcnModule(MemoryPool& pool, const Firebird::PathName& aFileName, void* m)
		: ModuleLoader::Module(pool, aFileName),
		  module(m)
	{}

	~DlfcnModule();
	void* findSymbol(ISC_STATUS*, const Firebird::string&);

	bool getRealPath(Firebird::PathName& realPath);

private:
	void* module;
};

static void makeErrorStatus(ISC_STATUS* status, const char* text)
{
	if (status)
	{
		status[0] = isc_arg_gds;
		status[1] = isc_random;
		status[2] = isc_arg_string;
		status[3] = (ISC_STATUS) text;
		status[4] = isc_arg_end;
	}
}

bool ModuleLoader::isLoadableModule(const Firebird::PathName& module)
{
	struct STAT sb;

	if (-1 == os_utils::stat(module.c_str(), &sb))
		return false;

	if ( ! (sb.st_mode & S_IFREG) )		// Make sure it is a plain file
		return false;

	if ( -1 == access(module.c_str(), R_OK | X_OK))
		return false;

	return true;
}

bool ModuleLoader::doctorModuleExtension(Firebird::PathName& name, int& step)
{
	if (name.isEmpty())
		return false;

	switch (step++)
	{
	case 0: // Step 0: append missing extension
		{
			Firebird::PathName::size_type pos = name.rfind("." SHRLIB_EXT);
			if (pos != name.length() - 3)
			{
				pos = name.rfind("." SHRLIB_EXT ".");
				if (pos == Firebird::PathName::npos)
				{
					name += "." SHRLIB_EXT;
					return true;
				}
			}
			step++; // instead of break
		}
	case 1: // Step 1: insert missing prefix
		{
			Firebird::PathName::size_type pos = name.rfind('/');
			pos = (pos == Firebird::PathName::npos) ? 0 : pos + 1;
			if (name.find("lib", pos) != pos)
			{
				name.insert(pos, "lib");
				return true;
			}
		}
	}
	return false;
}

#ifdef DEV_BUILD
#define FB_RTLD_MODE RTLD_NOW	// make sure nothing left unresolved
#else
#define FB_RTLD_MODE RTLD_LAZY	// save time when loading library
#endif

ModuleLoader::Module* ModuleLoader::loadModule(ISC_STATUS* status, const Firebird::PathName& modPath)
{
	void* module = dlopen(modPath.nullStr(), FB_RTLD_MODE);
	if (module == NULL)
	{
		makeErrorStatus(status, dlerror());
		return 0;
	}

#ifdef DEBUG_THREAD_IN_UNLOADED_LIBRARY
	Firebird::string command;
	command.printf("echo +++ %s +++ >>/tmp/fbmaps;date >> /tmp/fbmaps;cat /proc/%d/maps >>/tmp/fbmaps",
		modPath.c_str(), getpid());
	system(command.c_str());
#endif

	Firebird::PathName linkPath = modPath;
	char b[PATH_MAX];
	const char* newPath = realpath(modPath.c_str(), b);
	if (newPath)
		linkPath = newPath;

	return FB_NEW_POOL(*getDefaultMemoryPool()) DlfcnModule(*getDefaultMemoryPool(), linkPath, module);
}

DlfcnModule::~DlfcnModule()
{
	if (module)
		dlclose(module);
}

void* DlfcnModule::findSymbol(ISC_STATUS* status, const Firebird::string& symName)
{
	void* result = dlsym(module, symName.c_str());
	if (!result)
	{
		Firebird::string newSym ='_' + symName;
		result = dlsym(module, newSym.c_str());
	}

	if (!result)
	{
		makeErrorStatus(status, dlerror());
		return NULL;
	}

#ifdef HAVE_DLADDR
	Dl_info info;
	if (!dladdr(result, &info))
	{
		makeErrorStatus(status, dlerror());
		return NULL;
	}

	const char* errText = "Actual module name does not match requested";
	if (PathUtils::isRelative(fileName) || PathUtils::isRelative(info.dli_fname))
	{
		// check only name (not path) of the library
		Firebird::PathName dummyDir, nm1, nm2;
		PathUtils::splitLastComponent(dummyDir, nm1, fileName);
		PathUtils::splitLastComponent(dummyDir, nm2, info.dli_fname);
		if (nm1 != nm2)
		{
			makeErrorStatus(status, errText);
			return NULL;
		}
	}
	else if (fileName != info.dli_fname)
	{
		makeErrorStatus(status, errText);
		return NULL;
	}
#endif

	return result;
}

bool DlfcnModule::getRealPath(Firebird::PathName& realPath)
{
#ifdef HAVE_DLINFO
	char b[PATH_MAX];

#ifdef HAVE_RTLD_DI_ORIGIN
	if (dlinfo(module, RTLD_DI_ORIGIN, b) == 0)
	{
		realPath = b;
		realPath += '/';
		realPath += fileName;

		if (realpath(realPath.c_str(), b))
		{
			realPath = b;
			return true;
		}
	}
#endif

#ifdef HAVE_RTLD_DI_LINKMAP
	struct link_map* lm;
	if (dlinfo(module, RTLD_DI_LINKMAP, &lm) == 0)
	{
		if (realpath(lm->l_name, b))
		{
			realPath = b;
			return true;
		}
	}
#endif

#endif
	return false;
}
