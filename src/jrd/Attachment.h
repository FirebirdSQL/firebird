/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Attachment.h
 *      DESCRIPTION:    JRD Attachment class
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
 */

#ifndef JRD_ATTACHMENT_H
#define JRD_ATTACHMENT_H

#define DEBUG_LCK_LIST

#include "firebird.h"
// Definition of block types for data allocation in JRD
#include "../include/fb_blk.h"
#include "../jrd/scl.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/RandomGenerator.h"
#include "../jrd/RuntimeStatistics.h"
#include "../jrd/Coercion.h"
#include "../jrd/LocalTemporaryTable.h"

#include "../common/classes/ByteChunk.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/SyncObject.h"
#include "../common/classes/array.h"
#include "../common/classes/stack.h"
#include "../common/classes/timestamp.h"
#include "../common/classes/TimerImpl.h"
#include "../common/classes/TriState.h"
#include "../common/ThreadStart.h"
#include "../common/TimeZoneUtil.h"

#include "../jrd/EngineInterface.h"
#include "../jrd/sbm.h"

#include <atomic>
#include <initializer_list>
#include <optional>


namespace Firebird::Jrd::EDS {
	class Connection;
}

namespace Firebird::Jrd::Replication {
	class TableMatcher;
}

namespace Firebird::Jrd {
	class thread_db;
	class Database;
	class jrd_tra;
	class Request;
	class Lock;
	class jrd_file;
	class Format;
	class BufferControl;
	class PageToBufferMap;
	class jrd_rel;
	class ExternalFile;
	class ViewContext;
	class ArrayField;
	struct sort_context;
	class vcl;
	class Parameter;
	class jrd_fld;
	class dsql_dbb;
	class PreparedStatement;
	class TraceManager;
	template <typename T> class vec;
	class jrd_rel;
	class jrd_prc;
	class Trigger;
	class Triggers;
	class Function;
	class Statement;
	class ProfilerManager;
	class Validation;
	enum InternalRequest : USHORT;

	namespace Replication
	{
		class Applier;
	}


struct DSqlCacheItem
{
	DSqlCacheItem(MemoryPool& pool)
		: key(pool),
		  obsoleteMap(pool),
		  lock(nullptr),
		  locked(false)
	{
	}

	string key;
	LeftPooledMap<QualifiedName, bool> obsoleteMap;
	Lock* lock;
	bool locked;
};

typedef GenericMap<Pair<Full<
	string, DSqlCacheItem> > > DSqlCache;


struct DdlTriggerContext
{
	DdlTriggerContext()
		: eventType(*getDefaultMemoryPool()),
		  objectType(*getDefaultMemoryPool()),
		  objectName(*getDefaultMemoryPool()),
		  oldObjectName(*getDefaultMemoryPool()),
		  newObjectName(*getDefaultMemoryPool()),
		  sqlText(*getDefaultMemoryPool())
	{
	}

