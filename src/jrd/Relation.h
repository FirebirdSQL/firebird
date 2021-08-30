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

#include "../jrd/jrd.h"
#include "../jrd/btr.h"
#include "../jrd/lck.h"
#include "../jrd/pag.h"
#include "../jrd/val.h"
#include "../jrd/Attachment.h"

namespace Jrd
{

class BoolExprNode;
class RseNode;
class StmtNode;

// view context block to cache view aliases

class ViewContext
{
public:
	explicit ViewContext(MemoryPool& p, const TEXT* context_name,
						 const TEXT* relation_name, USHORT context,
						 ViewContextType type)
	: vcx_context_name(p, context_name, fb_strlen(context_name)),
	  vcx_relation_name(relation_name),
	  vcx_context(context),
	  vcx_type(type)
	{
	}

	static USHORT generate(const ViewContext* vc)
	{
		return vc->vcx_context;
	}

	const Firebird::string vcx_context_name;
	const Firebird::MetaName vcx_relation_name;
	const USHORT vcx_context;
	const ViewContextType vcx_type;
};

typedef Firebird::SortedArray<ViewContext*, Firebird::EmptyStorage<ViewContext*>,
		USHORT, ViewContext> ViewContexts;


class RelationPages
{
public:
	typedef SINT64 RP_INSTANCE_ID;

	vcl* rel_pages;				// vector of pointer page numbers
	RP_INSTANCE_ID rel_instance_id;		// 0 or att_attachment_id or tra_number

	// Vlad asked for this compile-time check to make sure we can contain a txn number here
	typedef int RangeCheck1[sizeof(RP_INSTANCE_ID) >= sizeof(TraNumber)];
	typedef int RangeCheck2[sizeof(RP_INSTANCE_ID) >= sizeof(AttNumber)];

	ULONG rel_index_root;		// index root page number
	ULONG rel_data_pages;		// count of relation data pages
	ULONG rel_slot_space;		// lowest pointer page with slot space
	ULONG rel_pri_data_space;	// lowest pointer page with primary data page space
	ULONG rel_sec_data_space;	// lowest pointer page with secondary data page space
	ULONG rel_last_free_pri_dp;	// last primary data page found with space
	USHORT rel_pg_space_id;

	RelationPages(Firebird::MemoryPool& pool)
		: rel_pages(NULL), rel_instance_id(0),
		  rel_index_root(0), rel_data_pages(0), rel_slot_space(0),
		  rel_pri_data_space(0), rel_sec_data_space(0),
		  rel_last_free_pri_dp(0),
		  rel_pg_space_id(DB_PAGE_SPACE), rel_next_free(NULL),
		  useCount(0),
		  dpMap(pool),
		  dpMapMark(0)
	{}

	inline SLONG addRef()
	{
		return useCount++;
	}

	void free(RelationPages*& nextFree);

	static inline RP_INSTANCE_ID generate(const RelationPages* item)
	{
		return item->rel_instance_id;
	}

	ULONG getDPNumber(ULONG dpSequence)
	{
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
			dpMap.insert(pos, DPItem(dpSequence, dpNumber, ++dpMapMark));

			if (dpMap.getCount() == MAX_DPMAP_ITEMS)
				freeOldestMapItems();
		}
	}

	void freeOldestMapItems()
	{
		ULONG minMark = MAX_ULONG;
		FB_SIZE_T i;
		for (i = 0; i < dpMap.getCount(); i++)
			if (minMark > dpMap[i].mark)
				minMark = dpMap[i].mark;

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
	RelationPages*	rel_next_free;
	SLONG	useCount;

	static const ULONG MAX_DPMAP_ITEMS = 64;

	struct DPItem
	{
		ULONG seqNum;
		ULONG physNum;
		ULONG mark;

		DPItem()
			: seqNum(0), physNum(0), mark(0)
		{}

		DPItem(ULONG sn, ULONG pn, ULONG mk)
			: seqNum(sn), physNum(pn), mark(mk)
		{}

		static ULONG generate(const DPItem& item)
		{
			return item.seqNum;
		}
	};

	Firebird::SortedArray<DPItem, Firebird::InlineStorage<DPItem, MAX_DPMAP_ITEMS>, ULONG, DPItem> dpMap;
	ULONG dpMapMark;

friend class jrd_rel;
};


// Primary dependencies from all foreign references to relation's
// primary/unique keys

struct prim
{
	vec<int>* prim_reference_ids;
	vec<int>* prim_relations;
	vec<int>* prim_indexes;
};


// Foreign references to other relations' primary/unique keys

struct frgn
{
	vec<int>* frgn_reference_ids;
	vec<int>* frgn_relations;
	vec<int>* frgn_indexes;
};

// Relation block; one is created for each relation referenced
// in the database, though it is not really filled out until
// the relation is scanned

class jrd_rel : public pool_alloc<type_rel>
{
	typedef Firebird::HalfStaticArray<Record*, 4> GCRecordList;

public:
	MemoryPool*		rel_pool;
	USHORT			rel_id;
	USHORT			rel_current_fmt;	// Current format number
	ULONG			rel_flags;
	Format*			rel_current_format;	// Current record format

