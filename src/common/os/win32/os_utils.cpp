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

#include <sys/types.h>
#include <sys/stat.h>

#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/gdsassert.h"
#include "../common/os/os_utils.h"
#include "../jrd/constants.h"
#include "../common/os/path_utils.h"
#include "../common/isc_proto.h"
#include "gen/iberror.h"

#include <direct.h>
#include <io.h> // isatty()
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <aclapi.h>
#include <Winsock2.h>
#include <ShlObj.h>
#include <shlwapi.h>
#include <wchar.h>

namespace os_utils
{

// waits for implementation
SLONG get_user_group_id(const TEXT* /*user_group_name*/)
{
	return 0;
}


// waits for implementation
SLONG get_user_id(const TEXT* /*user_name*/)
{
	return -1;
}


// waits for implementation
bool get_user_home(int /*user_id*/, Firebird::PathName& /*homeDir*/)
{
	return false;
}


// allow different users to read\write\delete files in lock directory
// in case of any error just log it and don't stop engine execution
void adjustLockDirectoryAccess(WCHAR* pathname)
{
	PSECURITY_DESCRIPTOR pSecDesc = NULL;
	PSID pSID_Users = NULL;
	PSID pSID_Administrators = NULL;
	PACL pNewACL = NULL;
	try
	{
		// We should pass root directory in format "C:\" into GetVolumeInformation().
		// In case of pathname is not local folder (i.e. \\share\folder) let
		// GetVolumeInformation() return an error.
		int drive = PathGetDriveNumberW(pathname);
		if (drive == -1) // Path contains no drive
			return;

		WCHAR root[5] = L"";
		DWORD fsflags;
		if (!GetVolumeInformationW(PathBuildRootW(root, drive), NULL, 0, NULL, NULL, &fsflags, NULL, 0))
			Firebird::system_error::raise("GetVolumeInformation");

		if (!(fsflags & FS_PERSISTENT_ACLS))
			return;

		// Adjust security for our new folder : allow BUILTIN\Users group to
		// read\write\delete files
		PACL pOldACL = NULL;

		if (GetNamedSecurityInfoW(pathname,
				SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
				NULL, NULL, &pOldACL, NULL,
				&pSecDesc) != ERROR_SUCCESS)
		{
			Firebird::system_error::raise("GetNamedSecurityInfo");
		}

		SID_IDENTIFIER_AUTHORITY sidAuth = SECURITY_NT_AUTHORITY;
		if (!AllocateAndInitializeSid(&sidAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &pSID_Users))
		{
			Firebird::system_error::raise("AllocateAndInitializeSid");
		}

		if (!AllocateAndInitializeSid(&sidAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSID_Administrators))
		{
			Firebird::system_error::raise("AllocateAndInitializeSid");
		}

		EXPLICIT_ACCESS eas[2];
		memset(eas, 0, sizeof(eas));

		eas[0].grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE;
		eas[0].grfAccessMode = GRANT_ACCESS;
		eas[0].grfInheritance = SUB_OBJECTS_ONLY_INHERIT;
		eas[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		eas[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
		eas[0].Trustee.ptstrName  = (LPSTR) pSID_Users;

		eas[1].grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE;
		eas[1].grfAccessMode = GRANT_ACCESS;
		eas[1].grfInheritance = SUB_OBJECTS_ONLY_INHERIT;
		eas[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		eas[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
		eas[1].Trustee.ptstrName  = (LPSTR) pSID_Administrators;

		if (SetEntriesInAcl(2, eas, pOldACL, &pNewACL) != ERROR_SUCCESS)
			Firebird::system_error::raise("SetEntriesInAcl");

		if (SetNamedSecurityInfoW(pathname,
				SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
				NULL, NULL, pNewACL, NULL) != ERROR_SUCCESS)
		{
			Firebird::system_error::raise("SetNamedSecurityInfo");
		}
	}
	catch (const Firebird::Exception& ex)
	{
		Firebird::string str;
		str.printf("Error adjusting access rights for folder \"%s\" :", pathname);

		iscLogException(str.c_str(), ex);
	}

	if (pSID_Users) {
		FreeSid(pSID_Users);
	}
	if (pSID_Administrators) {
		FreeSid(pSID_Administrators);
	}
	if (pNewACL) {
		LocalFree(pNewACL);
	}
	if (pSecDesc) {
		LocalFree(pSecDesc);
	}
}


// create directory for lock files and set appropriate access rights
void createLockDirectory(const char* pathname)
{
	static bool errorLogged = false;

	WideCharBuffer fn;
	fn.fromString(CP_UTF8, pathname);
	DWORD attr = GetFileAttributesW(fn);
	DWORD errcode = 0;
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		errcode = GetLastError();
		if (errcode == ERROR_FILE_NOT_FOUND)
		{
			if (!CreateDirectoryW(fn, NULL)) {
				errcode = GetLastError();
			}
			else
			{
				adjustLockDirectoryAccess(fn);

				attr = GetFileAttributesW(fn);
				if (attr == INVALID_FILE_ATTRIBUTES) {
					errcode = GetLastError();
				}
			}
		}
	}

	Firebird::string err;
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		err.printf("Can't create directory \"%s\". OS errno is %d", pathname, errcode);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		Firebird::fatal_exception::raise(err.c_str());
	}

	if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
	{
		err.printf("Can't create directory \"%s\". File with same name already exists", pathname);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		Firebird::fatal_exception::raise(err.c_str());
	}

	if (attr & FILE_ATTRIBUTE_READONLY)
	{
		err.printf("Can't create directory \"%s\". Readonly directory with same name already exists", pathname);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		Firebird::fatal_exception::raise(err.c_str());
	}
}

// open (or create if missing) and set appropriate access rights
int openCreateSharedFile(const char* pathname, int flags)
{
	int rc = open(pathname, flags | O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
	if (rc < 0)
	{
		(Firebird::Arg::Gds(isc_io_error) << "open" << pathname << Firebird::Arg::Gds(isc_io_open_err)
			<< strerror(errno)).raise();
	}
	return rc;
}

// set file's last access and modification time to current time
bool touchFile(const char* pathname)
{
    FILETIME ft;
    SYSTEMTIME st;

	WideCharBuffer fn;
	fn.fromString(CP_UTF8, pathname);
	HANDLE hFile = CreateFileW(fn,
		GENERIC_READ | FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		ISC_get_security_desc(),
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

    GetSystemTime(&st);
	const bool ret = SystemTimeToFileTime(&st, &ft) && SetFileTime(hFile, NULL, &ft, &ft);
	CloseHandle(hFile);

	return ret;
}

// check if OS have support for IPv6 protocol
bool isIPv6supported()
{
	INT proto[] = {IPPROTO_TCP, 0};

	Firebird::HalfStaticArray<char, sizeof(WSAPROTOCOL_INFO) * 4> buf;

	DWORD len = buf.getCapacity();
	LPWSAPROTOCOL_INFO pi = (LPWSAPROTOCOL_INFO) buf.getBuffer(len);

	int n = WSAEnumProtocols(proto, pi, &len);

	if (n == SOCKET_ERROR && GetLastError() == WSAENOBUFS)
	{
		pi = (LPWSAPROTOCOL_INFO) buf.getBuffer(len);
		n = WSAEnumProtocols(proto, pi, &len);
	}

	if (n == SOCKET_ERROR)
		return false;

	for (int i = 0; i < n; i++)
	{
		if (pi[i].iAddressFamily == AF_INET6 && pi[i].iProtocol == IPPROTO_TCP)
			return true;
	}

	WSASetLastError(0);
	return false;
}

int stat(const char* path, struct STAT* buf)
{
	WideCharBuffer fn;
	fn.fromString(CP_UTF8, path);
	return _wstat(fn, buf);
}
	
int fstat(int fd, struct STAT* buf)
{
	return _fstat(fd, buf);
}

int open(const char* pathname, int flags, mode_t mode)
{
	WideCharBuffer fn;
	if (fn.fromString(CP_UTF8, pathname))
	{
		return ::_wopen(fn, flags, mode);
	}
	else // Fallback to ANSI version
	{
		return ::_open(pathname, flags, mode);
	}
}

FILE* fopen(const char* pathname, const char* mode)
{
	WideCharBuffer wfn, wmode;
	if (wfn.fromString(CP_UTF8, pathname))
	{
		wmode.fromString(CP_ASCII, mode);
		return ::_wfopen(wfn, wmode);
	}
	else // fallback to ANSI version
		return ::fopen(pathname, mode);
}

int unlink(const char* pathname)
{
	WideCharBuffer wfn;
	if (wfn.fromString(CP_UTF8, pathname))
	{
		return ::_wunlink(wfn);
	}
	else // fallback to ANSI version
		return ::unlink(pathname);
}

void getUniqueFileId(HANDLE fd, Firebird::UCharBuffer& id)
{
	BY_HANDLE_FILE_INFORMATION file_info;
	GetFileInformationByHandle(fd, &file_info);

	// The identifier is [nFileIndexHigh, nFileIndexLow]
	// MSDN says: After a process opens a file, the identifier is constant until
	// the file is closed. An application can use this identifier and the
	// volume serial number to determine whether two handles refer to the same file.
	const size_t len1 = sizeof(file_info.dwVolumeSerialNumber);
	const size_t len2 = sizeof(file_info.nFileIndexHigh);
	const size_t len3 = sizeof(file_info.nFileIndexLow);

	UCHAR* p = id.getBuffer(len1 + len2 + len3);

	memcpy(p, &file_info.dwVolumeSerialNumber, len1);
	p += len1;
	memcpy(p, &file_info.nFileIndexHigh, len2);
	p += len2;
	memcpy(p, &file_info.nFileIndexLow, len3);
}


/// class CtrlCHandler

bool CtrlCHandler::terminated = false;

CtrlCHandler::CtrlCHandler()
{
	SetConsoleCtrlHandler(handler, TRUE);
}

CtrlCHandler::~CtrlCHandler()
{
	SetConsoleCtrlHandler(handler, FALSE);
}

BOOL WINAPI CtrlCHandler::handler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
		terminated = true;
		return TRUE;

	default:
		return FALSE;
	}
}

WideCharBuffer::WideCharBuffer(const Firebird::PathName& path) :
m_len16(0)
{
	if (!fromString(CP_UTF8, path))
	{
		Firebird::system_error::raise("MultiByteToWideChar"); // it hardly can happen, but just in case
	}
}

bool WideCharBuffer::fromString(UINT codePage, const char* str, const int length)
{
	if (codePage == CP_ASCII)
	{
		// No need to bother kernel functions for such simple case
		WCHAR* buf = m_utf16.getBuffer(length + 1, false);
		for (int i = 0; i < length; i++)
		{
			*buf++ = *str++;
		}
		*buf = 0;
		return true;
	}

	int bufSize = m_utf16.getCapacity() - 1; // Reserve space for terminating zero
	WCHAR* utf16Buffer = m_utf16.begin();
	if (str == NULL || length == 0)
	{
		m_len16 = 0;
		utf16Buffer[0] = 0;
		return true;
	}

	m_len16 = MultiByteToWideChar(codePage, 0, str, length, utf16Buffer, bufSize);

	if (m_len16 == 0)
	{
		DWORD err = GetLastError();
		if (err != ERROR_INSUFFICIENT_BUFFER)
			return false;

		bufSize = MultiByteToWideChar(codePage, 0, str, length, NULL, 0);
		if (bufSize == 0)
			return false;

		utf16Buffer = m_utf16.getBuffer(bufSize + 1);
		m_len16 = MultiByteToWideChar(codePage, 0, str, length, utf16Buffer, bufSize);
	}

	utf16Buffer[m_len16] = 0;
	return (m_len16 != 0);
}


bool WideCharBuffer::toString(UINT codePage, Firebird::AbstractString& str)
{
	if (m_len16 == 0)
	{
		str.resize(0);
		return true;
	}

	BOOL defaultCharUsed = FALSE;
	LPBOOL pDefaultCharUsed = &defaultCharUsed;
	if (codePage == CP_UTF8 || codePage == CP_UTF7)
		pDefaultCharUsed = NULL;

	WCHAR* utf16Buffer = m_utf16.begin();

	char* utf8Buffer = str.getBuffer(str.capacity());
	int len8 = WideCharToMultiByte(codePage, 0, utf16Buffer, m_len16,
		utf8Buffer, str.capacity(), NULL, pDefaultCharUsed);

	if (len8 == 0 || defaultCharUsed)
	{
		DWORD err = GetLastError();
		if (err != ERROR_INSUFFICIENT_BUFFER)
			return false;

		len8 = WideCharToMultiByte(codePage, 0, utf16Buffer, m_len16, NULL, 0, NULL, pDefaultCharUsed);
		if (len8 == 0 || defaultCharUsed)
			return false;

		utf8Buffer = str.getBuffer(len8);

		len8 = WideCharToMultiByte(codePage, 0, utf16Buffer, m_len16, utf8Buffer, len8,
			NULL, pDefaultCharUsed);
	}

	if (len8 == 0 || defaultCharUsed)
		return false;

	str.resize(len8);

	return true;
}

bool WideCharBuffer::toUpper()
{
	if (m_len16 == 0)
	{
		return true;
	}

	int bufSize = m_utf16.getCapacity();
	bufSize = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, getBuffer(), m_len16 + 1, getBuffer(), bufSize);
	if (bufSize == 0)
	{
		DWORD err = GetLastError();
		if (err != ERROR_INSUFFICIENT_BUFFER)
			return false;

		bufSize = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, getBuffer(), m_len16 + 1, NULL, 0);
		if (bufSize == 0)
			return false;

		WCHAR* utf16Buffer = m_utf16.getBuffer(bufSize);
		m_len16 = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, getBuffer(), m_len16 + 1, utf16Buffer, bufSize);
	}
	return (m_len16 != 0);
}

