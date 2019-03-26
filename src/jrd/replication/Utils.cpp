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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/GenericMap.h"
#include "../common/config/config_file.h"
#include "../common/isc_proto.h"
#include "../common/isc_f_proto.h"
#include "../common/utils_proto.h"
#include "../common/ScanDir.h"
#include "../common/os/mod_loader.h"
#include "../common/os/path_utils.h"
#include "../jrd/constants.h"

#include "Utils.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef WIN_NT
#include <Shellapi.h>
#endif

#include <stdlib.h>
#include <time.h>

using namespace Firebird;
using namespace Replication;

namespace
{
	// Must match items inside enum LogMsgType
	const char* LOG_MSG_TYPES[] = {
		"ERROR",	// LogMsgType::ERROR_MSG
		"WARNING",	// LogMsgType::WARNING_MSG
		"VERBOSE",	// LogMsgType::VERBOSE_MSG
		"DEBUG"		// LogMsgType::DEBUG_MSG
	};

	const char* REPLICATION_LOGFILE = "replication.log";

	class LogWriter : private GlobalStorage
	{
	public:
		LogWriter()
			: m_hostname(getPool()),
			  m_filename(getPool(), fb_utils::getPrefix(IConfigManager::DIR_LOG, REPLICATION_LOGFILE))
		{
			char host[BUFFER_LARGE];
			ISC_get_host(host, sizeof(host));
			m_hostname = host;
#ifdef WIN_NT
			m_mutex = CreateMutex(NULL, FALSE, "firebird_repl_mutex");
#endif
		}

		~LogWriter()
		{
#ifdef WIN_NT
			CloseHandle(m_mutex);
#endif
		}

		void logMessage(const string& source, const PathName& database,
						LogMsgType type, const string& message)
		{
			const time_t now = time(NULL);

			const auto file = os_utils::fopen(m_filename.c_str(), "a");
			if (file)
			{
				if (!lock(file))
				{
					fclose(file);
					return;
				}

				fseek(file, 0, SEEK_END);
				fprintf(file, "\n%s (%s) %s\tDatabase: %s\n\t%s: %s\n",
						m_hostname.c_str(), source.c_str(), ctime(&now),
						database.c_str(), LOG_MSG_TYPES[type], message.c_str());
				fclose(file);
				unlock();
			}
		}

	private:
		bool lock(FILE* file)
		{
#ifdef WIN_NT
			return (WaitForSingleObject(m_mutex, INFINITE) == WAIT_OBJECT_0);
#else
#ifdef HAVE_FLOCK
			if (flock(fileno(file), LOCK_EX))
#else
			if (os_utils::lockf(fileno(file), F_LOCK, 0))
#endif
			{
				return false;
			}

			return true;
#endif
		}

		void unlock()
		{
#ifdef WIN_NT
			ReleaseMutex(m_mutex);
#endif
		}

		string m_hostname;
		const PathName m_filename;
#ifdef WIN_NT
		HANDLE m_mutex;
#endif
	};

	void logMessage(const string& source, const PathName& database,
					const string& message, LogMsgType type)
	{
		static LogWriter g_writer;

		g_writer.logMessage(source, database, type, message);
	}

} // namespace

namespace Replication
{
	void raiseError(const char* msg, ...)
	{
		char buffer[BUFFER_LARGE];

		va_list ptr;
		va_start(ptr, msg);
		vsprintf(buffer, msg, ptr);
		va_end(ptr);

		Arg::StatusVector error;
		error << Arg::Gds(isc_random) << Arg::Str(buffer);
		error.raise();
	}

	int executeShell(const string& command)
	{
#ifdef WIN_NT
		string params;
		params.printf("/c %s", command.c_str());
		SHELLEXECUTEINFO seInfo = {0};
		seInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		seInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		seInfo.hwnd = NULL;
		seInfo.lpVerb = NULL;
		seInfo.lpFile = "cmd.exe";
		seInfo.lpParameters = params.c_str();
		seInfo.lpDirectory = NULL;
		seInfo.nShow = SW_HIDE;
		seInfo.hInstApp = NULL;
		ShellExecuteEx(&seInfo);
		WaitForSingleObject(seInfo.hProcess, INFINITE);
		DWORD exitCode = 0;
		GetExitCodeProcess(seInfo.hProcess, &exitCode);
		return (int) exitCode;
#else
		return system(command.c_str());
#endif
	}

	void logOriginMessage(const PathName& database,
						  const string& message,
						  LogMsgType type)
	{
		logMessage("origin", database, message, type);
	}

	void logReplicaMessage(const PathName& database,
						  const string& message,
						  LogMsgType type)
	{
		logMessage("replica", database, message, type);
	}

} // namespace
