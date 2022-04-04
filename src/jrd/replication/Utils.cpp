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
	// Must match items inside enum LogMsgSide
	const char* LOG_MSG_SIDES[] = {
		"primary",	// LogMsgSide::PRIMARY_SIDE
		"replica",	// LogMsgSide::REPLICA_SIDE
	};

	// Must match items inside enum LogMsgType
	const char* LOG_MSG_TYPES[] = {
		"ERROR",	// LogMsgType::ERROR_MSG
		"WARNING",	// LogMsgType::WARNING_MSG
		"VERBOSE"	// LogMsgType::VERBOSE_MSG
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

		void logMessage(LogMsgSide side, LogMsgType type,
						const PathName& database,
						const string& message)
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

				string dbname, text;

				if (database.hasData())
					dbname.printf("Database: %s\n\t", database.c_str());

				text.printf("\n%s (%s) %s\t%s%s: %s\n",
							m_hostname.c_str(), LOG_MSG_SIDES[side], ctime(&now),
							dbname.c_str(), LOG_MSG_TYPES[type], message.c_str());

				fseek(file, 0, SEEK_END);
				fputs(text.c_str(), file);
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

	void logMessage(LogMsgSide side, LogMsgType type,
					const PathName& database,
					const string& message)
	{
		static LogWriter g_writer;

		g_writer.logMessage(side, type, database, message);
	}

	void logStatus(LogMsgSide side, LogMsgType type,
				   const PathName& database,
				   const ISC_STATUS* status)
	{
		string message;
		char temp[BUFFER_LARGE];

		while (fb_interpret(temp, sizeof(temp), &status))
		{
			if (!message.isEmpty())
				message += "\n\t";

			message += temp;
		}

		logMessage(side, type, database, message);
	}

	void logStatus(LogMsgSide side,
				   const PathName& database,
				   const CheckStatusWrapper* status)
	{
		const auto state = status->getState();

		if (state & IStatus::STATE_WARNINGS)
			logStatus(side, WARNING_MSG, database, status->getWarnings());

		if (state & IStatus::STATE_ERRORS)
			logStatus(side, ERROR_MSG, database, status->getErrors());
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

	void logPrimaryError(const PathName& database, const string& message)
	{
		logMessage(PRIMARY_SIDE, ERROR_MSG, database, message);
	}

	void logPrimaryWarning(const PathName& database, const string& message)
	{
		logMessage(PRIMARY_SIDE, WARNING_MSG, database, message);
	}

	void logPrimaryStatus(const PathName& database, const CheckStatusWrapper* status)
	{
		logStatus(PRIMARY_SIDE, database, status);
	}

	void logReplicaError(const PathName& database, const string& message)
	{
		logMessage(REPLICA_SIDE, ERROR_MSG, database, message);
	}

	void logReplicaWarning(const PathName& database, const string& message)
	{
		logMessage(REPLICA_SIDE, WARNING_MSG, database, message);
	}

	void logReplicaStatus(const PathName& database, const CheckStatusWrapper* status)
	{
		logStatus(REPLICA_SIDE, database, status);
	}

	void logReplicaVerbose(const PathName& database, const string& message)
	{
		logMessage(REPLICA_SIDE, VERBOSE_MSG, database, message);
	}

} // namespace
