/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		PluginLogWriter.h
 *	DESCRIPTION:	Plugin log writer implementation
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
*/

#ifndef PLUGINLOGWRITER_H
#define PLUGINLOGWRITER_H


#include "firebird.h"
#include "../../jrd/ntrace.h"
#include "../../common/classes/timestamp.h"
#include "../../common/isc_s_proto.h"
#include "../../common/os/path_utils.h"
#include "../../common/classes/ImplementHelper.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


// Empty header. We need it to get shared mutex using SharedMemory
struct PluginLogWriterHeader : public Firebird::MemoryHeader
{
};

class PluginLogWriter FB_FINAL :
	public Firebird::RefCntIface<Firebird::ITraceLogWriterImpl<PluginLogWriter, Firebird::CheckStatusWrapper> >,
	public Firebird::IpcObject
{
public:
	PluginLogWriter(const char* fileName, size_t maxSize);
	~PluginLogWriter();

	// TraceLogWriter implementation
	virtual FB_SIZE_T write(const void* buf, FB_SIZE_T size);
	virtual FB_SIZE_T write_s(Firebird::CheckStatusWrapper* status, const void* buf, unsigned size);

	virtual int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

private:
	SINT64 seekToEnd();
	void reopen();
	void checkErrno(const char* operation);

	// Windows requires explicit syncronization when few processes appends to the
	// same file simultaneously, therefore we used our fastMutex for this
	// purposes. On Posix's platforms we honour O_APPEND flag which works
	// better as in this case syncronization is performed by OS kernel itself.
	// Mutex on Posix is needed to rotate log file.

	void mutexBug(int osErrorCode, const char* text);
	bool initialize(Firebird::SharedMemoryBase*, bool);

	void lock();
	void unlock();

	class Guard
	{
	public:
		explicit Guard(PluginLogWriter* log) : m_log(log)
		{
			if (m_log)
				m_log->lock();
		}

		~Guard()
		{
			if (m_log)
				m_log->unlock();
		}

	private:
		PluginLogWriter* m_log;
	};

	Firebird::PathName m_fileName;
	int		 m_fileHandle;
	size_t	 m_maxSize;
	Firebird::AutoPtr<Firebird::SharedMemory<PluginLogWriterHeader> > m_sharedMemory;
};

#endif // PLUGINLOGWRITER_H
