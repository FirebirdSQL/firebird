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
#include <fcntl.h>

#include <string>
#include <optional>

using namespace Firebird;

/// The POSIX implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '/';
const char* const PathUtils::curr_dir_link = ".";
const char* const PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ':';
const size_t PathUtils::curr_dir_link_len = std::char_traits<char>::length(curr_dir_link);
const size_t PathUtils::up_dir_link_len = std::char_traits<char>::length(up_dir_link);

class PosixDirIterator : public PathUtils::DirIterator
{
public:
	PosixDirIterator(MemoryPool& p, const PathName& path, const PathUtils::DirIterator::Flags flags)
		: DirIterator(p, path, flags),
		  m_file(p), m_tmp_buffer(p)
	{
		init();
	}

	PosixDirIterator(const PathName& path, const PathUtils::DirIterator::Flags flags)
		: DirIterator(path, flags)
	{
		init();
	}

	~PosixDirIterator();

	const PosixDirIterator& operator++() override;
	const PathName& operator*() noexcept override { return m_file; }
	operator bool() noexcept override { return !m_done; }

private:
	void init();

private:
	DIR* m_dir = nullptr;
	int m_dir_fd = -1;
	dirent* m_ent = nullptr;

	PathName m_file;
	// Used to minimize reallocation on every iteration.
	PathName m_tmp_buffer;

	bool m_done = false;
};

void PosixDirIterator::init()
{
	m_dir = opendir(dirPrefix.c_str());

	if (!m_dir)
		system_call_failed::raise("opendir");

	m_dir_fd = dirfd(m_dir);

	++(*this);
}

PosixDirIterator::~PosixDirIterator()
{
	if (m_dir)
	{
		closedir(m_dir);
		m_dir = nullptr;
	}

	m_done = true;
}

const PosixDirIterator& PosixDirIterator::operator++()
{
	if (!m_done)
	{
		while ( (m_ent = os_utils::readdir(m_dir)) )
		{
			std::optional<bool> is_reg_file;

#ifdef _DIRENT_HAVE_D_TYPE
			// Fast path of checking is file a regular file.
			if (m_ent->d_type == DT_REG)
				is_reg_file = true;
			else if (m_ent->d_type != DT_UNKNOWN)
				is_reg_file = false;
#endif
			if (!is_reg_file.has_value())
			{
				// Slow path, should be taken rarely because most of the FS support and fill `d_type` with actual value.
				struct stat stat_buf;
				if (fstatat(m_dir_fd, m_ent->d_name, &stat_buf, AT_SYMLINK_NOFOLLOW) != 0)
					continue;
				is_reg_file = S_ISREG(stat_buf.st_mode);
			}

			if (is_reg_file.value() == false)
				continue;

			if (flags & PathUtils::DirIterator::F_RETURN_ONLY_FILENAME)
				m_file = m_ent->d_name;
			else
			{
				m_tmp_buffer = m_ent->d_name;
				PathUtils::concatPath(m_file, dirPrefix, m_tmp_buffer);
			}

			break;
		}

		if (!m_ent)
		{
			m_done = true;
			m_file.clear();
		}
	}

	return *this;
}

PathUtils::DirIterator* PathUtils::newDirIterator(MemoryPool& p, const PathName& path, const DirIterator::Flags flags)
{
	return FB_NEW_POOL(p) PosixDirIterator(p, path, flags);
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

void PathUtils::fixupSeparators(char* path) noexcept
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
