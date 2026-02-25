/*
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
 *
 */

#ifndef JRD_RELATION_H
#define JRD_RELATION_H

#include "../jrd/vec.h"
#include <optional>
#include "../jrd/btr.h"
#include "../jrd/lck.h"
#include "../jrd/pag.h"
#include "../jrd/val.h"
#include "../jrd/Attachment.h"
#include "../jrd/CacheVector.h"
#include "../jrd/ExtEngineManager.h"
#include "../jrd/met_proto.h"
#include "../jrd/Resources.h"
#include "../common/classes/TriState.h"
#include "../common/sha2/sha2.h"

namespace Jrd
{

template <typename T> class vec;
class BoolExprNode;
class RseNode;
class StmtNode;
class jrd_fld;
class ExternalFile;
class RelationPermanent;
class jrd_rel;
class Record;

// trigger types
inline constexpr int TRIGGER_PRE_STORE		= 1;
inline constexpr int TRIGGER_POST_STORE		= 2;
inline constexpr int TRIGGER_PRE_MODIFY		= 3;
inline constexpr int TRIGGER_POST_MODIFY	= 4;
inline constexpr int TRIGGER_PRE_ERASE		= 5;
inline constexpr int TRIGGER_POST_ERASE		= 6;
inline constexpr int TRIGGER_MAX			= 7;

// trigger type prefixes
inline constexpr int TRIGGER_PRE			= 0;
inline constexpr int TRIGGER_POST			= 1;

// trigger type suffixes
inline constexpr int TRIGGER_STORE			= 1;
inline constexpr int TRIGGER_MODIFY			= 2;
inline constexpr int TRIGGER_ERASE			= 3;

// that's how trigger action types are encoded
/*
	bit 0 = TRIGGER_PRE/TRIGGER_POST flag,
	bits 1-2 = TRIGGER_STORE/TRIGGER_MODIFY/TRIGGER_ERASE (slot #1),
	bits 3-4 = TRIGGER_STORE/TRIGGER_MODIFY/TRIGGER_ERASE (slot #2),
	bits 5-6 = TRIGGER_STORE/TRIGGER_MODIFY/TRIGGER_ERASE (slot #3),
	and finally the above calculated value is decremented

example #1:
	TRIGGER_POST_ERASE =
	= ((TRIGGER_ERASE << 1) | TRIGGER_POST) - 1 =
	= ((3 << 1) | 1) - 1 =
	= 0x00000110 (6)

example #2:
	TRIGGER_PRE_STORE_MODIFY =
	= ((TRIGGER_MODIFY << 3) | (TRIGGER_STORE << 1) | TRIGGER_PRE) - 1 =
	= ((2 << 3) | (1 << 1) | 0) - 1 =
	= 0x00010001 (17)

example #3:
	TRIGGER_POST_MODIFY_ERASE_STORE =
	= ((TRIGGER_STORE << 5) | (TRIGGER_ERASE << 3) | (TRIGGER_MODIFY << 1) | TRIGGER_POST) - 1 =
	= ((1 << 5) | (3 << 3) | (2 << 1) | 1) - 1 =
	= 0x00111100 (60)
*/

// that's how trigger types are decoded
#define TRIGGER_ACTION(value, shift) \
	(((((value + 1) >> shift) & 3) << 1) | ((value + 1) & 1)) - 1

#define TRIGGER_ACTION_SLOT(value, slot) \
	TRIGGER_ACTION(value, (slot * 2 - 1) )

inline constexpr int TRIGGER_COMBINED_MAX	= 128;



// Relation trigger definition

class Trigger
{
public:
	Firebird::HalfStaticArray<UCHAR, 128> blr;			// BLR code
	Firebird::HalfStaticArray<UCHAR, 128> debugInfo;	// Debug info
	Statement* statement = nullptr;						// Compiled statement
	bool releaseInProgress = false;
	fb_sysflag sysTrigger = fb_sysflag_user;			// See fb_sysflag in constants.h
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

	MemoryPool& getPool();

	bool isActive() const;

	void compile(thread_db*);				// Ensure that trigger is compiled
	void free(thread_db*);					// Free trigger request

