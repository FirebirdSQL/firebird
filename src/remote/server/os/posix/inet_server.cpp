/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		inet_server.cpp
 *	DESCRIPTION:	Internet remote server.
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "SGI" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "../common/isc_proto.h"
#include "../common/os/divorce.h"
#include "ibase.h"
#include "../common/classes/init.h"
#include "../common/config/config.h"
#include "../common/os/fbsyslog.h"
#include "../common/os/os_utils.h"
#include "../common/status.h"
#include <sys/param.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <errno.h>
#include "ibase.h"

#include "../remote/remote.h"
#include "../jrd/license.h"
#include "../common/file_params.h"
#include "../remote/inet_proto.h"
#include "../remote/server/serve_proto.h"
#include "../remote/server/ReplServer.h"
#include "../yvalve/gds_proto.h"
#include "../common/utils_proto.h"
#include "../common/classes/fb_string.h"

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../auth/SecurityDatabase/LegacyServer.h"
#include "../auth/trusted/AuthSspi.h"
#include "../auth/SecureRemotePassword/server/SrpServer.h"
#include "../jrd/replication/Config.h"

#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#else
#include <signal.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif


#include "../common/classes/semaphore.h"

const char* TEMP_DIR = "/tmp";

static void set_signal(int, void (*)(int));
static void signal_handler(int);

static TEXT protocol[128];
static int INET_SERVER_start = 0;

#if defined(HAVE_SETRLIMIT) && defined(HAVE_GETRLIMIT)
#define FB_RAISE_LIMITS 1
static void raiseLimit(int resource);
#endif

static void logSecurityDatabaseError(const char* path, ISC_STATUS* status)
{
	// If I/O error happened then rather likely we just miss standard security DB
	// Since FB3 with its multiple security databases - not too big trouble
	if (fb_utils::containsErrorCode(status, isc_io_error))
		return;

	Firebird::Syslog::Record(Firebird::Syslog::Error, "Security database error");
	gds__log_status(path, status);
	if (isatty(2))
	{
		gds__put_error(path);
		gds__print_status(status);
	}

	const int SHUTDOWN_TIMEOUT = 5000;  // 5 sec
	fb_shutdown(SHUTDOWN_TIMEOUT, fb_shutrsn_exit_called);
	exit(STARTUP_ERROR);
}

static int closePort(const int reason, const int, void* arg)
{
	if (reason == fb_shutrsn_signal)
	{
		kill(getpid(), 15);		// make select() die in select_wait()
	}

	return 0;
}

