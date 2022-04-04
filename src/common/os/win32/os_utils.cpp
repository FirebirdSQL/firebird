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

#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/gdsassert.h"
#include "../common/os/guid.h"
#include "../common/os/os_utils.h"
#include "../jrd/constants.h"
#include "../common/os/path_utils.h"
#include "../common/isc_proto.h"
#include "iberror.h"

#include <direct.h>
#include <io.h> // isatty()
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <aclapi.h>
#include <Winsock2.h>

using namespace Firebird;

namespace
{
	// The types below are defined in SDK 8 for _WIN32_WINNT_WIN8 (0x0602) and above,
	// so we need to define them manually when building with older SDK and/or Windows version

#if (_WIN32_WINNT < 0x0602)
	typedef enum _FILE_INFO_BY_HANDLE_CLASS
	{
		FileBasicInfo,
		FileStandardInfo,
		FileNameInfo,
		FileRenameInfo,
		FileDispositionInfo,
		FileAllocationInfo,
		FileEndOfFileInfo,
		FileStreamInfo,
		FileCompressionInfo,
		FileAttributeTagInfo,
		FileIdBothDirectoryInfo,
		FileIdBothDirectoryRestartInfo,
		FileIoPriorityHintInfo,
		FileRemoteProtocolInfo,
		FileFullDirectoryInfo,
		FileFullDirectoryRestartInfo,
		FileStorageInfo,
		FileAlignmentInfo,
		FileIdInfo,
		FileIdExtdDirectoryInfo,
		FileIdExtdDirectoryRestartInfo,
		FileDispositionInfoEx,
		FileRenameInfoEx,
		FileCaseSensitiveInfo,
		FileNormalizedNameInfo,
		MaximumFileInfoByHandleClass
	} FILE_INFO_BY_HANDLE_CLASS;

	typedef struct _FILE_ID_128
	{
		BYTE Identifier[16];
	} FILE_ID_128;

	typedef struct _FILE_ID_INFO
	{
		ULONGLONG VolumeSerialNumber;
		FILE_ID_128 FileId;
	} FILE_ID_INFO;
#endif

	typedef DWORD (WINAPI *pfnGetFinalPathNameByHandle)
		(HANDLE, LPSTR, DWORD, DWORD);
	typedef BOOL (WINAPI *pfnGetFileInformationByHandleEx)
		(HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD);

	pfnGetFinalPathNameByHandle fnGetFinalPathNameByHandle = NULL;
	pfnGetFileInformationByHandleEx fnGetFileInformationByHandleEx = NULL;

	struct EntryPointLoader
	{
		static void init()
		{
			const HMODULE hmodKernel32 = GetModuleHandle("kernel32.dll");
			if (hmodKernel32)
			{
				fnGetFinalPathNameByHandle = (pfnGetFinalPathNameByHandle)
					GetProcAddress(hmodKernel32, "GetFinalPathNameByHandleA");
				fnGetFileInformationByHandleEx = (pfnGetFileInformationByHandleEx)
					GetProcAddress(hmodKernel32, "GetFileInformationByHandleEx");
			}
		}
	};

	InitMutex<EntryPointLoader> entryLoader("EntryPointLoader");
} // anonymous namespace

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
bool get_user_home(int /*user_id*/, PathName& /*homeDir*/)
{
	return false;
}