	explicit Trigger(MemoryPool& p)
		: blr(p), debugInfo(p), entryPoint(p), extBody(p)
	{}

	const QualifiedName& getName() const noexcept
	{
		return name;
	}
};

// Set of triggers (use separate arrays for triggers of different types)
class Triggers
{
public:
	explicit Triggers(MemoryPool& p)
		: triggers(p)
	{ }

	bool hasActive() const;
	void decompile(thread_db* tdbb);

	void addTrigger(thread_db*, Trigger* trigger)
	{
		triggers.add(trigger);
	}

	Trigger* const* begin() const
	{
		return triggers.begin();
	}

	Trigger* const* end() const
	{
		return triggers.end();
	}

	bool operator!() const
	{
		return !hasData();
	}

	operator bool() const
	{
		return hasData();
	}

	bool hasData() const
	{
		return triggers.hasData();
	}

	void release(thread_db* tdbb, bool destroy);

	static void destroy(thread_db* tdbb, Triggers* trigs);

private:
	Firebird::HalfStaticArray<Trigger*, 8> triggers;
};

class DbTriggersHeader : public Firebird::PermanentStorage
{
public:
	DbTriggersHeader(thread_db*, MemoryPool& p, MetaId& t, NoData = NoData());

	MetaId getId() const noexcept
	{
		return type;
	}

	const QualifiedName& getName() const noexcept;

	static bool destroy(thread_db* tdbb, DbTriggersHeader* trigs);
	void releaseLock(thread_db*) { }

private:
	MetaId type;
};

class DbTriggers final : public Triggers, public ObjectBase
{
public:
	DbTriggers(DbTriggersHeader* hdr)
		: Triggers(hdr->getPool()),
		  ObjectBase(),
		  perm(hdr)
	{ }

	static DbTriggers* create(thread_db*, MemoryPool&, DbTriggersHeader* hdr)
	{
		return FB_NEW_POOL(hdr->getPool()) DbTriggers(hdr);
	}

	static void destroy(thread_db* tdbb, DbTriggers* trigs)
	{
		Triggers::destroy(tdbb, trigs);
		delete trigs;
	}

	static const enum lck_t LOCKTYPE = LCK_dbwide_triggers;
	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);

	ScanResult reload(thread_db* tdbb, ObjectBase::Flag flags)
	{
		return scan(tdbb, flags);
	}

	bool hash(thread_db*, Firebird::sha512&)
	{
		return true;
	}

	static const char* objectFamily(void*)
	{
		return "set of database-wide triggers on";
	}

	static int objectType();

private:
	DbTriggersHeader* perm;

public:
	decltype(perm) getPermanent() const noexcept
	{
		return perm;
	}
};

class TrigArray
{
public:
	TrigArray(MemoryPool& p);
	Triggers& operator[](int t);
	const Triggers& operator[](int t) const;

private:
	Triggers preErase, postErase, preModify, postModify, preStore, postStore;
};


// view context block to cache view aliases

class ViewContext
{
public:
	explicit ViewContext(MemoryPool& p, const TEXT* context_name,
						 const QualifiedName& relation_name, USHORT context,
						 ViewContextType type)
	: vcx_context_name(p, context_name, fb_strlen(context_name)),
	  vcx_relation_name(relation_name),
	  vcx_context(context),
	  vcx_type(type)
	{
	}

	static USHORT generate(const ViewContext* vc) noexcept
	{
		return vc->vcx_context;
	}

	const Firebird::string vcx_context_name;
	const QualifiedName vcx_relation_name;
	const USHORT vcx_context;
	const ViewContextType vcx_type;
};

typedef Firebird::SortedArray<ViewContext*, Firebird::EmptyStorage<ViewContext*>,
		USHORT, ViewContext> ViewContexts;


class RelationPages
{
public:
	typedef FB_UINT64 InstanceId;

	// Vlad asked for this compile-time check to make sure we can contain a txn/att number here
	static_assert(sizeof(InstanceId) >= sizeof(TraNumber), "InstanceId must fit TraNumber");
	static_assert(sizeof(InstanceId) >= sizeof(AttNumber), "InstanceId must fit AttNumber");

	vcl* rel_pages;					// vector of pointer page numbers
	InstanceId rel_instance_id;		// 0 or att_attachment_id or tra_number

