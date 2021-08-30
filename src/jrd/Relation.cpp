/*
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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2005 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/Relation.h"

#include "../jrd/tra.h"
#include "../jrd/btr_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/vio_debug.h"

using namespace Jrd;

/// jrd_rel

RelationPages* jrd_rel::getPagesInternal(thread_db* tdbb, TraNumber tran, bool allocPages)
{
	if (tdbb->tdbb_flags & TDBB_use_db_page_space)
		return &rel_pages_base;

	Jrd::Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	SINT64 inst_id;
	// Vlad asked for this compile-time check to make sure we can contain a txn number here
	typedef int RangeCheck1[sizeof(inst_id) >= sizeof(TraNumber)];
	typedef int RangeCheck2[sizeof(inst_id) >= sizeof(AttNumber)];

	if (rel_flags & REL_temp_tran)
	{
		if (tran > 0 && tran != MAX_TRA_NUMBER) //if (tran > 0)
			inst_id = tran;
		else if (tdbb->tdbb_temp_traid)
			inst_id = tdbb->tdbb_temp_traid;
		else if (tdbb->getTransaction())
			inst_id = tdbb->getTransaction()->tra_number;
		else // called without transaction, maybe from OPT or CMP ?
			return &rel_pages_base;
	}
	else
		inst_id = PAG_attachment_id(tdbb);

	if (!rel_pages_inst)
		rel_pages_inst = FB_NEW_POOL(*rel_pool) RelationPagesInstances(*rel_pool);

	FB_SIZE_T pos;
	if (!rel_pages_inst->find(inst_id, pos))
	{
		if (!allocPages)
			return 0;

		RelationPages* newPages = rel_pages_free;
		if (!newPages) {
			newPages = FB_NEW_POOL(*rel_pool) RelationPages(*rel_pool);
		}
		else
		{
			rel_pages_free = newPages->rel_next_free;
			newPages->rel_next_free = 0;
		}

		fb_assert(newPages->useCount == 0);

		newPages->addRef();
		newPages->rel_instance_id = inst_id;
		newPages->rel_pg_space_id = dbb->dbb_page_manager.getTempPageSpaceID(tdbb);
		rel_pages_inst->add(newPages);

		// create primary pointer page and index root page
		DPM_create_relation_pages(tdbb, this, newPages);

#ifdef VIO_DEBUG
		VIO_trace(DEBUG_WRITES,
			"jrd_rel::getPages rel_id %u, inst %" SQUADFORMAT", ppp %" SLONGFORMAT", irp %" SLONGFORMAT", addr 0x%x\n",
			rel_id,
			newPages->rel_instance_id,
			newPages->rel_pages ? (*newPages->rel_pages)[0] : 0,
			newPages->rel_index_root,
			newPages);
#endif

		// create indexes
		MemoryPool* pool = tdbb->getDefaultPool();
		const bool poolCreated = !pool;

		if (poolCreated)
			pool = dbb->createPool();
		Jrd::ContextPoolHolder context(tdbb, pool);

		jrd_tra* idxTran = tdbb->getTransaction();
		if (!idxTran)
			idxTran = attachment->getSysTransaction();

		IndexDescAlloc* indices = NULL;
		// read indices from "base" index root page
		const USHORT idx_count = BTR_all(tdbb, this, &indices, &rel_pages_base);

		const index_desc* const end = indices->items + idx_count;
		for (index_desc* idx = indices->items; idx < end; idx++)
		{
			Firebird::MetaName idx_name;
			MET_lookup_index(tdbb, idx_name, this->rel_name, idx->idx_id + 1);

			idx->idx_root = 0;
			SelectivityList selectivity(*pool);
			IDX_create_index(tdbb, this, idx, idx_name.c_str(), NULL, idxTran, selectivity);

#ifdef VIO_DEBUG
			VIO_trace(DEBUG_WRITES,
				"jrd_rel::getPages rel_id %u, inst %" SQUADFORMAT", irp %" SLONGFORMAT", idx %u, idx_root %" SLONGFORMAT", addr 0x%x\n",
				rel_id,
				newPages->rel_instance_id,
				newPages->rel_index_root,
				idx->idx_id,
				idx->idx_root,
				newPages);
#endif
		}

		if (poolCreated)
			dbb->deletePool(pool);
		delete indices;

		return newPages;
	}

	RelationPages* pages = (*rel_pages_inst)[pos];
	fb_assert(pages->rel_instance_id == inst_id);
	return pages;
}

bool jrd_rel::delPages(thread_db* tdbb, TraNumber tran, RelationPages* aPages)
{
	RelationPages* pages = aPages ? aPages : getPages(tdbb, tran, false);
	if (!pages || !pages->rel_instance_id)
		return false;

	//fb_assert((tran <= 0) || ((tran > 0) && (pages->rel_instance_id == tran)));
	fb_assert(tran == 0 || tran == MAX_TRA_NUMBER ||
		(tran > 0 && pages->rel_instance_id == tran));

	fb_assert(pages->useCount > 0);

	if (--pages->useCount)
		return false;

#ifdef VIO_DEBUG
	VIO_trace(DEBUG_WRITES,
		"jrd_rel::delPages rel_id %u, inst %" SQUADFORMAT", ppp %" SLONGFORMAT", irp %" SLONGFORMAT", addr 0x%x\n",
		rel_id,
		pages->rel_instance_id,
		pages->rel_pages ? (*pages->rel_pages)[0] : 0,
		pages->rel_index_root,
		pages);
#endif

	FB_SIZE_T pos;
#ifdef DEV_BUILD
	const bool found =
#endif
		rel_pages_inst->find(pages->rel_instance_id, pos);
	fb_assert(found && ((*rel_pages_inst)[pos] == pages) );

	rel_pages_inst->remove(pos);

	if (pages->rel_index_root)
		IDX_delete_indices(tdbb, this, pages);

	if (pages->rel_pages)
		DPM_delete_relation_pages(tdbb, this, pages);

	pages->free(rel_pages_free);
	return true;
}

void jrd_rel::retainPages(thread_db* tdbb, TraNumber oldNumber, TraNumber newNumber)
{
	fb_assert(rel_flags & REL_temp_tran);
	fb_assert(oldNumber != 0);
	fb_assert(newNumber != 0);

	SINT64 inst_id = oldNumber;
	FB_SIZE_T pos;
	if (!rel_pages_inst->find(oldNumber, pos))
		return;

	RelationPages* pages = (*rel_pages_inst)[pos];
	fb_assert(pages->rel_instance_id == oldNumber);

	rel_pages_inst->remove(pos);

	pages->rel_instance_id = newNumber;
	rel_pages_inst->add(pages);
}

void jrd_rel::getRelLockKey(thread_db* tdbb, UCHAR* key)
{
	const ULONG val = rel_id;
	memcpy(key, &val, sizeof(ULONG));
	key += sizeof(ULONG);

	const SINT64 inst_id = getPages(tdbb)->rel_instance_id;
	memcpy(key, &inst_id, sizeof(SINT64));
}

USHORT jrd_rel::getRelLockKeyLength() const
{
	return sizeof(ULONG) + sizeof(SINT64);
}

void jrd_rel::cleanUp()
{
	delete rel_pages_inst;
	rel_pages_inst = NULL;
}


void jrd_rel::fillPagesSnapshot(RelPagesSnapshot& snapshot, const bool attachmentOnly)
{
	if (rel_pages_inst)
	{
		for (FB_SIZE_T i = 0; i < rel_pages_inst->getCount(); i++)
		{
			RelationPages* relPages = (*rel_pages_inst)[i];

			if (!attachmentOnly)
			{
				snapshot.add(relPages);
				relPages->addRef();
			}
			else if ((rel_flags & REL_temp_conn) &&
				PAG_attachment_id(snapshot.spt_tdbb) == relPages->rel_instance_id)
			{
				snapshot.add(relPages);
				relPages->addRef();
			}
			else if (rel_flags & REL_temp_tran)
			{
				const jrd_tra* tran = snapshot.spt_tdbb->getAttachment()->att_transactions;
				for (; tran; tran = tran->tra_next)
				{
					if (tran->tra_number == relPages->rel_instance_id)
					{
						snapshot.add(relPages);
						relPages->addRef();
					}
				}
			}
		}
	}
	else
		snapshot.add(&rel_pages_base);
}

void jrd_rel::RelPagesSnapshot::clear()
{
#ifdef DEV_BUILD
	thread_db* tdbb = NULL;
	SET_TDBB(tdbb);
	fb_assert(tdbb == spt_tdbb);
#endif

	for (FB_SIZE_T i = 0; i < getCount(); i++)
	{
		RelationPages* relPages = (*this)[i];
		(*this)[i] = NULL;

		spt_relation->delPages(spt_tdbb, MAX_TRA_NUMBER, relPages);
	}

	inherited::clear();
}

bool jrd_rel::hasTriggers() const
{
	typedef const TrigVector* ctv;
	ctv trigs[6] = // non-const array, don't want optimization tricks by the compiler.
	{
		rel_pre_erase,
		rel_post_erase,
		rel_pre_modify,
		rel_post_modify,
		rel_pre_store,
		rel_post_store
	};

	for (int i = 0; i < 6; ++i)
	{
		if (trigs[i] && trigs[i]->getCount())
			return true;
	}
	return false;
}

Lock* jrd_rel::createLock(thread_db* tdbb, MemoryPool* pool, jrd_rel* relation, lck_t lckType, bool noAst)
{
	if (!pool)
		pool = relation->rel_pool;

	const USHORT relLockLen = relation->getRelLockKeyLength();

	Lock* lock = FB_NEW_RPT(*pool, relLockLen) Lock(tdbb, relLockLen, lckType, relation);
	relation->getRelLockKey(tdbb, lock->getKeyString());

	lock->lck_type = lckType;
	switch (lckType)
	{
	case LCK_relation:
		break;

	case LCK_rel_gc:
		lock->lck_ast = noAst ? NULL : blocking_ast_gcLock;
		break;

	default:
		fb_assert(false);
	}

	return lock;
}

bool jrd_rel::acquireGCLock(thread_db* tdbb, int wait)
{
	fb_assert(rel_flags & REL_gc_lockneed);
	if (!(rel_flags & REL_gc_lockneed))
	{
		fb_assert(rel_gc_lock->lck_id);
		fb_assert(rel_gc_lock->lck_physical == (rel_flags & REL_gc_disabled ? LCK_SR : LCK_SW));
		return true;
	}

	if (!rel_gc_lock)
		rel_gc_lock = createLock(tdbb, NULL, this, LCK_rel_gc, false);

	fb_assert(!rel_gc_lock->lck_id);
	fb_assert(!(rel_flags & REL_gc_blocking));

	ThreadStatusGuard temp_status(tdbb);

	const USHORT level = (rel_flags & REL_gc_disabled) ? LCK_SR : LCK_SW;
	bool ret = LCK_lock(tdbb, rel_gc_lock, level, wait);
	if (!ret && (level == LCK_SW))
	{
		rel_flags |= REL_gc_disabled;
		ret = LCK_lock(tdbb, rel_gc_lock, LCK_SR, wait);
		if (!ret)
			rel_flags &= ~REL_gc_disabled;
	}

	if (ret)
		rel_flags &= ~REL_gc_lockneed;

	return ret;
}

void jrd_rel::downgradeGCLock(thread_db* tdbb)
{
	if (!rel_sweep_count && (rel_flags & REL_gc_blocking))
	{
		fb_assert(!(rel_flags & REL_gc_lockneed));
		fb_assert(rel_gc_lock->lck_id);
		fb_assert(rel_gc_lock->lck_physical == LCK_SW);

		rel_flags &= ~REL_gc_blocking;
		rel_flags |= REL_gc_disabled;

		LCK_downgrade(tdbb, rel_gc_lock);

		if (rel_gc_lock->lck_physical != LCK_SR)
		{
			rel_flags &= ~REL_gc_disabled;
			if (rel_gc_lock->lck_physical < LCK_SR)
				rel_flags |= REL_gc_lockneed;
		}
	}
}

int jrd_rel::blocking_ast_gcLock(void* ast_object)
{
	/****
	SR - gc forbidden, awaiting moment to re-establish SW lock
	SW - gc allowed, usual state
	PW - gc allowed to the one connection only
	****/

	jrd_rel* relation = static_cast<jrd_rel*>(ast_object);

	try
	{
		Lock* lock = relation->rel_gc_lock;
		Database* dbb = lock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, lock);

		fb_assert(!(relation->rel_flags & REL_gc_lockneed));
		if (relation->rel_flags & REL_gc_lockneed) // work already done synchronously ?
			return 0;

		relation->rel_flags |= REL_gc_blocking;
		if (relation->rel_sweep_count)
			return 0;

		if (relation->rel_flags & REL_gc_disabled)
		{
			// someone acquired EX lock

			fb_assert(lock->lck_id);
			fb_assert(lock->lck_physical == LCK_SR);

			LCK_release(tdbb, lock);
			relation->rel_flags &= ~(REL_gc_disabled | REL_gc_blocking);
			relation->rel_flags |= REL_gc_lockneed;
		}
		else
		{
			// someone acquired PW lock

			fb_assert(lock->lck_id);
			fb_assert(lock->lck_physical == LCK_SW);

			relation->rel_flags |= REL_gc_disabled;
			relation->downgradeGCLock(tdbb);
		}
	}
	catch (const Firebird::Exception&)
	{} // no-op

	return 0;
}


