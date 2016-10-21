/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		dir_list.cpp
 *	DESCRIPTION:	Directory listing config file operation
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Created by: Alex Peshkov <AlexPeshkov@users.sourceforge.net>
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/config/config.h"
#include "../common/config/dir_list.h"
#include "../common/os/path_utils.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/TempSpace.h"
#include "../common/utils_proto.h"

namespace Firebird {

void ParsedPath::parse(const PathName& path)
{
	clear();

	PathName oldpath = path;
	int toSkip = 0;

	do
	{
		PathName newpath, elem;
		PathUtils::splitLastComponent(newpath, elem, oldpath);
		oldpath = newpath;

		if (elem.isEmpty()) // Skip double dir separator
			continue;

		if (elem == PathUtils::curr_dir_link) // Skip current dir reference
			continue;

		if (elem == PathUtils::up_dir_link) // skip next up dir
		{
			toSkip++;
			continue;
		}

		if (toSkip > 0)
		{
			toSkip--;
			continue;
		}

		insert(0, elem);
	} while (oldpath.hasData());

	if (toSkip != 0)
	{
		// Malformed path, attempt to hack?..
		// Let it be, consequent comparison will rule it out
	}
}

PathName ParsedPath::subPath(FB_SIZE_T n) const
{
	PathName rc;
#ifndef WIN_NT
	// Code in DirectoryList::initialize() ensured that the path is absolute
	rc = PathUtils::dir_sep;
#endif
	for (FB_SIZE_T i = 0; i < n; i++)
	{
		rc.appendPath((*this)[i]);
	}

	return rc;
}

ParsedPath::operator PathName() const
{
	if (!getCount())
		return "";
	return subPath(getCount());
}

bool ParsedPath::contains(const ParsedPath& pPath) const
{
	FB_SIZE_T nFullElem = getCount();
	if (nFullElem > 1 && (*this)[nFullElem - 1].length() == 0)
		nFullElem--;

	if (pPath.getCount() < nFullElem) {
		return false;
	}

	for (FB_SIZE_T i = 0; i < nFullElem; i++)
	{
		if (pPath[i] != (*this)[i]) {
			return false;
		}
	}

	for (FB_SIZE_T i = nFullElem + 1; i <= pPath.getCount(); i++)
	{
		const PathName x = pPath.subPath(i);
		if (PathUtils::isSymLink(x)) {
			return false;
		}
	}
	return true;
}

bool DirectoryList::keyword(const ListMode keyMode, PathName& value, const char* key, const char* next)
{
	// Keywords are case-insensitive on all platforms
	NoCaseString keyValue;
	if (*next) // We have a list of separators
	{
		PathName::size_type pos = value.find_first_of(next);
		if (pos == string::npos)
			return false;

		keyValue.assign(value, 0, pos);
		if (keyValue != key)
			return false;

		value.erase(0, pos + 1);
		value.ltrim(next);
	}
	else
	{
		keyValue.assign(value);
		if (keyValue != key)
			return false;
	}
	mode = keyMode;
	return true;
}

void DirectoryList::initialize(bool simple_mode)
{
	if (mode != NotInitialized)
		return;

	clear();

	PathName val = getConfigString();

	if (simple_mode) {
		mode = SimpleList;
	}
	else
	{
		if (keyword(None, val, "None", "") || keyword(Full, val, "Full", "")) {
			return;
		}
		if (! keyword(Restrict, val, "Restrict", " \t"))
		{
			gds__log("DirectoryList: unknown parameter '%s', defaulting to None", val.c_str());
			mode = None;
			return;
		}
	}

	PathName root = Config::getRootDirectory();

	while (val.hasData())
	{
		string::size_type sep = val.find(';');
		if (sep == string::npos)
			sep = val.length();

		PathName dir(val.c_str(), sep);
		dir.alltrim(" \t\r");

		val.erase(0, sep + 1);

		add(ParsedPath(PathName(root, dir)));
	}
}

bool DirectoryList::isPathInList(const PathName& path) const
{
	if (fb_utils::bootBuild())
	{
		return true;
	}

	fb_assert(mode != NotInitialized);

	// Handle special cases
	switch (mode)
	{
	case None:
		return false;
	case Full:
		return true;
	}

	PathName varpath;
	if (path.isRelative())
	{
		varpath = Config::getRootDirectory();
		varpath.appendPath(path);
	}
	else
	{
		varpath = path;
	}

	ParsedPath pPath(varpath);
	bool rc = false;
	for (const_iterator itr = begin(); itr != end(); ++itr)
	{
		if (itr->contains(pPath))
		{
			rc = true;
			break;
		}
	}
	return rc;
}

bool DirectoryList::expandFileName(PathName& path, const PathName& name) const
{
	fb_assert(mode != NotInitialized);
	for (FB_SIZE_T i = 0; i < getCount(); i++)
	{
		path = (*this)[i];
		path.appendPath(name);
		if (PathUtils::canAccess(path, 4)) {
			return true;
		}
	}
	path = name;
	return false;
}

bool DirectoryList::defaultName(PathName& path, const PathName& name) const
{
	fb_assert(mode != NotInitialized);
	if (! getCount())
	{
		return false;
	}
	path = (*this)[0];
	path.appendPath(name);
	return true;
}

PathName TempDirectoryList::getConfigString() const
{
	const char* value = Config::getTempDirectories();
	if (!value)
	{
		// Temporary directory configuration has not been defined.
		// Let's make default configuration.
		PathName tempDir;
		TempFile::getTempPath(tempDir);
		return tempDir;
	}
	return PathName(value);
}

} //namespace Firebird
