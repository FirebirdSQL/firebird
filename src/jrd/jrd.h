/*
 *      PROGRAM:        JRD access method
 *      MODULE:         jrd.h
 *      DESCRIPTION:    Common descriptions
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
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 * Claudio Valderrama C.
 * Adriano dos Santos Fernandes
 *
 */

#ifndef JRD_JRD_H
#define JRD_JRD_H

#include "../common/gdsassert.h"
#include "../common/dsc.h"
#include "../jrd/err_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/obj.h"
#include "../jrd/val.h"
#include "../jrd/status.h"

#include "../common/classes/fb_atomic.h"
#include "../common/classes/fb_string.h"
#include "../jrd/MetaName.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/stack.h"
#include "../common/classes/timestamp.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/Synchronize.h"
#include "../common/utils_proto.h"
#include "../common/StatusHolder.h"
#include "../jrd/RandomGenerator.h"
#include "../common/os/guid.h"
#include "../jrd/sbm.h"
#include "../jrd/scl.h"
#include "../jrd/Routine.h"
#include "../jrd/ExtEngineManager.h"
#include "../jrd/Attachment.h"
#include "firebird/Interface.h"

#ifndef CDS_UNAVAILABLE
#include <cds/threading/model.h>	// cds::threading::Manager
#endif

#define BUGCHECK(number)		ERR_bugcheck(number, __FILE__, __LINE__)
#define SOFT_BUGCHECK(number)	ERR_soft_bugcheck(number, __FILE__, __LINE__)
#define CORRUPT(number)			ERR_corrupt(number)
#define IBERROR(number)			ERR_error(number)


#define BLKCHK(blk, type)       if (!blk->checkHandle()) BUGCHECK(147)

#define DEV_BLKCHK(blk, type)	do { } while (false)	// nothing


// Thread data block / IPC related data blocks
#include "../common/ThreadData.h"

// Definition of block types for data allocation in JRD
#include "../include/fb_blk.h"

#include "../jrd/blb.h"

// Definition of DatabasePlugins
#include "../jrd/flu.h"

#include "../jrd/pag.h"

#include "../jrd/RuntimeStatistics.h"
#include "../jrd/Database.h"
#include "../jrd/lck.h"

// Error codes
#include "iberror.h"

struct dsc;

namespace EDS {
	class Connection;
}

namespace Firebird {
	class TextType;
}

namespace Jrd {

inline constexpr unsigned MAX_CALLBACKS = 50;

// fwd. decl.
class thread_db;
class Attachment;
class jrd_tra;
class Request;
class Statement;
class jrd_file;
class Format;
class BufferDesc;
class SparseBitmap;
class jrd_rel;
class ExternalFile;
class ViewContext;
class IndexBlock;
class IndexLock;
class ArrayField;
struct sort_context;
class vcl;
class Parameter;
class jrd_fld;
class dsql_dbb;
class PreparedStatement;
class TraceManager;
class MessageNode;


// Relation trigger definition

class Trigger
{
public:
	Firebird::HalfStaticArray<UCHAR, 128> blr;			// BLR code
	Firebird::HalfStaticArray<UCHAR, 128> debugInfo;	// Debug info
	Statement* statement = nullptr;						// Compiled statement
	bool releaseInProgress = false;
	SSHORT sysTrigger = 0;				// See fb_sysflag in constants.h
	FB_UINT64 type = 0;					// Trigger type
	USHORT flags = 0;					// Flags as they are in RDB$TRIGGERS table
	jrd_rel* relation = nullptr;		// Trigger parent relation
	QualifiedName name;					// Trigger name
	MetaName engine;					// External engine name
	MetaName owner;						// Owner for SQL SECURITY
	Firebird::string entryPoint;		// External trigger entrypoint
	Firebird::string extBody;			// External trigger body
	Firebird::TriState ssDefiner;		// SQL SECURITY
	std::unique_ptr<ExtEngineManager::Trigger> extTrigger;	// External trigger

	bool isActive() const;

	void compile(thread_db*);				// Ensure that trigger is compiled
	void release(thread_db*);				// Try to free trigger request

	explicit Trigger(MemoryPool& p)
		: blr(p), debugInfo(p), entryPoint(p), extBody(p)
	{}
};


// Array of triggers (suppose separate arrays for triggers of different types)

class TrigVector : public Firebird::ObjectsArray<Trigger>
{
public:
	explicit TrigVector(Firebird::MemoryPool& pool)
		: Firebird::ObjectsArray<Trigger>(pool),
		  useCount(0)
	{ }