	string eventType;
	string objectType;
	QualifiedName objectName;
	QualifiedName oldObjectName;
	QualifiedName newObjectName;
	string sqlText;
};


// Attachment flags

inline constexpr ULONG ATT_no_cleanup			= 0x00001L;	// Don't expunge, purge, or garbage collect
inline constexpr ULONG ATT_shutdown				= 0x00002L;	// attachment has been shutdown
inline constexpr ULONG ATT_shutdown_manager		= 0x00004L;	// attachment requesting shutdown
inline constexpr ULONG ATT_exclusive			= 0x00008L;	// attachment wants exclusive database access
inline constexpr ULONG ATT_attach_pending		= 0x00010L;	// Indicate attachment is only pending
inline constexpr ULONG ATT_exclusive_pending	= 0x00020L;	// Indicate exclusive attachment pending
inline constexpr ULONG ATT_notify_gc			= 0x00040L;	// Notify garbage collector to expunge, purge ..
inline constexpr ULONG ATT_garbage_collector	= 0x00080L;	// I'm a garbage collector
inline constexpr ULONG ATT_cancel_raise			= 0x00100L;	// Cancel currently running operation
inline constexpr ULONG ATT_cancel_disable		= 0x00200L;	// Disable cancel operations
inline constexpr ULONG ATT_no_db_triggers		= 0x00400L;	// Don't execute database triggers
inline constexpr ULONG ATT_manual_lock			= 0x00800L;	// Was locked manually
inline constexpr ULONG ATT_async_manual_lock	= 0x01000L;	// Async mutex was locked manually
inline constexpr ULONG ATT_overwrite_check		= 0x02000L;	// Attachment checks is it possible to overwrite DB
inline constexpr ULONG ATT_system				= 0x04000L; // Special system attachment
inline constexpr ULONG ATT_creator				= 0x08000L; // This attachment created the DB
inline constexpr ULONG ATT_monitor_disabled		= 0x10000L; // Monitoring lock is downgraded
inline constexpr ULONG ATT_security_db			= 0x20000L; // Attachment used for security purposes
inline constexpr ULONG ATT_mapping				= 0x40000L; // Attachment used for mapping auth block
inline constexpr ULONG ATT_from_thread			= 0x80000L; // Attachment from internal special thread (sweep, crypt)
inline constexpr ULONG ATT_monitor_init			= 0x100000L; // Attachment is registered in monitoring
inline constexpr ULONG ATT_repl_reset			= 0x200000L; // Replication set has been reset
inline constexpr ULONG ATT_replicating			= 0x400000L; // Replication is active
inline constexpr ULONG ATT_resetting			= 0x800000L; // Session reset is in progress
inline constexpr ULONG ATT_worker				= 0x1000000L; // Worker attachment, managed by the engine
inline constexpr ULONG ATT_gbak_restore_has_schema = 0x2000000L;

inline constexpr ULONG ATT_NO_CLEANUP			= (ATT_no_cleanup | ATT_notify_gc);

class Attachment;
class DatabaseOptions;
struct bid;


class ActiveSnapshots
{
public:
	explicit ActiveSnapshots(MemoryPool& p);

	// Returns snapshot number given version belongs to.
	// It is not needed to maintain two versions for the same snapshot, so the latter
	// version can be garbage-collected.
	//
	// Function returns CN_ACTIVE if version was committed after we obtained
	// our list of snapshots. It means GC is not possible for this version.
	CommitNumber getSnapshotForVersion(CommitNumber version_cn);

private:
	SparseBitmap<CommitNumber> m_snapshots;		// List of active snapshots as of the moment of time
	CommitNumber m_lastCommit;		// CN_ACTIVE here means object is not populated
	ULONG m_releaseCount;			// Release event counter when list was last updated
	ULONG m_slots_used;				// Snapshot slots used when list was last updated

	friend class TipCache;
};


//
// RefCounted part of Attachment object, placed into permanent pool
//
class StableAttachmentPart : public RefCounted, public GlobalStorage
{
public:
	class Sync
	{
	public:
		Sync() noexcept
			: waiters(0), threadId(0), totalLocksCounter(0), currentLocksCounter(0)
		{ }

		// copying is prohibited
		Sync(const Sync&) = delete;
		Sync& operator=(const Sync&) = delete;

		void enter(const char* aReason)
		{
			const ThreadId curTid = getThreadId();

			if (threadId == curTid)
			{
				currentLocksCounter++;
				return;
			}

			if (threadId || !syncMutex.tryEnter(aReason))
			{
				// we have contention with another thread
				waiters.fetch_add(1, std::memory_order_relaxed);
				syncMutex.enter(aReason);
				waiters.fetch_sub(1, std::memory_order_relaxed);
			}

			threadId = curTid;
			totalLocksCounter++;
			fb_assert(currentLocksCounter == 0);
			currentLocksCounter++;
		}

		bool tryEnter(const char* aReason)
		{
			const ThreadId curTid = getThreadId();

			if (threadId == curTid)
			{
				currentLocksCounter++;
				return true;
			}

			if (threadId || !syncMutex.tryEnter(aReason))
				return false;

			threadId = curTid;
			totalLocksCounter++;
			fb_assert(currentLocksCounter == 0);
			currentLocksCounter++;
			return true;
		}

		void leave()
		{
			fb_assert(currentLocksCounter > 0);

			if (--currentLocksCounter == 0)
			{
				threadId = 0;
				syncMutex.leave();
			}
		}

		bool hasContention() const noexcept
		{
			return (waiters.load(std::memory_order_relaxed) > 0);
		}

		FB_UINT64 getLockCounter() const noexcept
		{
			return totalLocksCounter;
		}

		bool locked() const
		{
			return threadId == getThreadId();
		}

		~Sync()
		{
			if (threadId == getThreadId())
			{
				syncMutex.leave();
			}
		}