bool WideCharBuffer::getCwd()
{
	m_len16 = GetCurrentDirectoryW(m_utf16.getCapacity(), m_utf16.begin());
	if (m_len16 > m_utf16.getCapacity())
	{
		m_len16 = GetCurrentDirectoryW(m_len16, m_utf16.getBuffer(m_len16, false));
	}
	return m_len16 != 0;
}

bool WideCharBuffer::getTempPath()
{
	m_len16 = GetTempPathW(m_utf16.getCapacity(), m_utf16.begin());
	if (m_len16 > m_utf16.getCapacity())
	{
		m_len16 = GetTempPathW(m_len16, m_utf16.getBuffer(m_len16, false));
	}
	return m_len16 != 0;
}

bool WideCharBuffer::searchFile(WideCharBuffer& path, WideCharBuffer& fileName)
{
	m_len16 = SearchPathW(path.getBuffer(), fileName.getBuffer(), NULL, m_utf16.getCapacity(), getBuffer(), NULL);
	if (m_len16 > m_utf16.getCapacity())
	{
		m_len16 = SearchPathW(path.getBuffer(), fileName.getBuffer(), NULL, m_len16, m_utf16.getBuffer(m_len16, false), NULL);
	}
	return m_len16 != 0;
}


bool WideCharBuffer::getLongFileName()
{
	m_len16 = GetLongPathNameW(getBuffer(), getBuffer(), m_utf16.getCapacity());
	if (m_len16 > m_utf16.getCapacity())
	{
		WCHAR* buf = m_utf16.getBuffer(m_len16);
		m_len16 = GetLongPathNameW(buf, buf, m_len16);
	}
	return m_len16 != 0;
}