	TrigVector()
		: Firebird::ObjectsArray<Trigger>(),
		  useCount(0)
	{ }

	void addRef()
	{
		++useCount;
	}

	bool hasActive() const;

	void decompile(thread_db* tdbb);

	void release();
	void release(thread_db* tdbb);

	~TrigVector()
	{
		fb_assert(useCount.value() == 0);
	}

private:
	Firebird::AtomicCounter useCount;
};


//
// Flags to indicate normal internal requests vs. dyn internal requests
//
// IRQ_REQUESTS and DYN_REQUESTS are deprecated
inline constexpr int IRQ_REQUESTS		= 1;
inline constexpr int DYN_REQUESTS		= 2;
inline constexpr int CACHED_REQUESTS	= 3;


// Procedure block

class jrd_prc final : public Routine
{
public:
	const Format*	prc_record_format = nullptr;
	prc_t			prc_type = prc_legacy;			// procedure type

	const ExtEngineManager::Procedure* getExternal() const noexcept { return prc_external; }
	void setExternal(const ExtEngineManager::Procedure* value) noexcept { prc_external = value; }

private:
	const ExtEngineManager::Procedure* prc_external = nullptr;

public:
	explicit jrd_prc(MemoryPool& p)
		: Routine(p)
	{
	}

public:
	int getObjectType() const noexcept override
	{
		return obj_procedure;
	}

	SLONG getSclType() const noexcept override
	{
		return obj_procedures;
	}

	void releaseFormat() override
	{
		delete prc_record_format;
		prc_record_format = NULL;
	}

	~jrd_prc() override
	{
		delete prc_external;
	}

	bool checkCache(thread_db* tdbb) const override;
	void clearCache(thread_db* tdbb) override;

	void releaseExternal() override
	{
		delete prc_external;
		prc_external = NULL;
	}

protected:
	bool reload(thread_db* tdbb) override;	// impl is in met.epp
};


// Parameter block

class Parameter final : public pool_alloc<type_prm>
{
public:
	USHORT		prm_number;
	dsc			prm_desc;
	NestConst<ValueExprNode>	prm_default_value;
	bool		prm_nullable;
	prm_mech_t	prm_mechanism;
	MetaName prm_name;
	QualifiedName prm_field_source;
	MetaName prm_type_of_column;
	QualifiedName prm_type_of_table;
	std::optional<USHORT> prm_text_type;
	FUN_T		prm_fun_mechanism;

public:
	explicit Parameter(MemoryPool& p)
		: prm_name(p),
		  prm_field_source(p),
		  prm_type_of_column(p),
		  prm_type_of_table(p)
	{
	}
};

// Index block to cache index information

class IndexBlock final : public pool_alloc<type_idb>
{
public:
	IndexBlock*	idb_next;
	ValueExprNode* idb_expression;			// node tree for index expression
	Statement* idb_expression_statement;	// statement for index expression evaluation
	dsc			idb_expression_desc;		// descriptor for expression result
	BoolExprNode* idb_condition;			// node tree for index condition
	Statement* idb_condition_statement;		// statement for index condition evaluation
	Lock*		idb_lock;					// lock to synchronize changes to index
	USHORT		idb_id;
};


//
// Transaction element block
//
struct teb
{
	Attachment* teb_database;
	int teb_tpb_length;
	const UCHAR* teb_tpb;
};

typedef teb TEB;

// Window block for loading cached pages into
// CVC: Apparently, the only possible values are HEADER_PAGE==0 and LOG_PAGE==2
// and reside in ods.h, although I watched a place with 1 and others with members
// of a struct.

struct win
{
	PageNumber win_page;
	Ods::pag* win_buffer;
	class BufferDesc* win_bdb;
	SSHORT win_scans;
	USHORT win_flags;
	explicit win(const PageNumber& wp) noexcept
		: win_page(wp), win_bdb(NULL), win_flags(0)
	{}
	win(const USHORT pageSpaceID, const ULONG pageNum) noexcept
		: win_page(pageSpaceID, pageNum), win_bdb(NULL), win_flags(0)
	{}
};

typedef win WIN;

// This is a compilation artifact: I wanted to be sure I would pick all old "win"
// declarations at the top, so "win" was built with a mandatory argument in
// the constructor. This struct satisfies a single place with an array. The
// alternative would be to initialize 16 elements of the array with 16 calls
// to the constructor: win my_array[n] = {win(-1), ... (win-1)};
// When all places are changed, this class can disappear and win's constructor
// may get the default value of ~0 to "wp".
struct win_for_array : public win
{
	win_for_array() noexcept
		: win(DB_PAGE_SPACE, ~0)
	{}
};

// win_flags

inline constexpr USHORT WIN_large_scan			= 1;	// large sequential scan
inline constexpr USHORT WIN_secondary			= 2;	// secondary stream
inline constexpr USHORT WIN_garbage_collector	= 4;	// garbage collector's window
inline constexpr USHORT WIN_garbage_collect		= 8;	// scan left a page for garbage collector


#ifdef USE_ITIMER
class TimeoutTimer final :
	public Firebird::RefCntIface<Firebird::ITimerImpl<TimeoutTimer, Firebird::CheckStatusWrapper> >
{
public:
	explicit TimeoutTimer() noexcept
		: m_started(0),
		  m_expired(false),
		  m_value(0),
		  m_error(0)
	{ }

	// ITimer implementation
	void handler() override;

	bool expired() const noexcept
	{
		return m_expired;
	}

	unsigned int getValue() const noexcept
	{
		return m_value;
	}

	unsigned int getErrCode() const noexcept
	{
		return m_error;
	}

	// milliseconds left before timer expiration
	unsigned int timeToExpire() const;

	// evaluate expire timestamp using start timestamp
	bool getExpireTimestamp(const ISC_TIMESTAMP_TZ start, ISC_TIMESTAMP_TZ& exp) const;

	// set timeout value in milliseconds and secondary error code
	void setup(unsigned int value, ISC_STATUS error) noexcept
	{
		m_value = value;
		m_error = error;
	}

	void start();
	void stop();

private:
	SINT64 m_started;
	bool m_expired;
	unsigned int m_value;	// milliseconds
	ISC_STATUS m_error;
};
#else
class TimeoutTimer final : public Firebird::RefCounted
{
public:
	explicit TimeoutTimer() noexcept
		: m_start(0),
		  m_value(0),
		  m_error(0)
	{ }