	private:
		Mutex syncMutex;
		std::atomic<int> waiters;
		ThreadId threadId;
		std::atomic<FB_UINT64> totalLocksCounter;
		int currentLocksCounter;
	};

	explicit StableAttachmentPart(Attachment* handle)
		: att(handle), jAtt(NULL), shutError(0)
	{ }

	Attachment* getHandle() noexcept
	{
		return att;
	}

	JAttachment* getInterface() noexcept
	{
		return jAtt;
	}

	void setInterface(JAttachment* ja)
	{
		if (jAtt)
			jAtt->detachEngine();

		jAtt = ja;
		shutError = 0;
	}

	Sync* getSync(bool useAsync = false, bool forceAsync = false)
	{
		if (useAsync && !forceAsync)
		{
			fb_assert(!mainSync.locked());
		}
		return useAsync ? &async : &mainSync;
	}

	Mutex* getBlockingMutex() noexcept
	{
		return &blockingMutex;
	}

	void cancel()
	{
		fb_assert(async.locked());
		fb_assert(mainSync.locked());
		att = NULL;
	}

	jrd_tra* getEngineTransaction(CheckStatusWrapper* status, ITransaction* tra)
	{
		return getInterface()->getEngineTransaction(status, tra);
	}

	JTransaction* getTransactionInterface(CheckStatusWrapper* status, ITransaction* tra)
	{
		return getInterface()->getTransactionInterface(status, tra);
	}

	void manualLock(ULONG& flags, const ULONG whatLock = ATT_manual_lock | ATT_async_manual_lock);
	void manualUnlock(ULONG& flags);
	void manualAsyncUnlock(ULONG& flags);

	void setShutError(ISC_STATUS code) noexcept
	{
		if (!shutError)
			shutError = code;
	}

	ISC_STATUS getShutError() const noexcept
	{
		return shutError;
	}

	void onIdleTimer(TimerImpl* timer)
	{
		doOnIdleTimer(timer);
	}

protected:
	virtual void doOnIdleTimer(TimerImpl* timer);

private:
	Attachment* att;
	JAttachment* jAtt;
	ISC_STATUS shutError;

	// These syncs guarantee attachment existence. After releasing both of them with possibly
	// zero att_use_count one should check does attachment still exists calling getHandle().
	Sync mainSync, async;
	// This mutex guarantees attachment is not accessed by more than single external thread.
	Mutex blockingMutex;
};

typedef RaiiLockGuard<StableAttachmentPart::Sync> AttSyncLockGuard;
typedef RaiiUnlockGuard<StableAttachmentPart::Sync> AttSyncUnlockGuard;

//
// the attachment block; one is created for each attachment to a database
//
class Attachment : public pool_alloc<type_att>
{
public:
	class SyncGuard
	{
	public:
		SyncGuard(StableAttachmentPart* js, const char* f, bool optional = false)
			: jStable(js)
		{
			init(f, optional);
		}

		SyncGuard(Attachment* att, const char* f, bool optional = false)
			: jStable(att ? att->getStable() : NULL)
		{
			init(f, optional);
		}

		~SyncGuard()
		{
			if (jStable)
				jStable->getSync()->leave();
		}

		// copying is prohibited
		SyncGuard(const SyncGuard&) = delete;
		SyncGuard& operator=(const SyncGuard&) = delete;

	private:
		void init(const char* f, bool optional);

		RefPtr<StableAttachmentPart> jStable;
	};

	class InitialOptions
	{
	public:
		InitialOptions(MemoryPool& pool)
			: bindings(pool),
			  schemaSearchPath(FB_NEW_POOL(pool) AnyRef<ObjectsArray<MetaString>>(pool)),
			  blrRequestSchemaSearchPath(
				FB_NEW_POOL(pool) AnyRef<ObjectsArray<MetaString>>(pool))
		{
		}

	public:
		void setInitialOptions(thread_db* tdbb, const DatabaseOptions& options);
		void resetAttachment(Attachment* attachment) const;

		CoercionArray *getBindings() noexcept
		{
			return &bindings;
		}

		const CoercionArray *getBindings() const noexcept
		{
			return &bindings;
		}

	private:
		DecimalStatus decFloatStatus = DecimalStatus::DEFAULT;
		CoercionArray bindings;
		RefPtr<AnyRef<ObjectsArray<MetaString>>> schemaSearchPath;
		RefPtr<AnyRef<ObjectsArray<MetaString>>> blrRequestSchemaSearchPath;
		USHORT originalTimeZone = TimeZoneUtil::GMT_ZONE;
	};

