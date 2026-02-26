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

#include "../jrd/met.h"
#include "../jrd/tra.h"
#include "../jrd/btr_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/lck.h"
#include "../jrd/met_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/vio_debug.h"
#include "../jrd/ext_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/Statement.h"
#include "../common/StatusArg.h"

// Pick up relation ids
#include "../jrd/ini.h"

using namespace Jrd;
using namespace Firebird;


TrigArray::TrigArray(MemoryPool& p)
	: preErase(p), postErase(p), preModify(p),
	  postModify(p), preStore(p), postStore(p)
{ }

Triggers& TrigArray::operator[](int t)
{
	switch(t)
	{
	case TRIGGER_PRE_STORE:
		return preStore;

	case TRIGGER_POST_STORE:
		return postStore;

	case TRIGGER_PRE_MODIFY:
		return preModify;

	case TRIGGER_POST_MODIFY:
		return postModify;

	case TRIGGER_PRE_ERASE:
		return preErase;

	case TRIGGER_POST_ERASE:
		return postErase;
	}

	fb_assert(false);
	fatal_exception::raise("Invalid trigger type");
}

const Triggers& TrigArray::operator[](int t) const
{
	switch(t)
	{
	case TRIGGER_PRE_STORE:
		return preStore;

	case TRIGGER_POST_STORE:
		return postStore;

	case TRIGGER_PRE_MODIFY:
		return preModify;

	case TRIGGER_POST_MODIFY:
		return postModify;

	case TRIGGER_PRE_ERASE:
		return preErase;

	case TRIGGER_POST_ERASE:
		return postErase;
	}

	fb_assert(false);
	fatal_exception::raise("Invalid trigger type");
}

jrd_rel::jrd_rel(MemoryPool& p, Cached::Relation* r)
	: rel_pool(&p),
	  rel_perm(r),
	  rel_current_fmt(0),
	  rel_current_format(nullptr),
	  rel_fields(nullptr),
	  rel_view_rse(nullptr),
	  rel_view_contexts(p),
	  rel_triggers(p),
	  rel_ss_definer(false)
{ }

RelationPermanent::RelationPermanent(thread_db* tdbb, MemoryPool& p, MetaId id, NoData)
	: PermanentStorage(p),
	  rel_partners_lock(nullptr),
	  rel_gc_lock(this),
	  rel_gc_records(p),
	  rel_scan_count(0),
	  rel_formats(/*p*/),
	  rel_indices(p, this),
	  rel_name(p),
	  rel_id(id),
	  rel_flags(REL_check_partners),
	  rel_pages_inst(nullptr),
	  rel_pages_base(p),
	  rel_pages_free(nullptr),
	  rel_file(nullptr),
	  rel_clear_deps(p)
{
	rel_partners_lock = FB_NEW_RPT(getPool(), 0)
		Lock(tdbb, sizeof(SLONG), LCK_rel_partners, this, partners_ast_relation);
	rel_partners_lock->setKey(rel_id);
}

RelationPermanent::~RelationPermanent()
{
	fb_assert(!rel_partners_lock);
}

bool RelationPermanent::destroy(thread_db* tdbb, RelationPermanent* rel)
{
	if (rel->rel_partners_lock)
	{
		LCK_release(tdbb, rel->rel_partners_lock);
		rel->rel_partners_lock = nullptr;
	}

	if (rel->rel_file)
	{
		rel->rel_file->release();
		delete rel->rel_file;
	}

	rel->rel_indices.cleanup(tdbb);
/*
	// delete by pool is broken, needs enhancements in CacheVector
	auto& pool = rel->getPool();
	tdbb->getDatabase()->deletePool(&pool);

	return true;*/
	return false;
}

void RelationPermanent::removeDependsFrom(const QualifiedName& globField)
{
	// rel_clear_deps is protected by new relation version,
	// already created by MET_change_fields() at this moment

	rel_clear_deps.push(globField);
}

void RelationPermanent::removeDepends(thread_db* tdbb)
{
	while (rel_clear_deps.hasData())
		MET_delete_dependencies(tdbb, rel_clear_deps.pop(), obj_computed);
}

