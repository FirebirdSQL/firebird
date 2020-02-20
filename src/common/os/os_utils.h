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

#include <sys/stat.h>

#ifdef WIN_NT

#define mode_t int
#define DEFAULT_OPEN_MODE (_S_IREAD | _S_IWRITE)
#else
#define DEFAULT_OPEN_MODE (0666)
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