	class DebugOptions
	{
	public:
		bool getDsqlKeepBlr() const noexcept
		{
			return dsqlKeepBlr;
		}

		void setDsqlKeepBlr(bool value) noexcept
		{
			dsqlKeepBlr = value;
		}

	private:
		bool dsqlKeepBlr = false;
	};

	class UseCountHolder
	{
	public:
		explicit UseCountHolder(Attachment* a) noexcept
			: att(a)
		{
			if (att)
				att->att_use_count++;
		}

		~UseCountHolder()
		{
			if (att)
				att->att_use_count--;
		}

	private:
		Attachment* att;
	};

public:
	static Attachment* create(Database* dbb, JProvider* provider);
	static void destroy(Attachment* const attachment);

	MemoryPool* const att_pool;					// Memory pool
	MemoryStats att_memory_stats;

	Database*	att_database;				// Parent database block
	Attachment*	att_next;					// Next attachment to database
	UserId*		att_user;					// User identification
	UserId*		att_ss_user;				// User identification for SQL SECURITY actual user
	jrd_tra*	att_transactions;			// Transactions belonging to attachment
	jrd_tra*	att_dbkey_trans;			// transaction to control db-key scope
	TraNumber	att_oldest_snapshot;		// GTT's record versions older than this can be garbage-collected
	ActiveSnapshots att_active_snapshots;	// List of currently active snapshots for GC purposes

private:
	jrd_tra*	att_sys_transaction;		// system transaction
	jrd_tra*	att_meta_transaction;		// metadata read transaction
	StableAttachmentPart* att_stable;

public:
	SortedArray<Request*> att_requests;		// Requests belonging to attachment

	Lock*		att_id_lock;				// Attachment lock (if any)
	AttNumber	att_attachment_id;			// Attachment ID
	Lock*		att_cancel_lock;			// Lock to cancel the active request
	Lock*		att_monitor_lock;			// Lock for monitoring purposes
	ULONG		att_monitor_generation;		// Monitoring state generation
	Lock*		att_profiler_listener_lock;	// Lock for remote profiler listener
	const ULONG	att_lock_owner_id;			// ID for the lock manager
	SLONG		att_lock_owner_handle;		// Handle for the lock manager
	ULONG		att_backup_state_counter;	// Counter of backup state locks for attachment
	SLONG		att_event_session;			// Event session id, if any
	SecurityClass*	att_security_class;		// security class for database
	SecurityClassList*	att_security_classes;	// security classes
	RuntimeStatistics	att_stats;
	RuntimeStatistics	att_base_stats;
	ULONG		att_flags;					// Flags describing the state of the attachment
	CSetId		att_client_charset;			// user's charset specified in dpb
	CSetId		att_charset;				// current (client or external) attachment charset

	// ASF: Attention: att_in_system_routine was initially added to support the profiler plugin
	// writing to system tables. But a modified implementation used non-system tables and
	// a problem was discovered that when writing to user's table from a "system context"
	// (csb_internal) FK validations are not enforced becase MET_scan_relation is not called
	// for the relation.
	// Currently all "turning on" code for att_in_system_routine are disabled in SystemPackages.h.
	bool 		att_in_system_routine = false;	// running a system routine

	Lock*		att_long_locks;				// outstanding two phased locks
#ifdef DEBUG_LCK_LIST
	UCHAR		att_long_locks_type;		// Lock type of the first lock in list
#endif
	std::atomic<SLONG>	att_wait_owner_handle;	// lock owner with which attachment waits currently
	vec<Lock*>*	att_compatibility_table;	// hash table of compatible locks
	Validation*	att_validation;
	PathName	att_working_directory;	// Current working directory is cached
	PathName	att_filename;			// alias used to attach the database
	ISC_TIMESTAMP_TZ	att_timestamp;	    // Connection date and time
	StringMap att_context_vars;	// Context variables for the connection
	Stack<DdlTriggerContext*> ddlTriggersContext;	// Context variables for DDL trigger event
	string att_network_protocol;	// Network protocol used by client for connection
	PathName att_remote_crypt;	// Name of wire crypt plugin (if any)
	string att_remote_address;	// Protocol-specific address of remote client
	SLONG att_remote_pid;					// Process id of remote client
	ULONG att_remote_flags;					// Flags specific for server/client link
	PathName att_remote_process;	// Process name of remote client
	string att_client_version;	// Version of the client library
	string att_remote_protocol;	// Details about the remote protocol
	string att_remote_host;		// Host name of remote client
	string att_remote_os_user;	// OS user name of remote client
	RandomGenerator att_random_generator;	// Random bytes generator
	Lock*		att_temp_pg_lock;			// temporary pagespace ID lock
	DSqlCache att_dsql_cache;	// DSQL cache locks
	SortedArray<void*> att_udf_pointers;
	dsql_dbb* att_dsql_instance;
	bool att_in_use;						// attachment in use (can't be detached or dropped)
	int att_use_count;						// number of API calls running except of asynchronous ones
	ThreadId att_purge_tid;					// ID of thread running purge_attachment()

