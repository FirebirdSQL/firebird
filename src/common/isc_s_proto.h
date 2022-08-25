/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_s_proto.h
 *	DESCRIPTION:	Prototype header file for isc_sync.cpp
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
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef JRD_ISC_S_PROTO_H
#define JRD_ISC_S_PROTO_H

#include "../common/classes/alloc.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/timestamp.h"

// Firebird platform-specific synchronization data structures

#ifdef LINUX
// This hack fixes CORE-2896 - embedded connections fail on linux.
// Looks like a lot of linux kernels are buggy when working with PRIO_INHERIT mutexes.
// dimitr (10-11-2016): PRIO_INHERIT also causes undesired short-time sleeps (CPU idle 30-35%)
// during context switches under concurrent load. Proved on linux kernels up to 4.8.
#undef HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL
#endif

#if defined(HAVE_MMAP) || defined(WIN_NT)
#define HAVE_OBJECT_MAP
#endif

#if defined(HAVE_MMAP)
#define USE_MUTEX_MAP
#endif


#ifdef UNIX

#if defined(HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP) && defined(HAVE_PTHREAD_MUTEX_CONSISTENT_NP)
#define USE_ROBUST_MUTEX
#endif // ROBUST mutex

#include "fb_pthread.h"

#define HAVE_SHARED_MUTEX_SECTION

namespace Firebird {

struct mtx
{
	pthread_mutex_t mtx_mutex[1];
};

struct event_t
{
	SLONG event_count;
	int pid;
	pthread_mutex_t event_mutex[1];
	pthread_cond_t event_cond[1];
};

#endif // UNIX


#ifdef WIN_NT
#include <windows.h>

namespace Firebird {

struct FAST_MUTEX_SHARED_SECTION
{
	SLONG fInitialized;
	SLONG lSpinLock;
	SLONG lThreadsWaiting;
	SLONG lAvailable;
	SLONG lOwnerPID;
#ifdef DEV_BUILD
	SLONG lThreadId;
#endif
};

struct FAST_MUTEX
{
	HANDLE hEvent;
	HANDLE hFileMap;
	SLONG lSpinCount;
	volatile FAST_MUTEX_SHARED_SECTION* lpSharedInfo;
};

struct mtx
{
	FAST_MUTEX mtx_fast;
};

// Enforce equal binary layout for both 32 and 64-bit platforms as instances of
// event_t is used in shared memory.
struct event_t
{
	SLONG event_pid;
	SLONG event_id;
	SLONG event_count;
	union
	{
		HANDLE event_handle;
		FB_UINT64 dummy;
	};
};

#endif // WIN_NT

class MemoryHeader
{
public:
	static const USHORT HEADER_VERSION = 2;

	// Values for mhb_flags
	static const USHORT FLAG_DELETED = 1;	// Shared file has been deleted

	void init(USHORT type, USHORT version)
	{
		mhb_type = type;
		mhb_header_version = HEADER_VERSION;
		mhb_version = version;
		mhb_timestamp = TimeStamp::getCurrentTimeStamp().value();
		mhb_flags = 0;
#ifdef HAVE_SHARED_MUTEX_SECTION
		fb_assert(sizeof(mhb_mutex) <= sizeof(dummy));
#endif
	}

	void markAsDeleted()
	{
		mhb_flags |= FLAG_DELETED;
	}

	bool isDeleted() const
	{
		return (mhb_flags & FLAG_DELETED);
	}

	USHORT mhb_type;
	USHORT mhb_header_version;
	USHORT mhb_version;
	USHORT mhb_flags;
	GDS_TIMESTAMP mhb_timestamp;
	union
	{
#ifdef HAVE_SHARED_MUTEX_SECTION
		struct mtx mhb_mutex;
#endif
		FB_UINT64 dummy[8];			// make sizeof(MemoryHeader) OS-independent
	};
};


#ifdef UNIX

#if !defined(HAVE_FLOCK)
#define USE_FCNTL
#endif

class CountedFd;

class FileLock
{
public:
	enum LockMode {FLM_EXCLUSIVE, FLM_TRY_EXCLUSIVE, FLM_SHARED, FLM_TRY_SHARED};

