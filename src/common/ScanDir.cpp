/*
 *
 *     The contents of this file are subject to the Initial
 *     Developer's Public License Version 1.0 (the "License");
 *     you may not use this file except in compliance with the
 *     License. You may obtain a copy of the License at
 *     http://www.ibphoenix.com/idpl.html.
 *
 *     Software distributed under the License is distributed on
 *     an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
 *     express or implied.  See the License for the specific
 *     language governing rights and limitations under the License.
 *
 *     The contents of this file or any work derived from this file
 *     may not be distributed under any other license whatsoever
 *     without the express prior written permission of the original
 *     author.
 *
 *
 *  The Original Code was created by James A. Starkey for IBPhoenix.
 *
 *  Copyright (c) 1997 - 2000, 2001, 2003 James A. Starkey
 *  Copyright (c) 1997 - 2000, 2001, 2003 Netfrastructure, Inc.
 *  All Rights Reserved.
 */

// ScanDir.cpp: implementation of the ScanDir class.
//
//////////////////////////////////////////////////////////////////////


#include "firebird.h"
#include "ScanDir.h"
#include "os/os_utils.h"
#include "isc_f_proto.h"

// In order to have readdir() working correct on solaris 10,
// firebird.h should be included before sys/stat.h and unistd.
// Luckily this seems to be the only place where we use readdir().
// AP, 2007.

#ifndef _WIN32
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ScanDir::ScanDir(const char *direct, const char *pat) :
	directory(getPool()), pattern(getPool()), fileName(getPool()), filePath(getPool())
{
	directory = direct;
	pattern = pat;
#ifdef _WIN32
	handle = NULL;
#else
	dir = opendir (os_utils::SystemCharBuffer(direct));
#endif
}

ScanDir::~ScanDir()
{
#ifdef _WIN32
	if (handle)
		FindClose (handle);
#else
	if (dir)
		closedir (dir);
#endif
}

bool ScanDir::next()
{
#ifdef _WIN32
	if (handle == NULL)
	{
		Firebird::PathName fn(directory, pattern);
		handle = FindFirstFileW(os_utils::WideCharBuffer(fn), &data);
		return handle != INVALID_HANDLE_VALUE;
	}

	return FindNextFileW(handle, &data) != 0;
#else
	if (!dir)
		return false;

	while ((data = os_utils::readdir (dir)))
	{
		if (match (pattern.c_str(), data->d_name))
			return true;
	}

	return false;
#endif
}

const char* ScanDir::getFileName()
{
#ifdef _WIN32
	int len8 = WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, NULL, 0, NULL, NULL);
	if (len8 == 0)
		return "";

	char* utf8Buffer = fileName.getBuffer(len8);

	len8 = WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, utf8Buffer, len8, NULL, NULL);

	if (len8 == 0)
		return "";

	fileName.recalculate_length();
#else
	fileName = data->d_name;
	ISC_systemToUtf8(fileName);
#endif

	return fileName.c_str();
}


const char* ScanDir::getFilePath()
{
#ifdef _WIN32
	filePath.printf("%s\\%s", directory.c_str(), getFileName());
#else
	filePath.printf("%s/%s", directory.c_str(), data->d_name);
#endif

	return filePath.c_str();
}

bool ScanDir::match(const char *pattern, const char *name)
{
	if (*pattern == '*')
	{
		if (!pattern [1])
			return true;
		for (const char *p = name; *p; ++p)
		{
			if (match (pattern + 1, p))
				return true;
		}
		return false;
	}

	if (*pattern != *name)
		return false;

	if (!*pattern)
		return true;

	return match (pattern + 1, name + 1);
}

bool ScanDir::isDirectory()
{
#if defined(_WIN32)
	return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(HAVE_STRUCT_DIRENT_D_TYPE) && !defined(LSB_BUILD)
	return (data->d_type == DT_DIR);
#else
	struct STAT buf;

    if (os_utils::stat(getFilePath(), &buf))
		return false;

	return S_ISDIR (buf.st_mode);
#endif
}

bool ScanDir::isDots()
{
	return getFileName() [0] == '.';
}