	EDS::Connection* att_ext_connection;	// external connection executed by this attachment
	EDS::Connection* att_ext_parent;		// external connection, parent of this attachment
	ULONG att_ext_call_depth;				// external connection call depth, 0 for user attachment
	TraceManager* att_trace_manager;		// Trace API manager

	CoercionArray att_bindings;
	CoercionArray* att_dest_bind;
	USHORT att_original_timezone;
	USHORT att_current_timezone;
	RefPtr<AnyRef<ObjectsArray<MetaString>>> att_schema_search_path;
	RefPtr<AnyRef<ObjectsArray<MetaString>>> att_blr_request_schema_search_path;
	RefPtr<AnyRef<ObjectsArray<MetaString>>> att_system_schema_search_path;

	RefPtr<AnyRef<ObjectsArray<MetaString>>>
		att_unqualified_charset_resolved_cache_search_path;
	NonPooledMap<MetaName, QualifiedName> att_unqualified_charset_resolved_cache;

	int att_parallel_workers;
	TriState att_opt_first_rows;

	PageToBufferMap* att_bdb_cache;			// managed in CCH, created in att_pool, freed with it

	LeftPooledMap<QualifiedName, LocalTemporaryTable*> att_local_temporary_tables;
	std::optional<USHORT> att_next_ltt_id;		// Next available LTT relation ID

	RefPtr<IReplicatedSession> att_replicator;
	AutoPtr<Replication::TableMatcher> att_repl_matcher;
	Array<Replication::Applier*> att_repl_appliers;

	enum UtilType { UTIL_NONE, UTIL_GBAK, UTIL_GFIX, UTIL_GSTAT };

	UtilType att_utility;

	ICryptKeyCallback*	att_crypt_callback;		// callback for DB crypt
	DecimalStatus			att_dec_status;			// error handling and rounding

	void initLocks(thread_db* tdbb);
	void releaseLocks(thread_db* tdbb);
	void detachLocks();

	static int blockingAstShutdown(void*);
	static int blockingAstCancel(void*);
	static int blockingAstMonitor(void*);
	static int blockingAstReplSet(void*);

	bool locksmith(thread_db* tdbb, SystemPrivilege sp) const;

	jrd_tra* getSysTransaction() noexcept;
	void setSysTransaction(jrd_tra* trans) noexcept;		// used only by TRA_init
	jrd_tra* getMetaTransaction(thread_db* tdbb);			// RORC to read metadata
	void createMetaTransaction(thread_db* tdbb);
	void rollbackMetaTransaction(thread_db* tdbb);

	inline bool isSystem() const noexcept
	{
		return (att_flags & ATT_system);
	}

	inline bool isWorker() const noexcept
	{
		return (att_flags & ATT_worker);
	}

	bool isGbak() const noexcept;
	bool isRWGbak() const noexcept;
	bool isUtility() const noexcept; // gbak, gfix and gstat.

	PreparedStatement* prepareStatement(thread_db* tdbb, jrd_tra* transaction,
		const string& text, MemoryPool* pool = NULL);
	PreparedStatement* prepareStatement(thread_db* tdbb, jrd_tra* transaction,
		const PreparedStatement::Builder& builder, MemoryPool* pool = NULL);

	PreparedStatement* prepareUserStatement(thread_db* tdbb, jrd_tra* transaction,
		const string& text, MemoryPool* pool = NULL);

	MetaName nameToMetaCharSet(thread_db* tdbb, const MetaName& name);
	MetaName nameToUserCharSet(thread_db* tdbb, const MetaName& name);
	string stringToUserCharSet(thread_db* tdbb, const string& str);