	ULONG rel_index_root;		// index root page number
	ULONG rel_data_pages;		// count of relation data pages
	ULONG rel_slot_space;		// lowest pointer page with slot space
	ULONG rel_pri_data_space;	// lowest pointer page with primary data page space
	ULONG rel_sec_data_space;	// lowest pointer page with secondary data page space
	ULONG rel_last_free_pri_dp;	// last primary data page found with space
	ULONG rel_last_free_blb_dp;	// last blob data page found with space
	USHORT rel_pg_space_id;

	RelationPages(Firebird::MemoryPool& pool)
		: rel_pages(NULL), rel_instance_id(0),
		  rel_index_root(0), rel_data_pages(0), rel_slot_space(0),
		  rel_pri_data_space(0), rel_sec_data_space(0),
		  rel_last_free_pri_dp(0), rel_last_free_blb_dp(0),
		  rel_pg_space_id(DB_PAGE_SPACE), rel_next_free(NULL),
		  dpMap(pool),
		  dpMapMark(0)
	{}

	inline SLONG addRef() noexcept
	{
		return useCount++;
	}

	void free(RelationPages*& nextFree);

	static inline InstanceId generate(const RelationPages* item) noexcept
	{
		return item->rel_instance_id;
	}

	ULONG getDPNumber(ULONG dpSequence)
	{
		Firebird::MutexLockGuard g(dpMutex, FB_FUNCTION);

		FB_SIZE_T pos;
		if (dpMap.find(dpSequence, pos))
		{
			if (dpMap[pos].mark != dpMapMark)
				dpMap[pos].mark = ++dpMapMark;
			return dpMap[pos].physNum;
		}

		return 0;
	}

	void setDPNumber(ULONG dpSequence, ULONG dpNumber)
	{
		Firebird::MutexLockGuard g(dpMutex, FB_FUNCTION);

		FB_SIZE_T pos;
		if (dpMap.find(dpSequence, pos))
		{
			if (dpNumber)
			{
				dpMap[pos].physNum = dpNumber;
				dpMap[pos].mark = ++dpMapMark;
			}
			else
				dpMap.remove(pos);
		}
		else if (dpNumber)
		{
			dpMap.insert(pos, {dpSequence, dpNumber, ++dpMapMark});

			if (dpMap.getCount() == MAX_DPMAP_ITEMS)
				freeOldestMapItems();
		}
	}

	void freeOldestMapItems() noexcept
	{
		Firebird::MutexLockGuard g(dpMutex, FB_FUNCTION);

		ULONG minMark = MAX_ULONG;
		FB_SIZE_T i;

		for (i = 0; i < dpMap.getCount(); i++)
		{
			if (minMark > dpMap[i].mark)
				minMark = dpMap[i].mark;
		}

		minMark = (minMark + dpMapMark) / 2;

		i = 0;
		while (i < dpMap.getCount())
		{
			if (dpMap[i].mark > minMark)
				dpMap[i++].mark -= minMark;
			else
				dpMap.remove(i);
		}

		dpMapMark -= minMark;
	}

private:
	RelationPages*		rel_next_free;
	std::atomic<SLONG>	useCount = 0;

	static constexpr ULONG MAX_DPMAP_ITEMS = 64;

	struct DPItem
	{
		ULONG seqNum;
		ULONG physNum;
		ULONG mark;

		static ULONG generate(const DPItem& item) noexcept
		{
			return item.seqNum;
		}
	};

	Firebird::SortedArray<DPItem, Firebird::InlineStorage<DPItem, MAX_DPMAP_ITEMS>, ULONG, DPItem> dpMap;
	ULONG				dpMapMark;
	Firebird::Mutex		dpMutex;

friend class RelationPermanent;
};


// Index status

enum IndexStatus
{
	MET_index_active = 0,
	MET_index_inactive = 1,
	MET_index_deferred_active = 3,
	MET_index_deferred_drop = 4,
	MET_index_state_unknown = 999
};


// Index block

class IndexPermanent : public Firebird::PermanentStorage
{
public:
	IndexPermanent(thread_db* tdbb, MemoryPool& p, MetaId id, RelationPermanent* rel)
		: PermanentStorage(p),
		  idp_relation(rel),
		  idp_id(id)
	{ }