	bool expired() const;

	unsigned int getValue() const noexcept
	{
		return m_value;
	}

	unsigned int getErrCode() const noexcept
	{
		return m_error;
	}

	// milliseconds left before timer expiration
	unsigned int timeToExpire() const;

	// clock value when timer will expire
	bool getExpireClock(SINT64& clock) const noexcept;

	// set timeout value in milliseconds and secondary error code
	void setup(unsigned int value, ISC_STATUS error) noexcept
	{
		m_start = 0;
		m_value = value;
		m_error = error;
	}

	void start();
	void stop() noexcept;

private:
	SINT64 currTime() const
	{
		return fb_utils::query_performance_counter() * 1000 / fb_utils::query_performance_frequency();
	}

	SINT64 m_start;
	unsigned int m_value;	// milliseconds
	ISC_STATUS m_error;
};
#endif // USE_ITIMER

// Thread specific database block

// tdbb_flags

inline constexpr ULONG TDBB_sweeper				= 1;		// Thread sweeper or garbage collector
inline constexpr ULONG TDBB_no_cache_unwind		= 2;		// Don't unwind page buffer cache
inline constexpr ULONG TDBB_backup_write_locked	= 4;    	// BackupManager has write lock on LCK_backup_database
inline constexpr ULONG TDBB_stack_trace_done	= 8;		// PSQL stack trace is added into status-vector
inline constexpr ULONG TDBB_dont_post_dfw		= 16;		// dont post DFW tasks as deferred work is performed now
inline constexpr ULONG TDBB_sys_error			= 32;		// error shouldn't be handled by the looper
inline constexpr ULONG TDBB_verb_cleanup		= 64;		// verb cleanup is in progress
inline constexpr ULONG TDBB_use_db_page_space	= 128;		// use database (not temporary) page space in GTT operations
inline constexpr ULONG TDBB_detaching			= 256;		// detach is in progress
inline constexpr ULONG TDBB_wait_cancel_disable	= 512;		// don't cancel current waiting operation
inline constexpr ULONG TDBB_cache_unwound		= 1024;		// page cache was unwound
inline constexpr ULONG TDBB_reset_stack			= 2048;		// stack should be reset after stack overflow exception
inline constexpr ULONG TDBB_dfw_cleanup			= 4096;		// DFW cleanup phase is active
inline constexpr ULONG TDBB_repl_in_progress	= 8192;		// Prevent recursion in replication
inline constexpr ULONG TDBB_replicator			= 16384;	// Replicator
inline constexpr ULONG TDBB_async				= 32768;	// Async context (set in AST)

class thread_db final : public Firebird::ThreadData
{
	static constexpr int QUANTUM		= 100;	// Default quantum
	static constexpr int SWEEP_QUANTUM	= 10;	// Make sweeps less disruptive

private:
	MemoryPool*	defaultPool;
	void setDefaultPool(MemoryPool* p) noexcept
	{
		defaultPool = p;
	}
	friend class Firebird::SubsystemContextPoolHolder <Jrd::thread_db, MemoryPool>;
	Database*	database;
	Attachment*	attachment;
	jrd_tra*	transaction;
	Request*	request;
	RuntimeStatistics *reqStat, *traStat, *attStat, *dbbStat;

public:
	explicit thread_db(FbStatusVector* status)
		: ThreadData(ThreadData::tddDBB),
		  defaultPool(NULL),
		  database(NULL),
		  attachment(NULL),
		  transaction(NULL),
		  request(NULL),
		  tdbb_status_vector(status),
		  tdbb_quantum(QUANTUM),
		  tdbb_flags(0),
		  tdbb_temp_traid(0),
		  tdbb_bdbs(*getDefaultMemoryPool()),
		  tdbb_thread(Firebird::ThreadSync::getThread("thread_db"))
	{
		reqStat = traStat = attStat = dbbStat = RuntimeStatistics::getDummy();
		fb_utils::init_status(tdbb_status_vector);
	}

