/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceLog.cpp
 *	DESCRIPTION:	Trace API shared log file
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
 *  Copyright (c) 2008 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../../common/StatusArg.h"
#include "../../common/classes/TempFile.h"
#include "../../common/isc_proto.h"
#include "../../common/isc_s_proto.h"
#include "../../common/os/path_utils.h"
#include "../../common/os/os_utils.h"
#include "../../common/config/config.h"
#include "../../jrd/trace/TraceLog.h"
#include "../../common/utils_proto.h"
#include "../../common/StatusHolder.h"

using namespace Firebird;

namespace Jrd {

const unsigned int INIT_LOG_SIZE = 1024*1024;	// 1MB
const unsigned int FREE_SPACE_THRESHOLD = INIT_LOG_SIZE / 4;

TraceLog::TraceLog(MemoryPool& pool, const PathName& fileName, bool reader) :
	m_reader(reader),
	m_fullMsg(pool)
{
	try
	{
		m_sharedMemory.reset(FB_NEW_POOL(pool)
			SharedMemory<TraceLogHeader>(fileName.c_str(), INIT_LOG_SIZE, this));
	}
	catch (const Exception& ex)
	{
		iscLogException("TraceLog: cannot initialize the shared memory region", ex);
		throw;
	}
}

TraceLog::~TraceLog()
{
	bool removeMap = false;

	{
		TraceLogGuard guard(this);

		TraceLogHeader* header = m_sharedMemory->getHeader();
		if (m_reader)
			header->flags |= FLAG_DONE;

		removeMap = (header->flags & FLAG_DONE);
	}

	if (removeMap)
		m_sharedMemory->removeMapFile();
}

FB_SIZE_T TraceLog::read(void* buf, FB_SIZE_T size)
{
	if (!size)
		return 0;

	fb_assert(m_reader);
	TraceLogGuard guard(this);

	TraceLogHeader* header = m_sharedMemory->getHeader();
	const char* data = reinterpret_cast<const char*> (header);
	char* dest = reinterpret_cast<char*> (buf);
	FB_SIZE_T readCnt = 0;

	if (header->readPos > header->writePos)
	{
		const FB_SIZE_T toRead = MIN(header->allocated - header->readPos, size);
		memcpy(dest, data + header->readPos, toRead);

		readCnt += toRead;
		header->readPos += toRead;
		if (header->readPos == header->allocated)
			header->readPos = sizeof(TraceLogHeader);

		dest += toRead;
		size -= toRead;
	}

	if (size && header->readPos < header->writePos)
	{
		const FB_SIZE_T toRead = MIN(header->writePos - header->readPos, size);
		memcpy(dest, data + header->readPos, toRead);

		readCnt += toRead;
		header->readPos += toRead;
		if (header->readPos == header->allocated)
			header->readPos = sizeof(TraceLogHeader);
	}

	if (header->readPos == header->writePos)
		header->readPos = header->writePos = sizeof(TraceLogHeader);

	if ((header->flags & FLAG_FULL) && (getFree(true) >= FREE_SPACE_THRESHOLD))
		header->flags &= ~FLAG_FULL;

	return readCnt;
}

FB_SIZE_T TraceLog::write(const void* buf, FB_SIZE_T size)
{
	if (!size)
		return 0;

	fb_assert(!m_reader);

	TraceLogGuard guard(this);

	TraceLogHeader* header = m_sharedMemory->getHeader();

	// if reader already gone, don't write anything
	if (header->flags & FLAG_DONE)
		return size;

	if (header->flags & FLAG_FULL)
		return 0;

	const FB_SIZE_T msgLen = m_fullMsg.length();

	if (header->allocated < header->maxSize && (size + msgLen) > getFree(false))
	{
		extend(size + msgLen);
		header = m_sharedMemory->getHeader();
	}

	if (size + msgLen > getFree(true))	// log is full
	{
		header->flags |= FLAG_FULL;

		if (!msgLen)
			return 0;

		// write m_fullMsg into log and return zero
		buf = m_fullMsg.c_str();
		size = msgLen;
	}

	char* data = reinterpret_cast<char*> (header);
	const char* src = reinterpret_cast<const char*> (buf);
	FB_SIZE_T writeCnt = 0;

	if (header->writePos >= header->readPos)
	{
		const FB_SIZE_T toWrite = MIN(header->allocated - header->writePos, size);
		memcpy(data + header->writePos, src, toWrite);

		header->writePos += toWrite;
		if (header->writePos == header->allocated)
			header->writePos = sizeof(TraceLogHeader);

		writeCnt += toWrite;
		src += toWrite;
		size -= toWrite;
	}

	if (size && header->writePos < header->readPos)
	{
		const FB_SIZE_T toWrite = MIN(header->readPos - 1 - header->writePos, size);
		memcpy(data + header->writePos, src, toWrite);

		header->writePos += toWrite;
		writeCnt += toWrite;
		src += toWrite;
		size -= toWrite;
	}

	fb_assert(size == 0);

	if (header->flags & FLAG_FULL)
		return 0;

	return writeCnt;
}

void TraceLog::extend(FB_SIZE_T size)
{
	TraceLogHeader* header = m_sharedMemory->getHeader();
	const ULONG oldSize = header->allocated;
	const FB_SIZE_T oldUsed = getUsed();

	ULONG newSize = header->allocated * ((header->allocated + size) / header->allocated + 1);
	newSize = MIN(newSize, header->maxSize);

	LocalStatus ls;
	CheckStatusWrapper s(&ls);

	if (!m_sharedMemory->remapFile(&s, newSize, true))
		status_exception::raise(&s);

	header = m_sharedMemory->getHeader();
	header->allocated = newSize;

	if (header->writePos < header->readPos)
	{
		const FB_SIZE_T toMoveW = header->writePos - sizeof(TraceLogHeader);
		const FB_SIZE_T toMoveR = oldSize - header->readPos;

		char* data = reinterpret_cast<char*> (header);
		if (toMoveW < toMoveR)
		{
			memcpy(data + oldSize, data + sizeof(TraceLogHeader), toMoveW);
			header->writePos = oldSize + toMoveW;
		}
		else
		{
			memcpy(data + newSize - toMoveR, data + header->readPos, toMoveR);
			header->readPos = newSize - toMoveR;
		}
	}
	fb_assert(oldUsed == getUsed());
}

FB_SIZE_T TraceLog::getUsed()
{
	TraceLogHeader* header = m_sharedMemory->getHeader();

	if (header->readPos < header->writePos)
		return (header->writePos - header->readPos);
	if (header->readPos == header->writePos)
		return 0;

	return header->allocated - header->readPos + header->writePos - sizeof(TraceLogHeader);
}

FB_SIZE_T TraceLog::getFree(bool useMax)
{
	TraceLogHeader* header = m_sharedMemory->getHeader();
	return (useMax ? header->maxSize : header->allocated) - sizeof(TraceLogHeader) - getUsed() - 1;
}

bool TraceLog::isFull()
{
	TraceLogGuard guard(this);
	TraceLogHeader* header = m_sharedMemory->getHeader();
	return header->flags & FLAG_FULL;
}

void TraceLog::setFullMsg(const char* str)
{
	m_fullMsg = str;
}

void TraceLog::mutexBug(int state, const char* string)
{
	TEXT msg[BUFFER_TINY];

	// While string is kept below length 70, all is well.
	sprintf(msg, "TraceLog: mutex %s error, status = %d", string, state);
	fb_utils::logAndDie(msg);
}

bool TraceLog::initialize(SharedMemoryBase* sm, bool initialize)
{
	TraceLogHeader* hdr = reinterpret_cast<TraceLogHeader*>(sm->sh_mem_header);
	if (initialize)
	{
		hdr->init(SharedMemoryBase::SRAM_TRACE_LOG, TraceLogHeader::TRACE_LOG_VERSION);

		hdr->readPos = hdr->writePos = sizeof(TraceLogHeader);
		hdr->maxSize = Config::getMaxUserTraceLogSize() * 1024 * 1024;
		hdr->allocated = sm->sh_mem_length_mapped;
		hdr->flags = 0;
	}
	else
	{
		fb_assert(hdr->mhb_type == SharedMemoryBase::SRAM_TRACE_LOG);
		fb_assert(hdr->mhb_header_version == MemoryHeader::HEADER_VERSION);
		fb_assert(hdr->mhb_version == TraceLogHeader::TRACE_LOG_VERSION);
	}

	return true;
}

void TraceLog::lock()
{
	m_sharedMemory->mutexLock();

	TraceLogHeader* header = m_sharedMemory->getHeader();

	if (header->allocated != m_sharedMemory->sh_mem_length_mapped)
	{
		LocalStatus ls;
		CheckStatusWrapper s(&ls);

		if (!m_sharedMemory->remapFile(&s, header->allocated, false))
			status_exception::raise(&s);

		header = m_sharedMemory->getHeader();

		fb_assert(header->allocated == m_sharedMemory->sh_mem_length_mapped);
	}
}

void TraceLog::unlock()
{
	m_sharedMemory->mutexUnlock();
}

} // namespace Jrd