	~IndexPermanent()
	{ }

	static bool destroy(thread_db* tdbb, IndexPermanent* idp)
	{
		return false;
	}

	MetaId getId() const noexcept
	{
		return idp_id;
	}

	void releaseLock(thread_db*) { }

	RelationPermanent* getRelation()
	{
		return idp_relation;
	}

	static FB_UINT64 makeLockId(MetaId relId, MetaId indexId);
	const QualifiedName& getName();

private:
	RelationPermanent*	idp_relation;
	MetaId				idp_id;

	[[noreturn]] void errIndexGone();
};


class IndexVersion final : public ObjectBase
{
public:
	IndexVersion(MemoryPool& p, Cached::Index* idp);

	static IndexVersion* create(thread_db* tdbb, MemoryPool& p, Cached::Index* idp)
	{
		return FB_NEW_POOL(p) IndexVersion(p, idp);
	}
	static void destroy(thread_db* tdbb, IndexVersion* idv);

	ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
	ScanResult reload(thread_db* tdbb, ObjectBase::Flag flags)
	{
		return scan(tdbb, flags);
	}

	const QualifiedName& getName() const noexcept
	{
		return idv_name;
	}

	static const char* objectFamily(void*)
	{
		return "index";
	}

	QualifiedName getForeignKey() const
	{
		return idv_foreignKey;
	}

	MetaId getId() const noexcept
	{
		return perm->getId();
	}

	Cached::Index* getPermanent() const
	{
		return perm;
	}

	IndexStatus getActive()
	{
		return idv_active;
	}

	bool hash(thread_db*, Firebird::sha512& digest)
	{
		digest.process(sizeof(QualifiedName), &idv_foreignKey);
		digest.process(sizeof(idv_active), &idv_active);
		// to be done - take segments, expression & condition into an account
		return true;
	}

	void setLtt(thread_db* tdbb, LocalTemporaryTable::Index* ltt);

	static const enum lck_t LOCKTYPE = LCK_idx_rescan;

private:
	Cached::Index* perm;
	QualifiedName idv_name;
	SSHORT idv_uniqFlag = 0;
	SSHORT idv_segmentCount = 0;
	SSHORT idv_type = 0;
	QualifiedName idv_foreignKey;					// FOREIGN RELATION NAME
	IndexStatus idv_active = MET_index_state_unknown;

public:
	ValueExprNode* idv_expression = nullptr;		// node tree for index expression
	Statement* idv_expression_statement = nullptr;	// statement for index expression evaluation
	dsc			idv_expression_desc;				// descriptor for expression result
	BoolExprNode* idv_condition = nullptr;			// node tree for index condition
	Statement* idv_condition_statement = nullptr;	// statement for index condition evaluation
};


// Relation block; one is created for each relation referenced
// in the database, though it is not really filled out until
// the relation is scanned

class jrd_rel final : public ObjectBase
{
	jrd_rel(const jrd_rel&) = delete;
	jrd_rel(const jrd_rel&&) = delete;

public:
	jrd_rel(MemoryPool& p, Cached::Relation* r);

	MemoryPool*			rel_pool;

private:
	Cached::Relation*	rel_perm;

public:
	USHORT				rel_current_fmt;	// Current format number
	Format*				rel_current_format;	// Current record format
	USHORT				rel_dbkey_length;	// RDB$DBKEY length

	vec<jrd_fld*>*		rel_fields;			// vector of field blocks
	RseNode*			rel_view_rse;		// view record select expression
	ViewContexts		rel_view_contexts;	// sorted array of view contexts

	TrigArray			rel_triggers;

	Firebird::TriState	rel_ss_definer;
	Firebird::TriState	rel_repl_state;		// replication state

	bool hasData() const;
	MetaId getId() const noexcept;
	RelationPages* getPages(thread_db* tdbb, TraNumber tran = MAX_TRA_NUMBER, bool allocPages = true);
	bool isSystem() const noexcept;
	bool isTemporary() const noexcept;
	bool isLTT() const noexcept;
	bool isVirtual() const noexcept;
	bool isView() const noexcept;
	bool isReplicating(thread_db* tdbb);