	~thread_db()
	{
		resetStack();

#ifdef DEV_BUILD
		for (FB_SIZE_T n = 0; n < tdbb_bdbs.getCount(); ++n)
		{
			fb_assert(tdbb_bdbs[n] == NULL);
		}
#endif
	}

	FbStatusVector*	tdbb_status_vector;
	SLONG		tdbb_quantum;		// Cycles remaining until voluntary schedule
	ULONG		tdbb_flags;

	TraNumber	tdbb_temp_traid;	// current temporary table scope

	// BDB's held by thread
	Firebird::HalfStaticArray<BufferDesc*, 16> tdbb_bdbs;
	Firebird::ThreadSync* tdbb_thread;

	MemoryPool* getDefaultPool() noexcept
	{
		return defaultPool;
	}

	Database* getDatabase() noexcept
	{
		return database;
	}

	const Database* getDatabase() const noexcept
	{
		return database;
	}

	void setDatabase(Database* val);

	Attachment* getAttachment() noexcept
	{
		return attachment;
	}

	const Attachment* getAttachment() const noexcept
	{
		return attachment;
	}

	void setAttachment(Attachment* val);

	jrd_tra* getTransaction() noexcept
	{
		return transaction;
	}

	const jrd_tra* getTransaction() const noexcept
	{
		return transaction;
	}

	void setTransaction(jrd_tra* val);

	Request* getRequest() noexcept
	{
		return request;
	}

	const Request* getRequest() const noexcept
	{
		return request;
	}

	void setRequest(Request* val);

	SSHORT getCharSet() const noexcept;

	void markAsSweeper() noexcept
	{
		tdbb_quantum = SWEEP_QUANTUM;
		tdbb_flags |= TDBB_sweeper;
	}

	void bumpStats(const PageStatType type, SINT64 delta = 1)
	{
		reqStat->bumpValue(type, delta);
		traStat->bumpValue(type, delta);
		attStat->bumpValue(type, delta);

		if ((tdbb_flags & TDBB_async) && !attachment)
			dbbStat->bumpValue(type, delta);

		// else dbbStat is adjusted from attStat, see Attachment::mergeAsyncStats()
	}

	void bumpStats(const RecordStatType type, SLONG relation_id, SINT64 delta = 1)
	{
		// We don't bump counters for dbbStat here, they're merged from attStats on demand

		reqStat->bumpValue(type, delta);
		traStat->bumpValue(type, delta);
		attStat->bumpValue(type, delta);

		const RuntimeStatistics* const dummyStat = RuntimeStatistics::getDummy();

		// We expect that at least attStat is present (not a dummy object)

		fb_assert(attStat != dummyStat);

		// Relation statistics is a quite complex beast, so a conditional check
		// does not hurt. It also allows to avoid races while accessing the static
		// dummy object concurrently.

		if (reqStat != dummyStat)
			reqStat->bumpValue(type, relation_id, delta);

		if (traStat != dummyStat)
			traStat->bumpValue(type, relation_id, delta);

		if (attStat != dummyStat)
			attStat->bumpValue(type, relation_id, delta);
	}

