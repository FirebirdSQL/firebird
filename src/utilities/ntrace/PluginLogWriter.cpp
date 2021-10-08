/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		PluginLogWriter.cpp
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

#include "PluginLogWriter.h"
#include "../common/isc_proto.h"
#include "../common/classes/init.h"
#include "../common/ThreadStart.h"
#include "../common/file_params.h"

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

using namespace Firebird;

// seems to only be Solaris 9 that doesn't have strerror_r,
// maybe we can remove this in the future
#ifndef HAVE_STRERROR_R
void strerror_r(int err, char* buf, size_t bufSize)
{
	static Firebird::GlobalPtr<Firebird::Mutex> mutex;
	Firebird::MutexLockGuard guard(mutex, FB_FUNCTION);
	strncpy(buf, strerror(err), bufSize);
}
#endif

void getMappedFileName(PathName& file, PathName& mapFile)
{
	const ULONG hash = file.hash(0xFFFFFFFF);
	mapFile.printf("%s_%08x", FB_TRACE_LOG_MUTEX, hash);
}

PluginLogWriter::PluginLogWriter(const char* fileName, size_t maxSize) :
	m_fileName(*getDefaultMemoryPool()),
	m_fileHandle(-1),
	m_maxSize(maxSize),
	m_sharedMemory(NULL)
{
	m_fileName = fileName;

	PathName logFile(fileName);
	PathName mapFile;
	getMappedFileName(logFile, mapFile);

	try
	{
		m_sharedMemory.reset(FB_NEW_POOL(getPool())
			SharedMemory<PluginLogWriterHeader>(mapFile.c_str(), sizeof(PluginLogWriterHeader), this));
	}
	catch (const Exception& ex)
	{
		iscLogException("PluginLogWriter: Cannot initialize the shared memory region", ex);
		throw;
	}

#ifdef WIN_NT
	Guard guard(this);
#endif

	reopen();
}

PluginLogWriter::~PluginLogWriter()
{
	if (m_fileHandle != -1)
		::close(m_fileHandle);
}

SINT64 PluginLogWriter::seekToEnd()
{
#ifdef WIN_NT
	SINT64 nFileLen = _lseeki64(m_fileHandle, 0, SEEK_END);
#else
	off_t nFileLen = lseek(m_fileHandle, 0, SEEK_END);
#endif

	if (nFileLen < 0)
		checkErrno("lseek");

	return nFileLen;
}

void PluginLogWriter::reopen()
{
	if (m_fileHandle >= 0)
		::close(m_fileHandle);

#ifdef WIN_NT
	HANDLE hFile = CreateFile(
		m_fileName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_ALWAYS,
		0, // FILE_FLAG_SEQUENTIAL_SCAN,
		NULL
		);
	m_fileHandle = _open_osfhandle((intptr_t) hFile, 0);
#else
	m_fileHandle = ::open(m_fileName.c_str(), O_CREAT | O_APPEND | O_RDWR, S_IREAD | S_IWRITE);
#endif

	if (m_fileHandle < 0)
		checkErrno("open");
}

FB_SIZE_T PluginLogWriter::write(const void* buf, FB_SIZE_T size)
{
#ifdef WIN_NT
	Guard guard(this);
#else
	Guard guard(m_maxSize ? this : 0);
#endif

	if (m_fileHandle < 0)
		reopen();

	FB_UINT64 fileSize = seekToEnd();
	if (m_maxSize && (fileSize > m_maxSize))
	{
		reopen();
		fileSize = seekToEnd();
	}

	if (m_maxSize && (fileSize > m_maxSize))
	{
		PathName newName;

		while (true)
		{
			const TimeStamp stamp(TimeStamp::getCurrentTimeStamp());
			struct tm times;
			int fractions;
			stamp.decode(&times, &fractions);

			const FB_SIZE_T last_dot_pos = m_fileName.rfind(".");
			if (last_dot_pos > 0)
			{
				PathName log_name = m_fileName.substr(0, last_dot_pos);
				PathName log_ext = m_fileName.substr(last_dot_pos + 1, m_fileName.length());
				newName.printf("%s.%04d-%02d-%02dT%02d-%02d-%02d.%04d.%s", log_name.c_str(), times.tm_year + 1900,
					times.tm_mon + 1, times.tm_mday, times.tm_hour, times.tm_min, times.tm_sec, fractions, log_ext.c_str());
			}
			else
			{
				newName.printf("%s.%04d-%02d-%02dT%02d-%02d-%02d.%04d", m_fileName.c_str(), times.tm_year + 1900,
					times.tm_mon + 1, times.tm_mday, times.tm_hour, times.tm_min, times.tm_sec, fractions);
			}

			// Check if the file with the given name exists. If it doesn't, break the loop.
			struct stat st;

			if (stat(newName.c_str(), &st))
			{
				// errno == ENOENT is expected here. But if it's another error, we can still
				// break the loop and try to rename the file. For example, if there is a
				// problem with permissions, it will be caught on MoveFile/rename call.
				break;
			}

			Thread::sleep(10);
		}

#ifdef WIN_NT
		// hvlad: sad, but MSDN said "rename" returns EACCES when newName already
		// exists. Therefore we can't just check "rename" result for EEXIST and need
		// to write platform-dependent code. In reality, "rename" returns EEXIST to
		// me, not EACCES, strange...
		if (!MoveFile(m_fileName.c_str(), newName.c_str()))
		{
			const DWORD dwError = GetLastError();
			if (dwError != ERROR_ALREADY_EXISTS && dwError != ERROR_FILE_NOT_FOUND)
			{
				fatal_exception::raiseFmt("PluginLogWriter: MoveFile failed on file \"%s\". Error is : %d",
					m_fileName.c_str(), dwError);
			}
		}
#else
		if (rename(m_fileName.c_str(), newName.c_str()))
		{
			const int iErr = errno;
			if (iErr != ENOENT && iErr != EEXIST)
				checkErrno("rename");
		}
#endif

		reopen();
		seekToEnd();
	}

	const FB_SIZE_T written = ::write(m_fileHandle, buf, size);
	if (written != size)
		checkErrno("write");

	return written;
}

FB_SIZE_T PluginLogWriter::write_s(CheckStatusWrapper* status, const void* buf, FB_SIZE_T size)
{
	try
	{
		return write(buf, size);
	}
	catch (Exception &ex)
	{
		ex.stuffException(status);
	}

	return 0;
}

void PluginLogWriter::checkErrno(const char* operation)
{
	if (errno == 0)
		return;

	const char* strErr;
#ifdef WIN_NT
	strErr = strerror(errno);
#else
	char buff[256];
	strerror_r(errno, buff, sizeof(buff));
	strErr = buff;
#endif
	fatal_exception::raiseFmt("PluginLogWriter: operation \"%s\" failed on file \"%s\". Error is : %s",
		operation, m_fileName.c_str(), strErr);
}

void PluginLogWriter::mutexBug(int state, const TEXT* string)
{
	TEXT msg[BUFFER_TINY];

	sprintf(msg, "PluginLogWriter: mutex %s error, status = %d", string, state);
	fb_utils::logAndDie(msg);
}

bool PluginLogWriter::initialize(SharedMemoryBase* sm, bool init)
{
	return true;
}

void PluginLogWriter::lock()
{
	m_sharedMemory->mutexLock();
}

void PluginLogWriter::unlock()
{
	m_sharedMemory->mutexUnlock();
}