int RelationPermanent::partners_ast_relation(void* ast_object)
{
	auto* const relation = static_cast<RelationPermanent*>(ast_object);

	try
	{
		Lock* lock = relation->rel_partners_lock;
		Database* const dbb = lock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION);

		auto oldFlags = relation->rel_flags.fetch_or(REL_check_partners);
		if (!(oldFlags & REL_check_partners))
			LCK_release(tdbb, lock);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}



Record* RelationPermanent::getGCRecord(thread_db* tdbb, const Format* const format)
{
/**************************************
 *
 *	V I O _ g c _ r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Allocate from a relation's vector of garbage
 *	collect record blocks. Their scope is strictly
 *	limited to temporary usage and should never be
 *	copied to permanent record parameter blocks.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	// Set the active flag on an inactive garbage collect record block and return it

	MutexLockGuard g(rel_gc_records_mutex, FB_FUNCTION);

	for (Record* const record : rel_gc_records)
	{
		fb_assert(record);

		if (!record->isTempActive())
		{
			// initialize record for reuse
			record->reset(format);
			record->setTempActive();
			return record;
		}
	}

	// Allocate a garbage collect record block if all are active

	Record* const record = FB_NEW_POOL(getPool()) Record(getPool(), format, true);
	rel_gc_records.add(record);
	return record;
}

void RelationPermanent::checkPartners(thread_db* tdbb)
{
	rel_flags |= REL_check_partners;

	LCK_lock(tdbb, rel_partners_lock, LCK_EX, LCK_WAIT);
	LCK_release(tdbb, rel_partners_lock);
}

bool RelationPermanent::isReplicating(thread_db* tdbb)
{
	Database* const dbb = tdbb->getDatabase();
	if (!dbb->isReplicating(tdbb))
		return false;

	Attachment* const attachment = tdbb->getAttachment();		// Database? !!!!!!!!!!!!!!!!!!!!!!
	attachment->checkReplSetLock(tdbb);

	if (rel_repl_state.isUnknown())
		rel_repl_state = MET_get_repl_state(tdbb, getName());

	return rel_repl_state.asBool();
}

RelationPages* RelationPermanent::getPagesInternal(thread_db* tdbb, TraNumber tran, bool allocPages)
{
	if (tdbb->tdbb_flags & TDBB_use_db_page_space)
		return &rel_pages_base;

	Jrd::Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	RelationPages::InstanceId inst_id;

	if (rel_flags & REL_temp_tran)
	{
		if (tran != 0 && tran != MAX_TRA_NUMBER)
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

	MutexLockGuard relPerm(rel_pages_mutex, FB_FUNCTION);

	if (!rel_pages_inst)
		rel_pages_inst = FB_NEW_POOL(getPool()) RelationPagesInstances(getPool());

	FB_SIZE_T pos;
	if (!rel_pages_inst->find(inst_id, pos))
	{
		if (!allocPages)
			return nullptr;

		RelationPages* newPages = rel_pages_free;
		if (!newPages) {
			newPages = FB_NEW_POOL(getPool()) RelationPages(getPool());
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
			"jrd_rel::getPages rel_id %u, inst %" UQUADFORMAT", ppp %" ULONGFORMAT", irp %" ULONGFORMAT", addr 0x%x\n",
			getId(),
			newPages->rel_instance_id,
			newPages->rel_pages ? (*newPages->rel_pages)[0] : 0,
			newPages->rel_index_root,
			newPages);
#endif

		// create indexes
		MemoryPool* pool = tdbb->getDefaultPool();
		const bool poolCreated = !pool;

		if (poolCreated)
			pool = dbb->createPool(false);
		Jrd::ContextPoolHolder context(tdbb, pool);

		jrd_tra* idxTran = tdbb->getTransaction();
		if (!idxTran)
			idxTran = attachment->getSysTransaction();

		jrd_rel* rel = MetadataCache::getVersioned<Cached::Relation>(tdbb, getId(), CacheFlag::AUTOCREATE);
		fb_assert(rel);

		IndexDescList indices;
		BTR_all(tdbb, getPermanent(rel), indices, &rel_pages_base);

		for (auto& idx : indices)
		{
			auto* idp = this->lookupIndex(tdbb, idx.idx_id, CacheFlag::AUTOCREATE);
			QualifiedName idx_name;
			if (idp)
				idx_name = idp->getName();

			idx.idx_root = 0;
			SelectivityList selectivity(*pool);
			IDX_create_index(tdbb, IdxCreate::AtOnce, rel, &idx, idx_name, NULL, idxTran, selectivity);

#ifdef VIO_DEBUG
			VIO_trace(DEBUG_WRITES,
				"jrd_rel::getPages rel_id %u, inst %" UQUADFORMAT", irp %" ULONGFORMAT", idx %u, idx_root %" ULONGFORMAT", addr 0x%x\n",
				getId(),
				newPages->rel_instance_id,
				newPages->rel_index_root,
				idx.idx_id,
				idx.idx_root,
				newPages);
#endif
		}

		if (poolCreated)
			dbb->deletePool(pool);

		return newPages;
	}

	RelationPages* pages = (*rel_pages_inst)[pos];
	fb_assert(pages->rel_instance_id == inst_id);
	return pages;
}

bool RelationPermanent::delPages(thread_db* tdbb, TraNumber tran, RelationPages* aPages)
{
	RelationPages* pages = aPages ? aPages : getPages(tdbb, tran, false);
	if (!pages || !pages->rel_instance_id)
		return false;

	fb_assert(tran == 0 || tran == MAX_TRA_NUMBER || pages->rel_instance_id == tran);

	fb_assert(pages->useCount > 0);

	if (--pages->useCount)
		return false;

#ifdef VIO_DEBUG
	VIO_trace(DEBUG_WRITES,
		"jrd_rel::delPages rel_id %u, inst %" UQUADFORMAT", ppp %" ULONGFORMAT", irp %" ULONGFORMAT", addr 0x%x\n",
		getId(),
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
		IDX_delete_indices(tdbb, this, pages, false);

	if (pages->rel_pages)
		DPM_delete_relation_pages(tdbb, this, pages);

	pages->free(rel_pages_free);
	return true;
}

void RelationPermanent::retainPages(thread_db* tdbb, TraNumber oldNumber, TraNumber newNumber)
{
	fb_assert(rel_flags & REL_temp_tran);
	fb_assert(oldNumber != 0);
	fb_assert(newNumber != 0);

	if (!rel_pages_inst)
		return;

	FB_SIZE_T pos;
	if (!rel_pages_inst->find(oldNumber, pos))
		return;

	RelationPages* pages = (*rel_pages_inst)[pos];
	fb_assert(pages->rel_instance_id == oldNumber);

	rel_pages_inst->remove(pos);

	pages->rel_instance_id = newNumber;
	rel_pages_inst->add(pages);
}

void RelationPermanent::getRelLockKey(thread_db* tdbb, UCHAR* key)
{
	const ULONG val = getId();
	memcpy(key, &val, sizeof(ULONG));
	key += sizeof(ULONG);

	const RelationPages::InstanceId inst_id = getPages(tdbb)->rel_instance_id;
	memcpy(key, &inst_id, sizeof(inst_id));
}

constexpr USHORT RelationPermanent::getRelLockKeyLength() noexcept
{
	return sizeof(ULONG) + sizeof(SINT64);
}

void RelationPermanent::cleanUp() noexcept
{
	delete rel_pages_inst;
	rel_pages_inst = NULL;
}


void RelationPermanent::fillPagesSnapshot(RelPagesSnapshot& snapshot, const bool attachmentOnly)
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


void RelationPermanent::RelPagesSnapshot::clear()
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


IndexVersion* RelationPermanent::lookup_index(thread_db* tdbb, MetaId id, ObjectBase::Flag flags)
{
	return rel_indices.getVersioned(tdbb, id, flags);
}


Cached::Index* RelationPermanent::lookupIndex(thread_db* tdbb, MetaId id, ObjectBase::Flag flags)
{
	auto* idp = rel_indices.getDataNoChecks(id);
	if (idp)
		return idp;

	if (flags & CacheFlag::AUTOCREATE)
	{
		auto* idv = lookup_index(tdbb, id, flags);
		if (idv)
			return getPermanent(idv);
	}

	return nullptr;
}


PageNumber RelationPermanent::getIndexRootPage(thread_db* tdbb)
{
/**************************************
 *
 *	g e t _ r o o t _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Find the root page for a relation.
 *
 **************************************/
	SET_TDBB(tdbb);

	RelationPages* relPages = getPages(tdbb);
	SLONG page = relPages->rel_index_root;
	if (!page)
	{
		DPM_scan_pages(tdbb);
		page = relPages->rel_index_root;
	}

	return PageNumber(relPages->rel_pg_space_id, page);
}