	ISC_STATUS getCancelState(ISC_STATUS* secondary = NULL);
	void checkCancelState();
	void reschedule();
	const TimeoutTimer* getTimeoutTimer() const
	{
		return tdbb_reqTimer;
	}

	// Returns minimum of passed wait timeout and time to expiration of reqTimer.
	// Timer value is rounded to the upper whole second.
	ULONG adjustWait(ULONG wait) const;

	void registerBdb(BufferDesc* bdb)
	{
		if (tdbb_bdbs.isEmpty()) {
			tdbb_flags &= ~TDBB_cache_unwound;
		}
		fb_assert(!(tdbb_flags & TDBB_cache_unwound));

		FB_SIZE_T pos;
		if (tdbb_bdbs.find(NULL, pos))
			tdbb_bdbs[pos] = bdb;
		else
			tdbb_bdbs.add(bdb);
	}

	bool clearBdb(BufferDesc* bdb)
	{
		if (tdbb_bdbs.isEmpty())
		{
			// hvlad: the only legal case when thread holds no latches but someone
			// tried to release latch is when CCH_unwind was called (and released
			// all latches) but caller is unaware about it. See CORE-3034, for example.
			// Else it is bug and should be BUGCHECK'ed.

			if (tdbb_flags & TDBB_cache_unwound)
				return false;
		}
		fb_assert(!(tdbb_flags & TDBB_cache_unwound));

		FB_SIZE_T pos;
		if (!tdbb_bdbs.find(bdb, pos))
			BUGCHECK(300);	// can't find shared latch

		tdbb_bdbs[pos] = NULL;

		if (pos == tdbb_bdbs.getCount() - 1)
		{
			while (true)
			{
				if (tdbb_bdbs[pos] != NULL)
				{
					tdbb_bdbs.shrink(pos + 1);
					break;
				}

				if (pos == 0)
				{
					tdbb_bdbs.shrink(0);
					break;
				}

				--pos;
			}
		}

		return true;
	}

	void resetStack()
	{
		if (tdbb_flags & TDBB_reset_stack)
		{
			tdbb_flags &= ~TDBB_reset_stack;
#ifdef WIN_NT
			_resetstkoflw();
#endif
		}
	}

	class TimerGuard
	{
	public:
		TimerGuard(thread_db* tdbb, TimeoutTimer* timer, bool autoStop)
			: m_tdbb(tdbb),
			  m_autoStop(autoStop && timer),
			  m_saveTimer(tdbb->tdbb_reqTimer)
		{
			m_tdbb->tdbb_reqTimer = timer;
			if (timer && timer->expired())
				m_tdbb->tdbb_quantum = 0;
		}

		~TimerGuard()
		{
			if (m_autoStop)
				m_tdbb->tdbb_reqTimer->stop();

			m_tdbb->tdbb_reqTimer = m_saveTimer;
		}

	private:
		thread_db* m_tdbb;
		bool m_autoStop;
		Firebird::RefPtr<TimeoutTimer> m_saveTimer;
	};

private:
	Firebird::RefPtr<TimeoutTimer> tdbb_reqTimer;

};

class ThreadContextHolder
{
public:
	explicit ThreadContextHolder(Firebird::CheckStatusWrapper* status = NULL)
		: context(status ? status : &localStatus)
	{
		context.putSpecific();

#ifndef CDS_UNAVAILABLE
		if (!cds::threading::Manager::isThreadAttached())
			cds::threading::Manager::attachThread();
#endif
	}

	ThreadContextHolder(Database* dbb, Jrd::Attachment* att, FbStatusVector* status = NULL)
		: context(status ? status : &localStatus)
	{
		context.putSpecific();
		context.setDatabase(dbb);
		context.setAttachment(att);

#ifndef CDS_UNAVAILABLE
		if (!cds::threading::Manager::isThreadAttached())
			cds::threading::Manager::attachThread();
#endif
	}

	~ThreadContextHolder()
	{
		Firebird::ThreadData::restoreSpecific();
	}

	// copying is prohibited
	ThreadContextHolder(const ThreadContextHolder&) = delete;
	ThreadContextHolder& operator= (const ThreadContextHolder&) = delete;

