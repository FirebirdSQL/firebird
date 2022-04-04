/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		nbackup.cpp
 *	DESCRIPTION:	Command line utility for physical backup/restore
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  Adriano dos Santos Fernandes
 *
 */


#include "firebird.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/db_alias.h"
#include "../jrd/ods.h"
#include "../yvalve/gds_proto.h"
#include "../common/os/path_utils.h"
#include "../common/os/guid.h"
#include "ibase.h"
#include "../common/utils_proto.h"
#include "../common/classes/array.h"
#include "../common/classes/ClumpletWriter.h"
#include "../utilities/nbackup/nbk_proto.h"
#include "../jrd/license.h"
#include "../jrd/ods_proto.h"
#include "../common/classes/MsgPrint.h"
#include "../common/classes/Switches.h"
#include "../utilities/nbackup/nbkswi.h"
#include "../common/isc_f_proto.h"
#include "../common/StatusArg.h"
#include "../common/classes/objects_array.h"
#include "../common/os/os_utils.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

// How much we align memory when reading database header.
// Sector alignment of memory is necessary to use unbuffered IO on Windows.
// Actually, sectors may be bigger than 1K, but let's be consistent with
// JRD regarding the matter for the moment.
const FB_SIZE_T SECTOR_ALIGNMENT = PAGE_ALIGNMENT;

using namespace Firebird;

namespace
{
	using MsgFormat::SafeArg;
	const USHORT nbackup_msg_fac = 24;

	void printMsg(USHORT number, const SafeArg& arg, bool newLine = true)
	{
		char buffer[256];
		fb_msg_format(NULL, nbackup_msg_fac, number, sizeof(buffer), buffer, arg);
		if (newLine)
			fprintf(stderr, "%s\n", buffer);
		else
			fprintf(stderr, "%s", buffer);
	}

	void printMsg(USHORT number, bool newLine = true)
	{
		static const SafeArg dummy;
		printMsg(number, dummy, newLine);
	}

	void usage(UtilSvc* uSvc, const ISC_STATUS code, const char* message = NULL)
	{
		/*
		string msg;
		va_list params;
		if (message)
		{
			va_start(params, message);
			msg.vprintf(message, params);
			va_end(params);
		}
		*/

		if (uSvc->isService())
		{
			fb_assert(code);
			Arg::Gds gds(code);
			if (message)
				gds << message;
			gds.raise();
		}

		if (code)
		{
			printMsg(1, false); // ERROR:
			USHORT dummy;
			USHORT number = (USHORT) gds__decode(code, &dummy, &dummy);
			fb_assert(number);
			if (message)
				printMsg(number, SafeArg() << message);
			else
				printMsg(number);
			fprintf(stderr, "\n");
		}

		const int mainUsage[] = { 2, 3, 4, 5, 6, 0 };
		const int notes[] = { 19, 20, 21, 22, 26, 27, 28, 79, 0 };
		const Switches::in_sw_tab_t* const base = nbackup_action_in_sw_table;

		for (int i = 0; mainUsage[i]; ++i)
			printMsg(mainUsage[i]);

		printMsg(7); // exclusive options are:
		for (const Switches::in_sw_tab_t* p = base; p->in_sw; ++p)
		{
			if (p->in_sw_msg && p->in_sw_optype == nboExclusive)
				printMsg(p->in_sw_msg);
		}

		printMsg(72); // special options are:
		for (const Switches::in_sw_tab_t* p = base; p->in_sw; ++p)
		{
			if (p->in_sw_msg && p->in_sw_optype == nboSpecial)
				printMsg(p->in_sw_msg);
		}

		printMsg(24); // general options are:
		for (const Switches::in_sw_tab_t* p = base; p->in_sw; ++p)
		{
			if (p->in_sw_msg && p->in_sw_optype == nboGeneral)
				printMsg(p->in_sw_msg);
		}

		printMsg(25); // msg 25 switches can be abbreviated to the unparenthesized characters

		for (int i = 0; notes[i]; ++i)
			printMsg(notes[i]);

		exit(FINI_ERROR);
	}

	void missingParameterForSwitch(UtilSvc* uSvc, const char* sw)
	{
		usage(uSvc, isc_nbackup_missing_param, sw);
	}

	void singleAction(UtilSvc* uSvc)
	{
		usage(uSvc, isc_nbackup_allowed_switches);
	}

	// HPUX has non-posix-conformant method to return error codes from posix_fadvise().
	// Instead of error code, directly returned by function (like specified by posix),
	// -1 is returned in case of error and errno is set. Luckily, we can easily detect it runtime.
	// May be sometimes this function should be moved to os_util namespace.

#ifdef HAVE_POSIX_FADVISE
	int fb_fadvise(int fd, off_t offset, off_t len, int advice)
	{
		int rc = os_utils::posix_fadvise(fd, offset, len, advice);

		if (rc < 0)
		{
			rc = errno;
		}
		if (rc == ENOTTY ||		// posix_fadvise is not supported by underlying file system
			rc == ENOSYS)		// hint is not supported by the underlying file object
		{
			rc = 0;				// ignore such errors
		}

		return rc;
	}
#else // HAVE_POSIX_FADVISE
	int fb_fadvise(int, int, int, int)
	{
		return 0;
	}
#endif // HAVE_POSIX_FADVISE

	bool flShutdown = false;

	int nbackupShutdown(const int reason, const int, void*)
	{
		if (reason == fb_shutrsn_signal)
		{
			flShutdown = true;
			return FB_FAILURE;
		}
		return FB_SUCCESS;
	}

} // namespace


static void checkCtrlC(UtilSvc* /*uSvc*/)
{
	if (flShutdown)
	{
		Arg::Gds(isc_nbackup_user_stop).raise();
	}
}


#ifdef WIN_NT
#define FILE_HANDLE HANDLE
#else
#define FILE_HANDLE int
#endif


const char localhost[] = "localhost";

const char backup_signature[4] = {'N','B','A','K'};
const SSHORT BACKUP_VERSION = 2;

struct inc_header
{
	char signature[4];		// 'NBAK'
	SSHORT version;			// Incremental backup format version.
	SSHORT level;			// Backup level.
	// \\\\\ ---- this is 8 bytes. should not cause alignment problems
	Guid backup_guid;		// GUID of this backup
	Guid prev_guid;			// GUID of previous level backup
	ULONG page_size;		// Size of pages in the database and backup file
	// These fields are currently filled, but not used. May be used in future versions
	ULONG backup_scn;		// SCN of this backup
	ULONG prev_scn;			// SCN of previous level backup
};

class NBackup
{
public:
	NBackup(UtilSvc* _uSvc, const PathName& _database, const string& _username, const string& _role,
			const string& _password, bool _run_db_triggers, bool _direct_io, const string& _deco)
	  : uSvc(_uSvc), newdb(0), trans(0), database(_database),
		username(_username), role(_role), password(_password),
		run_db_triggers(_run_db_triggers), direct_io(_direct_io),
		dbase(INVALID_HANDLE_VALUE), backup(INVALID_HANDLE_VALUE),
		decompress(_deco), childId(0), db_size_pages(0),
		m_odsNumber(0), m_silent(false), m_printed(false), m_flash_map(false)
	{
		// Recognition of local prefix allows to work with
		// database using TCP/IP loopback while reading file locally.
		// RS: Maybe check if host is loopback via OS functions is more correct
		PathName db(_database), host_port;
		if (ISC_extract_host(db, host_port, false) == ISC_PROTOCOL_TCPIP)
		{
			const PathName host = host_port.substr(0, sizeof(localhost) - 1);
			const char delim = host_port.length() >= sizeof(localhost) ? host_port[sizeof(localhost) - 1] : '/';
			if (delim != '/' || !host.equalsNoCase(localhost))
				pr_error(status, "nbackup needs local access to database file");
		}

		expandDatabaseName(db, dbname, NULL);

		if (!uSvc->isService())
		{
			// It's time to take care about shutdown handling
			if (fb_shutdown_callback(status, nbackupShutdown, fb_shut_confirmation, NULL))
			{
				pr_error(status, "setting shutdown callback");
			}
		}
	}

