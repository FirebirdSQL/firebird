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

	/** An abstract base class for iterating through the contents of a directory.
		Instances of this class are created using the newDirIterator method of
		the PathUtils class.  Each platform implementation is expected to
		subclass DirIterator to create DirIterator objects that function
		correctly on the platform.
	**/
	class DirIterator : protected Firebird::AutoStorage
	{
	public:
		// The constructor requires a string that is the path of the directory being iterated.
		// DirIterator may be located on stack, therefore use AutoStorage.
		DirIterator(MemoryPool& p, const Firebird::PathName& dir)
			: AutoStorage(p), dirPrefix(getPool(), dir)
		{}

		DirIterator(const Firebird::PathName& dir)
			: AutoStorage(), dirPrefix(getPool(), dir)
		{}

		// Destructor provided for memory cleanup
		virtual ~DirIterator() {}

		// The prefix increment operator (++itr) advances the iteration by
		// one and returns a reference to itself to allow cascading operations
		virtual const DirIterator& operator++() = 0;

		// The dereference operator returns a reference to the current
		// item in the iteration.  This path is prefixed with the path of
		// the directory.  If the last element of the path is wanted use
		// PathUtils::splitLastComponent on the result of this function.
		virtual const Firebird::PathName& operator*() = 0;

		// Tests if the iterator has reached the end of the iteration.
		// It is implemented in such a way to make the following loop work correctly:
		// for (DirIterator *itr = PathUtils::newDirIterator(); *itr; ++(*itr))
		virtual operator bool() = 0;

	protected:
		// Stores the path to the directory as given in the constructor
		const Firebird::PathName dirPrefix;

	private:
		// Default constructor is not allowed
		DirIterator();
		// Copy constructor is not allowed
		DirIterator(const DirIterator&);
		// Assignment operator is not allowed
		const DirIterator& operator=(const DirIterator&);
	};

	/** isRelative returns true if the given path is relative, and false if not.
		A relative path is one specified in relation to the current directory.
		For example, the path 'firebird/bin' is a relative path in unix while
		the path '/opt/firebird/bin' is not.
	**/
	static bool isRelative(const Firebird::PathName& path);

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

	/** Concatenates the two paths given in the second and third parameters,
		and writes the resulting path into the first parameter.  The
		two path input arguments (arg 2 and 3) are concatenated in the order
		arg2 arg3.  The concatenation is done is such a way as to remove
		any duplicate directory separators that may have resulted from
		a simple string concatenation of the arguments with the directory
		separator character.
	**/
	static void concatPath(Firebird::PathName&, const Firebird::PathName&,
					const Firebird::PathName&);

	// Tries to ensure our path finishes with a platform-specific directory separator.
	// We don't work correctly with MBCS.
	static void ensureSeparator(Firebird::PathName& in_out);

	// Ensure the path separators are correct for the current platform
	static void fixupSeparators(char* path);

	static void fixupSeparators(Firebird::PathName& path)
	{
		fixupSeparators(path.begin());
	}

	/** splitLastComponent takes a path as the third argument and
		removes the last component in that path (usually a file or directory name).
		The removed component is returned in the second parameter, and the path left
		after the component is removed is returned in the first parameter.
		If the input path has only one component that component is returned in the
		second parameter and the first parameter is set to the empty string.
	**/
	static void splitLastComponent(Firebird::PathName&, Firebird::PathName&,
									const Firebird::PathName&);

	/**	splitPrefix takes a path as the first argument, splits OS-dependent prefix
		from it (something like C:\, D: or \ in windows or / in posix),
		and returns stripped path inplace, i.e. as first argument.
		Prefix is returned as the second argument.
	**/
	static void splitPrefix(Firebird::PathName& path, Firebird::PathName& prefix);

	/** This is the factory method for allocating DirIterator objects.
		It takes a reference to a memory pool to use for all heap allocations,
		and the path of the directory to iterate (in that order).  It is the
		responsibility of the caller to delete the object when they are done with it.
		All errors result in either exceptions being thrown, or a valid empty
		DirIterator being returned.
	**/
	static DirIterator* newDirIterator(MemoryPool&, const Firebird::PathName&);

	/** makeDir creates directory passed as parameter.
		return value is 0 on success or error code on error.
	**/
	static int makeDir(const Firebird::PathName& path);
};

#endif // JRD_OS_PATH_UTILS_H