	thread_db* operator->() noexcept
	{
		return &context;
	}

	operator thread_db*() noexcept
	{
		return &context;
	}

private:
	Firebird::FbLocalStatus localStatus;
	thread_db context;
};


// Helper class to temporarily activate sweeper context
class ThreadSweepGuard
{
public:
	explicit ThreadSweepGuard(thread_db* tdbb) noexcept
		: m_tdbb(tdbb)
	{
		m_tdbb->markAsSweeper();
	}

	~ThreadSweepGuard()
	{
		m_tdbb->tdbb_flags &= ~TDBB_sweeper;
	}

private:
	thread_db* const m_tdbb;
};

// CVC: This class was designed to restore the thread's default status vector automatically.
// In several places, tdbb_status_vector is replaced by a local temporary.
class ThreadStatusGuard
{
public:
	explicit ThreadStatusGuard(thread_db* tdbb)
		: m_tdbb(tdbb), m_old_status(tdbb->tdbb_status_vector)
	{
		m_tdbb->tdbb_status_vector = &m_local_status;
	}

	~ThreadStatusGuard()
	{
		m_tdbb->tdbb_status_vector = m_old_status;
	}

	// copying is prohibited
	ThreadStatusGuard(const ThreadStatusGuard&) = delete;
	ThreadStatusGuard& operator=(const ThreadStatusGuard&) = delete;

	FbStatusVector* restore() noexcept
	{
		m_tdbb->tdbb_status_vector = m_old_status;
		return m_old_status;
	}

	operator FbStatusVector*() noexcept { return &m_local_status; }
	FbStatusVector* operator->() noexcept { return &m_local_status; }

	operator const FbStatusVector*() const noexcept { return &m_local_status; }
	const FbStatusVector* operator->() const noexcept { return &m_local_status; }

	void copyToOriginal() noexcept
	{
		fb_utils::copyStatus(m_old_status, &m_local_status);
	}

private:
	Firebird::FbLocalStatus m_local_status;
	thread_db* const m_tdbb;
	FbStatusVector* const m_old_status;
};


// duplicate context of firebird string
inline char* stringDup(MemoryPool& p, const Firebird::string& s)
{
	char* rc = (char*) p.allocate(s.length() + 1 ALLOC_ARGS);
	strcpy(rc, s.c_str());
	return rc;
}

inline char* stringDup(MemoryPool& p, const char* s, size_t l)
{
	char* rc = (char*) p.allocate(l + 1 ALLOC_ARGS);
	memcpy(rc, s, l);
	rc[l] = 0;
	return rc;
}

inline char* stringDup(MemoryPool& p, const char* s)
{
	if (! s)
	{
		return 0;
	}
	return stringDup(p, s, strlen(s));
}

// Used in string conversion calls
typedef Firebird::HalfStaticArray<UCHAR, 256> MoveBuffer;

} //namespace Jrd

inline bool JRD_reschedule(Jrd::thread_db* tdbb, bool force = false)
{
	if (force || --tdbb->tdbb_quantum < 0)
	{
		tdbb->reschedule();
		return true;
	}

	return false;
}

// Threading macros

/* Define JRD_get_thread_data off the platform specific version.
 * If we're in DEV mode, also do consistancy checks on the
 * retrieved memory structure.  This was originally done to
 * track down cases of no "PUT_THREAD_DATA" on the NLM.
 *
 * This allows for NULL thread data (which might be an error by itself)
 * If there is thread data,
 * AND it is tagged as being a thread_db.
 * AND it has a non-NULL database field,
 * THEN we validate that the structure there is a database block.
 * Otherwise, we return what we got.
 * We can't always validate the database field, as during initialization
 * there is no database set up.
 */

#if defined(DEV_BUILD)
#include "../jrd/err_proto.h"

inline Jrd::thread_db* JRD_get_thread_data()
{
	Firebird::ThreadData* p1 = Firebird::ThreadData::getSpecific();
	if (p1 && p1->getType() == Firebird::ThreadData::tddDBB)
	{
		Jrd::thread_db* p2 = (Jrd::thread_db*) p1;
		if (p2->getDatabase() && !p2->getDatabase()->checkHandle())
		{
			BUGCHECK(147);
		}
	}
	return (Jrd::thread_db*) p1;
}

inline void CHECK_TDBB(const Jrd::thread_db* tdbb)
{
	fb_assert(tdbb && (tdbb->getType() == Firebird::ThreadData::tddDBB) &&
		(!tdbb->getDatabase() || tdbb->getDatabase()->checkHandle()));
}

