
#include "firebird.h"
#include "../common/os/path_utils.h"
#include <io.h> 		// _access
#include <direct.h>		// _mkdir

using namespace Firebird;

/// The Win32 implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '\\';
const char* PathUtils::curr_dir_link = ".";
const char* PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ';';
const size_t PathUtils::curr_dir_link_len = strlen(curr_dir_link);
const size_t PathUtils::up_dir_link_len = strlen(up_dir_link);

class Win32DirIterator : public PathUtils::DirIterator
{
public:
	Win32DirIterator(MemoryPool& p, const PathName& path)
		: DirIterator(p, path), dir(0), file(getPool()), done(false)
	{
		init();
	}

	Win32DirIterator(const PathName& path)
		: DirIterator(path), dir(0), file(getPool()), done(false)
	{
		init();
	}

	~Win32DirIterator();

	const PathUtils::DirIterator& operator++();
	const PathName& operator*() { return file; }
	operator bool() { return !done; }

private:
	HANDLE dir;
	WIN32_FIND_DATA fd;
	PathName file;
	bool done;

	void init();
};

void Win32DirIterator::init()
{
	PathName mask(dirPrefix);
	PathUtils::ensureSeparator(mask);
	mask += "*.*";

	dir = FindFirstFile(mask.c_str(), &fd);

	if (dir == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			system_call_failed::raise("FindFirstFile");

		dir = 0;
		done = true;
	}
	else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		++(*this);
	else
		PathUtils::concatPath(file, dirPrefix, fd.cFileName);
}

Win32DirIterator::~Win32DirIterator()
{
	if (dir)
	{
		FindClose(dir);
		dir = 0;
	}

	done = true;
}

const PathUtils::DirIterator& Win32DirIterator::operator++()
{
	if (!done)
	{
		while (true)
		{
			if (!FindNextFile(dir, &fd))
			{
				done = true;
				break;
			}
			else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				break;
		}

		if (!done)
			PathUtils::concatPath(file, dirPrefix, fd.cFileName);
	}

	return *this;
}

PathUtils::DirIterator* PathUtils::newDirIterator(MemoryPool& p, const PathName& path)
{
	return FB_NEW_POOL(p) Win32DirIterator(p, path);
}

void PathUtils::splitLastComponent(PathName& path, PathName& file,
		const PathName& orgPath)
{
	PathName::size_type pos = orgPath.rfind(PathUtils::dir_sep);
	if (pos == PathName::npos)
	{
		pos = orgPath.rfind('/');	// temp hack to make it work with paths,
									// not expanded by ISC_expand_filename
		if (pos == PathName::npos)
		{
			path = "";
			file = orgPath;
			return;
		}
	}

	path.erase();
	path.append(orgPath, 0, pos);	// skip the directory separator
	file.erase();
	file.append(orgPath, pos + 1, orgPath.length() - pos - 1);
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
	if (second.length() == 0)
	{
		return;
	}

	ensureSeparator(result);

	PathName::size_type cur_pos = 0;

	for (PathName::size_type pos = 0; cur_pos < second.length(); cur_pos = pos + 1)
	{
		static const char separators[] = "/\\";
		static const PathName::size_type separatorsLen =
			static_cast<PathName::size_type>(strlen(separators));

		pos = second.find_first_of(separators, cur_pos, separatorsLen);
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

			const PathName::size_type up_dir = result.find_last_of(
				separators, result.length() - 2, separatorsLen);

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
		if (*path == '/')
			*path = '\\';
	}
}

static bool hasDriveLetter(const PathName& path)
{
	return path.length() > 2 && path[1] == ':' &&
		(('A' <= path[0] && path[0] <= 'Z') ||
		 ('a' <= path[0] && path[0] <= 'z'));
}

bool PathUtils::isRelative(const PathName& path)
{
	if (path.length() > 0)
	{
		const char ds = hasDriveLetter(path) ? path[2] : path[0];
		return ds != PathUtils::dir_sep && ds != '/';
	}
	return true;
}

void PathUtils::splitPrefix(PathName& path, PathName& prefix)
{
	prefix.erase();
	if (hasDriveLetter(path))
	{
		prefix = path.substr(0, 2);
		path.erase(0, 2);
	}
	if (path.hasData() && (path[0] == PathUtils::dir_sep || path[0] == '/'))
	{
		prefix += path[0];
		path.erase(0, 1);
	}
}

// This function can be made to return something util if we consider junctions (since w2k)
// and NTFS symbolic links (since WinVista).
bool PathUtils::isSymLink(const PathName&)
{
	return false;
}

bool PathUtils::canAccess(const PathName& path, int mode)
{
	return _access(path.c_str(), mode) == 0;
}

int PathUtils::makeDir(const PathName& path)
{
	return _mkdir(path.c_str()) ? errno : 0;
}
