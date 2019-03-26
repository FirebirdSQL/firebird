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


#ifndef JRD_REPLICATION_UTILS_H
#define JRD_REPLICATION_UTILS_H

#include "../common/classes/fb_string.h"

#ifdef WIN_NT
#include <io.h>
#endif

#include <stdio.h>

namespace Replication
{
	enum LogMsgType
	{
		ERROR_MSG = 0,
		WARNING_MSG,
		VERBOSE_MSG,
		DEBUG_MSG
	};

	void raiseError(const char* msg, ...);
	int executeShell(const Firebird::string& command);

	void logOriginMessage(const Firebird::PathName& database,
						  const Firebird::string& message,
						  LogMsgType type);

	void logReplicaMessage(const Firebird::PathName& database,
						   const Firebird::string& message,
						   LogMsgType type);

	class AutoFile
	{
	public:
		explicit AutoFile(int fd)
			: m_handle(fd)
		{}

		~AutoFile()
		{
			release();
		}

		operator int() const
		{
			return m_handle;
		}

		void release()
		{
			if (m_handle)
			{
				::close(m_handle);
				m_handle = 0;
			}
		}

	protected:
		int m_handle;
	};
}

#endif // JRD_REPLICATION_UTILS_H