	ObjectType getObjectType() const noexcept
	{
		return isView() ? obj_view : obj_relation;
	}

	const QualifiedName& getName() const noexcept;
	MemoryPool& getPool() const noexcept;
	const QualifiedName& getSecurityName() const noexcept;
	MetaName getOwnerName() const noexcept;
	ExternalFile* getExtFile() const noexcept;

	static void destroy(thread_db* tdbb, jrd_rel *rel);
	static jrd_rel* create(thread_db* tdbb, MemoryPool& p, Cached::Relation* perm);

	static const enum lck_t LOCKTYPE = LCK_rel_rescan;

	ScanResult scan(thread_db* tdbb, ObjectBase::Flag& flags);		// Scan the newly loaded relation for meta data
	static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
	ScanResult reload(thread_db* tdbb, ObjectBase::Flag& flags)
	{
		return scan(tdbb, flags);
	}

	bool hash(thread_db* tdbb, Firebird::sha512& digest);

	static const char* objectFamily(RelationPermanent* perm);
	static int objectType();

	void releaseTriggers(thread_db* tdbb, bool destroy);
	const Trigger* findTrigger(const QualifiedName& trig_name) const;
	const Format* currentFormat(thread_db* tdbb);

	decltype(rel_perm) getPermanent() const
	{
		return rel_perm;
	}

	Record* getGCRecord(thread_db* tdbb);
};

// rel_flags

inline constexpr ULONG REL_system				= 0x0001;
inline constexpr ULONG REL_get_dependencies		= 0x0002;	// New relation needs dependencies during scan
inline constexpr ULONG REL_sql_relation			= 0x0004;	// Relation defined as sql table
inline constexpr ULONG REL_check_partners		= 0x0008;	// Rescan primary dependencies and foreign references
inline constexpr ULONG REL_temp_tran			= 0x0010;	// relation is a GTT delete rows
inline constexpr ULONG REL_temp_conn			= 0x0020;	// relation is a GTT preserve rows
inline constexpr ULONG REL_virtual				= 0x0040;	// relation is virtual
inline constexpr ULONG REL_jrd_view				= 0x0080;	// relation is VIEW
inline constexpr ULONG REL_temp_gtt				= 0x0100;	// relation is a GTT
inline constexpr ULONG REL_temp_ltt				= 0x0200;	// relation is a LTT

class GCLock
{
public:
	GCLock(RelationPermanent* rl)
		: gcRel(rl)
	{ }

	// This guard is used by regular code to prevent online validation while
	// dead- or back- versions is removed from disk.
	class Shared
	{
	public:
		Shared(thread_db* tdbb, RelationPermanent* rl);
		~Shared();

		bool gcEnabled() const
		{
			return m_gcEnabled;
		}

	private:
		thread_db*	m_tdbb;
		RelationPermanent*	m_rl;
		bool		m_gcEnabled;
	};

	// This guard is used by online validation to prevent any modifications of
	// table data while it is checked.
	class Exclusive
	{
	public:
		Exclusive(thread_db* tdbb, RelationPermanent* rl)
			: m_tdbb(tdbb), m_rl(rl), m_lock(nullptr)
		{ }

		~Exclusive()
		{
			release();
			delete m_lock;
		}

		bool acquire(int wait);
		void release();

	private:
		thread_db*		m_tdbb;
		RelationPermanent*		m_rl;
		Lock*			m_lock;
	};

	friend Shared;
	friend Exclusive;

private:
	bool acquire(thread_db* tdbb, int wait);
	void downgrade(thread_db* tdbb);
	void enable(thread_db* tdbb, Lock* tempLock);
	bool disable(thread_db* tdbb, int wait, Lock*& tempLock);

public:
	bool checkDisabled() const
	{
		return gcFlags.load(std::memory_order_acquire) & GC_disabled;
	}

	unsigned getSweepCount() const;		// violates rules of atomic counters
										// but OK for zerocheck when count can not grow
#ifdef DEV_BUILD
	enum class State {unknown, enabled, disabled};
	State isGCEnabled() const;			// violates rules of atomic counters
										// but OK for assertions
#endif //DEV_BUILD