/// jrd_rel::GCExclusive

jrd_rel::GCExclusive::GCExclusive(thread_db* tdbb, jrd_rel* relation) :
	m_tdbb(tdbb),
	m_relation(relation),
	m_lock(NULL)
{
}

jrd_rel::GCExclusive::~GCExclusive()
{
	release();
	delete m_lock;
}

bool jrd_rel::GCExclusive::acquire(int wait)
{
	// if validation is already running - go out
	if (m_relation->rel_flags & REL_gc_disabled)
		return false;

	ThreadStatusGuard temp_status(m_tdbb);

	m_relation->rel_flags |= REL_gc_disabled;

	int sleeps = -wait * 10;
	while (m_relation->rel_sweep_count)
	{
		EngineCheckout cout(m_tdbb, FB_FUNCTION);
		Thread::sleep(100);

		if (wait < 0 && --sleeps == 0)
			break;
	}

	if (m_relation->rel_sweep_count)
	{
		m_relation->rel_flags &= ~REL_gc_disabled;
		return false;
	}

	if (!(m_relation->rel_flags & REL_gc_lockneed))
	{
		m_relation->rel_flags |= REL_gc_lockneed;
		LCK_release(m_tdbb, m_relation->rel_gc_lock);
	}

	// we need no AST here
	if (!m_lock)
		m_lock = jrd_rel::createLock(m_tdbb, NULL, m_relation, LCK_rel_gc, true);

	const bool ret = LCK_lock(m_tdbb, m_lock, LCK_PW, wait);
	if (!ret)
		m_relation->rel_flags &= ~REL_gc_disabled;

	return ret;
}

void jrd_rel::GCExclusive::release()
{
	if (!m_lock || !m_lock->lck_id)
		return;

	fb_assert(m_relation->rel_flags & REL_gc_disabled);

	if (!(m_relation->rel_flags & REL_gc_lockneed))
	{
		m_relation->rel_flags |= REL_gc_lockneed;
		LCK_release(m_tdbb, m_relation->rel_gc_lock);
	}

	LCK_convert(m_tdbb, m_lock, LCK_EX, LCK_WAIT);
	m_relation->rel_flags &= ~REL_gc_disabled;

	LCK_release(m_tdbb, m_lock);
}


/// RelationPages

void RelationPages::free(RelationPages*& nextFree)
{
	rel_next_free = nextFree;
	nextFree = this;

	if (rel_pages)
		rel_pages->clear();

	rel_index_root = rel_data_pages = 0;
	rel_slot_space = rel_pri_data_space = rel_sec_data_space = 0;
	rel_last_free_pri_dp = 0;
	rel_instance_id = 0;

	dpMap.clear();
	dpMapMark = 0;
}
