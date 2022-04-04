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

#include "firebird.h"
#include "iberror.h"

#include "../common/classes/init.h"
#include "../common/gdsassert.h"
#include "../common/os/os_utils.h"
#include "../common/os/isc_i_proto.h"
#include "../jrd/constants.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_DLADDR
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#endif	// HAVE_DLADDR

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef AIX_PPC
#define _UNIX95
#endif
#include <grp.h>
#ifdef AIX_PPC
#undef _UNIX95
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_ACCEPT4
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#include <sys/types.h>
#include <sys/socket.h>

#if defined(HAVE_SIGNAL_H)
#include <signal.h>
#elif defined(HAVE_SYS_SIGNAL_H)
#include <sys/signal.h>
#endif

#if defined(LSB_BUILD) && LSB_BUILD < 50
#define O_CLOEXEC       02000000
#endif


#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#include <stdio.h>

using namespace Firebird;

namespace os_utils
{

static GlobalPtr<Mutex> grMutex;

// Return user group id if user group name found, otherwise return 0.
SLONG get_user_group_id(const TEXT* user_group_name)
{
	MutexLockGuard guard(grMutex, "get_user_group_id");

	const struct group* user_group = getgrnam(user_group_name);
	return user_group ? user_group->gr_gid : -1;
}

static GlobalPtr<Mutex> pwMutex;

// Return user id if user found, otherwise return -1.
SLONG get_user_id(const TEXT* user_name)
{
	MutexLockGuard guard(pwMutex, "get_user_id");

	const struct passwd* user = getpwnam(user_name);
	return user ? user->pw_uid : -1;
}

// Fills the buffer with home directory if user found
bool get_user_home(int user_id, PathName& homeDir)
{
	MutexLockGuard guard(pwMutex, "get_user_home");

	const struct passwd* user = getpwuid(user_id);
	if (user)
	{
		homeDir = user->pw_dir;
		return true;
	}
	return false;
}

namespace
{
	// runuser/rungroup
	const char* const FIREBIRD = "firebird";

	// change ownership and access of file
	void changeFileRights(const char* pathname, const mode_t mode)
	{
		uid_t uid = geteuid() == 0 ? get_user_id(FIREBIRD) : -1;
		gid_t gid = get_user_group_id(FIREBIRD);
		while (chown(pathname, uid, gid) < 0 && SYSCALL_INTERRUPTED(errno))
			;

		while (chmod(pathname, mode) < 0 && SYSCALL_INTERRUPTED(errno))
			;
	}