inline void CHECK_DBB(const Jrd::Database* dbb)
{
	fb_assert(dbb && dbb->checkHandle());
}

#else // PROD_BUILD

inline Jrd::thread_db* JRD_get_thread_data()
{
	return (Jrd::thread_db*) Firebird::ThreadData::getSpecific();
}

inline void CHECK_DBB(const Jrd::Database*)
{
}

inline void CHECK_TDBB(const Jrd::thread_db*)
{
}

#endif

inline Jrd::Database* GET_DBB()
{
	return JRD_get_thread_data()->getDatabase();
}

/*-------------------------------------------------------------------------*
 * macros used to set thread_db and Database pointers when there are not set already *
 *-------------------------------------------------------------------------*/
inline void SET_TDBB(Jrd::thread_db*& tdbb)
{
	if (tdbb == NULL) {
		tdbb = JRD_get_thread_data();
	}
	CHECK_TDBB(tdbb);
}

inline void SET_DBB(Jrd::Database*& dbb)
{
	if (dbb == NULL) {
		dbb = GET_DBB();
	}
	CHECK_DBB(dbb);
}


// global variables for engine

namespace Jrd {
	typedef Firebird::SubsystemContextPoolHolder <Jrd::thread_db, MemoryPool> ContextPoolHolder;

	class DatabaseContextHolder : public Jrd::ContextPoolHolder
	{
	public:
		explicit DatabaseContextHolder(thread_db* tdbb)
			: Jrd::ContextPoolHolder(tdbb, tdbb->getDatabase()->dbb_permanent)
		{}

		// copying is prohibited
		DatabaseContextHolder(const DatabaseContextHolder&) = delete;
		DatabaseContextHolder& operator=(const DatabaseContextHolder&) = delete;
	};

	class BackgroundContextHolder : public ThreadContextHolder, public DatabaseContextHolder,
		public Jrd::Attachment::SyncGuard
	{
	public:
		BackgroundContextHolder(Database* dbb, Jrd::Attachment* att, FbStatusVector* status, const char* f)
			: ThreadContextHolder(dbb, att, status),
			  DatabaseContextHolder(operator thread_db*()),
			  Jrd::Attachment::SyncGuard(att, f)
		{}

		// copying is prohibited
		BackgroundContextHolder(const BackgroundContextHolder&) = delete;
		BackgroundContextHolder& operator=(const BackgroundContextHolder&) = delete;
	};

	class AttachmentHolder
	{
	public:
		static constexpr unsigned ATT_LOCK_ASYNC		= 1;
		static constexpr unsigned ATT_DONT_LOCK			= 2;
		static constexpr unsigned ATT_NO_SHUTDOWN_CHECK	= 4;
		static constexpr unsigned ATT_NON_BLOCKING		= 8;

		AttachmentHolder(thread_db* tdbb, StableAttachmentPart* sa, unsigned lockFlags, const char* from);
		~AttachmentHolder();

		// copying is prohibited
		AttachmentHolder(const AttachmentHolder&) = delete;
		AttachmentHolder& operator =(const AttachmentHolder&) = delete;

	private:
		Firebird::RefPtr<StableAttachmentPart> sAtt;
		bool async;			// async mutex should be locked instead normal
		bool nolock; 		// if locked manually, no need to take lock recursively
		bool blocking;		// holder instance is blocking other instances
	};

	class EngineContextHolder final : public ThreadContextHolder, private AttachmentHolder, private DatabaseContextHolder
	{
	public:
		template <typename I>
		EngineContextHolder(Firebird::CheckStatusWrapper* status, I* interfacePtr, const char* from,
							unsigned lockFlags = 0);
	};

	class AstLockHolder : public Firebird::ReadLockGuard
	{
	public:
		AstLockHolder(Database* dbb, const char* f)
			: Firebird::ReadLockGuard(dbb->dbb_ast_lock, f)
		{
			if (dbb->dbb_flags & DBB_no_ast)
			{
				// usually to be swallowed by the AST, but it allows to skip its execution
				Firebird::status_exception::raise(Firebird::Arg::Gds(isc_unavailable));
			}
		}
	};