	typedef void InitFunction(int fd);
	explicit FileLock(const char* fileName, InitFunction* init = NULL);		// main ctor
	FileLock(const FileLock* main, int s);	// creates additional lock for existing file
	~FileLock();

	// Main function to lock file
	int setlock(const LockMode mode);

	// Alternative locker is using status vector to report errors
	bool setlock(Firebird::CheckStatusWrapper* status, const LockMode mode);

	// unlocking can only put error into log file - we can't throw in dtors
	void unlock();

	int getFd();

private:
	enum LockLevel {LCK_NONE, LCK_SHARED, LCK_EXCL};

	LockLevel level;
	CountedFd* oFile;
#ifdef USE_FCNTL
	int lStart;
#endif
	class CountedRWLock* rwcl;		// Due to order of init in ctor rwcl must go after fd & start

	Firebird::string getLockId();
	class CountedRWLock* getRw();
	void rwUnlock();
};

#endif // UNIX


class SharedMemoryBase;		// forward

class IpcObject
{
public:
	virtual bool initialize(SharedMemoryBase*, bool) = 0;
	virtual void mutexBug(int osErrorCode, const char* text) = 0;
	//virtual void eventBug(int osErrorCode, const char* text) = 0;
};


// NS 2014-08-11: FIXME - this class needs major refactoring:
// - remove conditional compilation from header file (for example,
//   you can use virtual interface and factory method to create
//   implementation-specific instance)
// - hide implementation details from interface
// - users of shared mutex implemented by this class shall be using
//   RAII SharedMutexGuard to improve reliability of code
// - shared memory initialization shall be separate from construction
//   so users (event.cpp, tpc.cpp) can avoid storing pointers to a
//   partially constructed object in code
class SharedMemoryBase
{
public:
	SharedMemoryBase(const TEXT* fileName, ULONG size, IpcObject* cb, bool skipLock);
	~SharedMemoryBase();

#ifdef HAVE_OBJECT_MAP
	UCHAR* mapObject(Firebird::CheckStatusWrapper* status, ULONG offset, ULONG size);
	void unmapObject(Firebird::CheckStatusWrapper* status, UCHAR** object, ULONG size);
#endif
	bool remapFile(Firebird::CheckStatusWrapper* status, ULONG newSize, bool truncateFlag);
	void removeMapFile();
	static void unlinkFile(const TEXT* expanded_filename) noexcept;
	Firebird::PathName getMapFileName();

	void mutexLock();
	bool mutexLockCond();
	void mutexUnlock();

	int eventInit(event_t* event);
	void eventFini(event_t* event);
	SLONG eventClear(event_t* event);
	int eventWait(event_t* event, const SLONG value, const SLONG micro_seconds);
	int eventPost(event_t* event);

public:
#ifdef UNIX
	Firebird::AutoPtr<FileLock> mainLock;
#endif
#ifdef WIN_NT
	struct mtx sh_mem_winMutex;
	struct mtx* sh_mem_mutex;
#endif
#ifdef HAVE_SHARED_MUTEX_SECTION
	struct mtx* sh_mem_mutex;
#endif

#ifdef UNIX
	Firebird::AutoPtr<FileLock> initFile;
#endif

