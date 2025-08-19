/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ConfigCache.cpp
 *	DESCRIPTION:	Base for class, representing cached config file.
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "../common/config/ConfigCache.h"
#include "../common/config/config_file.h"
#include "../common/os/os_utils.h"

#include "iberror.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN_NT
#include <unistd.h>
#else
#include <errno.h>
#endif

using namespace Firebird;

ConfigCache::ConfigCache(MemoryPool& p, const PathName& fName)
	: PermanentStorage(p), files(FB_NEW_POOL(getPool()) ConfigCache::File(getPool(), fName))
{ }

ConfigCache::~ConfigCache()
{
	delete files;
}

void ConfigCache::checkLoadConfig()
{
	{	// scope
		ReadLockGuard guard(rwLock, "ConfigCache::checkLoadConfig");
		if (files->checkLoadConfig(false))
		{
			return;
		}
	}

	WriteLockGuard guard(rwLock, "ConfigCache::checkLoadConfig");
	// may be someone already reloaded?
	if (files->checkLoadConfig(true))
	{
		return;
	}

	files->trim();
	loadConfig();
}

bool ConfigCache::addFile(const Firebird::PathName& fName)
{
	return files->add(fName);
}

Firebird::PathName ConfigCache::getFileName()
{
	return files->fileName;
}

#ifdef WIN_NT
void ConfigCache::File::getTime(DWORD& timeLow, DWORD& timeHigh)
{
	WIN32_FILE_ATTRIBUTE_DATA fInfo;

	if (!GetFileAttributesEx(fileName.c_str(), GetFileExInfoStandard, &fInfo))
	{
		timeLow = 0;
		timeHigh = 0;
		return;
	}

	timeLow = fInfo.ftLastWriteTime.dwLowDateTime;
	timeHigh = fInfo.ftLastWriteTime.dwHighDateTime;
}
#else
void ConfigCache::File::getTime(timespec& time)
{
	struct STAT st;

	if (os_utils::stat(fileName.c_str(), &st) != 0)
	{
		if (errno == ENOENT)
		{
			// config file is missing, but this is not our problem
			time.tv_sec = 0;
			time.tv_nsec = 0;
			return;
		}
		system_call_failed::raise("stat");
	}

#ifdef DARWIN
	time.tv_sec = st.st_mtimespec.tv_sec;
	time.tv_nsec = st.st_mtimespec.tv_nsec;
#else
	time.tv_sec = st.st_mtim.tv_sec;
	time.tv_nsec = st.st_mtim.tv_nsec;
#endif
}
#endif

ConfigCache::File::File(MemoryPool& p, const PathName& fName)
	: PermanentStorage(p), fileName(getPool(), fName),
	next(NULL)
{
#ifdef WIN_NT
	fileTimeLow = 0;
	fileTimeHigh = 0;
#else
	fileTime.tv_sec = 0;
	fileTime.tv_nsec = 0;
#endif
}

ConfigCache::File::~File()
{
	delete next;
}

bool ConfigCache::File::checkLoadConfig(bool set)
{
#ifdef WIN_NT
	DWORD newTimeLow;
	DWORD newTimeHigh;
	getTime(newTimeLow, newTimeHigh);
	if (fileTimeLow == newTimeLow && fileTimeHigh == newTimeHigh)
#else
	timespec newTime;
	getTime(newTime);
	if (fileTime.tv_sec == newTime.tv_sec && fileTime.tv_nsec == newTime.tv_nsec)
#endif
	{
		return next ? next->checkLoadConfig(set) : true;
	}

	if (set)
	{
#ifdef WIN_NT
		fileTimeLow = newTimeLow;
		fileTimeHigh = newTimeHigh;
#else
		fileTime.tv_sec = newTime.tv_sec;
		fileTime.tv_nsec = newTime.tv_nsec;
#endif

		if (next)
		{
			next->checkLoadConfig(set);
		}
	}
	return false;
}

bool ConfigCache::File::add(const PathName& fName)
{
	if (fName == fileName)
	{
		return false;
	}

	if (next)
	{
		return next->add(fName);
	}
	else
	{
		next = FB_NEW_POOL(getPool()) ConfigCache::File(getPool(), fName);
		next->checkLoadConfig(true); // To set current time and avoid unnecessary invalidation
	}

	return true;
}

void ConfigCache::File::trim()
{
	delete next;
	next = NULL;
}