	static int ast(void* self)
	{
		try
		{
			reinterpret_cast<GCLock*>(self)->blockingAst();
		}
		catch(const Firebird::Exception&) { }

		return 0;
	}

	void forcedRelease(thread_db* tdbb);

private:
	void blockingAst();
	void ensureReleased(thread_db* tdbb);

	void checkGuard(unsigned flags);

private:
	Firebird::AutoPtr<Lock> gcLck;
	RelationPermanent* gcRel;
	std::atomic<unsigned> gcFlags = 0u;

	static const unsigned GC_counterMask =	0x0FFFFFFF;
	static const unsigned GC_guardBit =		0x10000000;
	static const unsigned GC_disabled =		0x20000000;
	static const unsigned GC_locked =		0x40000000;
	static const unsigned GC_blocking =		0x80000000;
};


// Non-versioned part of relation in cache

class RelationPermanent : public Firebird::PermanentStorage
{
	typedef CacheVector<Cached::Index, 4, RelationPermanent*> Indices;
	typedef Firebird::HalfStaticArray<Record*, 4> GCRecordList;

public:
	RelationPermanent(thread_db* tdbb, MemoryPool& p, MetaId id, NoData);
	~RelationPermanent();
	static bool destroy(thread_db* tdbb, RelationPermanent* rel);

	void makeLocks(thread_db* tdbb, Cached::Relation* relation);
	static constexpr USHORT getRelLockKeyLength() noexcept;
	Lock* createLock(thread_db* tdbb, lck_t, bool);
	Lock* createLock(thread_db* tdbb, MemoryPool& pool, lck_t, bool);
	void extFile(thread_db* tdbb, const TEXT* file_name);		// impl in ext.cpp

	IndexVersion* lookup_index(thread_db* tdbb, MetaId id, ObjectBase::Flag flags);
	IndexVersion* lookup_index(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag flags);
	Cached::Index* lookupIndex(thread_db* tdbb, MetaId id, ObjectBase::Flag flags);
	Cached::Index* lookupIndex(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag flags);

	void newIndexVersion(thread_db* tdbb, MetaId id, ObjectBase::Flag scanType)
	{
		auto chk = rel_indices.makeObject(tdbb, id, CacheFlag::NOCOMMIT | scanType);
		fb_assert(chk);
	}

	IndexVersion* oldIndexVersion(thread_db* tdbb, MetaId id, ObjectBase::Flag scanType)
	{
		return rel_indices.getVersioned(tdbb, id, CacheFlag::AUTOCREATE | scanType);
	}

	Cached::Index* eraseIndex(thread_db* tdbb, MetaId id)
	{
		return rel_indices.erase(tdbb, id);
	}

	Lock*		rel_partners_lock;		// partners lock
	GCLock		rel_gc_lock;			// garbage collection lock

	void releaseLock(thread_db* tdbb);

private:
	GCRecordList	rel_gc_records;		// records for garbage collection
	Firebird::Mutex	rel_gc_records_mutex;

public:
	std::atomic<SSHORT>	rel_scan_count;		// concurrent sequential scan count

	class RelPagesSnapshot : public Firebird::Array<RelationPages*>
	{
	public:
		typedef Firebird::Array<RelationPages*> inherited;

		RelPagesSnapshot(thread_db* tdbb, RelationPermanent* relation)
		{
			spt_tdbb = tdbb;
			spt_relation = relation;
		}

		~RelPagesSnapshot() { clear(); }

		void clear();
	private:
		thread_db*	spt_tdbb;
		RelationPermanent*	spt_relation;

	friend class RelationPermanent;
	};

	RelationPages* getPages(thread_db* tdbb, TraNumber tran = MAX_TRA_NUMBER, bool allocPages = true);
	bool	delPages(thread_db* tdbb, TraNumber tran = MAX_TRA_NUMBER, RelationPages* aPages = NULL);
	void	retainPages(thread_db* tdbb, TraNumber oldNumber, TraNumber newNumber);
	void	cleanUp() noexcept;
	void	fillPagesSnapshot(RelPagesSnapshot&, const bool AttachmentOnly = false);
	void	scanPartners(thread_db* tdbb);		// Foreign keys scan - impl. in met.epp

