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
#include "gen/iberror.h"

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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
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
	SystemCharBuffer fn(pathname);
	do
	{
		if (access(fn, R_OK | W_OK | X_OK) == 0)
		{
			struct STAT st;
			if (os_utils::stat(fn, &st) != 0)
				system_call_failed::raise("stat");

			if (S_ISDIR(st.st_mode))
				return;

			// not exactly original meaning, but very close to it
			system_call_failed::raise("access", ENOTDIR);
		}
	} while (SYSCALL_INTERRUPTED(errno));

	while (mkdir(fn, 0700) != 0)		// anyway need chmod to avoid umask effects
	{
		if (SYSCALL_INTERRUPTED(errno))
		{
			continue;
		}
		(Arg::Gds(isc_lock_dir_access) << pathname).raise();
	}

	changeFileRights(pathname, 0770);
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
		raiseError(fd, pathname);

	// Security check - avoid symbolic links in /tmp.
	// Malicious user can create a symlink with this name pointing to say
	// security2.fdb and when the lock file is created the file will be damaged.

	struct STAT st;
	int rc;

	rc = os_utils::fstat(fd, &st);

	if (rc != 0)
	{
		int e = errno;
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
	while (utime(SystemCharBuffer(pathname), NULL) < 0)
	{
		if (SYSCALL_INTERRUPTED(errno))
		{
			continue;
		}
		return false;
	}

	return true;
#else
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

// setting flag is not absolutely required, therefore ignore errors here
void setCloseOnExec(int fd)
{
	if (fd >= 0)
	{
		while (fcntl(fd, F_SETFD, O_CLOEXEC) < 0 && SYSCALL_INTERRUPTED(errno))
			;
	}
}

int stat(const char* path, struct STAT* buf)
{
	int rc;
	do {
#ifdef LSB_BUILD
		rc = stat64(SystemCharBuffer(path), buf);
#else
		rc = ::stat(SystemCharBuffer(path), buf);
#endif
	} while (rc == -1 && SYSCALL_INTERRUPTED(errno));
	return rc;
}

int fstat(int fd, struct STAT* buf)
{
	int rc;
	do {
#ifdef LSB_BUILD
		rc = fstat64(fd, buf);
#else
		rc = ::fstat(fd, buf);
#endif
	} while (rc == -1 && SYSCALL_INTERRUPTED(errno));
	return rc;
}

// force file descriptor to have O_CLOEXEC set
int open(const char* pathname, int flags, mode_t mode)
{
	SystemCharBuffer fn(pathname);
	int fd;
	fd = openFile(fn, flags | O_CLOEXEC, mode);

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
		f = fopen64(SystemCharBuffer(pathname), mode);
#else
		f = ::fopen(SystemCharBuffer(pathname), mode);
#endif
	} while (f == NULL && SYSCALL_INTERRUPTED(errno));

	if (f)
		setCloseOnExec(fileno(f));
	return f;
}

int unlink(const char* pathname)
{
	return ::unlink(SystemCharBuffer(pathname));
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

namespace {

	class Converter
	{
	public:
		virtual ~Converter() {}
		virtual char* convert(const char* fromBuf, size_t fromLen) = 0;
		virtual void destroy(char* &outBuf) = 0;
	};

	// No real conversion, system locale is UTF-8 or iconv is not available
	class NullConverter : public Converter
	{
		char* convert(const char* fromBuf, size_t fromLen) override
		{
			return const_cast<char*>(fromBuf);
		}
		void destroy(char* &outBuf)
		{
			outBuf = NULL;
		}
	};

#ifdef HAVE_ICONV_H
	class IConvConverter : public Converter
	{
	public:

		IConvConverter(const char* sysLocale) : ic(NULL)
		{
			iconv_t tmp = iconv_open(sysLocale, "UTF-8");
			if (tmp == (iconv_t)-1)
			{
				(Arg::Gds(isc_random) << "Error opening conversion descriptor" <<
					Arg::Unix(errno)).raise();
				// adding text "from @1 to @2" is good idea
			}
			ic = tmp;
		}

		~IConvConverter()
		{
			if (ic && iconv_close(ic) < 0)
			{
				system_call_failed::raise("iconv_close");
			}
		}

		char* convert(const char* fromBuf, size_t fromLen) override
		{
			if (fromBuf == NULL)
				return NULL;

			size_t outSize = fromLen + 1; // FIXME: Conversion from utf-8 cannot make string longer
			char* res = FB_NEW_POOL(AutoStorage::getAutoMemoryPool()) char[outSize];
			char* outBuf = res;
			char* inBuf = const_cast<char*>(fromBuf);

			MutexLockGuard g(mtx, FB_FUNCTION);

			// First of al reset state of iconv struct
			iconv(ic, NULL, NULL, NULL, NULL);

			if (iconv(ic, &inBuf, &fromLen, &outBuf, &outSize) == (size_t)-1)
			{
				delete[] res;
				(Arg::Gds(isc_bad_conn_str) <<
					Arg::Gds(isc_transliteration_failed) <<
					Arg::Unix(errno)).raise();
			}

			*outBuf = '\0';

			return res;
		}

		void destroy(char* &outBuf)
		{
			delete[] outBuf;
			outBuf = NULL;
		}

	private:
		iconv_t ic;
		Mutex mtx;
	};
#endif

	class ConverterAllocator
	{
	public:
		static Converter* create()
		{
#ifdef HAVE_ICONV_H
			string charmap;
#ifdef HAVE_LANGINFO_H
			charmap = nl_langinfo(CODESET);
#else
			if (!fb_utils::readenv("LC_CTYPE", charmap))
			{
				charmap = "ANSI_X3.4-1968";		// ascii
			}
#endif
			if (charmap != "UTF-8" && charmap != "ANSI_X3.4-1968")		// cross finers and pray that ascii only means not called setlocale(), not real ascii
			{
				charmap += "//TRANSLIT";
				return FB_NEW IConvConverter(charmap.c_str());
			}
#endif
			return FB_NEW NullConverter();
		}

		static void destroy(Converter* inst)
		{
			delete inst;
		}

	};

	InitInstance<Converter, ConverterAllocator> conv;
} // namespace

SystemCharBuffer::SystemCharBuffer(const char* buffer, size_t len)
{
	internalBuffer = conv().convert(buffer, len);
}

SystemCharBuffer::SystemCharBuffer(const char* buffer)
{
	if (buffer)
	{
		internalBuffer = conv().convert(buffer, strlen(buffer));
	}
	else
	{
		internalBuffer = NULL;
	}
}

SystemCharBuffer::~SystemCharBuffer()
{
	conv().destroy(internalBuffer);
}

} // namespace os_utils
