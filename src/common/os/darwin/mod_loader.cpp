/*
 *	PROGRAM:		JRD Module Loader
 *	MODULE:			mod_loader.cpp
 *	DESCRIPTION:	Darwin specific class for loadable modules.
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
 *  Paul Beach
 */

#include "firebird.h"

#include "../common/os/mod_loader.h"
#include "../../common.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

/// This is the Darwin implementation of the mod_loader abstraction.

//#define DEBUG_LOADER

class DlfcnModule : public ModuleLoader::Module
{
public:
	DlfcnModule(void* m)
		: module(m)
	{}

	~DlfcnModule();
	void* findSymbol (const Firebird::string&);

private:
	void* module;
};

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

void ModuleLoader::doctorModuleExtension(Firebird::PathName& name)
{
	Firebird::PathName::size_type pos = name.rfind('/');
	pos = (pos == Firebird::PathName::npos) ? 0 : pos + 1;
	if (name.find("lib", pos) != pos)
	{
		name.insert(pos, "lib");
	}

	pos = name.rfind(".dylib");
	if (pos == name.length() - 6)
		return;
	name += ".dylib";
}

#ifdef DEV_BUILD
#define FB_RTLD_MODE RTLD_NOW
#else
#define FB_RTLD_MODE RTLD_LAZY
#endif

ModuleLoader::Module* ModuleLoader::loadModule(ISC_STATUS* status, const Firebird::PathName& modPath)
{
	void* module = dlopen(modPath.c_str(), FB_RTLD_MODE);
	if (module == NULL)
	{
		if (status)
		{
			status[0] = isc_arg_gds;
			status[1] = isc_random;
			status[2] = isc_arg_string;
			status[3] = (ISC_STATUS) dlerror();
			status[4] = isc_arg_end;
		}
		return 0;
	}

	return FB_NEW_POOL(*getDefaultMemoryPool()) DlfcnModule(module);
}

DlfcnModule::~DlfcnModule()
{
	if (module)
		dlclose(module);
}

void* DlfcnModule::findSymbol(const Firebird::string& symName)
{
	void* result = dlsym(module, symName.c_str());
	if (result == NULL)
	{
		Firebird::string newSym ='_' + symName;
		result = dlsym(module, newSym.c_str());
	}
	return result;

}


