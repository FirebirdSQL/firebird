/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceLog.h
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

#ifndef TRACE_LOG
#define TRACE_LOG

#include "../../common/classes/fb_string.h"
#include "../../common/isc_s_proto.h"

namespace Jrd {

struct TraceLogHeader : public Firebird::MemoryHeader
{
	static const USHORT TRACE_LOG_VERSION = 2;

	ULONG readPos;
	ULONG writePos;
	ULONG maxSize;
	ULONG allocated;		// zero when reader gone
	ULONG flags;
};

class TraceLog : public Firebird::IpcObject
{
public:
	TraceLog(Firebird::MemoryPool& pool, const Firebird::PathName& fileName, bool reader);
	virtual ~TraceLog();

	FB_SIZE_T read(void* buf, FB_SIZE_T size);
	FB_SIZE_T write(const void* buf, FB_SIZE_T size);

	bool isFull();		// true if free space left is less than threshold
	void setFullMsg(const char* str);

private:
	// flags in header
	const ULONG FLAG_FULL = 0x0001;		// log is full, set by writer, reset by reader
	const ULONG FLAG_DONE = 0x0002;		// set when reader is gone

	void mutexBug(int osErrorCode, const char* text) override;
	bool initialize(Firebird::SharedMemoryBase*, bool) override;

	USHORT getType() const override { return Firebird::SharedMemoryBase::SRAM_TRACE_LOG; }
	USHORT getVersion() const override { return TraceLogHeader::TRACE_LOG_VERSION; }
	const char* getName() const override { return "TraceLog"; }

	void lock();
	void unlock();

	FB_SIZE_T getUsed();	// available to read
	FB_SIZE_T getFree(bool useMax);	// available for write
	void extend(FB_SIZE_T size);

	Firebird::AutoPtr<Firebird::SharedMemory<TraceLogHeader> > m_sharedMemory;
	bool m_reader;
	Firebird::string m_fullMsg;

	class TraceLogGuard
	{
	public:
		explicit TraceLogGuard(TraceLog* log) : m_log(*log)
		{
			m_log.lock();
		}

		~TraceLogGuard()
		{
			m_log.unlock();
		}

	private:
		TraceLog& m_log;
	};
};


} // namespace Jrd

#endif // TRACE_LOG
