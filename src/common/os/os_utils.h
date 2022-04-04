/*
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */


// =====================================
// File functions

#ifndef INCLUDE_OS_FILE_UTILS_H
#define INCLUDE_OS_FILE_UTILS_H

#include <stdio.h>

#include "../common/classes/fb_string.h"
#include "../common/StatusArg.h"
#include "../common/classes/array.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef O_BINARY
#define O_BINARY	0
#endif

#ifdef WIN_NT
#include <io.h>

#define mode_t int
#define DEFAULT_OPEN_MODE (_S_IREAD | _S_IWRITE)
#else
#include <dirent.h>
#include <sys/mman.h>
#include <sys/resource.h>

#define DEFAULT_OPEN_MODE (0666)
#endif

// LSB uses 32bit IO functions by default
#ifdef LSB_BUILD
#define off_t loff_t
#define fpos_t fpos64_t
#define dirent dirent64
#define rlimit rlimit64
#define STAT stat64
#define FLOCK flock64
#else
#define STAT stat
#define FLOCK flock
#endif

namespace os_utils
{

	SLONG get_user_group_id(const TEXT* user_group_name);
	SLONG get_user_id(const TEXT* user_name);
	bool get_user_home(int user_id, Firebird::PathName& homeDir);

	void createLockDirectory(const char* pathname);
	int openCreateSharedFile(const char* pathname, int flags);
	bool touchFile(const char* pathname);

	bool isIPv6supported();

	bool getCurrentModulePath(char* buffer, size_t bufferSize);

	// force descriptor to have O_CLOEXEC set
	int open(const char* pathname, int flags, mode_t mode = DEFAULT_OPEN_MODE);
	void setCloseOnExec(int fd);	// posix only
	FILE* fopen(const char* pathname, const char* mode);

	// return a binary string that uniquely identifies the file
#ifdef WIN_NT
	void getUniqueFileId(HANDLE fd, Firebird::UCharBuffer& id);
#else
	void getUniqueFileId(int fd, Firebird::UCharBuffer& id);
#define HAVE_ID_BY_NAME
	void getUniqueFileId(const char* name, Firebird::UCharBuffer& id);
#endif

	inline SINT64 lseek(int fd, SINT64 offset, int origin)
	{
#ifdef WIN_NT
		return _lseeki64(fd, offset, origin);
#else
		off_t rc;

		do
		{
#ifdef LSB_BUILD
			rc = lseek64(fd, offset, origin);
#else
			rc = ::lseek(fd, offset, origin);
#endif
		} while (rc == (off_t) -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
#endif
	}

	inline int stat(const char* path, struct STAT* buf)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = stat64(path, buf);
#else
			rc = ::stat(path, buf);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int fstat(int fd, struct STAT* buf)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = fstat64(fd, buf);
#else
			rc = ::fstat(fd, buf);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int fgetpos(FILE* stream, fpos_t* pos)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = fgetpos64(stream, pos);
#else
			rc = ::fgetpos(stream, pos);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int fsetpos(FILE* stream, const fpos_t* pos)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = fsetpos64(stream, pos);
#else
			rc = ::fsetpos(stream, pos);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

#ifndef WIN_NT

#ifndef HAVE_FLOCK
	inline int lockf(int fd, int cmd, off_t len)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = lockf64(fd, cmd, len);
#else
			rc = ::lockf(fd, cmd, len);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}
#endif

	inline int mkstemp(char* templ)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = mkstemp64(templ);
#else
			rc = ::mkstemp(templ);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline ssize_t pread(int fd, void* buf, size_t count, off_t offset)
	{
		// Don't check EINTR because it's done by caller
#ifdef LSB_BUILD
		return pread64(fd, buf, count, offset);
#else
		return ::pread(fd, buf, count, offset);
#endif
	}

	inline ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset)
	{
		// Don't check EINTR because it's done by caller
#ifdef LSB_BUILD
		return pwrite64(fd, buf, count, offset);
#else
		return ::pwrite(fd, buf, count, offset);
#endif
	}

	inline struct dirent* readdir(DIR* dirp)
	{
		struct dirent* rc;

		do
		{
#ifdef LSB_BUILD
			rc = readdir64(dirp);
#else
			rc = ::readdir(dirp);
#endif
		} while (rc == NULL && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
	{
		void* rc;

		do
		{
#ifdef LSB_BUILD
			rc = mmap64(addr, length, prot, flags, fd, offset);
#else
			rc = ::mmap(addr, length, prot, flags, fd, offset);
#endif
		} while (rc == MAP_FAILED && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int ftruncate(int fd, off_t length)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = ftruncate64(fd, length);
#else
			rc = ::ftruncate(fd, length);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int lstat(const char* path, struct STAT* buf)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = lstat64(path, buf);
#else
			rc = ::lstat(path, buf);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

#ifdef HAVE_POSIX_FADVISE
	inline int posix_fadvise(int fd, off_t offset, off_t len, int advice)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = posix_fadvise64(fd, offset, len, advice);
#else
			rc = ::posix_fadvise(fd, offset, len, advice);
#endif
		} while (rc != 0 && SYSCALL_INTERRUPTED(rc));

		return rc;
	}
#else
	inline int posix_fadvise(int, off_t, off_t, int)
	{
		return 0;
	}
#endif

	inline int getrlimit(int resource, struct rlimit* rlim)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = getrlimit64(resource, rlim);
#else
			rc = ::getrlimit(resource, rlim);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

	inline int setrlimit(int resource, const struct rlimit* rlim)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = setrlimit64(resource, rlim);
#else
			rc = ::setrlimit(resource, rlim);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

#endif	// WIN_NT

	class CtrlCHandler
	{
	public:
		CtrlCHandler();
		~CtrlCHandler();

		bool getTerminated() const
		{
			return terminated;
		}

	private:
		static bool terminated;

#ifdef WIN_NT
		static BOOL WINAPI handler(DWORD dwCtrlType);
#else
		static void handler(void*);

		bool procInt;
		bool procTerm;
#endif
	};

} // namespace os_utils

#endif // INCLUDE_OS_FILE_UTILS_H