	Firebird::MetaName	rel_name;		// ascii relation name
	Firebird::MetaName	rel_owner_name;	// ascii owner
	Firebird::MetaName	rel_security_name;	// security class name for relation

	vec<Format*>*	rel_formats;		// Known record formats
	vec<jrd_fld*>*	rel_fields;			// vector of field blocks

	RseNode*		rel_view_rse;		// view record select expression
	ViewContexts	rel_view_contexts;	// sorted array of view contexts

	ExternalFile* 	rel_file;			// external file name

	GCRecordList	rel_gc_records;		// records for garbage collection

	USHORT		rel_use_count;		// requests compiled with relation
	USHORT		rel_sweep_count;	// sweep and/or garbage collector threads active
	SSHORT		rel_scan_count;		// concurrent sequential scan count

	Lock*		rel_existence_lock;	// existence lock, if any
	Lock*		rel_partners_lock;	// partners lock
	Lock*		rel_rescan_lock;	// lock forcing relation to be scanned
	Lock*		rel_gc_lock;		// garbage collection lock
	IndexLock*	rel_index_locks;	// index existence locks
	IndexBlock*	rel_index_blocks;	// index blocks for caching index info
	TrigVector*	rel_pre_erase; 		// Pre-operation erase trigger
	TrigVector*	rel_post_erase;		// Post-operation erase trigger
	TrigVector*	rel_pre_modify;		// Pre-operation modify trigger
	TrigVector*	rel_post_modify;	// Post-operation modify trigger
	TrigVector*	rel_pre_store;		// Pre-operation store trigger
	TrigVector*	rel_post_store;		// Post-operation store trigger
	prim		rel_primary_dpnds;	// foreign dependencies on this relation's primary key
	frgn		rel_foreign_refs;	// foreign references to other relations' primary keys

	Firebird::Mutex rel_drop_mutex;

	bool isSystem() const;
	bool isTemporary() const;
	bool isVirtual() const;
	bool isView() const;

	// global temporary relations attributes
	RelationPages* getPages(thread_db* tdbb, TraNumber tran = MAX_TRA_NUMBER, bool allocPages = true);

	RelationPages* getBasePages()
	{
		return &rel_pages_base;
	}

	bool	delPages(thread_db* tdbb, TraNumber tran = MAX_TRA_NUMBER, RelationPages* aPages = NULL);
	void	retainPages(thread_db* tdbb, TraNumber oldNumber, TraNumber newNumber);

	void	getRelLockKey(thread_db* tdbb, UCHAR* key);
	USHORT	getRelLockKeyLength() const;

	void	cleanUp();

	class RelPagesSnapshot : public Firebird::Array<RelationPages*>
	{
	public:
		typedef Firebird::Array<RelationPages*> inherited;

		RelPagesSnapshot(thread_db* tdbb, jrd_rel* relation)
		{
			spt_tdbb = tdbb;
			spt_relation = relation;
		}

		~RelPagesSnapshot() { clear(); }

		void clear();
	private:
		thread_db*	spt_tdbb;
		jrd_rel*	spt_relation;

	friend class jrd_rel;
	};

	void fillPagesSnapshot(RelPagesSnapshot&, const bool AttachmentOnly = false);

private:
	typedef Firebird::SortedArray<
				RelationPages*,
				Firebird::EmptyStorage<RelationPages*>,
				SINT64,
				RelationPages>
			RelationPagesInstances;

	// Vlad asked for this compile-time check to make sure we can contain a txn number here
	typedef int RangeCheck1[sizeof(SINT64) >= sizeof(TraNumber)];
	typedef int RangeCheck2[sizeof(SINT64) >= sizeof(AttNumber)];

	RelationPagesInstances* rel_pages_inst;
	RelationPages			rel_pages_base;
	RelationPages*			rel_pages_free;

	RelationPages* getPagesInternal(thread_db* tdbb, TraNumber tran, bool allocPages);

public:
	explicit jrd_rel(MemoryPool& p);

	bool hasTriggers() const;

	static Lock* createLock(thread_db* tdbb, MemoryPool* pool, jrd_rel* relation, lck_t, bool);
	static int blocking_ast_gcLock(void*);

	void downgradeGCLock(thread_db* tdbb);
	bool acquireGCLock(thread_db* tdbb, int wait);

	// This guard is used by regular code to prevent online validation while
	// dead- or back- versions is removed from disk.
	class GCShared
	{
	public:
		GCShared(thread_db* tdbb, jrd_rel* relation);
		~GCShared();

		bool gcEnabled() const
		{
			return m_gcEnabled;
		}

	private:
		thread_db*	m_tdbb;
		jrd_rel*	m_relation;
		bool		m_gcEnabled;
	};

	// This guard is used by online validation to prevent any modifications of
	// table data while it is checked.
	class GCExclusive
	{
	public:
		GCExclusive(thread_db* tdbb, jrd_rel* relation);
		~GCExclusive();

		bool acquire(int wait);
		void release();