	RelationPages* getBasePages() noexcept
	{
		return &rel_pages_base;
	}

	bool hasData() const
	{
		return rel_name.hasData();
	}

	const QualifiedName& getName() const noexcept
	{
		return rel_name;
	}

	MetaId getId() const noexcept
	{
		return rel_id;
	}

	const QualifiedName& getSecurityName() const noexcept
	{
		return rel_security_name;
	}

	MetaName getOwnerName() const noexcept
	{
		return rel_owner_name;
	}

	ExternalFile* getExtFile() const noexcept
	{
		return rel_file;
	}

	void setExtFile(ExternalFile* f) noexcept
	{
		fb_assert(!rel_file);
		rel_file = f;
	}

	void getRelLockKey(thread_db* tdbb, UCHAR* key);
	PageNumber getIndexRootPage(thread_db* tdbb);
	Record* getGCRecord(thread_db* tdbb, const Format* const format);

	bool isSystem() const noexcept;
	bool isTemporary() const noexcept;
	bool isLTT() const noexcept;
	bool isVirtual() const noexcept;
	bool isView() const noexcept;
	bool isReplicating(thread_db* tdbb);

	static int partners_ast_relation(void* ast_object);

	// Relation must be updated on next use or commit
	static Cached::Relation* newVersion(thread_db* tdbb, const QualifiedName& name);

	// Lists of FK partners should be updated on next update
	void checkPartners(thread_db* tdbb);

	// On commit of relation dependencies of global field to be cleaned ...
	void removeDependsFrom(const QualifiedName& globField);
	//			... will be removed
	void removeDepends(thread_db* tdbb);

	typedef SharedReadVector<Format*, 16> Formats;

private:
	SharedReadVector<Format*, 16> rel_formats;	// Known record formats
	Firebird::Mutex rel_formats_grow;	// Mutex to grow rel_formats

public:
	HazardPtr<Formats::Generation> getFormats()
	{
		return rel_formats.readAccessor();
	}

	void addFormat(Format* fmt);

	Indices			rel_indices;		// Active indices
	QualifiedName	rel_name;			// ascii relation name
	MetaId			rel_id;

	MetaName		rel_owner_name;		// ascii owner
	QualifiedName	rel_security_name;	// security class name for relation
	std::atomic<ULONG>	rel_flags;		// flags

	Firebird::TriState	rel_repl_state;	// replication state

	PrimaryDeps*	rel_primary_dpnds = nullptr;	// foreign dependencies on this relation's primary key
	ForeignRefs*	rel_foreign_refs = nullptr;		// foreign references to other relations' primary keys

private:
	Firebird::Mutex			rel_pages_mutex;

	typedef Firebird::SortedArray<
				RelationPages*,
				Firebird::EmptyStorage<RelationPages*>,
				RelationPages::InstanceId,
				RelationPages>
			RelationPagesInstances;

	RelationPagesInstances* rel_pages_inst;
	RelationPages			rel_pages_base;
	RelationPages*			rel_pages_free;

	RelationPages* getPagesInternal(thread_db* tdbb, TraNumber tran, bool allocPages);

	ExternalFile* rel_file;

	Firebird::Array<QualifiedName> rel_clear_deps;
};


// specialization
template <> template <>
inline FB_UINT64 CacheElement<IndexVersion, IndexPermanent>::makeId<RelationPermanent*>(MetaId id,
	RelationPermanent* rel)
{
	return IndexPermanent::makeLockId(rel->getId(), id);
}


inline bool jrd_rel::hasData() const
{
	return rel_perm->rel_name.hasData();
}

inline const QualifiedName& jrd_rel::getName() const noexcept
{
	return rel_perm->getName();
}

inline MemoryPool& jrd_rel::getPool() const noexcept
{
	return rel_perm->getPool();
}

inline ExternalFile* jrd_rel::getExtFile() const noexcept
{
	return rel_perm->getExtFile();
}

inline const QualifiedName& jrd_rel::getSecurityName() const noexcept
{
	return rel_perm->getSecurityName();
}

inline MetaName jrd_rel::getOwnerName() const noexcept
{
	return rel_perm->getOwnerName();
}