	inline int openFile(const char* pathname, int flags, mode_t mode = 0666)
	{
		int rc;

		do
		{
#ifdef LSB_BUILD
			rc = open64(pathname, flags, mode);
#else
			rc = ::open(pathname, flags, mode);
#endif
		} while (rc == -1 && SYSCALL_INTERRUPTED(errno));

		return rc;
	}

} // anonymous namespace


// create directory for lock files and set appropriate access rights
void createLockDirectory(const char* pathname)
{
	struct STAT st;
	for(;;)
	{
		if (access(pathname, R_OK | W_OK | X_OK) == 0)
		{
			if (os_utils::stat(pathname, &st) != 0)
				system_call_failed::raise("stat", pathname);
			if (S_ISDIR(st.st_mode))
				return;
			// not exactly original meaning, but very close to it
			system_call_failed::raise("mkdir", pathname, ENOTDIR);
		}

		if (SYSCALL_INTERRUPTED(errno))
			continue;
		if (errno == ENOENT)
			break;
		system_call_failed::raise("access", pathname);
	}

	Firebird::PathName newname(pathname);
	newname.rtrim("/");
	newname += ".tmp.XXXXXX";
	char* pathname2 = newname.begin();

	while (!mkdtemp(pathname2))
	{
		if (SYSCALL_INTERRUPTED(errno))
			continue;
		(Arg::Gds(isc_lock_dir_access) << pathname2).raise();
	}
	changeFileRights(pathname2, 0770);

	Firebird::PathName renameGuard(pathname2);
	renameGuard += "/fb_rename_guard";
	for(;;)
	{
		int gfd = creat(renameGuard.c_str(), 0600);
		if (gfd >= 0)
		{
			close(gfd);
			break;
		}
		if (SYSCALL_INTERRUPTED(errno))
			continue;
		(Arg::Gds(isc_lock_dir_access) << renameGuard).raise();
	}

	while (rename(pathname2, pathname) != 0)
	{
		if (SYSCALL_INTERRUPTED(errno))
			continue;

		if (errno == EEXIST || errno == ENOTEMPTY)
		{
			while (unlink(renameGuard.c_str()) != 0)
			{
				if (SYSCALL_INTERRUPTED(errno))
					continue;
				(Arg::Gds(isc_lock_dir_access) << pathname).raise();
			}

			while (rmdir(pathname2) != 0)
			{
				if (SYSCALL_INTERRUPTED(errno))
					continue;
				(Arg::Gds(isc_lock_dir_access) << pathname).raise();
			}

			for(;;)
			{
				if (access(pathname, R_OK | W_OK | X_OK) == 0)
				{
					if (os_utils::stat(pathname, &st) != 0)
						system_call_failed::raise("stat", pathname);
					if (S_ISDIR(st.st_mode))
						return;
					// not exactly original meaning, but very close to it
					system_call_failed::raise("stat", pathname, ENOTDIR);
				}

				if (SYSCALL_INTERRUPTED(errno))
					continue;
				system_call_failed::raise("access", pathname);
			}

			return;
		}

		(Arg::Gds(isc_lock_dir_access) << pathname).raise();
	}
}

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

static void raiseError(int errCode, const char* filename)
{
	(Arg::Gds(isc_io_error) << "open" << filename << Arg::Gds(isc_io_open_err)
		<< SYS_ERR(errCode)).raise();
}


// open (or create if missing) and set appropriate access rights
int openCreateSharedFile(const char* pathname, int flags)
{
	int fd = os_utils::open(pathname, flags | O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
	if (fd < 0)
		raiseError(ERRNO, pathname);

	// Security check - avoid symbolic links in /tmp.
	// Malicious user can create a symlink with this name pointing to say
	// security2.fdb and when the lock file is created the file will be damaged.

	struct STAT st;
	int rc;

	rc = os_utils::fstat(fd, &st);

	if (rc != 0)
	{
		const int e = ERRNO;
		close(fd);
		raiseError(e, pathname);
	}

	if (S_ISLNK(st.st_mode))
	{
		close(fd);
		raiseError(ELOOP, pathname);
	}

	changeFileRights(pathname, 0660);

	return fd;
}

// set file's last access and modification time to current time
bool touchFile(const char* pathname)
{
#ifdef HAVE_UTIME_H
	while (utime(pathname, NULL) < 0)
	{
		if (SYSCALL_INTERRUPTED(errno))
		{
			continue;
		}
		return false;
	}

	return true;
#else
	errno = ENOSYS;
	return false;
#endif
}

// check if OS has support for IPv6 protocol
bool isIPv6supported()
{
#ifdef ANDROID
	return false;
#else
	return true;
#endif
}

bool getCurrentModulePath(char* buffer, size_t bufferSize)
{
#ifdef HAVE_DLADDR
	Dl_info path;

	if (dladdr((void*) &getCurrentModulePath, &path))
	{
		strncpy(buffer, path.dli_fname, bufferSize);
		return true;
	}
#endif

	return false;
}

// setting flag is not absolutely required, therefore ignore errors here
void setCloseOnExec(int fd)
{
	if (fd >= 0)
	{
		while (fcntl(fd, F_SETFD, O_CLOEXEC) < 0 && SYSCALL_INTERRUPTED(errno))
			;
	}
}

// force file descriptor to have O_CLOEXEC set
int open(const char* pathname, int flags, mode_t mode)
{
	int fd;
	fd = openFile(pathname, flags | O_CLOEXEC, mode);

	if (fd < 0 && errno == EINVAL)	// probably O_CLOEXEC not accepted
		fd = openFile(pathname, flags, mode);

	setCloseOnExec(fd);
	return fd;
}

FILE* fopen(const char* pathname, const char* mode)
{
	FILE* f = NULL;
	do
	{
#ifdef LSB_BUILD
		// TODO: use open + fdopen to avoid races
		f = fopen64(pathname, mode);
#else
		f = ::fopen(pathname, mode);
#endif
	} while (f == NULL && SYSCALL_INTERRUPTED(errno));

	if (f)
		setCloseOnExec(fileno(f));
	return f;
}

static void makeUniqueFileId(const struct STAT& statistics, UCharBuffer& id)
{
	const size_t len1 = sizeof(statistics.st_dev);
	const size_t len2 = sizeof(statistics.st_ino);

	UCHAR* p = id.getBuffer(len1 + len2);

	memcpy(p, &statistics.st_dev, len1);
	p += len1;
	memcpy(p, &statistics.st_ino, len2);
}


void getUniqueFileId(int fd, UCharBuffer& id)
{
	struct STAT statistics;
	if (os_utils::fstat(fd, &statistics) != 0)
		system_call_failed::raise("fstat");

	makeUniqueFileId(statistics, id);
}


void getUniqueFileId(const char* name, UCharBuffer& id)
{
	struct STAT statistics;
	if (os_utils::stat(name, &statistics) != 0)
	{
		id.clear();
		return;
	}

	makeUniqueFileId(statistics, id);
}

/// class CtrlCHandler

bool CtrlCHandler::terminated = false;

CtrlCHandler::CtrlCHandler()
{
	procInt = ISC_signal(SIGINT, handler, 0);
	procTerm = ISC_signal(SIGTERM, handler, 0);
}

CtrlCHandler::~CtrlCHandler()
{
	if (procInt)
		ISC_signal_cancel(SIGINT, handler, 0);
	if (procTerm)
		ISC_signal_cancel(SIGTERM, handler, 0);
}

void CtrlCHandler::handler(void*)
{
	terminated = true;
}

} // namespace os_utils