	class AsyncContextHolder : public AstLockHolder, public Jrd::Attachment::SyncGuard,
		public ThreadContextHolder, public DatabaseContextHolder
	{
	public:
		AsyncContextHolder(Database* dbb, const char* f, Lock* lck = NULL)
			: AstLockHolder(dbb, f),
			  Jrd::Attachment::SyncGuard(lck ?
				lck->getLockStable() : Firebird::RefPtr<StableAttachmentPart>(), f, true),
			  ThreadContextHolder(dbb, lck ? lck->getLockAttachment() : NULL),
			  DatabaseContextHolder(operator thread_db*())
		{
			if (lck)
			{
				// The lock could be released while we were waiting on the attachment mutex.
				// This may cause a segfault due to lck_attachment == NULL stored in tdbb.

				if (!lck->lck_id)
				{
					// usually to be swallowed by the AST, but it allows to skip its execution
					Firebird::status_exception::raise(Firebird::Arg::Gds(isc_unavailable));
				}

				fb_assert((operator thread_db*())->getAttachment());
			}

			(*this)->tdbb_flags |= TDBB_async;
		}

		// copying is prohibited
		AsyncContextHolder(const AsyncContextHolder&) = delete;
		AsyncContextHolder& operator=(const AsyncContextHolder&) = delete;
	};

	class EngineCheckout
	{
	public:
		enum Type
		{
			REQUIRED,
			UNNECESSARY,
			AVOID
		};

		EngineCheckout(thread_db* tdbb, const char* from, Type type = REQUIRED)
			: m_tdbb(tdbb), m_from(from)
		{
			if (type != AVOID)
			{
				Attachment* const att = tdbb ? tdbb->getAttachment() : NULL;

				if (att)
					m_ref = att->getStable();

				fb_assert(type == UNNECESSARY || m_ref.hasData());

				if (m_ref.hasData())
					m_ref->getSync()->leave();
			}
		}

		EngineCheckout(Attachment* att, const char* from, Type type = REQUIRED)
			: m_tdbb(nullptr), m_from(from)
		{
			if (type != AVOID)
			{
				fb_assert(type == UNNECESSARY || att);

				if (att && att->att_use_count)
				{
					m_ref = att->getStable();
					m_ref->getSync()->leave();
				}
			}
		}

		~EngineCheckout()
		{
			if (m_ref.hasData())
				m_ref->getSync()->enter(m_from);

			// If we were signalled to cancel/shutdown, react as soon as possible.
			// We cannot throw immediately, but we can reschedule ourselves.

			if (m_tdbb && m_tdbb->tdbb_quantum > 0 && m_tdbb->getCancelState() != FB_SUCCESS)
				m_tdbb->tdbb_quantum = 0;
		}

		// copying is prohibited
		EngineCheckout(const EngineCheckout&) = delete;
		EngineCheckout& operator=(const EngineCheckout&) = delete;

	private:
		thread_db* const m_tdbb;
		Firebird::RefPtr<StableAttachmentPart> m_ref;
		const char* m_from;
	};

	class CheckoutLockGuard
	{
	public:
		CheckoutLockGuard(thread_db* tdbb, Firebird::Mutex& mutex,
						  const char* from, bool optional = false)
			: m_mutex(mutex)
		{
			if (!m_mutex.tryEnter(from))
			{
				EngineCheckout cout(tdbb, from, optional ? EngineCheckout::UNNECESSARY : EngineCheckout::REQUIRED);
				m_mutex.enter(from);
			}
		}

		~CheckoutLockGuard()
		{
			m_mutex.leave();
		}

		// copying is prohibited
		CheckoutLockGuard(const CheckoutLockGuard&) = delete;
		CheckoutLockGuard& operator=(const CheckoutLockGuard&) = delete;

	private:
		Firebird::Mutex& m_mutex;
	};

	class CheckoutSyncGuard
	{
	public:
		CheckoutSyncGuard(thread_db* tdbb, Firebird::SyncObject& sync,
						  Firebird::SyncType type,
						  const char* from, bool optional = false)
			: m_sync(&sync, from)
		{
			if (!m_sync.lockConditional(type, from))
			{
				EngineCheckout cout(tdbb, from, optional ? EngineCheckout::UNNECESSARY : EngineCheckout::REQUIRED);
				m_sync.lock(type);
			}
		}

		// copying is prohibited
		CheckoutSyncGuard(const CheckoutSyncGuard&) = delete;
		CheckoutSyncGuard& operator=(const CheckoutSyncGuard&) = delete;

	private:
		Firebird::Sync m_sync;
	};
}

#endif // JRD_JRD_H