Cached::Relation* RelationPermanent::newVersion(thread_db* tdbb, const QualifiedName& name)
{
	auto id = jrd_rel::getIdByName(tdbb, name);
	if (id.has_value())
	{
		auto* relation = MetadataCache::newVersion<Cached::Relation>(tdbb, id.value());
		fb_assert(relation);

		if (relation)
			DFW_post_work(tdbb->getTransaction(), dfw_commit_relation, nullptr, nullptr, id.value());

		return relation;
	}

	return nullptr;
}


void RelationPermanent::releaseLock(thread_db* tdbb)
{
	if (rel_partners_lock)
		LCK_release(tdbb, rel_partners_lock);

	rel_gc_lock.forcedRelease(tdbb);

	for (auto* index : rel_indices)
	{
		if (index)
			index->releaseLocks(tdbb);
	}
}


IndexVersion::IndexVersion(MemoryPool& p, Cached::Index* idp)
	: perm(idp)
{ }

void IndexVersion::destroy(thread_db* tdbb, IndexVersion* idv)
{
	if (idv->idv_expression_statement)
		idv->idv_expression_statement->release(tdbb);

	if (idv->idv_condition_statement)
		idv->idv_condition_statement->release(tdbb);

	delete idv;
}


void jrd_rel::releaseTriggers(thread_db* tdbb, bool destroy)
{
	for (int n = 1; n < TRIGGER_MAX; ++n)
	{
		rel_triggers[n].release(tdbb, destroy);
	}
}