inline MetaId jrd_rel::getId() const noexcept
{
	return rel_perm->getId();
}

inline RelationPages* jrd_rel::getPages(thread_db* tdbb, TraNumber tran, bool allocPages)
{
	return rel_perm->getPages(tdbb, tran, allocPages);
}

inline bool jrd_rel::isTemporary() const noexcept
{
	return rel_perm->isTemporary();
}

inline bool jrd_rel::isLTT() const noexcept
{
	return rel_perm->isLTT();
}

inline bool jrd_rel::isVirtual() const noexcept
{
	return rel_perm->isVirtual();
}

inline bool jrd_rel::isView() const noexcept
{
	return rel_perm->isView();
}

inline bool jrd_rel::isSystem() const noexcept
{
	return rel_perm->isSystem();
}

inline bool jrd_rel::isReplicating(thread_db* tdbb)
{
	return rel_perm->isReplicating(tdbb);
}

inline Record* jrd_rel::getGCRecord(thread_db* tdbb)
{
	return rel_perm->getGCRecord(tdbb, currentFormat(tdbb));
}


inline bool RelationPermanent::isSystem() const noexcept
{
	return rel_flags & REL_system;
}

inline bool RelationPermanent::isTemporary() const noexcept
{
	return (rel_flags & (REL_temp_tran | REL_temp_conn));
}

inline bool RelationPermanent::isVirtual() const noexcept
{
	return (rel_flags & REL_virtual);
}

inline bool RelationPermanent::isView() const noexcept
{
	return (rel_flags & REL_jrd_view);
}

inline bool RelationPermanent::isLTT() const noexcept
{
	return (rel_flags & REL_temp_ltt);
}

inline RelationPages* RelationPermanent::getPages(thread_db* tdbb, TraNumber tran, bool allocPages)
{
	if (!isTemporary())
		return &rel_pages_base;

	return getPagesInternal(tdbb, tran, allocPages);
}


/// class GCLock::Shared

inline GCLock::Shared::Shared(thread_db* tdbb, RelationPermanent* rl)
	: m_tdbb(tdbb),
	  m_rl(rl),
	  m_gcEnabled(m_rl->rel_gc_lock.acquire(m_tdbb, LCK_NO_WAIT))
{ }

inline GCLock::Shared::~Shared()
{
	if (m_gcEnabled)
		m_rl->rel_gc_lock.downgrade(m_tdbb);
}


/// class GCLock::Exclusive

inline bool GCLock::Exclusive::acquire(int wait)
{
	return m_rl->rel_gc_lock.disable(m_tdbb, wait, m_lock);
}

inline void GCLock::Exclusive::release()
{
	return m_rl->rel_gc_lock.enable(m_tdbb, m_lock);
}


// Field block, one for each field in a scanned relation

inline constexpr USHORT FLD_parse_computed = 0x0001;	// computed expression is being parsed

class jrd_fld : public pool_alloc<type_fld>
{
public:
	BoolExprNode*	fld_validation;		// validation clause, if any
	BoolExprNode*	fld_not_null;		// if field cannot be NULL
	ValueExprNode*	fld_missing_value;	// missing value, if any
	ValueExprNode*	fld_computation;	// computation for virtual field
	ValueExprNode*	fld_source;			// source for view fields
	ValueExprNode*	fld_default_value;	// default value, if any
	ArrayField*	fld_array;				// array description, if array
	MetaName	fld_name;				// Field name
	MetaName	fld_security_name;		// security class name for field
	QualifiedName	fld_generator_name;	// identity generator name
	QualifiedName	fld_source_name;	// RDB%FIELD name
	QualifiedNameMetaNamePair	fld_source_rel_field;	// Relation/field source name
	std::optional<IdentityType> fld_identity_type;
	USHORT fld_length;
	USHORT fld_segment_length;
	USHORT fld_character_length;
	USHORT fld_pos;
	USHORT fld_flags;

public:
	explicit jrd_fld(MemoryPool& p)
		: fld_name(p),
		  fld_security_name(p),
		  fld_generator_name(p),
		  fld_source_rel_field(p)
	{
	}
};

}

#endif	// JRD_RELATION_H
