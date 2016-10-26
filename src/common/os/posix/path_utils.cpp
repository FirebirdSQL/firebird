/*
 *	PROGRAM:		JRD FileSystem Path Handler
 *	MODULE:			path_utils.cpp
 *	DESCRIPTION:	POSIX_specific class for file path management
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
#include "../common/os/os_utils.h"
#include "../common/os/path_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

/// The POSIX implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '/';
const char* PathUtils::curr_dir_link = ".";
const char* PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ':';
const size_t PathUtils::curr_dir_link_len = strlen(curr_dir_link);
const size_t PathUtils::up_dir_link_len = strlen(up_dir_link);


void PathUtils::splitLastComponent(Firebird::PathName& path, Firebird::PathName& file,
		const Firebird::PathName& orgPath)
{
	Firebird::PathName::size_type pos = orgPath.rfind(dir_sep);
	if (pos == Firebird::PathName::npos)
	{
		path = "";
		file = orgPath;
		return;
	}

	if (&orgPath == &file)
	{
		path.assign(orgPath, 0, pos);	// skip the directory separator
		file.assign(orgPath, pos + 1, orgPath.length() - pos - 1);
	}
	else
	{
		file.assign(orgPath, pos + 1, orgPath.length() - pos - 1);
		path.assign(orgPath, 0, pos);	// skip the directory separator
	}
}

bool PathUtils::isSymLink(const Firebird::PathName& path)
{
	struct STAT st, lst;

	if (os_utils::stat(path.c_str(), &st) != 0)
		return false;

	if (os_utils::lstat(path.c_str(), &lst) != 0)
		return false;

	return st.st_ino != lst.st_ino;
}

bool PathUtils::canAccess(const Firebird::PathName& path, int mode)
{
	return access(os_utils::SystemCharBuffer(path), mode) == 0;
}

void PathUtils::setDirIterator(char* path)
{
	for (; *path; ++path)
	{
		if (*path == '\\')
			*path = '/';
	}
}

int PathUtils::makeDir(const Firebird::PathName& path)
{
	os_utils::SystemCharBuffer dn(path);
	int rc = mkdir(dn, 0770) ? errno : 0;
	if (rc == 0)
	{
		// try to set exact access we need but ignore possible errors
		chmod(dn, 0770);
	}

	return rc;
}