void Triggers::release(thread_db* tdbb, bool destroy)
{
/***********************************************
 *
 *      M E T _ r e l e a s e _ t r i g g e r s
 *
 ***********************************************
 *
 * Functional description
 *      Release a possibly null vector of triggers.
 *      If triggers are still active let someone
 *      else do the work.
 *
 **************************************/
	if (destroy)
	{
		Triggers::destroy(tdbb, this);
		return;
	}
}

Lock* RelationPermanent::createLock(thread_db* tdbb, lck_t lckType, bool noAst)
{
	return createLock(tdbb, getPool(), lckType, noAst);
}

Lock* RelationPermanent::createLock(thread_db* tdbb, MemoryPool& pool, lck_t lckType, bool noAst)
{
	const USHORT relLockLen = getRelLockKeyLength();

	Lock* lock = FB_NEW_RPT(pool, relLockLen)
		Lock(tdbb, relLockLen, lckType, lckType == LCK_relation ? (void*)this : (void*)&rel_gc_lock);
	getRelLockKey(tdbb, lock->getKeyPtr());

	lock->lck_type = lckType;
	switch (lckType)
	{
	case LCK_relation:
		break;

	case LCK_rel_gc:
		lock->lck_ast = noAst ? nullptr : GCLock::ast;
		break;

	default:
		fb_assert(false);
	}

	return lock;
}

void RelationPermanent::addFormat(Format* fmt)
{
	MutexLockGuard g(rel_formats_grow, FB_FUNCTION);

	rel_formats.grow(fmt->fmt_version + 1, true);
	rel_formats.writeAccessor()->value(fmt->fmt_version) = fmt;
}


