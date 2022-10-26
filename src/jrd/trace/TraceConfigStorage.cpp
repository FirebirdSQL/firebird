/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceConfigStorage.cpp
 *	DESCRIPTION:	Trace API shared configurations storage
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

#include "../../common/classes/TempFile.h"
#include "../../common/StatusArg.h"
#include "../../common/utils_proto.h"
#include "../../jrd/err_proto.h"
#include "../../common/isc_proto.h"
#include "../../common/isc_s_proto.h"
#include "../../jrd/jrd.h"
#include "../../common/os/path_utils.h"
#include "../../common/os/os_utils.h"
#include "../../jrd/trace/TraceConfigStorage.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef WIN_NT
#include <process.h>
#define getpid _getpid
#endif


using namespace Firebird;

namespace Jrd {

static const FB_UINT64 TOUCH_INTERVAL = 60 * 60;	// in seconds, one hour should be enough

void checkFileError(const char* filename, const char* operation, ISC_STATUS iscError)
{
	if (errno == 0)
		return;

#ifdef WIN_NT
	// we can't use SYS_ERR(errno) on Windows as errno codes is not
	// the same as GetLastError() codes
	const char* strErr = strerror(errno);

	(Arg::Gds(isc_io_error) << Arg::Str(operation) << Arg::Str(filename) <<
		Arg::Gds(iscError) << Arg::Str(strErr)).raise();
#else
	(Arg::Gds(isc_io_error) << Arg::Str(operation) << Arg::Str(filename) <<
		Arg::Gds(iscError) << SYS_ERR(errno)).raise();
#endif
}

ConfigStorage::ConfigStorage()
	: m_timer(FB_NEW TouchFile),
	  m_sharedMemory(NULL),
	  m_filename(getPool()),
	  m_recursive(0),
	  m_mutexTID(0),
	  m_dirty(false),
	  m_nextIdx(0)
{
#ifdef WIN_NT
	DWORD sesID = 0;

	typedef BOOL (WINAPI *PFnProcessIdToSessionId) (DWORD, DWORD *);

	HMODULE hmodKernel32 = GetModuleHandle("kernel32.dll");

	PFnProcessIdToSessionId pfnProcessIdToSessionId =
		(PFnProcessIdToSessionId) GetProcAddress(hmodKernel32, "ProcessIdToSessionId");

	if (fb_utils::isGlobalKernelPrefix() ||
		!pfnProcessIdToSessionId ||
		pfnProcessIdToSessionId(GetCurrentProcessId(), &sesID) == 0 ||
		sesID == 0)
	{
		m_filename.printf(TRACE_FILE); // TODO: it must be per engine instance
	}
	else
	{
		m_filename.printf("%s.%u", TRACE_FILE, sesID);
	}
#else
	m_filename.printf(TRACE_FILE); // TODO: it must be per engine instance
#endif

	initSharedFile();

	StorageGuard guard(this);
	checkAudit();

	TEXT fullName[MAXPATHLEN];
	iscPrefixLock(fullName, m_filename.c_str(), false);
	m_timer->start(fullName);	// do we still need a timer ?

	++(m_sharedMemory->getHeader()->cnt_uses);
}

ConfigStorage::~ConfigStorage()
{
	fb_assert(!m_timer);
}

void ConfigStorage::initSharedFile()
{
	try
	{
		m_sharedMemory.reset(FB_NEW_POOL(getPool())
			SharedMemory<TraceCSHeader>(m_filename.c_str(), TraceCSHeader::TRACE_STORAGE_MIN_SIZE, this));

		fb_assert(m_sharedMemory->getHeader());
		fb_assert(m_sharedMemory->getHeader()->mhb_version == TraceCSHeader::TRACE_STORAGE_VERSION);
	}
	catch (const Exception& ex)
	{
		iscLogException("ConfigStorage: Cannot initialize the shared memory region", ex);
		throw;
	}
}

void ConfigStorage::shutdown()
{
	if (!m_timer)
		return;

	MutexLockGuard localGuard(m_localMutex, FB_FUNCTION);

	m_timer->stop();
	m_timer = NULL;

	{
		StorageGuard guard(this);
		--(m_sharedMemory->getHeader()->cnt_uses);
		if (m_sharedMemory->getHeader()->cnt_uses == 0)
		{
			m_sharedMemory->removeMapFile();
		}
	}

	m_sharedMemory = NULL;
}

void ConfigStorage::mutexBug(int state, const char* string)
{
	TEXT msg[BUFFER_TINY];

	// While string is kept below length 70, all is well.
	sprintf(msg, "ConfigStorage: mutex %s error, status = %d", string, state);
	fb_utils::logAndDie(msg);
}

bool ConfigStorage::initialize(SharedMemoryBase* sm, bool init)
{
	TraceCSHeader* header = reinterpret_cast<TraceCSHeader*>(sm->sh_mem_header);

	// Initialize the shared data header
	if (init)
	{
		header->init(SharedMemoryBase::SRAM_TRACE_CONFIG, TraceCSHeader::TRACE_STORAGE_VERSION);

		header->change_number = 0;
		header->session_number = 1;
		header->cnt_uses = 0;

		header->mem_max_size = TraceCSHeader::TRACE_STORAGE_MAX_SIZE;
		header->mem_allocated = sm->sh_mem_length_mapped;
		header->mem_used = sizeof(TraceCSHeader);
		header->mem_offset = sizeof(TraceCSHeader);
		header->slots_free = 0;
		header->slots_cnt = 0;
		memset(header->slots, 0, sizeof(TraceCSHeader::slots));
	}
	else
	{
		fb_assert(header->mhb_type == SharedMemoryBase::SRAM_TRACE_CONFIG);
		fb_assert(header->mhb_header_version == MemoryHeader::HEADER_VERSION);
		fb_assert(header->mhb_version == TraceCSHeader::TRACE_STORAGE_VERSION);
	}

	return true;
}

void ConfigStorage::checkAudit()
{
	if (m_sharedMemory->getHeader()->change_number != 0)
		return;

	// Prevent second attempt to create audit session if first one was failed.
	// This also prevents multiply logging of the same error.
	setDirty();

	// put default (audit) trace file contents into storage
	AutoPtr<FILE> cfgFile;

	try
	{
		PathName configFileName(Config::getAuditTraceConfigFile());

		// remove quotes around path if present
		configFileName.alltrim(" '\"");

		if (configFileName.empty())
			return;

		if (PathUtils::isRelative(configFileName))
		{
			PathName root(Config::getRootDirectory());
			PathUtils::ensureSeparator(root);
			configFileName.insert(0, root);
		}

		cfgFile = os_utils::fopen(configFileName.c_str(), "rb");
		if (!cfgFile) {
			checkFileError(configFileName.c_str(), "fopen", isc_io_open_err);
		}

		TraceSession session(*getDefaultMemoryPool());

		fseek(cfgFile, 0, SEEK_END);
		const long len = ftell(cfgFile);
		if (len)
		{
			fseek(cfgFile, 0, SEEK_SET);
			char* p = session.ses_config.getBuffer(len + 1);

			if (fread(p, 1, len, cfgFile) != size_t(len)) {
				checkFileError(configFileName.c_str(), "fread", isc_io_read_err);
			}
			p[len] = 0;
		}
		else
		{
			gds__log("Audit configuration file \"%s\" is empty", configFileName.c_str());
			return;
		}

		session.ses_user = DBA_USER_NAME;
		session.ses_name = "Firebird Audit";
		session.ses_flags = trs_admin | trs_system;

		addSession(session);
	}
	catch (const Exception& ex)
	{
		iscLogException("Cannot open audit configuration file", ex);
	}
}


void ConfigStorage::acquire()
{
	if (!m_sharedMemory)
		(Arg::Gds(isc_random) << "Trace shared memory can not be accessed").raise();

	fb_assert(m_recursive >= 0);
	const ThreadId currTID = getThreadId();

	if (m_mutexTID == currTID)
	{
		m_recursive++;
		return;
	}

	m_sharedMemory->mutexLock();

	fb_assert(m_recursive == 0);
	m_recursive = 1;

	fb_assert(m_mutexTID == 0);
	m_mutexTID = currTID;

	while (m_sharedMemory->getHeader()->isDeleted())
	{
		// Shared memory must be empty at this point
		fb_assert(m_sharedMemory->getHeader()->cnt_uses == 0);

		m_sharedMemory->mutexUnlock();
		m_sharedMemory.reset();

		Thread::yield();

		initSharedFile();
		m_sharedMemory->mutexLock();
	}

	TraceCSHeader* header = m_sharedMemory->getHeader();
	if (header->mem_allocated > m_sharedMemory->sh_mem_length_mapped)
	{
#ifdef HAVE_OBJECT_MAP
		FbLocalStatus status;
		if (!m_sharedMemory->remapFile(&status, header->mem_allocated, false))
		{
			release();
			status_exception::raise(&status);
		}
#else
		release();
		fb_assert(false);
		(Arg::Gds(isc_random) << Arg::Str("Trace storage memory remapping error")).raise();
#endif
	}
}

void ConfigStorage::release()
{
	fb_assert(m_sharedMemory);

	fb_assert(m_recursive > 0);
	fb_assert(m_mutexTID == getThreadId());

	if (--m_recursive == 0)
	{
		checkDirty();
		m_mutexTID = 0;
		m_sharedMemory->mutexUnlock();
	}
}

ULONG ConfigStorage::allocSlot(ULONG slotSize)
{
	fb_assert(validate());

	TraceCSHeader* header = m_sharedMemory->getHeader();

	if (header->slots_free == 0 && header->slots_cnt == TraceCSHeader::TRACE_STORAGE_MAX_SLOTS)
		(Arg::Gds(isc_random) << Arg::Str("No enough free slots")).raise();

	// try to extend shared memory, if needed
	if (header->mem_used + slotSize > header->mem_allocated)
	{
		if (header->mem_allocated >= header->mem_max_size)
			(Arg::Gds(isc_random) << Arg::Str("No enough memory for new trase session")).raise();

		ULONG newAlloc = FB_ALIGN(header->mem_used + slotSize, header->mem_allocated);
		newAlloc = MIN(newAlloc, header->mem_max_size);

#ifdef HAVE_OBJECT_MAP
		FbLocalStatus status;
		if (!m_sharedMemory->remapFile(&status, newAlloc, true))
			status_exception::raise(&status);
#else
		(Arg::Gds(isc_random) << Arg::Str("Can not remap trace storage memory")).raise();
#endif
		header = m_sharedMemory->getHeader();
		header->mem_allocated = m_sharedMemory->sh_mem_length_mapped;

		fb_assert(validate());
	}

	fb_assert(header->mem_used + slotSize <= header->mem_allocated);
	setDirty();

	bool reuseFreeSlot = false;
	if (header->slots_free)
	{
		// find free slot with best fit size
		ULONG idxFound = 0;
		ULONG lenFound = 0;
		for (ULONG i = 0; i < header->slots_cnt; i++)
		{
			TraceCSHeader::Slot* slot = header->slots + i;
			if (!slot->used && slot->size >= slotSize &&
				(!lenFound || lenFound > slot->size))
			{
				lenFound = slot->size;
				idxFound = i;
			}
		}

		if (lenFound)
		{
			header->slots_free--;
			reuseFreeSlot = true;

			// move free slot to the top position
			if (idxFound != header->slots_cnt - 1)
			{
				TraceCSHeader::Slot tmp = header->slots[idxFound];

				const FB_SIZE_T mv = sizeof(TraceCSHeader::Slot) * (header->slots_cnt - idxFound - 1);
				memmove(&header->slots[idxFound], &header->slots[idxFound + 1], mv);

				header->slots[header->slots_cnt - 1] = tmp;
			}
		}
	}

	if (!reuseFreeSlot)
	{
		if (header->mem_offset + slotSize > header->mem_allocated || header->slots_free)
		{
			compact();

			fb_assert(validate());
			fb_assert(header->mem_offset + slotSize <= header->mem_allocated);
		}

		header->slots_cnt++;
	}

	TraceCSHeader::Slot* slot = &header->slots[header->slots_cnt - 1];

	if (!reuseFreeSlot)
	{
		fb_assert(header->mem_offset + slotSize < header->mem_allocated);

		slot->size = slotSize;
		slot->offset = header->mem_offset;
		header->mem_offset += slotSize;
	}
	header->mem_used += slotSize;
	slot->used = slotSize;

	slot->ses_id = header->session_number++;
	slot->ses_flags = 0;
	slot->ses_pid = getpid();

	fb_assert(validate());
	return header->slots_cnt - 1;
}

struct SlotByOffset
{
	ULONG index;		// slot index
	ULONG offset;		// initial data ofset

