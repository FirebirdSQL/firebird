/*
 *	PROGRAM:		JRD FileSystem Path Handler
 *	MODULE:			path_utils.h
 *	DESCRIPTION:	Abstract class for file path management
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

#ifndef JRD_OS_PATH_UTILS_H
#define JRD_OS_PATH_UTILS_H

#include "../common/classes/fb_string.h"
#include "../../common/classes/alloc.h"


/** This is a utility class that provides a platform independent way to do some
	file path operations.  The operations include determining if a path is
	relative ro absolute, combining to paths to form a new path (adding directory
	separator), isolating the last component in a path, and getting a listing
	of all the files in a given directory.  Each supported platform will require
	an implementation of these abstractions that is appropriate for that platform.
**/
class PathUtils
{
public:
	/// The directory separator for the platform.
	static const char dir_sep;

	/// String used to point to current directory
	static const char* curr_dir_link;
	static const size_t curr_dir_link_len;

	/// String used to point to parent directory
	static const char* up_dir_link;
	static const size_t up_dir_link_len;

	/// The directory list separator for the platform.
	static const char dir_list_sep;

	/** isSymLink returns true if the given path is symbolic link, and false if not.
		Use of this links may provide way to override system security.
		Example: ln -s /usr/firebird/ExternalTables/mytable /etc/xinet.d/remoteshell
		and insert desired rows into mytable.
	**/
	static bool isSymLink(const Firebird::PathName& path);

	/** canAccess returns true if the given path can be accessed
		by this process. mode - like in ACCESS(2).
	**/
	static bool canAccess(const Firebird::PathName& path, int mode);

	/** splitLastComponent takes a path as the third argument and
		removes the last component in that path (usually a file or directory name).
		The removed component is returned in the second parameter, and the path left
		after the component is removed is returned in the first parameter.
		If the input path has only one component that component is returned in the
		second parameter and the first parameter is set to the empty string.
		It is safe to use the same variable as input and any output parameters.
	**/
	static void splitLastComponent(Firebird::PathName&, Firebird::PathName&,
									const Firebird::PathName&);

	/** setDirIterator converts all dir iterators to one required on current
	platform.
	**/
	static void setDirIterator(char* path);

	/** makeDir creates directory passed as parameter.
		return value is 0 on success or error code on error.
	**/
	static int makeDir(const Firebird::PathName& path);
};

#endif // JRD_OS_PATH_UTILS_H