void GCLock::blockingAst()
{
	/****
	 SR - gc forbidden, awaiting moment to re-establish SW lock
	 SW - gc allowed, usual state
	 PW - gc allowed to the one connection only
	****/

	Database* dbb = gcLck->lck_dbb;

	AsyncContextHolder tdbb(dbb, FB_FUNCTION);

	unsigned oldFlags = gcFlags.load(std::memory_order_acquire);
	do
	{
		fb_assert(oldFlags & GC_locked);
		if (!(oldFlags & GC_locked)) // work already done synchronously ?
			return;
	} while (!gcFlags.compare_exchange_weak(oldFlags, oldFlags | GC_blocking,
										  std::memory_order_release, std::memory_order_acquire));

	if (oldFlags & GC_counterMask)
		return;

	if (oldFlags & GC_disabled)
	{
		// someone acquired EX lock

		fb_assert(gcLck->lck_id);
		fb_assert(gcLck->lck_physical == LCK_SR);

		LCK_release(tdbb, gcLck);
		gcFlags.fetch_and(~(GC_disabled | GC_blocking | GC_locked));
	}
	else
	{
		// someone acquired PW lock

		fb_assert(gcLck->lck_id);
		fb_assert(gcLck->lck_physical == LCK_SW);

		gcFlags.fetch_or(GC_disabled);
		downgrade(tdbb);
	}
}

void GCLock::checkGuard(unsigned flags)
{
	if (flags & GC_guardBit)
	{
		fatal_exception::raiseFmt
			("Overflow or underflow when changing GC lock atomic counter (guard bit set), flags %08x", flags);
	}
}

bool GCLock::acquire(thread_db* tdbb, int wait)
{
	unsigned oldFlags = gcFlags.load(std::memory_order_acquire);
	for(;;)
	{
		if (oldFlags & (GC_blocking | GC_disabled))		// lock should not be obtained
			return false;

		const unsigned newFlags = oldFlags + 1;
		checkGuard(newFlags);

		if (!gcFlags.compare_exchange_weak(oldFlags, newFlags, std::memory_order_release, std::memory_order_acquire))
			continue;

		if (oldFlags & GC_locked)			// lock was already taken when we checked flags
			return true;

		if (!(oldFlags & GC_counterMask))	// we must take lock
			break;

		// undefined state - someone else is taking/releasing a lock right now:
		oldFlags = gcFlags.fetch_sub(1, std::memory_order_acquire);	// decrement our lock counter,
		checkGuard(oldFlags - 1);
		Thread::yield();											// wait a bit
		oldFlags = gcFlags.load(std::memory_order_acquire);			// and retry
	}

	// We incremented counter from 0 to 1 - take care about lck
	if (!gcLck)
		gcLck = gcRel->createLock(tdbb, LCK_rel_gc, false);

	fb_assert(!gcLck->lck_id);

	ThreadStatusGuard temp_status(tdbb);

	bool ret;
	if (oldFlags & GC_disabled)
		ret = LCK_lock(tdbb, gcLck, LCK_SR, wait);
	else
	{
		ret = LCK_lock(tdbb, gcLck, LCK_SW, wait);
		if (ret)
		{
			gcFlags.fetch_or(GC_locked);
			return true;
		}

		oldFlags = gcFlags.fetch_or(GC_disabled, std::memory_order_seq_cst) | GC_disabled;
		ret = LCK_lock(tdbb, gcLck, LCK_SR, wait);
	}

	unsigned newFlags;
	do
	{
		newFlags = oldFlags - 1;
		checkGuard(newFlags);

		if (!ret)
			newFlags &= ~GC_disabled;

	} while (!gcFlags.compare_exchange_weak(oldFlags, newFlags, std::memory_order_release, std::memory_order_acquire));

	return false;
}

void GCLock::downgrade(thread_db* tdbb)
{
	unsigned oldFlags = gcFlags.load(std::memory_order_acquire);
	unsigned newFlags;
	do
	{
		newFlags = oldFlags - 1;
		checkGuard(newFlags);
	} while (!gcFlags.compare_exchange_weak(oldFlags, newFlags, std::memory_order_release, std::memory_order_acquire));

	if ((newFlags & GC_counterMask == 0) && (newFlags & GC_blocking))
	{
		fb_assert(newFlags & GC_locked);
		fb_assert(gcLck->lck_id);
		fb_assert(gcLck->lck_physical == LCK_SW);

		LCK_downgrade(tdbb, gcLck);

		oldFlags = newFlags;
		do
		{
			newFlags = oldFlags;
			if (gcLck->lck_physical != LCK_SR)
			{
				newFlags &= ~GC_disabled;
				if (gcLck->lck_physical < LCK_SR)
					newFlags &= ~GC_locked;
			}
			else
				newFlags |= GC_disabled;

			newFlags &= ~GC_blocking;
		} while (!gcFlags.compare_exchange_weak(oldFlags, newFlags, std::memory_order_release, std::memory_order_acquire));
	}
}