// allow different users to read\write\delete files in lock directory
// in case of any error just log it and don't stop engine execution
void adjustLockDirectoryAccess(const char* pathname)
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
		PathName root(pathname);
		const PathName::size_type pos = root.find(':', 0);
		if (pos == 1)
		{
			root.erase(pos + 1, root.length());
			PathUtils::ensureSeparator(root);
		}

		DWORD fsflags;
		if (!GetVolumeInformation(root.c_str(), NULL, 0, NULL, NULL, &fsflags, NULL, 0))
			system_error::raise("GetVolumeInformation");

		if (!(fsflags & FS_PERSISTENT_ACLS))
			return;

		// Adjust security for our new folder : allow BUILTIN\Users group to
		// read\write\delete files
		PACL pOldACL = NULL;

		if (GetNamedSecurityInfo((LPSTR) pathname,
				SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
				NULL, NULL, &pOldACL, NULL,
				&pSecDesc) != ERROR_SUCCESS)
		{
			system_error::raise("GetNamedSecurityInfo");
		}

		SID_IDENTIFIER_AUTHORITY sidAuth = SECURITY_NT_AUTHORITY;
		if (!AllocateAndInitializeSid(&sidAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &pSID_Users))
		{
			system_error::raise("AllocateAndInitializeSid");
		}

		if (!AllocateAndInitializeSid(&sidAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSID_Administrators))
		{
			system_error::raise("AllocateAndInitializeSid");
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
			system_error::raise("SetEntriesInAcl");

		if (SetNamedSecurityInfo((LPSTR) pathname,
				SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
				NULL, NULL, pNewACL, NULL) != ERROR_SUCCESS)
		{
			system_error::raise("SetNamedSecurityInfo");
		}
	}
	catch (const Exception& ex)
	{
		string str;
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

	DWORD attr = GetFileAttributes(pathname);
	DWORD errcode = 0;
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		errcode = GetLastError();
		if (errcode == ERROR_FILE_NOT_FOUND)
		{
			if (!CreateDirectory(pathname, NULL)) {
				errcode = GetLastError();
			}
			else
			{
				adjustLockDirectoryAccess(pathname);

				attr = GetFileAttributes(pathname);
				if (attr == INVALID_FILE_ATTRIBUTES) {
					errcode = GetLastError();
				}
			}
		}
	}

	string err;
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		err.printf("Can't create directory \"%s\". OS errno is %d", pathname, errcode);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		fatal_exception::raise(err.c_str());
	}

	if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
	{
		err.printf("Can't create directory \"%s\". File with same name already exists", pathname);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		fatal_exception::raise(err.c_str());
	}

	if (attr & FILE_ATTRIBUTE_READONLY)
	{
		err.printf("Can't create directory \"%s\". Readonly directory with same name already exists", pathname);
		if (!errorLogged)
		{
			errorLogged = true;
			gds__log(err.c_str());
		}
		fatal_exception::raise(err.c_str());
	}
}

