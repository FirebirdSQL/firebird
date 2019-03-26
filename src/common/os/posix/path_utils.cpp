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

using namespace Firebird;

/// The POSIX implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '/';
const char* PathUtils::curr_dir_link = ".";
const char* PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ':';
const size_t PathUtils::curr_dir_link_len = strlen(curr_dir_link);
const size_t PathUtils::up_dir_link_len = strlen(up_dir_link);

class PosixDirIterator : public PathUtils::DirIterator
{
public:
	PosixDirIterator(MemoryPool& p, const PathName& path)
		: DirIterator(p, path), dir(nullptr), ent(nullptr), file(p), done(false)
	{
		init();
	}

	PosixDirIterator(const PathName& path)
		: DirIterator(path), dir(0), done(false)
	{
		init();
	}

	~PosixDirIterator();

	const PosixDirIterator& operator++();
	const PathName& operator*() { return file; }
	operator bool() { return !done; }

private:
	DIR* dir;
	dirent* ent;
	PathName file;
	bool done;

	void init();
};

void PosixDirIterator::init()
{
	dir = opendir(dirPrefix.c_str());

	if (!dir)
		system_call_failed::raise("opendir");

	++(*this);
}

PosixDirIterator::~PosixDirIterator()
{
	if (dir)
	{
		closedir(dir);
		dir = nullptr;
	}

	done = true;
}

const PosixDirIterator& PosixDirIterator::operator++()
{
	if (!done)
	{
		while ( (ent = os_utils::readdir(dir)) )
		{
			PathName entryname;
			PathUtils::concatPath(entryname, dirPrefix, ent->d_name);

			struct stat stats;
			if (!stat(entryname.c_str(), &stats) && S_ISREG(stats.st_mode))
				break;
		}

		if (ent)
			PathUtils::concatPath(file, dirPrefix, ent->d_name);
		else
			done = true;
	}

	return *this;
}

PathUtils::DirIterator* PathUtils::newDirIterator(MemoryPool& p, const PathName& path)
{
	return FB_NEW_POOL(p) PosixDirIterator(p, path);
}

void PathUtils::splitLastComponent(PathName& path, PathName& file,
		const PathName& orgPath)
{
	PathName::size_type pos = orgPath.rfind(dir_sep);
	if (pos == PathName::npos)
	{
		path = "";
		file = orgPath;
		return;
	}

	path.erase();
	path.append(orgPath, 0, pos);	// skip the directory separator
	file.erase();
	file.append(orgPath, pos + 1, orgPath.length() - pos - 1);
}

void PathUtils::splitPrefix(PathName& path, PathName& prefix)
{
	prefix.erase();
	while (path.hasData() && path[0] == dir_sep)
	{
		prefix = dir_sep;
		path.erase(0, 1);
	}
}

void PathUtils::concatPath(PathName& result,
		const PathName& first,
		const PathName& second)
{
	if (first.length() == 0)
	{
		result = second;
		return;
	}

	result = first;

	// First path used to be from trusted sources like getRootDirectory, etc.
	// Second path is mostly user-entered and must be carefully parsed to avoid hacking
	if (second.isEmpty())
	{
		return;
	}

	ensureSeparator(result);

	PathName::size_type cur_pos = 0;

	for (PathName::size_type pos = 0; cur_pos < second.length(); cur_pos = pos + 1)
	{
		pos = second.find(dir_sep, cur_pos);

		if (pos == PathName::npos) // simple name, simple handling
			pos = second.length();

		if (pos == cur_pos) // Empty piece, ignore
			continue;

		if (pos == cur_pos + curr_dir_link_len &&
			memcmp(second.c_str() + cur_pos, curr_dir_link, curr_dir_link_len) == 0) // Current dir, ignore
		{
			continue;
		}

		if (pos == cur_pos + up_dir_link_len &&
			memcmp(second.c_str() + cur_pos, up_dir_link, up_dir_link_len) == 0) // One dir up
		{
			if (result.length() < 2)
			{
				// We have nothing to cut off, ignore this piece (may be throw an error?..)
				continue;
			}

			const PathName::size_type up_dir = result.rfind(dir_sep, result.length() - 2);
			if (up_dir == PathName::npos)
				continue;

			result.erase(up_dir + 1);
			continue;
		}

		result.append(second, cur_pos, pos - cur_pos + 1); // append the piece including separator
	}
}

// We don't work correctly with MBCS.
void PathUtils::ensureSeparator(PathName& in_out)
{
	if (in_out.length() == 0)
		in_out = PathUtils::dir_sep;

	if (in_out[in_out.length() - 1] != PathUtils::dir_sep)
		in_out += PathUtils::dir_sep;
}

void PathUtils::fixupSeparators(char* path)
{
	for (; *path; ++path)
	{
		if (*path == '\\')
			*path = '/';
	}
}

bool PathUtils::isRelative(const PathName& path)
{
	if (path.length() > 0)
		return path[0] != dir_sep;
	return false;
}

bool PathUtils::isSymLink(const PathName& path)
{
	struct STAT st, lst;

	if (os_utils::stat(path.c_str(), &st) != 0)
		return false;

	if (os_utils::lstat(path.c_str(), &lst) != 0)
		return false;

	return st.st_ino != lst.st_ino;
}

bool PathUtils::canAccess(const PathName& path, int mode)
{
	return access(path.c_str(), mode) == 0;
}

int PathUtils::makeDir(const PathName& path)
{
	int rc = mkdir(path.c_str(), 0770) ? errno : 0;
	if (rc == 0)
	{
		// try to set exact access we need but ignore possible errors
		chmod(path.c_str(), 0770);
	}

	return rc;
}