	typedef ObjectsArray<PathName> BackupFiles;

	// External calls must clean up resources after themselves
	void fixup_database(bool repl_seq, bool set_readonly = false);
	void lock_database(bool get_size);
	void unlock_database();
	void backup_database(int level, Guid& guid, const PathName& fname);
	void restore_database(const BackupFiles& files, bool repl_seq, bool inc_rest);

	bool printed() const
	{
		return m_printed;
	}

private:
	UtilSvc* uSvc;

    ISC_STATUS_ARRAY status; // status vector
	isc_db_handle newdb; // database handle
    isc_tr_handle trans; // transaction handle

	PathName database;
	string username, role, password;
	bool run_db_triggers, direct_io;

	PathName dbname; // Database file name
	PathName bakname;
	FILE_HANDLE dbase;
	FILE_HANDLE backup;
	string decompress;
#ifdef WIN_NT
	HANDLE childId;
	HANDLE childStdErr;
#else
	int childId;
#endif
	ULONG db_size_pages;	// In pages
	USHORT m_odsNumber;
	bool m_silent;		// are we already handling an exception?
	bool m_printed;		// pr_error() was called to print status vector
	bool m_flash_map;	// clear mapping cache on attach

	// IO functions
	FB_SIZE_T read_file(FILE_HANDLE &file, void *buffer, FB_SIZE_T bufsize);
	void write_file(FILE_HANDLE &file, void *buffer, FB_SIZE_T bufsize);
	void seek_file(FILE_HANDLE &file, SINT64 pos);

	void pr_error(const ISC_STATUS* status, const char* operation);
	void print_child_stderr();

	void internal_lock_database();
	void get_database_size();
	void get_ods();
	void internal_unlock_database();
	void attach_database();
	void detach_database();
	string to_system(const PathName& from);

	// Create/open database and backup
	void open_database_write(bool exclusive = false);
	void open_database_scan();
	void create_database();
	void close_database();

	void open_backup_scan();
	void open_backup_decompress();
	void create_backup();
	void close_backup();
};


FB_SIZE_T NBackup::read_file(FILE_HANDLE &file, void *buffer, FB_SIZE_T bufsize)
{
	FB_SIZE_T rc = 0;
	while (bufsize)
	{
#ifdef WIN_NT
		// Read child's stderr often to prevent child process hung if it writes
		// too much data to the pipe and overflow the pipe buffer.
		const bool checkChild = (childStdErr != 0 && file == backup);
		if (checkChild)
			print_child_stderr();

		DWORD res;
		if (!ReadFile(file, buffer, bufsize, &res, NULL))
		{
			const DWORD err = GetLastError();
			if (checkChild)
			{
				print_child_stderr();

				if (err == ERROR_BROKEN_PIPE)
				{
					DWORD exitCode;
					if (GetExitCodeProcess(childId, &exitCode) && (exitCode == 0 || exitCode == STILL_ACTIVE))
						break;
				}
			}
#else
		const ssize_t res = read(file, buffer, bufsize);
		if (res < 0)
		{
			const int err = errno;
#endif
			status_exception::raise(Arg::Gds(isc_nbackup_err_read) <<
				(&file == &dbase ? dbname.c_str() :
					&file == &backup ? bakname.c_str() : "unknown") <<
				Arg::OsError(err));
		}

		if (!res)
			break;

		rc += res;
		bufsize -= res;
		buffer = &((UCHAR*) buffer)[res];
	}

	return rc;


	return 0; // silence compiler
}

void NBackup::write_file(FILE_HANDLE &file, void *buffer, FB_SIZE_T bufsize)
{
#ifdef WIN_NT
	DWORD bytesDone;
	if (WriteFile(file, buffer, bufsize, &bytesDone, NULL) && bytesDone == bufsize)
		return;
#else
	if (write(file, buffer, bufsize) == (ssize_t) bufsize)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_write) <<
		(&file == &dbase ? dbname.c_str() :
			&file == &backup ? bakname.c_str() : "unknown") <<
		Arg::OsError());
}

void NBackup::seek_file(FILE_HANDLE &file, SINT64 pos)
{
#ifdef WIN_NT
	LARGE_INTEGER offset;
	offset.QuadPart = pos;
	if (SetFilePointer(file, offset.LowPart, &offset.HighPart, FILE_BEGIN) !=
			INVALID_SET_FILE_POINTER ||
		GetLastError() == NO_ERROR)
	{
		return;
	}
#else
	if (os_utils::lseek(file, pos, SEEK_SET) != (off_t) -1)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_seek) <<
		(&file == &dbase ? dbname.c_str() :
			&file == &backup ? bakname.c_str() : "unknown") <<
		Arg::OsError());
}

void NBackup::open_database_write(bool exclusive)
{
#ifdef WIN_NT
	const DWORD shareFlags = exclusive ?
		FILE_SHARE_READ :
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

	dbase = CreateFile(dbname.c_str(), GENERIC_READ | GENERIC_WRITE,
		shareFlags, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (dbase != INVALID_HANDLE_VALUE)
		return;
#else
	const int flags = exclusive ?
		O_EXCL | O_RDWR | O_LARGEFILE :
		O_RDWR | O_LARGEFILE;

	dbase = os_utils::open(dbname.c_str(), flags);
	if (dbase >= 0)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_opendb) << dbname.c_str() << Arg::OsError());
}

void NBackup::open_database_scan()
{
#ifdef WIN_NT

	// On Windows we use unbuffered IO to work around bug in Windows Server 2003
	// which has little problems with managing size of disk cache. If you read
	// very large file (5 GB or more) on this platform filesystem page cache
	// consumes all RAM of machine and causes excessive paging of user programs
	// and OS itself. Basically, reading any large file brings the whole system
	// down for extended period of time. Documented workaround is to avoid using
	// system cache when reading large files.
	dbase = CreateFile(dbname.c_str(),
		GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | (direct_io ? FILE_FLAG_NO_BUFFERING : 0),
		NULL);
	if (dbase == INVALID_HANDLE_VALUE)
		status_exception::raise(Arg::Gds(isc_nbackup_err_opendb) << dbname.c_str() << Arg::OsError());

#else // WIN_NT

#ifndef O_NOATIME
#define O_NOATIME 0
#endif // O_NOATIME

//
// Solaris does not have O_DIRECT!!!
// TODO: Implement using Solaris directio or suffer performance problems. :-(
//
#ifndef O_DIRECT
#define O_DIRECT 0
#endif // O_DIRECT

	dbase = os_utils::open(dbname.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | (direct_io ? O_DIRECT : 0));
	if (dbase < 0)
	{
		// Non-root may fail when opening file of another user with O_NOATIME
		dbase = os_utils::open(dbname.c_str(), O_RDONLY | O_LARGEFILE | (direct_io ? O_DIRECT : 0));
	}
	if (dbase < 0)
	{
		status_exception::raise(Arg::Gds(isc_nbackup_err_opendb) << dbname.c_str() << Arg::OsError());
	}

#ifdef POSIX_FADV_SEQUENTIAL
	int rc = fb_fadvise(dbase, 0, 0, POSIX_FADV_SEQUENTIAL);
	if (rc)
	{
		status_exception::raise(Arg::Gds(isc_nbackup_err_fadvice) <<
								"SEQUENTIAL" << dbname.c_str() << Arg::Unix(rc));
	}
#endif // POSIX_FADV_SEQUENTIAL

#ifdef POSIX_FADV_NOREUSE
	if (direct_io)
	{
		rc = fb_fadvise(dbase, 0, 0, POSIX_FADV_NOREUSE);
		if (rc)
		{
			status_exception::raise(Arg::Gds(isc_nbackup_err_fadvice) <<
									"NOREUSE" << dbname.c_str() << Arg::Unix(rc));
		}
	}
#endif // POSIX_FADV_NOREUSE

#endif // WIN_NT
}