	void storeMetaDataBlob(thread_db* tdbb, jrd_tra* transaction,
		bid* blobId, const string& text, USHORT fromCharSet = CS_METADATA);
	void storeBinaryBlob(thread_db* tdbb, jrd_tra* transaction, bid* blobId,
		const ByteChunk& chunk);

	void releaseBatches();
	void releaseLocalTempTables(thread_db* tdbb);
	void resetSession(thread_db* tdbb, jrd_tra** traHandle);

	void signalCancel();
	void signalShutdown(ISC_STATUS code);

	void mergeStats(bool pageStatsOnly = false);
	bool hasActiveRequests() const noexcept;

	bool backupStateWriteLock(thread_db* tdbb, SSHORT wait);
	void backupStateWriteUnLock(thread_db* tdbb);
	bool backupStateReadLock(thread_db* tdbb, SSHORT wait);
	void backupStateReadUnLock(thread_db* tdbb);

	StableAttachmentPart* getStable() noexcept
	{
		return att_stable;
	}

	void setStable(StableAttachmentPart *js) noexcept
	{
		att_stable = js;
	}

	JAttachment* getInterface() noexcept;

	unsigned int getIdleTimeout() const noexcept
	{
		return att_idle_timeout;
	}

	void setIdleTimeout(unsigned int timeOut) noexcept
	{
		att_idle_timeout = timeOut;
	}

	unsigned int getActualIdleTimeout() const;

	unsigned int getStatementTimeout() const noexcept
	{
		return att_stmt_timeout;
	}

	void setStatementTimeout(unsigned int timeOut) noexcept
	{
		att_stmt_timeout = timeOut;
	}

	// evaluate new value or clear idle timer
	void setupIdleTimer(bool clear);

	// returns time when idle timer will be expired, if set
	bool getIdleTimerClock(SINT64& clock) const
	{
		if (!att_idle_timer)
			return false;

		clock = att_idle_timer->getExpireClock();
		return (clock != 0);
	}

	// batches control
	void registerBatch(DsqlBatch* b)
	{
		att_batches.add(b);
	}

	void deregisterBatch(DsqlBatch* b)
	{
		att_batches.findAndRemove(b);
	}

	UserId* getUserId(const MetaString& userName);

	const MetaString& getUserName(const MetaString& emptyName = "")
	{
		saveMetaString(!att_user, att_retUser, emptyName);
		return att_user ? att_user->getUserName() : *att_retUser;
	}

	const MetaString& getSqlRole(const MetaString& emptyName = "")
	{
		saveMetaString(!att_user, att_retRole, emptyName);
		return att_user ? att_user->getSqlRole() : *att_retRole;
	}

	const UserId* getEffectiveUserId() const noexcept
	{
		if (att_ss_user)
			return att_ss_user;
		return att_user;
	}

	const MetaString& getEffectiveUserName(const MetaString& emptyName = "") const
	{
		const auto user = getEffectiveUserId();
		return user ? user->getUserName() : emptyName;
	}

	void setInitialOptions(thread_db* tdbb, const DatabaseOptions& options, bool newDb);

	const CoercionArray* getInitialBindings() const noexcept
	{
		return att_initial_options.getBindings();
	}

	DebugOptions& getDebugOptions() noexcept
	{
		return att_debug_options;
	}

	void checkReplSetLock(thread_db* tdbb);
	void invalidateReplSet(thread_db* tdbb, bool broadcast);

	ProfilerManager* getProfilerManager(thread_db* tdbb);
	ProfilerManager* getActiveProfilerManagerForNonInternalStatement(thread_db* tdbb);
	bool isProfilerActive();
	void releaseProfilerManager(thread_db* tdbb);
	void purgeTransactions(thread_db* tdbb, const bool force_flag);

	JProvider* getProvider() noexcept
	{
		fb_assert(att_provider);
		return att_provider;
	}

	bool qualifyNewName(thread_db* tdbb, QualifiedName& name,
		const ObjectsArray<MetaString>* schemaSearchPath = nullptr);

	void qualifyExistingName(thread_db* tdbb, QualifiedName& name,
		std::initializer_list<ObjectType> objTypes,
		const ObjectsArray<MetaString>* schemaSearchPath = nullptr);

private:
	Attachment(MemoryPool* pool, Database* dbb, JProvider* provider);
	~Attachment();