	static ULONG generate(const SlotByOffset& i) { return i.offset; }
};

typedef SortedArray<SlotByOffset, EmptyStorage<SlotByOffset>, ULONG, SlotByOffset>
			SlotsByOffsetArray;


void ConfigStorage::compact()
{
	SlotsByOffsetArray data(*getDefaultMemoryPool());

	TraceCSHeader* header = m_sharedMemory->getHeader();

	const ULONG pid = getpid();

	ULONG check_used, check_size;
	check_used = check_size = sizeof(TraceCSHeader);

	// collect used slots, sort them by offset
	for (TraceCSHeader::Slot* slot = header->slots; slot < header->slots + header->slots_cnt; slot++)
	{
		if (slot->used && slot->ses_pid != pid &&
			!ISC_check_process_existence(slot->ses_pid))
		{
			header->cnt_uses--; // Process that created trace session disappeared, count it out
			markDeleted(slot);
		}

		SlotByOffset item;
		item.index = slot - header->slots;
		item.offset = slot->offset;

		check_used += slot->used;
		check_size += slot->size;
		data.add(item);
	}

	fb_assert(check_used == header->mem_used);
	fb_assert(check_size == header->mem_offset);

	// remove unused space between sessions data
	ULONG destOffset = sizeof(TraceCSHeader);
	for (SlotByOffset* item = data.begin(); item < data.end(); item++)
	{
		TraceCSHeader::Slot* slot = header->slots + item->index;
		if (slot->used == 0)
		{
			slot->size = 0;
			continue;
		}

		fb_assert(slot->offset >= destOffset);

		if (slot->offset > destOffset)
		{
			char* dst = reinterpret_cast<char*>(header) + destOffset;
			const char* src = reinterpret_cast<const char*>(header) + slot->offset;
			memmove(dst, src, slot->used);

			slot->offset = destOffset;
		}
		slot->size = slot->used;
		destOffset += slot->used;
	}

	header->mem_offset = destOffset;

	// remove unused slots
	if (header->slots_free)
	{
		TraceCSHeader::Slot* dst, *src, *last;
		dst = src = header->slots;
		last = header->slots + header->slots_cnt;
		while (src < last)
		{
			if (src->used && !dst->used)
			{
				memcpy(dst, src, sizeof(TraceCSHeader::Slot));
				src->used = 0;
			}
			src++;

			while (dst->used && dst < src)
				dst++;

			fb_assert(dst == src || !dst->used && dst < src);
		}

		fb_assert(dst == last || !dst->used && dst < last);
		fb_assert(dst - header->slots == header->slots_cnt - header->slots_free);

		header->slots_free = 0;
		header->slots_cnt = dst - header->slots;
	}
}

bool ConfigStorage::validate()
{
	TraceCSHeader* header = m_sharedMemory->getHeader();

	if (!header)
		return true;

	if (header->mem_max_size != TraceCSHeader::TRACE_STORAGE_MAX_SIZE)
		return false;

	if (header->mem_allocated > header->mem_max_size)
		return false;

	if (header->mem_allocated < TraceCSHeader::TRACE_STORAGE_MIN_SIZE)
		return false;

	if (header->mem_offset < sizeof(TraceCSHeader))
		return false;

	if (header->mem_offset > header->mem_allocated)
		return false;

	if (header->mem_used < sizeof(TraceCSHeader))
		return false;

	if (header->mem_used > header->mem_offset)
		return false;

	if (header->slots_cnt > TraceCSHeader::TRACE_STORAGE_MAX_SLOTS)
		return false;

	if (header->slots_free > header->slots_cnt)
		return false;

	ULONG check_used, check_size, check_free, prev_id;
	check_used = check_size = sizeof(TraceCSHeader);
	check_free = prev_id = 0;

	SlotsByOffsetArray data(*getDefaultMemoryPool());

	TraceCSHeader::Slot* slot;
	for (slot = header->slots; slot < header->slots + header->slots_cnt; slot++)
	{
		if (slot->offset < sizeof(TraceCSHeader))
			return false;

		if (slot->offset + slot->size > header->mem_offset)
			return false;

		if (slot->used > slot->size)
			return false;

		if (slot->ses_id > header->session_number)
			return false;

		if (prev_id >= slot->ses_id)
			return false;

		prev_id = slot->ses_id;
		check_used += slot->used;
		check_size += slot->size;
		if (!slot->used)
			check_free++;

		SlotByOffset item;
		item.index = slot - header->slots;
		item.offset = slot->offset;
		data.add(item);
	};

	if (check_used != header->mem_used)
		return false;

	if (check_size != header->mem_offset)
		return false;

	if (check_free != header->slots_free)
		return false;

	ULONG check_offset = sizeof(TraceCSHeader);
	for (SlotByOffset* item = data.begin(); item < data.end(); item++)
	{
		slot = header->slots + item->index;
		if (slot->offset != check_offset)
			return false;

		check_offset += slot->size;
	}

	return true;
}


ULONG ConfigStorage::getSessionSize(const TraceSession& session)
{
	ULONG ret = 1; // tagEnd
	const ULONG sz = 1 + sizeof(ULONG);		// sizeof tag + sizeof len

	ULONG len = session.ses_name.length();
	if (len)
		ret += sz + len;

	if ((len = session.ses_auth.getCount()))
		ret += sz + len;

	if ((len = session.ses_user.getCount()))
		ret += sz + len;

	if ((len = session.ses_role.length()))
		ret += sz + len;

	if ((len = session.ses_config.length()))
		ret += sz + len;

	if ((len = sizeof(session.ses_start)))
		ret += sz + len;

	if ((len = session.ses_logfile.length()))
		ret += sz + len;

	return ret;
}

bool ConfigStorage::findSession(ULONG sesId, ULONG& idx)
{
	TraceCSHeader* header = m_sharedMemory->getHeader();

	ULONG hi = header->slots_cnt, lo = 0;
	while (hi > lo)
	{
		const ULONG temp = (hi + lo) >> 1;
		if (sesId > header->slots[temp].ses_id)
			lo = temp + 1;
		else
			hi = temp;
	}
	idx = lo;
	return (hi != header->slots_cnt) && (header->slots[lo].ses_id <= sesId);
}

void ConfigStorage::addSession(TraceSession& session)
{
	const ULONG size = getSessionSize(session);
	const ULONG idx = allocSlot(size);

	TraceCSHeader* header = m_sharedMemory->getHeader();
	TraceCSHeader::Slot* slot = &header->slots[idx];

	setDirty();

	// setup some session fields
	session.ses_id = slot->ses_id;
	session.ses_flags |= trs_active;
	slot->ses_flags = session.ses_flags;
	time(&session.ses_start);

	char* p = reinterpret_cast<char*> (header) + slot->offset;
	Writer writer(p, slot->size);

	if (!session.ses_name.empty()) {
		writer.write(tagName, session.ses_name.length(), session.ses_name.c_str());
	}
	if (session.ses_auth.hasData()) {
		writer.write(tagAuthBlock, session.ses_auth.getCount(), session.ses_auth.begin());
	}
	if (!session.ses_user.empty()) {
		writer.write(tagUserName, session.ses_user.length(), session.ses_user.c_str());
	}
	if (session.ses_role.hasData()) {
		writer.write(tagRole, session.ses_role.length(), session.ses_role.c_str());
	}
	if (!session.ses_config.empty()) {
		writer.write(tagConfig, session.ses_config.length(), session.ses_config.c_str());
	}
	writer.write(tagStartTS, sizeof(session.ses_start), &session.ses_start);
	if (!session.ses_logfile.empty()) {
		writer.write(tagLogFile, session.ses_logfile.length(), session.ses_logfile.c_str());
	}
	writer.write(tagEnd, 0, NULL);
}

bool ConfigStorage::getSession(Firebird::TraceSession& session, GET_FLAGS getFlag)
{
	ULONG idx;
	if (!findSession(session.ses_id, idx))
		return false;

	TraceCSHeader* header = m_sharedMemory->getHeader();
	TraceCSHeader::Slot* slot = &header->slots[idx];

	if (slot->ses_id != session.ses_id || !slot->used)
		return false;

	return readSession(slot, session, getFlag);
}

void ConfigStorage::restart()
{
	m_nextIdx = 0;
}

bool ConfigStorage::getNextSession(TraceSession& session, GET_FLAGS getFlag)
{
	TraceCSHeader* header = m_sharedMemory->getHeader();

	while (m_nextIdx < header->slots_cnt)
	{
		TraceCSHeader::Slot* slot = header->slots + m_nextIdx;
		m_nextIdx++;

		if (slot->used)
			return readSession(slot, session, getFlag);
	}
	return false;
}

bool ConfigStorage::readSession(TraceCSHeader::Slot* slot, TraceSession& session, GET_FLAGS getFlag)
{
	const ULONG getMask[3] =
	{
		MAX_ULONG,				// ALL
		0,						// FLAGS
		(1 << tagAuthBlock)	|
		(1 << tagUserName) |
		(1 << tagRole)			// AUTH
	};

	TraceCSHeader* header = m_sharedMemory->getHeader();

	session.clear();
	session.ses_id = slot->ses_id;
	session.ses_flags = slot->ses_flags;

	if (getFlag == FLAGS)
		return true;

	char* p = reinterpret_cast<char*> (header) + slot->offset;
	Reader reader(p, slot->size);

	while (true)
	{
		ITEM tag;
		ULONG len;

		void* p = NULL;
		const void* data = reader.read(tag, len);

		if (!data)
			return false;

		if (tag == tagEnd)
			return true;

		const ULONG tagMask = (1 << tag);
		if (!(tagMask & getMask[getFlag]))
			continue;

		switch (tag)
		{
			case tagName:
				p = session.ses_name.getBuffer(len);
				break;

			case tagUserName:
				p = session.ses_user.getBuffer(len);
				break;

			case tagConfig:
				p = session.ses_config.getBuffer(len);
				break;

			case tagStartTS:
				fb_assert(len == sizeof(session.ses_start));
				p = &session.ses_start;
				break;

			case tagLogFile:
				p = session.ses_logfile.getBuffer(len);
				break;

			case tagAuthBlock:
				p = session.ses_auth.getBuffer(len);
				break;

			case tagRole:
				p = session.ses_role.getBuffer(len);
				break;

			default:
				fb_assert(false);
				return false;
		}

		if (p)
			memcpy(p, data, len);
	}

	return false;
}

void ConfigStorage::removeSession(ULONG id)
{
	ULONG idx;
	if (!findSession(id, idx))
		return;

	TraceCSHeader* header = m_sharedMemory->getHeader();
	TraceCSHeader::Slot* slot = &header->slots[idx];

	if (slot->ses_id != id)
		return;

	fb_assert(validate());
	markDeleted(slot);
	fb_assert(validate());
}

void ConfigStorage::markDeleted(TraceCSHeader::Slot* slot)
{
	if (!slot->used)
		return;

	TraceCSHeader* header = m_sharedMemory->getHeader();
	setDirty();

	header->slots_free++;
	header->mem_used -= slot->used;

	slot->used = 0;
}

void ConfigStorage::updateFlags(TraceSession& session)
{
	ULONG idx;
	if (!findSession(session.ses_id, idx))
		return;

	TraceCSHeader* header = m_sharedMemory->getHeader();
	TraceCSHeader::Slot* slot = &header->slots[idx];

	if (slot->ses_id != session.ses_id)
		return;

	setDirty();
	slot->ses_flags = session.ses_flags;
}

void ConfigStorage::Writer::write(ITEM tag, ULONG len, const void* data)
{
	if (m_mem + 1 > m_end)
		(Arg::Gds(isc_random) << Arg::Str("Item data not fits into memory")).raise();

	*m_mem++ = tag;
	if (tag == tagEnd)
		return;

	if (m_mem + sizeof(len) + len > m_end)
		(Arg::Gds(isc_random) << Arg::Str("Item data not fits into memory")).raise();

	memcpy(m_mem, &len, sizeof(len));
	m_mem += sizeof(len);

	memcpy(m_mem, data, len);
	m_mem += len;
}

const void* ConfigStorage::Reader::read(ITEM& tag, ULONG& len)
{
	if (m_mem + 1 > m_end)
		return NULL;

	tag = (ITEM) *m_mem++;
	if (tag == tagEnd)
	{
		len = 0;
		return m_mem;
	}

	if (m_mem + sizeof(ULONG) > m_end)
		return NULL;

	memcpy(&len, m_mem, sizeof(ULONG));
	m_mem += sizeof(ULONG);

	if (m_mem + len <= m_end)
	{
		const void* data = m_mem;
		m_mem += len;
		return data;
	}

	return NULL;
}

void ConfigStorage::TouchFile::handler()
{
	try
	{
		if (!os_utils::touchFile(fileName.c_str()))
			system_call_failed::raise("utime");

		FbLocalStatus s;
		TimerInterfacePtr()->start(&s, this, TOUCH_INTERVAL * 1000 * 1000);
		s.check();
	}
	catch (const Exception& e)
	{
		iscLogException("TouchFile failed", e);
	}
}

void ConfigStorage::TouchFile::start(const char* fName)
{
	fileName = fName;

	FbLocalStatus s;
	TimerInterfacePtr()->start(&s, this, TOUCH_INTERVAL * 1000 * 1000);
	check(&s);
}

void ConfigStorage::TouchFile::stop()
{
	FbLocalStatus s;
	TimerInterfacePtr()->stop(&s, this);
	// ignore error in stop timer
}

} // namespace Jrd