void NBackup::create_database()
{
#ifdef WIN_NT
	dbase = CreateFile(dbname.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE,
		NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (dbase != INVALID_HANDLE_VALUE)
		return;
#else
	dbase = os_utils::open(dbname.c_str(), O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE, 0660);
	if (dbase >= 0)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_createdb) << dbname.c_str() << Arg::OsError());
}

void NBackup::close_database()
{
#ifdef WIN_NT
	CloseHandle(dbase);
#else
	close(dbase);
#endif
}

string NBackup::to_system(const PathName& from)
{
	string to = from.ToString();
	if (uSvc->utf8FileNames())
		ISC_utf8ToSystem(to);
	return to;
}


void NBackup::open_backup_scan()
{
	if (decompress.hasData())
	{
		open_backup_decompress();
		return;
	}

	string nm = to_system(bakname);
#ifdef WIN_NT
	backup = CreateFile(nm.c_str(), GENERIC_READ, 0,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (backup != INVALID_HANDLE_VALUE)
		return;
#else
	backup = os_utils::open(nm.c_str(), O_RDONLY | O_LARGEFILE);
	if (backup >= 0)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_openbk) << bakname.c_str() << Arg::OsError());
}

void NBackup::open_backup_decompress()
{
	string command = decompress;

#ifdef WIN_NT
	const PathName::size_type n = command.find('@');

	if (n != PathName::npos)
	{
		command.replace(n, 1, bakname);
	}
	else
	{
		command.append(" ");
		command.append(bakname);
	}

	SECURITY_ATTRIBUTES sa;
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);

	HANDLE hChildStdOut;
	if (!CreatePipe(&backup, &hChildStdOut, &sa, 0))
		system_call_failed::raise("CreatePipe");

	SetHandleInformation(backup, HANDLE_FLAG_INHERIT, 0);

	HANDLE hChildStdErr;
	if (!CreatePipe(&childStdErr, &hChildStdErr, &sa, 0))
		system_call_failed::raise("CreatePipe");

	SetHandleInformation(childStdErr, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO start_crud;
	memset(&start_crud, 0, sizeof(STARTUPINFO));
	start_crud.cb = sizeof(STARTUPINFO);
	start_crud.dwFlags = STARTF_USESTDHANDLES;
	start_crud.hStdOutput = hChildStdOut;
	start_crud.hStdError = hChildStdErr;

	PROCESS_INFORMATION pi;
	if (CreateProcess(NULL, command.begin(), NULL, NULL, TRUE,
					  NORMAL_PRIORITY_CLASS | DETACHED_PROCESS,
					  NULL, NULL, &start_crud, &pi))
	{
		childId = pi.hProcess;
		CloseHandle(pi.hThread);
		CloseHandle(hChildStdOut);
		CloseHandle(hChildStdErr);
	}
	else
	{
		const DWORD err = GetLastError();

		CloseHandle(backup);
		CloseHandle(hChildStdOut);
		CloseHandle(hChildStdErr);

		// error creating child process
		system_call_failed::raise("CreateProcess", err);
	}
#else
		const unsigned ARGCOUNT = 20;
		unsigned narg = 0;
		char* args[ARGCOUNT + 1];
		bool inStr = false;
		for (unsigned i = 0; i < command.length(); ++i)
		{
			switch (command[i])
			{
			case ' ':
			case '\t':
				command[i] = '\0';
				inStr = false;
				break;
			default:
				if (!inStr)
				{
					if (narg >= ARGCOUNT)
					{
						status_exception::raise(Arg::Gds(isc_nbackup_deco_parse) << Arg::Num(ARGCOUNT));
					}
					inStr = true;
					args[narg++] = &command[i];
				}
				break;
			}
		}

		string expanded;
		for (unsigned i = 0; i < narg; ++i)
		{
			expanded = args[i];
			PathName::size_type n = expanded.find('@');
			if (n != PathName::npos)
			{
				expanded.replace(n, 1, bakname);
				args[i] = &expanded[0];
				break;
			}
			expanded.erase();
		}
		if (!expanded.hasData())
		{
			if (narg >= ARGCOUNT)
			{
				status_exception::raise(Arg::Gds(isc_nbackup_deco_parse) << Arg::Num(ARGCOUNT));
			}
			args[narg++] = &bakname[0];
		}
		args[narg] = NULL;

		int pfd[2];
		if (pipe(pfd) < 0)
			system_call_failed::raise("pipe");

		fb_assert(!newdb);		// FB 2.5 & 3 can't fork when attached to database
		childId = fork();
		if (childId < 0)
			system_call_failed::raise("fork");

		if (childId == 0)
		{
			close(pfd[0]);
			dup2(pfd[1], 1);
			close(pfd[1]);

			execvp(args[0], args);
		}
		else
		{
			backup = pfd[0];
			close(pfd[1]);
		}
#endif
}