	ULONG	sh_mem_length_mapped;
#ifdef WIN_NT
	HANDLE	sh_mem_handle;					// file handle
	HANDLE	sh_mem_object;					// file mapping
	HANDLE	sh_mem_interest;				// event
	HANDLE	sh_mem_hdr_object;				// file mapping
	ULONG*	sh_mem_hdr_address;
#endif
	TEXT	sh_mem_name[MAXPATHLEN];
	MemoryHeader* volatile sh_mem_header; // NS 2014-08-01: FIXME: Why use volatile pointer and not volatile MemoryHeader????
										  // Also, "volatile" variables should never be used directly, only via atomics API
										  // because there is no portable barrier semantics for them. Only MS2005+ generate
										  // barriers, and all other compilers generally do not.

private:
	IpcObject* sh_mem_callback;
#ifdef WIN_NT
	bool sh_mem_unlink;
#endif
	void unlinkFile();
	void internalUnmap();

public:
	enum MemoryTypes
	{
		SRAM_LOCK_MANAGER = 0xFF,		// To avoid mixing with old files no matter of endianness
		SRAM_DATABASE_SNAPSHOT = 0xFE,	// use downcount for shared memory types
		SRAM_EVENT_MANAGER = 0xFD,
		SRAM_TRACE_CONFIG = 0xFC,
		SRAM_TRACE_LOG = 0xFB,
		SRAM_MAPPING_RESET = 0xFA,
		SRAM_TPC_HEADER = 0xF9,
		SRAM_TPC_BLOCK = 0xF8,
		SRAM_TPC_SNAPSHOTS = 0xF7,
		SRAM_CHANGELOG_STATE = 0xF6
	};

protected:
	void logError(const char* text, const Firebird::CheckStatusWrapper* status);
};

template <class Header>		// Header must be "public MemoryHeader"
class SharedMemory : public SharedMemoryBase
{
public:
	SharedMemory(const TEXT* fileName, ULONG size, IpcObject* cb, bool skipLock = false)
		: SharedMemoryBase(fileName, size, cb, skipLock)
	{ }

#ifdef HAVE_OBJECT_MAP
	template <class Object> Object* mapObject(Firebird::CheckStatusWrapper* status, ULONG offset)
	{
		return (Object*) SharedMemoryBase::mapObject(status, offset, sizeof(Object));
	}

	template <class Object> void unmapObject(Firebird::CheckStatusWrapper* status, Object** object)
	{
		SharedMemoryBase::unmapObject(status, (UCHAR**) object, sizeof(Object));
	}
#endif

public:
	void setHeader(Header* hdr)
	{
		sh_mem_header = hdr;	// This implicit cast ensures that Header is "public MemoryHeader"
	}

	const Header* getHeader() const
	{
		return (const Header*) sh_mem_header;
	}

	Header* getHeader()
	{
		return (Header*) sh_mem_header;
	}

};

class SharedMutexGuard
{
public:
	explicit SharedMutexGuard(SharedMemoryBase* shmem, bool init_lock = true)
		: m_shmem(shmem)
	{
		if (init_lock)
			lock();
		else
			m_locked = false;
	}

	bool tryLock() {
		m_locked = m_shmem->mutexLockCond();
		return m_locked;
	}

	void lock() {
		m_shmem->mutexLock();
		m_locked = true;
	}

	void unlock() {
		m_shmem->mutexUnlock();
		m_locked = false;
	}

	bool isLocked() {
		return m_locked;
	}

	~SharedMutexGuard()
	{
		if (m_locked)
			m_shmem->mutexUnlock();
	}

private:
	SharedMutexGuard(const SharedMutexGuard&);
	SharedMutexGuard& operator=(const SharedMutexGuard&);

	SharedMemoryBase* m_shmem;
	bool m_locked;
};


} // namespace Firebird

#ifdef WIN_NT
int		ISC_mutex_init(struct Firebird::mtx*, const TEXT*);
void	ISC_mutex_fini(struct Firebird::mtx*);
int		ISC_mutex_lock(struct Firebird::mtx*);
int		ISC_mutex_unlock(struct Firebird::mtx*);
#endif

ULONG	ISC_exception_post(ULONG, const TEXT*, ISC_STATUS&);

#endif // JRD_ISC_S_PROTO_H