// violates rules of atomic counters - ok ONLY for ASSERT
unsigned GCLock::getSweepCount() const
{
	return gcFlags.load(std::memory_order_relaxed) & GC_counterMask;
}

bool GCLock::disable(thread_db* tdbb, int wait, Lock*& tempLock)
{
	ThreadStatusGuard temp_status(tdbb);

	// if validation is already running - go out
	unsigned oldFlags = gcFlags.load(std::memory_order_acquire);
	do {
		if (oldFlags & GC_disabled)
			return false;
	} while (gcFlags.compare_exchange_weak(oldFlags, oldFlags | GC_disabled,
										 std::memory_order_release, std::memory_order_acquire));

	int sleeps = -wait * 10;
	while (gcFlags.load(std::memory_order_relaxed) & GC_counterMask)
	{
		EngineCheckout cout(tdbb, FB_FUNCTION);
		Thread::sleep(100);

		if (wait < 0 && --sleeps == 0)
			break;
	}

	if (gcFlags.load(std::memory_order_relaxed) & GC_counterMask)
	{
		gcFlags.fetch_and(~GC_disabled);
		return false;
	}

	ensureReleased(tdbb);

	// we need no AST here
	if (!tempLock)
		tempLock = gcRel->createLock(tdbb, LCK_rel_gc, true);

	const bool ret = LCK_lock(tdbb, tempLock, LCK_PW, wait);
	if (!ret)
		gcFlags.fetch_and(~GC_disabled);

	return ret;
}

void GCLock::ensureReleased(thread_db* tdbb)
{
	unsigned oldFlags = gcFlags.load(std::memory_order_acquire);
	for (;;)
	{
		if (oldFlags & GC_locked)
		{
			if (!gcFlags.compare_exchange_strong(oldFlags, oldFlags & ~GC_locked,
											   std::memory_order_release, std::memory_order_acquire))
			{
				continue;
			}

			// exactly one who cleared GC_locked bit releases a lock
			LCK_release(tdbb, gcLck);
		}

		return;
	}
}

void GCLock::forcedRelease(thread_db* tdbb)
{
	gcFlags.fetch_and(~GC_locked);
	if (gcLck)
		LCK_release(tdbb, gcLck);
}

void GCLock::enable(thread_db* tdbb, Lock* tempLock)
{
	if (!(tempLock && tempLock->lck_id))
		return;

	fb_assert(gcFlags.load() & GC_disabled);

	ensureReleased(tdbb);

	LCK_convert(tdbb, tempLock, LCK_EX, LCK_WAIT);
	gcFlags.fetch_and(~GC_disabled);

	LCK_release(tdbb, tempLock);
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
	rel_last_free_pri_dp = rel_last_free_blb_dp = 0;
	rel_instance_id = 0;

	dpMap.clear();
	dpMapMark = 0;
}


/// IndexPermanent

[[noreturn]] void IndexPermanent::errIndexGone()
{
	fatal_exception::raise("Index is gone unexpectedly");
}

const QualifiedName& IndexPermanent::getName()
{
	static QualifiedName empty("");
	Cached::Index* element = static_cast<Cached::Index*>(this);
	auto* v = element->getVersioned(JRD_get_thread_data(), CacheFlag::AUTOCREATE | CacheFlag::MINISCAN);
	return v ? v->getName() : empty;
}

FB_UINT64 IndexPermanent::makeLockId(MetaId relId, MetaId indexId)
{
	if (MetadataCache::isLtt(relId))
		return ElementBase::NO_METALOCK;

	const int REL_ID_KEY_OFFSET = 16;
	return (FB_UINT64(relId) << REL_ID_KEY_OFFSET) + indexId;
}