void NBackup::create_backup()
{
	string nm = to_system(bakname);
#ifdef WIN_NT
	if (bakname == "stdout") {
		backup = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	else
	{
		// See CORE-4913 and "Create File" article on MSDN:
		// When an application creates a file across a network, it is better to use
		// GENERIC_READ | GENERIC_WRITE for dwDesiredAccess than to use GENERIC_WRITE
		// alone. The resulting code is faster, because the redirector can use the
		// cache manager and send fewer SMBs with more data. This combination also
		// avoids an issue where writing to a file across a network can occasionally
		// return ERROR_ACCESS_DENIED.

		backup = CreateFile(nm.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE,
			NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	}
	if (backup != INVALID_HANDLE_VALUE)
		return;
#else
	if (bakname == "stdout")
	{
		backup = 1; // Posix file handle for stdout
		return;
	}
	backup = os_utils::open(nm.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_LARGEFILE, 0660);
	if (backup >= 0)
		return;
#endif

	status_exception::raise(Arg::Gds(isc_nbackup_err_createbk) << bakname.c_str() << Arg::OsError());
}

void NBackup::close_backup()
{
	if (bakname == "stdout")
		return;
#ifdef WIN_NT
	CloseHandle(backup);
	if (childId != 0)
	{
		const bool killed = (WaitForSingleObject(childId, 5000) != WAIT_OBJECT_0);
		if (killed)
			TerminateProcess(childId, 1);

		print_child_stderr();
		CloseHandle(childId);
		CloseHandle(childStdErr);
		childId = childStdErr = 0;

		if (killed)
			status_exception::raise(Arg::Gds(isc_random) << "Child process seems hung. Killed");
	}
#else
	close(backup);
	if (childId > 0)
	{
		wait(NULL);
		childId = 0;
	}
#endif
}

void NBackup::fixup_database(bool repl_seq, bool set_readonly)
{
	open_database_write();

	HalfStaticArray<UCHAR, MIN_PAGE_SIZE> header_buffer;
	auto size = HDR_SIZE;
	auto header = reinterpret_cast<Ods::header_page*>(header_buffer.getBuffer(size));

	if (read_file(dbase, header, size) != size)
		status_exception::raise(Arg::Gds(isc_nbackup_err_eofdb) << dbname.c_str());

	const auto org_flags = header->hdr_flags;
	const auto page_size = header->hdr_page_size;

	const int backup_state = org_flags & Ods::hdr_backup_mask;
	if (backup_state != Ods::hdr_nbak_stalled)
	{
		status_exception::raise(Arg::Gds(isc_nbackup_fixup_wrongstate) << dbname.c_str() <<
			Arg::Num(Ods::hdr_nbak_stalled));
	}

	if (!repl_seq)
	{
		size = page_size;
		header = reinterpret_cast<Ods::header_page*>(header_buffer.getBuffer(size));

		seek_file(dbase, 0);

		if (read_file(dbase, header, size) != size)
			status_exception::raise(Arg::Gds(isc_nbackup_err_eofdb) << dbname.c_str());

		auto p = header->hdr_data;
		const auto end = (UCHAR*) header + header->hdr_page_size;
		while (p < end && *p != Ods::HDR_end)
		{
			if (*p == Ods::HDR_db_guid)
			{
				// Replace existing database GUID with a regenerated one
				Guid guid;
				GenerateGuid(&guid);
				fb_assert(p[1] == sizeof(guid));
				memcpy(p + 2, &guid, sizeof(guid));
			}
			else if (*p == Ods::HDR_repl_seq)
			{
				// Reset the sequence counter
				const FB_UINT64 sequence = 0;
				fb_assert(p[1] == sizeof(sequence));
				memcpy(p + 2, &sequence, sizeof(sequence));
			}

			p += p[1] + 2;
		}
	}

	// Update the flags and write the header page back

	const auto new_flags =
		(org_flags & ~Ods::hdr_backup_mask) | Ods::hdr_nbak_normal |
		(set_readonly ? Ods::hdr_read_only : 0);

	header->hdr_flags = new_flags;

	seek_file(dbase, 0);
	write_file(dbase, header, size);

	close_database();
}


// Print the status, the SQLCODE, and exit.
// Also, indicate which operation the error occurred on.
void NBackup::pr_error(const ISC_STATUS* status, const char* operation)
{
	if (uSvc->isService())
		status_exception::raise(status);

	fprintf(stderr, "[\n");
	printMsg(23, SafeArg() << operation); // PROBLEM ON "%s".
	isc_print_status(status);
	fprintf(stderr, "SQLCODE:%" SLONGFORMAT"\n", isc_sqlcode(status));
	fprintf(stderr, "]\n");

	m_printed = true;

	status_exception::raise(Arg::Gds(isc_nbackup_err_db));
}

void NBackup::print_child_stderr()
{
#ifdef WIN_NT
	// Read stderr of child process (decompressor) and print it at our stdout.
	// Prepend each line by prefix DE> to let user distinguish nbackup's output
	// from decompressor's one.

	const int BUFF_SIZE = 8192;
	char buff[BUFF_SIZE];
	const char* end = buff + BUFF_SIZE - 1;
	static bool atNewLine = true;

	DWORD bytesRead;
	while (true)
	{
		// Check if pipe have data to read. This is necessary to avoid hung if
		// pipe is empty. Ignore read error as ReadFile set bytesRead to zero
		// in this case and it is enough for our usage.
		const BOOL ret = PeekNamedPipe(childStdErr, NULL, 1, NULL, &bytesRead, NULL);
		if (ret && bytesRead)
			ReadFile(childStdErr, buff, end - buff, &bytesRead, NULL);
		else
			bytesRead = 0;

		if (bytesRead == 0)
			break;

		buff[bytesRead] = 0;

		char* p = buff;
		while (true)
		{
			char* pEndL = strchr(p, '\r');
			if (pEndL)
			{
				pEndL++;
				if (*pEndL == '\n')
					pEndL++;
			}
			else
			{
				pEndL = strchr(p, '\n');
				if (pEndL)
					pEndL++;
			}
			const bool eol = (pEndL != NULL);

			if (!pEndL)
				pEndL = buff + bytesRead;

			char ch = *pEndL;
			*pEndL = 0;

			if (atNewLine)
				uSvc->printf(false, "DE> %s", p);
			else
				uSvc->printf(false, "%s", p);

			*pEndL = ch;
			atNewLine = eol;

			if (pEndL >= buff + bytesRead)
				break;

			p = pEndL;
		}
	}
#endif
}

void NBackup::attach_database()
{
	if (username.length() > 255 || password.length() > 255)
	{
		if (m_silent)
			return;
		status_exception::raise(Arg::Gds(isc_nbackup_userpw_toolong));
	}

	ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);

	const unsigned char* authBlock;
	unsigned int authBlockSize = uSvc->getAuthBlock(&authBlock);
	if (authBlockSize)
	{
		dpb.insertBytes(isc_dpb_auth_block, authBlock, authBlockSize);
	}
	else
	{
		if (username.hasData())
			dpb.insertString(isc_dpb_user_name, username);

		if (password.hasData())
			dpb.insertString(isc_dpb_password, password);
	}
	if (role.hasData())
		dpb.insertString(isc_dpb_sql_role_name, role);

	if (!run_db_triggers)
		dpb.insertByte(isc_dpb_no_db_triggers, 1);

	if (m_flash_map)
		dpb.insertByte(isc_dpb_clear_map, 1);

	if (m_silent)
	{
		ISC_STATUS_ARRAY temp;
		isc_attach_database(temp, 0, database.c_str(), &newdb,
			dpb.getBufferLength(), reinterpret_cast<const char*>(dpb.getBuffer()));
	}
	else if (isc_attach_database(status, 0, database.c_str(), &newdb,
				dpb.getBufferLength(), reinterpret_cast<const char*>(dpb.getBuffer())))
	{
		pr_error(status, "attach database");
	}
}

void NBackup::detach_database()
{
	if (m_silent)
	{
		ISC_STATUS_ARRAY temp;
		if (trans)
			isc_rollback_transaction(temp, &trans);

		isc_detach_database(temp, &newdb);
	}
	else
	{
		if (trans)
		{
			if (isc_rollback_transaction(status, &trans))
				pr_error(status, "rollback transaction");
		}
		if (isc_detach_database(status, &newdb))
			pr_error(status, "detach database");
	}
}

void NBackup::internal_lock_database()
{
	if (isc_start_transaction(status, &trans, 1, &newdb, 0, NULL))
		pr_error(status, "start transaction");
	if (isc_dsql_execute_immediate(status, &newdb, &trans, 0, "ALTER DATABASE BEGIN BACKUP", 1, NULL))
		pr_error(status, "begin backup");
	if (isc_commit_transaction(status, &trans))
		pr_error(status, "begin backup: commit");
}

void NBackup::get_database_size()
{
	db_size_pages = 0;
	const char fs[] = {isc_info_db_file_size};
	char res[128];
	if (isc_database_info(status, &newdb, sizeof(fs), fs, sizeof(res), res))
	{
		pr_error(status, "size info");
	}
	else if (res[0] == isc_info_db_file_size)
	{
		USHORT len = isc_vax_integer (&res[1], 2);
		db_size_pages = isc_vax_integer (&res[3], len);
	}
}

void NBackup::get_ods()
{
	m_odsNumber = 0;
	const char db_version_info[] = { isc_info_ods_version };
	char res[128];
	if (isc_database_info(status, &newdb, sizeof(db_version_info), db_version_info, sizeof(res), res))
	{
		pr_error(status, "ods info");
	}
	else if (res[0] == isc_info_ods_version)
	{
		USHORT len = isc_vax_integer (&res[1], 2);
		m_odsNumber = isc_vax_integer (&res[3], len);
	}
}

void NBackup::internal_unlock_database()
{
	if (m_silent)
	{
		ISC_STATUS_ARRAY temp;
		if (!isc_start_transaction(temp, &trans, 1, &newdb, 0, NULL))
		{
			if (isc_dsql_execute_immediate(temp, &newdb, &trans, 0, "ALTER DATABASE END BACKUP", 1, NULL))
				isc_rollback_transaction(temp, &trans);
			else if (isc_commit_transaction(temp, &trans))
				isc_rollback_transaction(temp, &trans);
		}
	}
	else
	{
		if (isc_start_transaction(status, &trans, 1, &newdb, 0, NULL))
			pr_error(status, "start transaction");
		if (isc_dsql_execute_immediate(status, &newdb, &trans, 0, "ALTER DATABASE END BACKUP", 1, NULL))
			pr_error(status, "end backup");
		if (isc_commit_transaction(status, &trans))
			pr_error(status, "end backup: commit");
	}
}

void NBackup::lock_database(bool get_size)
{
	attach_database();
	db_size_pages = 0;
	try {
		internal_lock_database();
		if (get_size)
		{
			get_database_size();
			if (db_size_pages && (!uSvc->isService()))
				printf("%d\n", db_size_pages);
		}
	}
	catch (const Exception&)
	{
		m_silent = true;
		detach_database();
		throw;
	}
	detach_database();
}

void NBackup::unlock_database()
{
	attach_database();
	try {
		internal_unlock_database();
	}
	catch (const Exception&)
	{
		m_silent = true;
		detach_database();
		throw;
	}
	detach_database();
}

void NBackup::backup_database(int level, Guid& guid, const PathName& fname)
{
	bool database_locked = false;
	// We set this flag when backup file is in inconsistent state
	bool delete_backup = false;
	ULONG prev_scn = 0;
	char prev_guid[GUID_BUFF_SIZE] = "";
	char str_guid[GUID_BUFF_SIZE] = "";
	Ods::pag* page_buff = NULL;
	attach_database();
	ULONG page_writes = 0, page_reads = 0;

	time_t start = time(NULL);
	struct tm today;
#ifdef HAVE_LOCALTIME_R
	if (!localtime_r(&start, &today))
	{
		// What to do here?
	}
#else
	{ //scope
		struct tm* times = localtime(&start);
		if (!times)
		{
			// What do to here?
		}
		today = *times;
	} //
#endif

	try {
		// Look for SCN and GUID of previous-level backup in history table
		if (level)
		{
			if (level < 0)
				GuidToString(str_guid, &guid);

			if (isc_start_transaction(status, &trans, 1, &newdb, 0, NULL))
				pr_error(status, "start transaction");
			char out_sqlda_data[XSQLDA_LENGTH(2)];
			XSQLDA *out_sqlda = (XSQLDA*)out_sqlda_data;
			out_sqlda->version = SQLDA_VERSION1;
			out_sqlda->sqln = 2;

			isc_stmt_handle stmt = 0;
			if (isc_dsql_allocate_statement(status, &newdb, &stmt))
				pr_error(status, "allocate statement");
			char str[200];
			if (level > 0)
			{
				sprintf(str, "SELECT RDB$GUID, RDB$SCN FROM RDB$BACKUP_HISTORY "
					"WHERE RDB$BACKUP_ID = "
					"(SELECT MAX(RDB$BACKUP_ID) FROM RDB$BACKUP_HISTORY "
					"WHERE RDB$BACKUP_LEVEL = %d)", level - 1);
			}
			else
			{
				sprintf(str, "SELECT RDB$GUID, RDB$SCN FROM RDB$BACKUP_HISTORY "
					"WHERE RDB$GUID = '%s'", str_guid);
			}
			if (isc_dsql_prepare(status, &trans, &stmt, 0, str, 1, NULL))
				pr_error(status, "prepare history query");
			if (isc_dsql_describe(status, &stmt, 1, out_sqlda))
				pr_error(status, "describe history query");
			short guid_null, scn_null;
			out_sqlda->sqlvar[0].sqlind = &guid_null;
			out_sqlda->sqlvar[0].sqldata = prev_guid;
			out_sqlda->sqlvar[1].sqlind = &scn_null;
			out_sqlda->sqlvar[1].sqldata = (char*) &prev_scn;
			if (isc_dsql_execute(status, &trans, &stmt, 1, NULL))
				pr_error(status, "execute history query");

			switch (isc_dsql_fetch(status, &stmt, 1, out_sqlda))
			{
			case 100: // No more records available
				if (level > 0)
				{
					status_exception::raise(Arg::Gds(isc_nbackup_lostrec_db) << database.c_str() <<
											Arg::Num(level - 1));
				}
				else
				{
					status_exception::raise(Arg::Gds(isc_nbackup_lostrec_guid_db) << database.c_str() <<
											Arg::Str(str_guid));
				}

			case 0:
				if (guid_null || scn_null)
					status_exception::raise(Arg::Gds(isc_nbackup_lostguid_db));
				prev_guid[sizeof(prev_guid) - 1] = 0;
				break;
			default:
				pr_error(status, "fetch history query");
			}
			isc_dsql_free_statement(status, &stmt, DSQL_close);
			if (isc_commit_transaction(status, &trans))
				pr_error(status, "commit history query");
		}

		// Lock database for backup
		internal_lock_database();
		database_locked = true;
		get_database_size();
		detach_database();

		if (fname.hasData())
			bakname = fname;
		else
		{
			// Let's generate nice new filename
			PathName begin, fil;
			PathUtils::splitLastComponent(begin, fil, database);
			if (level >= 0)
			{
				bakname.printf("%s-%d-%04d%02d%02d-%02d%02d.nbk", fil.c_str(), level,
					today.tm_year + 1900, today.tm_mon + 1, today.tm_mday,
					today.tm_hour, today.tm_min);
			}
			else
			{
				bakname.printf("%s-%s-%04d%02d%02d-%02d%02d.nbk", fil.c_str(), str_guid,
					today.tm_year + 1900, today.tm_mon + 1, today.tm_mday,
					today.tm_hour, today.tm_min);
			}
			if (!uSvc->isService())
				printf("%s\n", bakname.c_str()); // Print out generated filename for script processing
		}

		// Level 0 backup is a full reconstructed database image that can be
		// used directly after fixup. Incremenal backups of other levels are
		// consisted of header followed by page data. Each page is preceded
		// by 4-byte integer page number. Note: since ODS12 page header contains
		// page number, therefore no need to store page numbers before page image.

		// Actual IO is optimized to get maximum performance
		// from the IO subsystem while taking as little CPU time as possible

		// NOTE: this is still possible to improve performance by implementing
		// version using asynchronous unbuffered IO on NT series of OS.
		// But this task is for another day. 02 Aug 2003, Nickolay Samofatov.

		// Create backup file and open database file
		create_backup();
		delete_backup = true;

		open_database_scan();

		// Read database header
		char unaligned_header_buffer[RAW_HEADER_SIZE + SECTOR_ALIGNMENT];

		auto header = reinterpret_cast<Ods::header_page*>(
			FB_ALIGN(unaligned_header_buffer, SECTOR_ALIGNMENT));

		if (read_file(dbase, header, RAW_HEADER_SIZE) != RAW_HEADER_SIZE)
			status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdrdb) << dbname.c_str() << Arg::Num(1));

		if (!Ods::isSupported(header))
		{
			const USHORT ods_version = header->hdr_ods_version & ~ODS_FIREBIRD_FLAG;
			status_exception::raise(Arg::Gds(isc_wrong_ods) << Arg::Str(database.c_str()) <<
									Arg::Num(ods_version) <<
									Arg::Num(header->hdr_ods_minor) <<
									Arg::Num(ODS_VERSION) <<
									Arg::Num(ODS_CURRENT));
		}

		if ((header->hdr_flags & Ods::hdr_backup_mask) != Ods::hdr_nbak_stalled)
			status_exception::raise(Arg::Gds(isc_nbackup_db_notlock) << Arg::Num(header->hdr_flags));

		Array<UCHAR> unaligned_page_buffer;
		{ // scope
			UCHAR* buf = unaligned_page_buffer.getBuffer(header->hdr_page_size + SECTOR_ALIGNMENT);
			page_buff = reinterpret_cast<Ods::pag*>(FB_ALIGN(buf, SECTOR_ALIGNMENT));
		} // end scope

		ULONG db_size = db_size_pages;
		seek_file(dbase, 0);

		if (read_file(dbase, page_buff, header->hdr_page_size) != header->hdr_page_size)
			status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdrdb) << dbname.c_str() << Arg::Num(2));
		--db_size;
		page_reads++;

		Guid backup_guid;
		bool guid_found = false;
		auto p = reinterpret_cast<Ods::header_page*>(page_buff)->hdr_data;
		const auto end = reinterpret_cast<UCHAR*>(page_buff) + header->hdr_page_size;
		while (p < end && *p != Ods::HDR_end)
		{
			if (*p == Ods::HDR_backup_guid)
			{
				if (p[1] == sizeof(Guid))
				{
					memcpy(&backup_guid, p + 2, sizeof(Guid));
					guid_found = true;
				}
				break;
			}

			p += p[1] + 2;
		}

		if (!guid_found)
			status_exception::raise(Arg::Gds(isc_nbackup_lostguid_bk));

		// Write data to backup file
		ULONG backup_scn = header->hdr_header.pag_scn - 1;
		if (level)
		{
			inc_header bh;
			memcpy(bh.signature, backup_signature, sizeof(backup_signature));
			bh.version = BACKUP_VERSION;
			bh.level = level > 0 ? level : 0;
			bh.backup_guid = backup_guid;
			StringToGuid(&bh.prev_guid, prev_guid);
			bh.page_size = header->hdr_page_size;
			bh.backup_scn = backup_scn;
			bh.prev_scn = prev_scn;

			memset(page_buff, 0, header->hdr_page_size);
			memcpy(page_buff, &bh, sizeof(bh));
			write_file(backup, page_buff, header->hdr_page_size);
			page_writes++;

			seek_file(dbase, 0);
			if (read_file(dbase, page_buff, header->hdr_page_size) != header->hdr_page_size)
				status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdrdb) << dbname.c_str() << Arg::Num(2));
		}

		ULONG curPage = 0;
		ULONG lastPage = FIRST_PIP_PAGE;
		const ULONG pagesPerPIP = Ods::pagesPerPIP(header->hdr_page_size);

		ULONG scnsSlot = 0;
		const ULONG pagesPerSCN = Ods::pagesPerSCN(header->hdr_page_size);

		Array<UCHAR> unaligned_scns_buffer;
		Ods::scns_page* scns = NULL, *scns_buf = NULL;
		{ // scope
			UCHAR* buf = unaligned_scns_buffer.getBuffer(header->hdr_page_size + SECTOR_ALIGNMENT);
			scns_buf = reinterpret_cast<Ods::scns_page*>(FB_ALIGN(buf, SECTOR_ALIGNMENT));
		}

		while (true)
		{
			if (curPage && page_buff->pag_scn > backup_scn)
			{
				status_exception::raise(Arg::Gds(isc_nbackup_page_changed) << Arg::Num(curPage) <<
										Arg::Num(page_buff->pag_scn) << Arg::Num(backup_scn));
			}

			if (!level || page_buff->pag_scn > prev_scn)
			{
				write_file(backup, page_buff, header->hdr_page_size);
				page_writes++;
			}

			checkCtrlC(uSvc);

			if ((db_size_pages != 0) && (db_size == 0))
				break;

			if (level)
			{
				fb_assert(scnsSlot < pagesPerSCN);
				fb_assert(scns && scns->scn_sequence * pagesPerSCN + scnsSlot == curPage ||
						 !scns && curPage % pagesPerSCN == scnsSlot);

				ULONG nextSCN = scns ? (scns->scn_sequence + 1) * pagesPerSCN : FIRST_SCN_PAGE;

				while (true)
				{
					curPage++;
					scnsSlot++;
					if (!scns || scns->scn_pages[scnsSlot] > prev_scn ||
						scnsSlot == pagesPerSCN ||
						curPage == nextSCN ||
						curPage == lastPage)
					{
						seek_file(dbase, (SINT64) curPage * header->hdr_page_size);
						break;
					}
				}

				if (scnsSlot == pagesPerSCN)
				{
					scnsSlot = 0;
					scns = NULL;
				}

				fb_assert(scnsSlot < pagesPerSCN);
				fb_assert(scns && scns->scn_sequence * pagesPerSCN + scnsSlot == curPage ||
						 !scns && curPage % pagesPerSCN == scnsSlot);
			}
			else
				curPage++;


			const FB_SIZE_T bytesDone = read_file(dbase, page_buff, header->hdr_page_size);
			--db_size;
			page_reads++;
			if (bytesDone == 0)
				break;
			if (bytesDone != header->hdr_page_size)
				status_exception::raise(Arg::Gds(isc_nbackup_dbsize_inconsistent));

			if (level && page_buff->pag_type == pag_scns)
			{
				fb_assert(scnsSlot == 0 || scnsSlot == FIRST_SCN_PAGE);

				// pick up next SCN's page
				memcpy(scns_buf, page_buff, header->hdr_page_size);
				scns = scns_buf;
			}


			if (curPage == lastPage)
			{
				// Starting from ODS 11.1 we can expand file but never use some last
				// pages in it. There are no need to backup this empty pages. More,
				// we can't be sure its not used pages have right SCN assigned.
				// How many pages are really used we know from page_inv_page::pip_used
				// where stored number of pages allocated from this pointer page.
				if (page_buff->pag_type == pag_pages)
				{
					Ods::page_inv_page* pip = (Ods::page_inv_page*) page_buff;
					if (lastPage == FIRST_PIP_PAGE)
						lastPage = pip->pip_used - 1;
					else
						lastPage += pip->pip_used;

					if (pip->pip_used < pagesPerPIP)
						lastPage++;
				}
				else
				{
					fb_assert(page_buff->pag_type == pag_undefined);
					break;
				}
			}
		}
		close_database();
		close_backup();

		delete_backup = false; // Backup file is consistent. No need to delete it

		attach_database();
		// Write about successful backup to backup history table
		if (isc_start_transaction(status, &trans, 1, &newdb, 0, NULL))
			pr_error(status, "start transaction");

		char in_sqlda_data[XSQLDA_LENGTH(4)];
		XSQLDA *in_sqlda = (XSQLDA *)in_sqlda_data;
		in_sqlda->version = SQLDA_VERSION1;
		in_sqlda->sqln = 4;
		isc_stmt_handle stmt = 0;
		if (isc_dsql_allocate_statement(status, &newdb, &stmt))
			pr_error(status, "allocate statement");

		const char* insHistory;
		get_ods();
		if (m_odsNumber >= ODS_VERSION12)
		{
			insHistory =
				"INSERT INTO RDB$BACKUP_HISTORY(RDB$BACKUP_ID, RDB$TIMESTAMP, "
					"RDB$BACKUP_LEVEL, RDB$GUID, RDB$SCN, RDB$FILE_NAME) "
				"VALUES(NULL, 'NOW', ?, ?, ?, ?)";
		}
		else
		{
			insHistory =
				"INSERT INTO RDB$BACKUP_HISTORY(RDB$BACKUP_ID, RDB$TIMESTAMP, "
					"RDB$BACKUP_LEVEL, RDB$GUID, RDB$SCN, RDB$FILE_NAME) "
				"VALUES(GEN_ID(RDB$BACKUP_HISTORY, 1), 'NOW', ?, ?, ?, ?)";
		}

		if (isc_dsql_prepare(status, &trans, &stmt, 0, insHistory, 1, NULL))
		{
			pr_error(status, "prepare history insert");
		}
		if (isc_dsql_describe_bind(status, &stmt, 1, in_sqlda))
			pr_error(status, "bind history insert");
		short null_flag = 0;
		short null_ind = -1;
		if (level >= 0)
		{
			in_sqlda->sqlvar[0].sqldata = (char*) &level;
			in_sqlda->sqlvar[0].sqlind = &null_flag;
		}
		else
		{
			in_sqlda->sqlvar[0].sqldata = NULL;
			in_sqlda->sqlvar[0].sqlind = &null_ind;
		}
		char temp[GUID_BUFF_SIZE];
		GuidToString(temp, &backup_guid);
		in_sqlda->sqlvar[1].sqldata = temp;
		in_sqlda->sqlvar[1].sqlind = &null_flag;
		in_sqlda->sqlvar[2].sqldata = (char*) &backup_scn;
		in_sqlda->sqlvar[2].sqlind = &null_flag;

		char buff[256]; // RDB$FILE_NAME has length of 253
		FB_SIZE_T len = bakname.length();
		if (len > 253)
			len = 253;
		*(USHORT*) buff = len;
		memcpy(buff + 2, bakname.c_str(), len);
		in_sqlda->sqlvar[3].sqldata = buff;
		in_sqlda->sqlvar[3].sqlind = &null_flag;
		if (isc_dsql_execute(status, &trans, &stmt, 1, in_sqlda))
			pr_error(status, "execute history insert");
		isc_dsql_free_statement(status, &stmt, DSQL_drop);
		if (isc_commit_transaction(status, &trans))
			pr_error(status, "commit history insert");

	}
	catch (const Exception&)
	{
		m_silent = true;
		close_database();
		close_backup();

		if (delete_backup)
			remove(bakname.c_str());
		if (trans)
		{
			// Do not report a secondary exception
			//if (isc_rollback_transaction(status, &trans))
			//	pr_error(status, "rollback transaction");
			ISC_STATUS_ARRAY temp;
			isc_rollback_transaction(temp, &trans);
		}
		if (database_locked)
		{
			if (!newdb)
				attach_database();
			if (newdb)
				internal_unlock_database();
		}
		if (newdb)
			detach_database();
		throw;
	}

	if (!newdb)
		attach_database();
	internal_unlock_database();
	detach_database();

	time_t finish = time(NULL);
	double elapsed = difftime(finish, start);
	if (bakname != "stdout")
	{
		uSvc->printf(false, "time elapsed\t%.0f sec \npage reads\t%u \npage writes\t%u\n",
			elapsed, page_reads, page_writes);
	}
}

