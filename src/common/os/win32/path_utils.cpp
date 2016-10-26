
#include "firebird.h"
#include "../common/os/path_utils.h"
#include "../common/os/os_utils.h"
#include <io.h> 		// _access
#include <direct.h>		// _mkdir

/// The Win32 implementation of the path_utils abstraction.

const char PathUtils::dir_sep = '\\';
const char* PathUtils::curr_dir_link = ".";
const char* PathUtils::up_dir_link = "..";
const char PathUtils::dir_list_sep = ';';
const size_t PathUtils::curr_dir_link_len = strlen(curr_dir_link);
const size_t PathUtils::up_dir_link_len = strlen(up_dir_link);


void PathUtils::splitLastComponent(Firebird::PathName& path, Firebird::PathName& file,
		const Firebird::PathName& orgPath)
{
	Firebird::PathName::size_type pos = orgPath.rfind(PathUtils::dir_sep);
	if (pos == Firebird::PathName::npos)
	{
		file = orgPath;
		path.erase();
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

// This function can be made to return something util if we consider junctions (since w2k)
// and NTFS symbolic links (since WinVista).
bool PathUtils::isSymLink(const Firebird::PathName&)
{
	return false;
}

bool PathUtils::canAccess(const Firebird::PathName& path, int mode)
{
	return _waccess(os_utils::WideCharBuffer(path), mode) == 0;
}

void PathUtils::setDirIterator(char* path)
{
	for (; *path; ++path)
	{
		if (*path == '/')
			*path = '\\';
	}
}

int PathUtils::makeDir(const Firebird::PathName& path)
{
	return _wmkdir(os_utils::WideCharBuffer(path)) ? errno : 0;
}