void WideCharBuffer::getEnvironmentVariable(const char* name)
{
	WideCharBuffer wname;
	wname.fromString(CP_UTF8, name);
	m_len16 = GetEnvironmentVariableW(wname.getBuffer(), m_utf16.begin(), m_utf16.getCapacity());
	if (m_len16 > m_utf16.getCapacity())
	{
		m_len16 = GetEnvironmentVariableW(wname.getBuffer(), m_utf16.getBuffer(m_len16), m_len16);
	}
}

bool WideCharBuffer::getModuleFileName(HMODULE module)
{
	m_len16 = GetModuleFileNameW(module, getBuffer(), m_utf16.getCapacity());
	if (m_len16 != 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		// Try to blindly repeat with bigger buffer
		WCHAR* buf = m_utf16.getBuffer(m_len16 * 2);
		m_len16 = GetModuleFileNameW(module, buf, m_utf16.getCapacity());
	}
	return m_len16 != 0;
}

bool WideCharBuffer::getSpecialFolderPath(int csidl, BOOL fCreate)
{
	WCHAR* buf = m_utf16.getBuffer(MAXPATHLEN, false);
	BOOL res = SHGetSpecialFolderPathW(NULL, buf, csidl, fCreate);
	if (res)
	{
		m_len16 = static_cast<unsigned int>(wcslen(buf));
		return true;
	}
	m_len16 = 0;
	return false;
}

void WideCharBuffer::insert(unsigned int pos, WCHAR c)
{
	if (pos > m_len16)
		pos = m_len16;
	WCHAR* buf = m_utf16.getBuffer(m_len16 + 1);
	wmemmove(buf + pos + 1, buf + pos, m_len16 - pos + 1); // Include terminating zero to copy op
	buf[pos] = c;
}

} // namespace os_utils