void NBackup::restore_database(const BackupFiles& files, bool repl_seq, bool inc_rest)
{
	// We set this flag when database file is in inconsistent state
	bool delete_database = false;
	const int filecount = files.getCount();

	if (!inc_rest)
	{
		create_database();
		delete_database = true;
	}

	try
	{
		Array<UCHAR> page_buffer;

		int curLevel = 0;
		Guid prev_guid;
		while (true)
		{
			if (!filecount)
			{
				while (true)
				{
					if (uSvc->isService())
						bakname = ".";
					else
					{
						//Enter name of the backup file of level %d (\".\" - do not restore further):\n
						printMsg(69, SafeArg() << curLevel);
						char temp[256];
						FB_UNUSED(scanf("%255s", temp));
						bakname = temp;
					}
					if (bakname == ".")
					{
						close_database();
						if (!curLevel)
						{
							remove(dbname.c_str());
							status_exception::raise(Arg::Gds(isc_nbackup_failed_lzbk));
						}
						fixup_database(repl_seq);
						return;
					}
					// Never reaches this point when run as service
					try {
						fb_assert(!uSvc->isService());
						open_backup_scan();
						break;
					}
					catch (const status_exception& e)
					{
						const ISC_STATUS* s = e.value();
						isc_print_status(s);
					}
					catch (const Exception& e) {
						fprintf(stderr, "%s\n", e.what());
					}
				}
			}
			else
			{
				if (curLevel >= filecount + (inc_rest ? 1 : 0))
				{
					close_database();
					fixup_database(repl_seq, inc_rest);

					m_silent = true;
					m_flash_map = true;
					run_db_triggers = false;

					attach_database();
					detach_database();

					return;
				}
				if (!inc_rest || curLevel)
					bakname = files[curLevel - (inc_rest ? 1 : 0)];
				if (!inc_rest || curLevel)
					open_backup_scan();
			}

			if (curLevel)
			{
				inc_header bakheader;
				if (read_file(backup, &bakheader, sizeof(bakheader)) != sizeof(bakheader))
					status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdrbk) << bakname.c_str());
				if (memcmp(bakheader.signature, backup_signature, sizeof(backup_signature)) != 0)
					status_exception::raise(Arg::Gds(isc_nbackup_invalid_incbk) << bakname.c_str());
				if (bakheader.version != BACKUP_VERSION)
				{
					status_exception::raise(Arg::Gds(isc_nbackup_unsupvers_incbk) <<
										Arg::Num(bakheader.version) << bakname.c_str());
				}
				if (bakheader.level && bakheader.level != curLevel)
				{
					status_exception::raise(Arg::Gds(isc_nbackup_invlevel_incbk) <<
						Arg::Num(bakheader.level) << bakname.c_str() << Arg::Num(curLevel));
				}
				// We may also add SCN check, but GUID check covers this case too
				if (memcmp(&bakheader.prev_guid, &prev_guid, sizeof(Guid)) != 0)
					status_exception::raise(Arg::Gds(isc_nbackup_wrong_orderbk) << bakname.c_str());

				// Emulate seek_file(backup, bakheader.page_size)
				// Backup is stream-oriented, if -decompress is used pipe can't be seek()'ed
				FB_SIZE_T left = bakheader.page_size - sizeof(bakheader);
				while (left)
				{
					char char_buf[1024];
					FB_SIZE_T step = left > sizeof(char_buf) ? sizeof(char_buf) : left;
					if (read_file(backup, &char_buf, step) != step)
						status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdrbk) << bakname.c_str());
					left -= step;
				}

				if (!inc_rest)
					delete_database = true;
				prev_guid = bakheader.backup_guid;
				const auto page_ptr = page_buffer.begin();
				while (true)
				{
					const FB_SIZE_T bytesDone = read_file(backup, page_ptr, bakheader.page_size);
					if (bytesDone == 0)
						break;
					if (bytesDone != bakheader.page_size) {
						status_exception::raise(Arg::Gds(isc_nbackup_err_eofbk) << bakname.c_str());
					}
					const SINT64 pageNum = reinterpret_cast<Ods::pag*>(page_ptr)->pag_pageno;
					seek_file(dbase, pageNum * bakheader.page_size);
					write_file(dbase, page_ptr, bakheader.page_size);
					checkCtrlC(uSvc);
				}
				delete_database = false;
			}
			else
			{
				if (!inc_rest)
				{
					// Use relatively small buffer to make use of prefetch and lazy flush
					char buffer[65536];
					while (true)
					{
						const FB_SIZE_T bytesRead = read_file(backup, buffer, sizeof(buffer));
						if (bytesRead == 0)
							break;
						write_file(dbase, buffer, bytesRead);
						checkCtrlC(uSvc);
					}
					seek_file(dbase, 0);
				}
				else
					open_database_write(true);

				// Read database header
				Ods::header_page header;
				if (read_file(dbase, &header, HDR_SIZE) != HDR_SIZE)
					status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdr_restdb) << Arg::Num(1));

				const auto page_ptr = page_buffer.getBuffer(header.hdr_page_size);

				seek_file(dbase, 0);

				if (read_file(dbase, page_ptr, header.hdr_page_size) != header.hdr_page_size)
					status_exception::raise(Arg::Gds(isc_nbackup_err_eofhdr_restdb) << Arg::Num(2));

				bool guid_found = false;
				auto p = reinterpret_cast<Ods::header_page*>(page_ptr)->hdr_data;
				const auto end = page_ptr + header.hdr_page_size;
				while (p < end && *p != Ods::HDR_end)
				{
					if (*p == Ods::HDR_backup_guid)
					{
						if (p[1] == sizeof(Guid))
						{
							memcpy(&prev_guid, p + 2, sizeof(Guid));
							guid_found = true;
						}
						break;
					}

					p += p[1] + 2;
				}
				if (!guid_found)
					status_exception::raise(Arg::Gds(isc_nbackup_lostguid_l0bk));
				// We are likely to have normal database here
				delete_database = false;
			}
			close_backup();
			curLevel++;
		}
	}
	catch (const Exception&)
	{
		m_silent = true;

		close_database();
		close_backup();

		if (delete_database)
			remove(dbname.c_str());
		throw;
	}
}