/// jrd_rel

void jrd_rel::destroy(thread_db* tdbb, jrd_rel* rel)
{
    rel->releaseTriggers(tdbb, true);

	delete rel;
}

jrd_rel* jrd_rel::create(thread_db* tdbb, MemoryPool& pool, Cached::Relation* rlp)
{
	return FB_NEW_POOL(pool) jrd_rel(pool, rlp);
}

const char* jrd_rel::objectFamily(RelationPermanent* perm)
{
	return perm->isView() ? "view" : "table";
}

int jrd_rel::objectType()
{
	return obj_relation;
}

const Format* jrd_rel::currentFormat(thread_db* tdbb)
{
/**************************************
 *
 *      M E T _ c u r r e n t
 *
 **************************************
 *
 * Functional description
 *      Get the current format for a relation.  The current format is the
 *      format in which new records are to be stored.
 *
 **************************************/

	// dimitr:	rel_current_format may sometimes get out of sync,
	//			e.g. after DFW error raised during ALTER TABLE command.
	//			Thus it makes sense to validate it before usage and
	//			fetch the proper one if something is suspicious.

	if (rel_current_format && rel_current_format->fmt_version == rel_current_fmt)
		return rel_current_format;

	// Usually, format numbers start with one and they are present in RDB$FORMATS.
	// However, system tables have zero as their initial format and they don't have
	// any related records in RDB$FORMATS, instead their rel_formats[0] is initialized
	// directly (see ini.epp). Every other case of zero format number found for an already
	// scanned table must be catched here and investigated.
	fb_assert(rel_current_fmt || isSystem());

	if (!tdbb)
		tdbb = JRD_get_thread_data();

	rel_current_format = MET_format(tdbb, getPermanent(), rel_current_fmt);

	return rel_current_format;
}

bool jrd_rel::hash(thread_db* tdbb, sha512& digest)
{
	if (!(rel_current_fmt || isSystem()))
		return false;

	currentFormat(tdbb)->hash(digest);
	return true;
}

const Trigger* jrd_rel::findTrigger(const QualifiedName& trig_name) const
{
	for (int n = TRIGGER_PRE_STORE; n <= TRIGGER_POST_ERASE; ++n)
	{
		for (auto t : rel_triggers[n])
		{
			if (t->name == trig_name)
				return t;
		}
	}

	return nullptr;
}


// class Triggers

void Triggers::destroy(thread_db* tdbb, Triggers* trigs)
{
	for (auto t : trigs->triggers)
	{
		t->free(tdbb);
		delete t;
	}
	trigs->triggers.clear();
}


// class Trigger

void Trigger::free(thread_db* tdbb)
{
	extTrigger.reset();

	if (releaseInProgress || !statement)
		return;

	AutoSetRestore<bool> autoProgressFlag(&releaseInProgress, true);

	statement->release(tdbb);
	statement = nullptr;
}


// class DbTriggers

DbTriggersHeader::DbTriggersHeader(thread_db* tdbb, MemoryPool& p, MetaId& t, NoData)
	: Firebird::PermanentStorage(p),
	  type(t)
{ }

bool DbTriggersHeader::destroy(thread_db* tdbb, DbTriggersHeader* trigs)
{
	return false;
}

const QualifiedName& DbTriggersHeader::getName() const noexcept
{
	static const QualifiedName tnames[] = {
		QualifiedName("CONNECT"),
		QualifiedName("DISCONNECT"),
		QualifiedName("TRANSACTION START"),
		QualifiedName("TRANSACTION COMMIT"),
		QualifiedName("TRANSACTION ROLLBACK"),
		QualifiedName("DDL")
	};

	fb_assert(type < FB_NELEM(tnames));
	return tnames[type];
}

int DbTriggers::objectType()
{
	return obj_trigger;
}

#ifdef DEV_BUILD
GCLock::State GCLock::isGCEnabled() const
{
	if (getSweepCount() || gcRel->isSystem() || gcRel->isTemporary())
		return State::enabled;

	if (gcFlags & GC_disabled)
		return State::disabled;

	return State::unknown;
}
#endif //DEV_BUILD

