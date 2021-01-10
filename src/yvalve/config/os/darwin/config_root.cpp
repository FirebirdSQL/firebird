/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		config_root.cpp
 *	DESCRIPTION:	Configuration manager (platform specific - Darwin)
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "fb_types.h"
#include "../common/classes/fb_string.h"
#include "../yvalve/config/os/config_root.h"
#include "../common/os/path_utils.h"
#include "../common/file_params.h"

#ifndef IOS
#include <CoreServices/CoreServices.h>
#endif
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURL.h>
#include <mach-o/dyld.h>
#include <stdlib.h>

#include <dlfcn.h>

typedef Firebird::PathName string;

static string getFrameworkFromBundle()
{
	// Attempt to locate the Firebird.framework bundle
	CFBundleRef fbFramework = CFBundleGetBundleWithIdentifier(CFSTR(DARWIN_FRAMEWORK_ID));
	if (fbFramework)
	{
		CFURLRef msgFileUrl = CFBundleCopyResourceURL(fbFramework,
			CFSTR(DARWIN_GEN_DIR), NULL, NULL);
		if (msgFileUrl)
		{
			CFStringRef	msgFilePath = CFURLCopyFileSystemPath(msgFileUrl,
				kCFURLPOSIXPathStyle);
			if (msgFilePath)
			{
				char file_buff[MAXPATHLEN];
				if (CFStringGetCString(msgFilePath, file_buff, MAXPATHLEN,
					kCFStringEncodingMacRoman))
				{
					string dir = file_buff;
					dir += PathUtils::dir_sep;
					return dir;
				}
			}
		}
	}

	// No luck
	return "";
}

static string getExecutablePath()
{
	char file_buff[MAXPATHLEN];
	uint32_t bufsize = sizeof(file_buff);
	_NSGetExecutablePath(file_buff, &bufsize);
	char canonic[PATH_MAX];
	if (!realpath(file_buff, canonic))
		Firebird::system_call_failed::raise("realpath");
	string bin_dir = canonic;
	// get rid of the filename
	int index = bin_dir.rfind(PathUtils::dir_sep);
	bin_dir = bin_dir.substr(0, index);
	// go to parent directory
	index = bin_dir.rfind(PathUtils::dir_sep, bin_dir.length());
	string dir = (index ? bin_dir.substr(0,index) : bin_dir) + PathUtils::dir_sep;
	return dir;
}

static string getDlInfoPath()
{
	Dl_info dlInfo;
	static int test = 1;
	if (dladdr(&test, &dlInfo) != 0)		// non-zero is success in dladdr
	{
		string next, dummy;
		PathUtils::splitLastComponent(next, dummy, dlInfo.dli_fname);
		string dir;
		PathUtils::splitLastComponent(dir, dummy, next);
		return dir;
	}
	return "";
}


void ConfigRoot::osConfigRoot()
{
	// Attempt to locate the Firebird.framework bundle
	root_dir = getFrameworkFromBundle();
	if (root_dir.hasData())
	{
		return;
	}

	// Ask dynamic loader to provide info about current dynamic library
	root_dir = getDlInfoPath();
	if (root_dir.hasData())
	{
		return;
	}

	// Use executable file path
	root_dir = getExecutablePath();
	if (root_dir.hasData())
	{
		return;
	}

	// As a last resort get it from the default install directory
	root_dir = FB_PREFIX;
}


void ConfigRoot::osConfigInstallDir()
{
	// Attempt to locate the Firebird.framework bundle
	install_dir = getFrameworkFromBundle();
	if (install_dir.hasData())
	{
		return;
	}

	// Ask dynamic loader to provide info about current dynamic library
	install_dir = getDlInfoPath();
	if (install_dir.hasData())
	{
		return;
	}

	install_dir = getExecutablePath();
	if (install_dir.hasData())
	{
		return;
	}

	// As a last resort get it from the default install directory
	install_dir = FB_PREFIX;
}