// open (or create if missing) and set appropriate access rights
int openCreateSharedFile(const char* pathname, int flags)
{
	int rc = open(pathname, flags | O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
	if (rc < 0)
	{
		(Arg::Gds(isc_io_error) << "open" << pathname << Arg::Gds(isc_io_open_err)
			<< strerror(errno)).raise();
	}
	return rc;
}

// set file's last access and modification time to current time
bool touchFile(const char* pathname)
{
    FILETIME ft;
    SYSTEMTIME st;

	HANDLE hFile = CreateFile(pathname,
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

	HalfStaticArray<char, sizeof(WSAPROTOCOL_INFO) * 4> buf;

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

bool getCurrentModulePath(char* buffer, size_t bufferSize)
{
	HMODULE hmod = 0;

	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR) &getCurrentModulePath,
		&hmod);

	if (hmod)
	{
		GetModuleFileName(hmod, buffer, bufferSize);
		return true;
	}

	return false;
}

int open(const char* pathname, int flags, mode_t mode)
{
	return ::_open(pathname, flags, mode);
}

FILE* fopen(const char* pathname, const char* mode)
{
	return ::fopen(pathname, mode);
}

void getUniqueFileId(HANDLE fd, UCharBuffer& id)
{
	entryLoader.init();
	id.clear();

	// Let's try getting the file identifier. It's not as easy as it may look.
	// MSDN says: "After a process opens a file, the identifier is constant until
	// the file is closed". So far so good, this is perfectly OK for us.
	// But MSDN also says: "An application can use this identifier and the
	// volume serial number to determine whether two handles refer to the same file".
	// And this part is not true, unfortunately. Volume serial number (VSN) is not
	// guaranteed to be unique. It's generated when then volume is formatted and
	// it's stored inside the volume's master boot record. But if the volume is cloned
	// at the physical block level, or if a virtual (preformatted) drive is used,
	// or if volume snapshot is attached as a different logical volume, then VSN may
	// duplicate an existing VSN. And we would stay confused thinking that two
	// different files are actually the same file. To avoid such a disaster we
	// retrieve the final pathname (with symlinks and mount points expanded)
	// and extract the volume GUID (for local drives) or its share name
	// (for remote drives) as unique volume ID.

	if (fnGetFinalPathNameByHandle)
	{
		char pathbuf[MAX_PATH + 1];
		DWORD res = fnGetFinalPathNameByHandle(fd,
			pathbuf, sizeof(pathbuf), VOLUME_NAME_GUID);
		if (res && res < sizeof(pathbuf))
		{
			string path(pathbuf);

			// Expected format is \\?\Volume{GUID}\pathname,
			// we extract {GUID} and convert into binary format

			const char* const pattern = "\\\\?\\Volume";
			const FB_SIZE_T pos1 = (FB_SIZE_T) strlen(pattern);

			if (path.find(pattern) == 0)
			{
				const FB_SIZE_T pos2 = path.find_first_of('}');

				if (path.find_first_of('{') == pos1 && pos2 != string::npos)
				{
					fb_assert(id.isEmpty());
					id.resize(sizeof(Guid));
					UCHAR* ptr = id.begin();
					bool num_start = true;

					for (FB_SIZE_T n = pos1 + 1; n < pos2 && ptr < id.end(); n++)
					{
						const char symbol = path[n];

						if (symbol == '-')
							continue;

						fb_assert(isalpha(symbol) || isdigit(symbol));

						if (symbol >= '0' && symbol <= '9')
							*ptr += symbol - '0';
						else if (symbol >= 'a' && symbol <= 'z')
							*ptr += symbol - 'a' + 10;
						else if (symbol >= 'A' && symbol <= 'Z')
							*ptr += symbol - 'A' + 10;

						if (num_start)
							*ptr *= 16;
						else
							ptr++;

						num_start = !num_start;
					}

					fb_assert(ptr == id.end());
				}
			}
		}

		if (!res && GetLastError() == ERROR_PATH_NOT_FOUND)
		{
			res = fnGetFinalPathNameByHandle(fd,
				pathbuf, sizeof(pathbuf), VOLUME_NAME_DOS);

			if (res && res < sizeof(pathbuf))
			{
				const string path(pathbuf);

				// Expected format is \\?\UNC\server\share\pathname,
				// we extract <server> and <share> and use them together

				const char* const pattern = "\\\\?\\UNC\\";
				const FB_SIZE_T pos1 = (FB_SIZE_T) strlen(pattern);

				if (path.find(pattern) == 0)
				{
					const FB_SIZE_T pos2 = path.find_first_of('\\', pos1);
					if (pos2 != string::npos)
					{
						// add <server>
						id.add(reinterpret_cast<const UCHAR*>(path.begin() + pos1),
							   pos2 - pos1);

						const FB_SIZE_T pos3 = path.find_first_of('\\', pos2 + 1);
						if (pos3 != string::npos)
						{
							// add <share>
							id.add(reinterpret_cast<const UCHAR*>(path.begin() + pos2 + 1),
								   pos3 - pos2 - 1);
						}
					}
				}
			}
		}
	}

	if (fnGetFileInformationByHandleEx)
	{
		// This function returns the volume serial number and 128-bit file ID.
		// We use the VSN only if we failed to get the other volume ID above.

		FILE_ID_INFO file_id;
		if (fnGetFileInformationByHandleEx(fd, FileIdInfo, &file_id, sizeof(file_id)))
		{
			if (id.isEmpty())
			{
				id.add(reinterpret_cast<UCHAR*>(&file_id.VolumeSerialNumber),
					   sizeof(file_id.VolumeSerialNumber));
			}

			id.add(reinterpret_cast<UCHAR*>(&file_id.FileId),
				   sizeof(file_id.FileId));

			return;
		}
	}

	// This function returns the volume serial number and 64-bit file ID.
	// We use the VSN only if we failed to get the other volume ID above.

	BY_HANDLE_FILE_INFORMATION file_info;
	if (!GetFileInformationByHandle(fd, &file_info))
		system_call_failed::raise("GetFileInformationByHandle");

	if (id.isEmpty())
	{
		id.add(reinterpret_cast<UCHAR*>(&file_info.dwVolumeSerialNumber),
			   sizeof(file_info.dwVolumeSerialNumber));
	}

	id.add(reinterpret_cast<UCHAR*>(&file_info.nFileIndexHigh),
		   sizeof(file_info.nFileIndexHigh));
	id.add(reinterpret_cast<UCHAR*>(&file_info.nFileIndexLow),
		   sizeof(file_info.nFileIndexLow));
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

} // namespace os_utils