int NBACKUP_main(UtilSvc* uSvc)
{
	int exit_code = FB_SUCCESS;

	try {
		nbackup(uSvc);
	}
	catch (const status_exception& e)
	{
		if (!uSvc->isService())
		{
			const ISC_STATUS* s = e.value();
			isc_print_status(s);
		}

 		StaticStatusVector status;
 		e.stuffException(status);
 		uSvc->initStatus();
 		uSvc->setServiceStatus(status.begin());
		exit_code = FB_FAILURE;
	}
	catch (const Exception& e)
	{
		if (!uSvc->isService())
			fprintf(stderr, "%s\n", e.what());

 		StaticStatusVector status;
 		e.stuffException(status);
 		uSvc->initStatus();
 		uSvc->setServiceStatus(status.begin());
		exit_code = FB_FAILURE;
	}

	return exit_code;
}

enum NbOperation {nbNone, nbLock, nbUnlock, nbFixup, nbBackup, nbRestore};

void nbackup(UtilSvc* uSvc)
{
	UtilSvc::ArgvType& argv = uSvc->argv;
	const int argc = argv.getCount();

	NbOperation op = nbNone;
	string username, role, password;
	PathName database, filename;
	string decompress;
	bool run_db_triggers = true;
	bool direct_io =
#ifdef WIN_NT
		true;
#else
		false;
#endif
	NBackup::BackupFiles backup_files;
	int level = -1;
	Guid guid;
	bool print_size = false, version = false, inc_rest = false, repl_seq = false;
	string onOff;

	const Switches switches(nbackup_action_in_sw_table, FB_NELEM(nbackup_action_in_sw_table),
							false, true);

	// Read global command line parameters
	for (int itr = 1; itr < argc; ++itr)
	{
		// We must recognize all parameters here
		if (argv[itr][0] != '-') {
			usage(uSvc, isc_nbackup_unknown_param, argv[itr]);
		}

		const Switches::in_sw_tab_t* rc = switches.findSwitch(argv[itr]);
		if (!rc)
		{
			usage(uSvc, isc_nbackup_unknown_switch, argv[itr]);
			break;
		}

		switch (rc->in_sw)
		{
		case IN_SW_NBK_USER_NAME:
			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			username = argv[itr];
			break;

		case IN_SW_NBK_ROLE:
			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			role = argv[itr];
			break;

		case IN_SW_NBK_PASSWORD:
			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			password = argv[itr];
			uSvc->hidePasswd(argv, itr);
			break;

		case IN_SW_NBK_NODBTRIG:
			run_db_triggers = false;
			break;

		case IN_SW_NBK_DIRECT:
 			if (++itr >= argc)
 				missingParameterForSwitch(uSvc, argv[itr - 1]);

 			onOff = argv[itr];
 			onOff.upper();
 			if (onOff == "ON")
 				direct_io = true;
 			else if (onOff == "OFF")
 				direct_io = false;
 			else
 				usage(uSvc, isc_nbackup_switchd_parameter, onOff.c_str());
			break;

		case IN_SW_NBK_DECOMPRESS:
 			if (++itr >= argc)
 				missingParameterForSwitch(uSvc, argv[itr - 1]);

 			decompress = argv[itr];
			break;

		case IN_SW_NBK_FIXUP:
			if (op != nbNone)
				singleAction(uSvc);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			database = argv[itr];
			op = nbFixup;
			break;

		case IN_SW_NBK_FETCH:
			if (uSvc->isService())
				usage(uSvc, isc_nbackup_nofetchpw_svc);
			else
			{
				if (++itr >= argc)
					missingParameterForSwitch(uSvc, argv[itr - 1]);

				const char* passwd = NULL;
				if (fb_utils::fetchPassword(argv[itr], passwd) != fb_utils::FETCH_PASS_OK)
				{
					usage(uSvc, isc_nbackup_pwfile_error, argv[itr]);
					break;
				}
				password = passwd;
			}
			break;


		case IN_SW_NBK_LOCK:
			if (op != nbNone)
				singleAction(uSvc);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			database = argv[itr];
			op = nbLock;
			break;

		case IN_SW_NBK_UNLOCK:
			if (op != nbNone)
				singleAction(uSvc);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			database = argv[itr];
			op = nbUnlock;
			break;

		case IN_SW_NBK_BACKUP:
			if (op != nbNone)
				singleAction(uSvc);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			if (argv[itr][0] == '{')
				StringToGuid(&guid, argv[itr]);
			else
				level = atoi(argv[itr]);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 2]);

			database = argv[itr];

			if (itr + 1 < argc)
				filename = argv[++itr];

			op = nbBackup;
			break;

		case IN_SW_NBK_RESTORE:
			if (op != nbNone)
				singleAction(uSvc);

			if (++itr >= argc)
				missingParameterForSwitch(uSvc, argv[itr - 1]);

			database = argv[itr];
			while (++itr < argc)
				backup_files.push(argv[itr]);

			op = nbRestore;
			break;

		case IN_SW_NBK_SIZE:
			print_size = true;
			break;

		case IN_SW_NBK_HELP:
			if (uSvc->isService())
				usage(uSvc, isc_nbackup_unknown_switch, argv[itr]);
			else
				usage(uSvc, 0);
			break;

		case IN_SW_NBK_VERSION:
			if (uSvc->isService())
				usage(uSvc, isc_nbackup_unknown_switch, argv[itr]);
			else
				version = true;
			break;

		case IN_SW_NBK_INPLACE:
			inc_rest = true;
			break;

		case IN_SW_NBK_SEQUENCE:
			repl_seq = true;
			break;

		default:
			usage(uSvc, isc_nbackup_unknown_switch, argv[itr]);
			break;
		}
	}

	if (version)
	{
		printMsg(68, SafeArg() << FB_VERSION);
	}

	if (op == nbNone)
	{
		if (!version)
		{
			usage(uSvc, isc_nbackup_no_switch);
		}
		exit(FINI_OK);
	}

	if (print_size && op != nbLock)
	{
		usage(uSvc, isc_nbackup_size_with_lock);
	}

	if (repl_seq && op != nbFixup && op != nbRestore)
	{
		usage(uSvc, isc_nbackup_seq_misuse);
	}

	NBackup nbk(uSvc, database, username, role, password, run_db_triggers, direct_io, decompress);
	try
	{
		switch (op)
		{
			case nbLock:
				nbk.lock_database(print_size);
				break;

			case nbUnlock:
				nbk.unlock_database();
				break;

			case nbFixup:
				nbk.fixup_database(repl_seq);
				break;

			case nbBackup:
				nbk.backup_database(level, guid, filename);
				break;

			case nbRestore:
				nbk.restore_database(backup_files, repl_seq, inc_rest);
				break;
		}
	}
	catch (const Exception& e)
	{
		if (!uSvc->isService() && !nbk.printed())
		{
	 		StaticStatusVector status;
			e.stuffException(status);
			isc_print_status(status.begin());
		}

		throw;
	}
}