	private:
		thread_db*	m_tdbb;
		jrd_rel*	m_relation;
		Lock*		m_lock;
	};
};

// rel_flags

const ULONG REL_scanned					= 0x0001;	// Field expressions scanned (or being scanned)
const ULONG REL_system					= 0x0002;
const ULONG REL_deleted					= 0x0004;	// Relation known gonzo
const ULONG REL_get_dependencies		= 0x0008;	// New relation needs dependencies during scan
const ULONG REL_force_scan				= 0x0010;	// system relation has been updated since ODS change, force a scan
const ULONG REL_check_existence			= 0x0020;	// Existence lock released pending drop of relation
const ULONG REL_blocking				= 0x0040;	// Blocking someone from dropping relation
const ULONG REL_sys_triggers			= 0x0080;	// The relation has system triggers to compile
const ULONG REL_sql_relation			= 0x0100;	// Relation defined as sql table
const ULONG REL_check_partners			= 0x0200;	// Rescan primary dependencies and foreign references
const ULONG REL_being_scanned			= 0x0400;	// relation scan in progress
const ULONG REL_sys_trigs_being_loaded	= 0x0800;	// System triggers being loaded
const ULONG REL_deleting				= 0x1000;	// relation delete in progress
const ULONG REL_temp_tran				= 0x2000;	// relation is a GTT delete rows
const ULONG REL_temp_conn				= 0x4000;	// relation is a GTT preserve rows
const ULONG REL_virtual					= 0x8000;	// relation is virtual
const ULONG REL_jrd_view				= 0x10000;	// relation is VIEW
const ULONG REL_gc_blocking				= 0x20000;	// request to downgrade\release gc lock
const ULONG REL_gc_disabled				= 0x40000;	// gc is disabled temporarily
const ULONG REL_gc_lockneed				= 0x80000;	// gc lock should be acquired


/// class jrd_rel

inline jrd_rel::jrd_rel(MemoryPool& p)
	: rel_pool(&p), rel_flags(REL_gc_lockneed),
	  rel_name(p), rel_owner_name(p), rel_security_name(p),
	  rel_view_contexts(p), rel_gc_records(p), rel_pages_base(p)
{
}

inline bool jrd_rel::isSystem() const
{
	return rel_flags & REL_system;
}

inline bool jrd_rel::isTemporary() const
{
	return (rel_flags & (REL_temp_tran | REL_temp_conn));
}

inline bool jrd_rel::isVirtual() const
{
	return (rel_flags & REL_virtual);
}

inline bool jrd_rel::isView() const
{
	return (rel_flags & REL_jrd_view);
}

inline RelationPages* jrd_rel::getPages(thread_db* tdbb, TraNumber tran, bool allocPages)
{
	if (!isTemporary())
		return &rel_pages_base;

	return getPagesInternal(tdbb, tran, allocPages);
}

/// class jrd_rel::GCShared

inline jrd_rel::GCShared::GCShared(thread_db* tdbb, jrd_rel* relation)
	: m_tdbb(tdbb),
	  m_relation(relation),
	  m_gcEnabled(false)
{
	if (m_relation->rel_flags & (REL_gc_blocking | REL_gc_disabled))
		return;

	if (m_relation->rel_flags & REL_gc_lockneed)
		m_relation->acquireGCLock(tdbb, LCK_NO_WAIT);

	if (!(m_relation->rel_flags & (REL_gc_blocking | REL_gc_disabled | REL_gc_lockneed)))
	{
		++m_relation->rel_sweep_count;
		m_gcEnabled = true;
	}

	if ((m_relation->rel_flags & REL_gc_blocking) && !m_relation->rel_sweep_count)
		m_relation->downgradeGCLock(m_tdbb);
}

inline jrd_rel::GCShared::~GCShared()
{
	if (m_gcEnabled)
		--m_relation->rel_sweep_count;

	if ((m_relation->rel_flags & REL_gc_blocking) && !m_relation->rel_sweep_count)
		m_relation->downgradeGCLock(m_tdbb);
}


// Field block, one for each field in a scanned relation

const USHORT FLD_parse_computed = 0x0001;		// computed expression is being parsed

class jrd_fld : public pool_alloc<type_fld>
{
public:
	BoolExprNode*	fld_validation;		// validation clause, if any
	BoolExprNode*	fld_not_null;		// if field cannot be NULL
	ValueExprNode*	fld_missing_value;	// missing value, if any
	ValueExprNode*	fld_computation;	// computation for virtual field
	ValueExprNode*	fld_source;			// source for view fields
	ValueExprNode*	fld_default_value;	// default value, if any
	ArrayField*	fld_array;			// array description, if array
	Firebird::MetaName	fld_name;	// Field name
	Firebird::MetaName	fld_security_name;	// security class name for field
	Firebird::MetaName	fld_generator_name;	// identity generator name
	USHORT fld_flags;

public:
	explicit jrd_fld(MemoryPool& p)
		: fld_name(p), fld_security_name(p), fld_generator_name(p)
	{ }
};

}

#endif	// JRD_RELATION_H
