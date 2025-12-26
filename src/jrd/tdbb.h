/*
 *      PROGRAM:        JRD access method
 *      MODULE:         tdbb.h
 *      DESCRIPTION:    Thread specific database block
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

#ifndef JRD_TDBB_H
#define JRD_TDBB_H

#include <cds/threading/model.h>	// cds::threading::Manager

#include "../common/gdsassert.h"

#include "../common/classes/Synchronize.h"

#include "../jrd/RuntimeStatistics.h"
#include "../jrd/status.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl.h"

#define BUGCHECK(number) ERR_bugcheck(number, __FILE__, __LINE__)


namespace Firebird {

class MemoryPool;

}


namespace Jrd
{

class Database;
class Attachment;
class jrd_tra;
class Request;
class BufferDesc;
class Lock;


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

	CSetId getCharSet() const noexcept;

	void markAsSweeper() noexcept
	{
		tdbb_quantum = SWEEP_QUANTUM;
		tdbb_flags |= TDBB_sweeper;
	}

	void bumpStats(const PageStatType type, ULONG pageSpaceId, SINT64 delta = 1)
	{
		fb_assert(pageSpaceId != INVALID_PAGE_SPACE);

		// [0] element stores statistics for temporary page spaces
		if (PageSpace::isTemporary(pageSpaceId))
			pageSpaceId = 0;

		reqStat->bumpValue(type, pageSpaceId, delta);
		traStat->bumpValue(type, pageSpaceId, delta);
		attStat->bumpValue(type, pageSpaceId, delta);

		if ((tdbb_flags & TDBB_async) && !attachment)
			dbbStat->bumpValue(type, pageSpaceId, delta);

		// else dbbStat is adjusted from attStat, see Attachment::mergeStats()
	}

	void bumpStats(const RecordStatType type, SLONG relationId, SINT64 delta = 1)
	{
		// We expect that at least attStat is present (not a dummy object)

		fb_assert(attStat != RuntimeStatistics::getDummy());

		reqStat->bumpValue(type, relationId, delta);
		traStat->bumpValue(type, relationId, delta);
		attStat->bumpValue(type, relationId, delta);

		// We don't bump counters for dbbStat here, they're merged from attStats on demand
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

} // namespace Jrd

#endif // JRD_TDBB_H