bool check_fd(int fd)
{
    return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

extern "C" {

int CLIB_ROUTINE main( int argc, char** argv)
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Run the server with apollo mailboxes.
 *
 **************************************/
	try
	{
		RemPortPtr port;

		// We should support 3 modes:
		// 1. Standalone single-process listener (like SS).
		// 2. Standalone listener, forking on each packet accepted (look -s switch in CS).
		// 3. Process spawned by (x)inetd (like CS).
		bool classic = false;
		bool standaloneClassic = false;
		bool super = false;

		int replPid = 0;

		// It's very easy to detect that we are spawned - just check fd 0 to be a socket.
		const int channel = 0;
		struct STAT stat0;
		if (os_utils::fstat(channel, &stat0) == 0 && S_ISSOCK(stat0.st_mode))
		{
			// classic server mode
			classic = true;
		}

		const TEXT* const* const end = argc + argv;
		argv++;
		bool debug = false;
		USHORT INET_SERVER_flag = 0;
		protocol[0] = 0;

		bool done = false;

		while (argv < end)
		{
			TEXT c;
			const TEXT* p = *argv++;

			if (*p++ == '-')
			{
				while ((c = *p++))
				{
					switch (UPPER(c))
					{
					case 'D':
						debug = true;
						break;

					case 'E':
						if (argv < end)
						{
							if (ISC_set_prefix(p, *argv) == -1)
								printf("Invalid argument Ignored\n");
							else
								argv++;	// do not skip next argument if this one is invalid
						}
						else
						{
							printf("Missing argument, switch -E ignored\n");
						}
						done = true;
						break;

					case 'P':
						if (argv < end)
						{
							if (!classic)
							{
								fb_utils::snprintf(protocol, sizeof(protocol), "/%s", *argv++);
							}
							else
							{
								gds__log("Switch -P ignored in CS mode\n");
							}
						}
						else
						{
							printf("Missing argument, switch -P ignored\n");
						}
						break;

		            case 'H':
					case '?':
						printf("Firebird TCP/IP server options are:\n");
						printf("  -d        : debug on\n");
						printf("  -p <port> : specify port to listen on\n");
						printf("  -z        : print version and exit\n");
						printf("  -h|?      : print this help\n");
		                printf("\n");
		                printf("  (The following -e options used to be -h options)\n");
						printf("  -e <firebird_root_dir>   : set firebird_root path\n");
						printf("  -el <firebird_lock_dir>  : set runtime firebird_lock dir\n");
						printf("  -em <firebird_msg_dir>   : set firebird_msg dir path\n");

						exit(FINI_OK);

					case 'Z':
						printf("Firebird TCP/IP server version %s\n", FB_VERSION);
						exit(FINI_OK);

					default:
						printf("Unknown switch '%c', ignored\n", c);
						break;
					}
					if (done)
						break;
				}
			}
		}

		if (Firebird::Config::getServerMode() == Firebird::MODE_CLASSIC)
		{
			if (!classic)
				standaloneClassic = true;
		}
		else
		{
			if (classic)
			{
				gds__log("Server misconfigured - to start it from (x)inetd add ServerMode=Classic to firebird.conf");
				Firebird::Syslog::Record(Firebird::Syslog::Error, "Server misconfigured - add ServerMode=Classic to firebird.conf");
				exit(STARTUP_ERROR);
			}
			INET_SERVER_flag |= SRVR_multi_client;
			super = true;
		}

		{	// scope
			Firebird::MasterInterfacePtr master;
			master->serverMode(super ? 1 : 0);
		}

		if (debug)
		{
			INET_SERVER_flag |= SRVR_debug;
		}

		// activate paths set with -e family of switches
		ISC_set_prefix(0, 0);

		// ignore some signals
		set_signal(SIGPIPE, signal_handler);
		set_signal(SIGUSR1, signal_handler);
		set_signal(SIGUSR2, signal_handler);

		// First of all change directory to tmp
		if (chdir(TEMP_DIR))
		{
			// error on changing the directory
			gds__log("Could not change directory to %s due to errno %d", TEMP_DIR, errno);
		}

#ifdef FB_RAISE_LIMITS

#ifdef RLIMIT_NPROC
		raiseLimit(RLIMIT_NPROC);
#endif

#if !(defined(DEV_BUILD))
		if (Firebird::Config::getBugcheckAbort())
#endif
		{
			// try to force core files creation
			raiseLimit(RLIMIT_CORE);
		}

#ifdef LINUX
		// instruct kernel to include shared memory regions into core dump
		FILE* coreproc = fopen("/proc/self/coredump_filter", "r+");
		if (coreproc)
		{
			fprintf(coreproc, "0x3f\n");
			fclose(coreproc);
		}
#endif

#if (defined SOLARIS || defined HPUX || defined LINUX)
		if (super)
		{
			// Increase max open files to hard limit for Unix
			// platforms which are known to have low soft limits.

			raiseLimit(RLIMIT_NOFILE);
		}
#endif // Unix platforms

#endif // FB_RAISE_LIMITS

#ifdef HAVE_LOCALE_H
		// Pick up the system locale to allow SYSTEM<->UTF8 conversions inside the engine
		setlocale(LC_CTYPE, "");
#endif

		if (!(debug || classic))
		{
			// Keep stdout and stderr openened always. We decide allow output
			// from binary or redirect it according to config
			int mask = 0; // FD_ZERO(&mask);
			mask |= (1 << 1 | 1 << 2); // FD_SET(1, &mask); FD_SET(2, &mask);
			divorce_terminal(mask);
		}

		// check firebird.conf presence - must be for server
		if (Firebird::Config::missFirebirdConf())
		{
			Firebird::Syslog::Record(Firebird::Syslog::Error, "Missing master config file firebird.conf");
			exit(STARTUP_ERROR);
		}

		if (!debug)
		{
			const char* redirection_file = Firebird::Config::getOutputRedirectionFile();

			int stdout_no = fileno(stdout);
			int stderr_no = fileno(stderr);
			const char* dev_null_file = "/dev/null";
			bool keep_as_is = !redirection_file ||
				(redirection_file && (strcmp(redirection_file, "-") == 0 || strcmp(redirection_file, "") == 0));

			// guard close all fds to properly demonize. Detect this case
			// and if we spawned from daemon we reopen stdout and stderr
			// and redirect it to /dev/null if user want us to print to stdout
			if ((!check_fd(stdout_no) || !check_fd(stderr_no)) && keep_as_is)
			{
				redirection_file = dev_null_file;
				keep_as_is = false;
			}

			if (!keep_as_is)
			{
				int f = open(redirection_file, O_CREAT|O_APPEND|O_WRONLY, 0644);

				if (f >= 0)
				{

					if (f != stdout_no)
						dup2(f, stdout_no);

					if (f != stderr_no)
						dup2(f, stderr_no);

					if (f != stdout_no && f != stderr_no)
						close(f);
				}
				else
					gds__log("Unable to open file %s for output redirection", redirection_file);
			}
		}

		if (super || standaloneClassic)
		{
			if (standaloneClassic && Replication::Config::hasReplicas())
			{
				// Start the replication server now (in the forked process),
				// because INET_connect() never returns for the standalone Classic

				if ((replPid = fork()) <= 0)
				{
					try
					{
						if (replPid) // failed fork attempt
							Firebird::system_error::raise("fork", replPid);

						// We've been forked successfully
						Firebird::FbLocalStatus localStatus;
						if (!REPL_server(&localStatus, true))
							localStatus.check();
					}
					catch (const Firebird::Exception& ex)
					{
						const char* const errorMsg = "Replication server startup error";
						iscLogException(errorMsg, ex);
						Firebird::Syslog::Record(Firebird::Syslog::Error, errorMsg);
					}

					if (!replPid)
					{
						fb_shutdown(10000, fb_shutrsn_exit_called);
						return FINI_OK;
					}
				}
			}

			// Start the network listener

			try
			{
				port = INET_connect(protocol, 0, INET_SERVER_flag, 0, NULL);
			}
			catch (const Firebird::Exception& ex)
			{
				iscLogException("INET server startup error", ex);
				exit(STARTUP_ERROR);
			}

			// If INET_connect() returns NULL for the standalone classic, then game is over.
			// Signal the forked replication server process to terminate and then exit.

			if (!port && replPid > 0) // this implies standaloneClassic being true
			{
				if (!kill(replPid, SIGTERM))
				{
					int status = 0;

					// Wait up to one second for the replicator process to finish gracefully
					for (unsigned n = 0; n < 10; n++)
					{
						Thread::sleep(100); // milliseconds

						const auto res = waitpid(replPid, &status, WNOHANG);

						if (res == replPid) // process is terminated
							break;

						if (res < 0 && !SYSCALL_INTERRUPTED(errno)) // error
							break;

						// continue waiting otherwise
					}

					// Force terminating the replicator process if it's still alive
					if (!WIFEXITED(status))
						kill(replPid, SIGKILL);
				}

				fb_shutdown(10000, fb_shutrsn_exit_called);
				return FINI_OK;
			}
		}

		if (classic)
		{
			port = INET_server(channel);
			if (!port)
			{
				gds__log("Unable to start INET_server");
				Firebird::Syslog::Record(Firebird::Syslog::Error, "Unable to start INET_server");
				exit(STARTUP_ERROR);
			}
		}

		{ // scope for interface ptr
			Firebird::PluginManagerInterfacePtr pi;
			Auth::registerSrpServer(pi);
		}

		if (super)
		{
			// Server tries to attach to security2.fdb to make sure everything is OK
			// This code fixes bug# 8429 + all other bug of that kind - from
			// now on the server exits if it cannot attach to the database
			// (wrong or no license, not enough memory, etc.

			ISC_STATUS_ARRAY status;
			isc_db_handle db_handle = 0L;

			const Firebird::RefPtr<const Firebird::Config> defConf(Firebird::Config::getDefaultConfig());
			const char* path = defConf->getSecurityDatabase();
			const char dpb[] = {isc_dpb_version1, isc_dpb_sec_attach, 1, 1, isc_dpb_address_path, 0};

			isc_attach_database(status, strlen(path), path, &db_handle, sizeof dpb, dpb);
			if (status[0] == 1 && status[1] > 0)
			{
				logSecurityDatabaseError(path, status);
			}
			else
			{
				isc_detach_database(status, &db_handle);
				if (status[0] == 1 && status[1] > 0)
				{
					logSecurityDatabaseError(path, status);
				}
			}

			// Start replication server

			Firebird::FbLocalStatus localStatus;
			if (!REPL_server(&localStatus, false))
			{
				const char* const errorMsg = "Replication server startup error";
				iscLogStatus(errorMsg, localStatus->getErrors());
				Firebird::Syslog::Record(Firebird::Syslog::Error, errorMsg);
			}
		}

		fb_shutdown_callback(NULL, closePort, fb_shut_exit, port);

		SRVR_multi_thread(port, INET_SERVER_flag);

		// perform atexit shutdown here when all globals in embedded library are active
		// also sync with possibly already running shutdown in dedicated thread
		fb_shutdown(10000, fb_shutrsn_exit_called);

		return FINI_OK;
	}
	catch (const Firebird::Exception& ex)
	{
 		Firebird::StaticStatusVector st;
		ex.stuffException(st);

		char s[100];
		const ISC_STATUS* status = st.begin();
		fb_interpret(s, sizeof(s), &status);

		iscLogException("Firebird startup error:", ex);
		Firebird::Syslog::Record(Firebird::Syslog::Error, "Firebird startup error");
		Firebird::Syslog::Record(Firebird::Syslog::Error, s);

		exit(STARTUP_ERROR);
	}
}

} // extern "C"


static void set_signal(int signal_number, void (*handler) (int))
{
/**************************************
 *
 *	s e t _ s i g n a l
 *
 **************************************
 *
 * Functional description
 *	Establish signal handler.
 *
 **************************************/
#ifdef UNIX
	struct sigaction vec, old_vec;

	vec.sa_handler = handler;
	sigemptyset(&vec.sa_mask);
	vec.sa_flags = 0;
	sigaction(signal_number, &vec, &old_vec);
#endif
}


static void signal_handler(int)
{
/**************************************
 *
 *	s i g n a l _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Dummy signal handler.
 *
 **************************************/

	++INET_SERVER_start;
}


#ifdef FB_RAISE_LIMITS
static void raiseLimit(int resource)
{
	struct rlimit lim;

	if (os_utils::getrlimit(resource, &lim) == 0)
	{
		if (lim.rlim_cur != lim.rlim_max)
		{
			lim.rlim_cur = lim.rlim_max;
			if (os_utils::setrlimit(resource, &lim) != 0)
			{
				gds__log("setrlimit() failed, errno=%d", errno);
			}
		}
	}
	else
	{
		gds__log("getrlimit() failed, errno=%d", errno);
	}
}
#endif // FB_RAISE_LIMITS
