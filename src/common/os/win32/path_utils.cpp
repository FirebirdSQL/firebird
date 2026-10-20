
#include "firebird.h"
#include "../common/os/path_utils.h"
#include <io.h> 		// _access
#include <direct.h>		// _mkdir
#include <string>

using namespace Firebird;

/// The Win32 implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '\\';
const char* const PathUtils::curr_dir_link = ".";
const char* const PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ';';
const size_t PathUtils::curr_dir_link_len = std::char_traits<char>::length(curr_dir_link);
const size_t PathUtils::up_dir_link_len = std::char_traits<char>::length(up_dir_link);

class Win32DirIterator final : public PathUtils::DirIterator
{
public:
	Win32DirIterator(MemoryPool& p, const PathName& path, const PathUtils::DirIterator::Flags flags)
		: DirIterator(p, path, flags), file(getPool()), tmp_buffer(getPool())
	{
		init();
	}

	Win32DirIterator(const PathName& path, const PathUtils::DirIterator::Flags flags)
		: DirIterator(path, flags), file(getPool()), tmp_buffer(getPool())
	{
		init();
	}

	~Win32DirIterator();

	const PathUtils::DirIterator& operator++() override;
	const PathName& operator*() noexcept override { return file; }
	operator bool() noexcept override { return !done; }

private:
	void init();
	void setCurrentFile();

	bool isRegularFile() const noexcept
	{
		return !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	}

private:
	HANDLE dir = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA fd;

	PathName file;
	// Used to minimize reallocation on every iteration.
	PathName tmp_buffer;

	bool done = false;
};

void Win32DirIterator::init()
{
	PathName mask(dirPrefix);
	PathUtils::ensureSeparator(mask);
	mask += '*';

	// `FindExInfoBasic` skips the short name lookup (legacy stuff) and `FIND_FIRST_EX_LARGE_FETCH`
	// makes the kernel return entries in larger batches, so fewer system calls are made.
	// Both are rejected with `ERROR_INVALID_PARAMETER` before Windows 7, fall back then.
	dir = FindFirstFileEx(mask.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch,
		NULL, FIND_FIRST_EX_LARGE_FETCH);

	if (dir == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER)
	{
		dir = FindFirstFileEx(mask.c_str(), FindExInfoStandard, &fd, FindExSearchNameMatch,
			NULL, 0);
	}

	if (dir == INVALID_HANDLE_VALUE)
	{
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_FILE_NOT_FOUND && lastError != ERROR_PATH_NOT_FOUND)
			system_call_failed::raise("FindFirstFileEx", lastError);

		done = true;
		return;
	}

	if (isRegularFile())
		setCurrentFile();
	else
		++(*this);
}

void Win32DirIterator::setCurrentFile()
{
	if (flags & PathUtils::DirIterator::F_RETURN_ONLY_FILENAME)
		file = fd.cFileName;
	else
	{
		tmp_buffer = fd.cFileName;
		PathUtils::concatPath(file, dirPrefix, tmp_buffer);
	}
}

Win32DirIterator::~Win32DirIterator()
{
	if (dir != INVALID_HANDLE_VALUE)
	{
		FindClose(dir);
		dir = INVALID_HANDLE_VALUE;
	}

	done = true;
}

const PathUtils::DirIterator& Win32DirIterator::operator++()
{
	if (done)
		return *this;

	while (FindNextFile(dir, &fd))
	{
		if (!isRegularFile())
			continue;

		setCurrentFile();
		return *this;
	}

	done = true;
	return *this;
}

PathUtils::DirIterator* PathUtils::newDirIterator(MemoryPool& p, const PathName& path,
	const DirIterator::Flags flags)
{
	return FB_NEW_POOL(p) Win32DirIterator(p, path, flags);
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

void PathUtils::fixupSeparators(char* path) noexcept
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