	unsigned int att_idle_timeout;		// seconds
	unsigned int att_stmt_timeout;		// milliseconds
	RefPtr<TimerImpl> att_idle_timer;

	Array<DsqlBatch*> att_batches;
	InitialOptions att_initial_options;	// Initial session options
	DebugOptions att_debug_options;
	AutoPtr<ProfilerManager> att_profiler_manager;	// ProfilerManager

	Lock* att_repl_lock;				// Replication set lock
	JProvider* att_provider;	// Provider which created this attachment

	AutoPtr<MetaString> att_retUser, att_retRole;
	void saveMetaString(bool cond, AutoPtr<MetaString>& meta, const MetaString& src)
	{
		if (cond)
		{
			if (!meta)
				meta = FB_NEW_POOL(*att_pool) MetaString(*att_pool);
			meta->operator=(src);
		}
	}
};


inline bool Attachment::locksmith(thread_db* tdbb, SystemPrivilege sp) const
{
	const auto user = getEffectiveUserId();
	return (user && user->locksmith(tdbb, sp));
}

inline jrd_tra* Attachment::getSysTransaction() noexcept
{
	return att_sys_transaction;
}

inline void Attachment::setSysTransaction(jrd_tra* trans) noexcept
{
	att_sys_transaction = trans;
}

// Connection is from GBAK
inline bool Attachment::isGbak() const noexcept
{
	return (att_utility == UTIL_GBAK);
}

// Gbak changes objects when it's restoring (creating) a db.
// Other attempts are fake. Gbak reconnects to change R/O status and other db-wide settings,
// but it doesn't modify generators or tables that seconds time.
inline bool Attachment::isRWGbak() const noexcept
{
	return (isGbak() && (att_flags & ATT_creator));
}

// Any of the three original utilities: gbak, gfix or gstat.
inline bool Attachment::isUtility() const noexcept
{
	return (att_utility != UTIL_NONE);
}

// This class holds references to all attachments it contains

class AttachmentsRefHolder
{
	friend class Iterator;

public:
	class Iterator
	{
	public:
		explicit Iterator(AttachmentsRefHolder& list) noexcept
			: m_list(list), m_index(0)
		{}

		// copying is prohibited
		Iterator(const Iterator&) = delete;
		Iterator& operator=(const Iterator&) = delete;

		StableAttachmentPart* operator*() noexcept
		{
			if (m_index < m_list.m_attachments.getCount())
				return m_list.m_attachments[m_index];

			return NULL;
		}

		void operator++() noexcept
		{
			m_index++;
		}

		void remove()
		{
			if (m_index < m_list.m_attachments.getCount())
			{
				m_list.m_attachments[m_index]->release();
				m_list.m_attachments.remove(m_index);
			}
		}

	private:
		AttachmentsRefHolder& m_list;
		FB_SIZE_T m_index;
	};

	explicit AttachmentsRefHolder(MemoryPool& p)
		: m_attachments(p)
	{}

	AttachmentsRefHolder()
		: m_attachments(*MemoryPool::getContextPool())
	{}

	AttachmentsRefHolder& operator=(const AttachmentsRefHolder& other)
	{
		clear();

		for (FB_SIZE_T i = 0; i < other.m_attachments.getCount(); i++)
			add(other.m_attachments[i]);

		return *this;
	}

	void clear()
	{
		while (m_attachments.hasData())
		{
			m_attachments.pop()->release();
		}
	}

	~AttachmentsRefHolder()
	{
		clear();
	}

	void add(StableAttachmentPart* jAtt)
	{
		if (jAtt)
		{
			jAtt->addRef();
			m_attachments.add(jAtt);
		}
	}

	bool hasData() const noexcept
	{
		return m_attachments.hasData();
	}

private:
	AttachmentsRefHolder(const AttachmentsRefHolder&);

	HalfStaticArray<StableAttachmentPart*, 128> m_attachments;
};

// Class used in system background threads

class SysStableAttachment : public StableAttachmentPart
{
public:
	explicit SysStableAttachment(Attachment* handle);

	void initDone();

	virtual ~SysStableAttachment()
	{
		Attachment* attachment = getHandle();
		if (attachment)
		{
			destroy(attachment);
		}
	}

protected:
	void destroy(Attachment* attachment);

	// "public" interface for internal (system) attachment
	RefPtr<JAttachment> m_JAttachment;
};

} // namespace Firebird::Jrd

#endif // JRD_ATTACHMENT_H
