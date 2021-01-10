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
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../jrd/tra.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/vio_proto.h"

#include "Savepoint.h"

using namespace Firebird;
using namespace Jrd;


// UndoItem implementation

UndoItem::UndoItem(jrd_tra* transaction, RecordNumber recordNumber, const Record* record)
	: m_number(recordNumber.getValue()), m_format(record->getFormat())
{
	fb_assert(m_format);
	m_offset = transaction->getUndoSpace()->allocateSpace(m_format->fmt_length);
	transaction->getUndoSpace()->write(m_offset, record->getData(), record->getLength());
}

Record* UndoItem::setupRecord(jrd_tra* transaction) const
{
	if (m_format)
	{
		Record* const record = transaction->getUndoRecord(m_format);
		transaction->getUndoSpace()->read(m_offset, record->getData(), record->getLength());
		return record;
	}

	return NULL;
}

void UndoItem::release(jrd_tra* transaction)
{
	if (m_format)
	{
		transaction->getUndoSpace()->releaseSpace(m_offset, m_format->fmt_length);
		m_format = NULL;
	}
}


// VerbAction implementation

void VerbAction::garbageCollectIdxLite(thread_db* tdbb, jrd_tra* transaction, SINT64 recordNumber,
									   VerbAction* nextAction, Record* goingRecord)
{
	// Clean up index entries and referenced BLOBs.
	// This routine uses smaller set of staying record than original VIO_garbage_collect_idx().
	//
	// Notes:
	//
	// This speed trick is possible only because btr.cpp:insert_node() allows duplicate nodes
	// which work as an index entry reference counter.

	record_param rpb;
	rpb.rpb_relation = vct_relation;
	rpb.rpb_number.setValue(recordNumber);
	rpb.rpb_record = NULL;
	rpb.getWindow(tdbb).win_flags = 0;
	rpb.rpb_transaction_nr = transaction->tra_number;

	Record* next_ver;
	AutoUndoRecord undo_next_ver(transaction->findNextUndo(this, vct_relation, recordNumber));
	AutoPtr<Record> real_next_ver;

	next_ver = undo_next_ver;

	if (!DPM_get(tdbb, &rpb, LCK_read))
		BUGCHECK(186);	// msg 186 record disappeared
	else
	{
		if (next_ver || (rpb.rpb_flags & rpb_deleted))
			CCH_RELEASE(tdbb, &rpb.getWindow(tdbb));
		else
		{
			VIO_data(tdbb, &rpb, transaction->tra_pool);
			next_ver = real_next_ver = rpb.rpb_record;
		}
	}

	if (rpb.rpb_transaction_nr != transaction->tra_number)
		BUGCHECK(185);	// msg 185 wrong record version

	Record* prev_ver = NULL;
	AutoUndoRecord undo_prev_ver;
	AutoPtr<Record> real_prev_ver;

	if (nextAction && nextAction->vct_undo && nextAction->vct_undo->locate(recordNumber))
	{
		prev_ver = undo_prev_ver = nextAction->vct_undo->current().setupRecord(transaction);
	}
	else if (rpb.rpb_b_page) // previous version exists and we have to find it in a hard way
	{
		record_param temp = rpb;
		temp.rpb_record = NULL;
		temp.rpb_page = rpb.rpb_b_page;
		temp.rpb_line = rpb.rpb_b_line;

		if (!DPM_fetch(tdbb, &temp, LCK_read))
			BUGCHECK(291); // Back version disappeared

		if (temp.rpb_flags & rpb_deleted)
			CCH_RELEASE(tdbb, &temp.getWindow(tdbb));
		else
			VIO_data(tdbb, &temp, transaction->tra_pool);

		prev_ver = real_prev_ver = temp.rpb_record;
	}

	RecordStack going, staying;
	going.push(goingRecord);

	if (prev_ver)
		staying.push(prev_ver);

	if (next_ver)
		staying.push(next_ver);

	IDX_garbage_collect(tdbb, &rpb, going, staying);
	BLB_garbage_collect(tdbb, going, staying, rpb.rpb_page, vct_relation);
}

void VerbAction::mergeTo(thread_db* tdbb, jrd_tra* transaction, VerbAction* nextAction)
{
	// Post bitmap of modified records and undo data to the next savepoint.
	//
	// Notes:
	//
	// If previous savepoint already touched a record, undo data must be dropped and
	// all BLOBs it refers to should be cleaned out because under no circumstances
	// this undo data can become an active record.

	// Merge undo records first

	if (vct_undo && vct_undo->getFirst())
	{
		do
		{
			UndoItem& item = vct_undo->current();

			if (item.hasData()) // this item wasn't released yet
			{
				const SINT64 recordNumber = item.generate(NULL, item);

				if (nextAction && !(RecordBitmap::test(nextAction->vct_records, recordNumber)))
				{
					if (!nextAction->vct_undo)
					{
						nextAction->vct_undo =
							FB_NEW_POOL(*transaction->tra_pool) UndoItemTree(*transaction->tra_pool);
						// We cannot just push whole current undo items list, some items still may be released
					}
					else
					{
						if (nextAction->vct_undo->locate(recordNumber))
						{
							// It looks like something went wrong on previous loop and undo record
							// was moved to next action but record bit in bitmap wasn't set
							fb_assert(false);
							item.clear();
							continue;
						}
					}

					nextAction->vct_undo->add(item);
					item.clear(); // Do not release undo data, it now belongs to next action
					continue;
				}

				// garbage cleanup and release
				// because going version for sure has all index entries successfully set up (in contrast with undo)
				// we can use lightweigth version of garbage collection without collection of full staying list

				AutoUndoRecord this_ver(item.setupRecord(transaction));

				garbageCollectIdxLite(tdbb, transaction, recordNumber, nextAction, this_ver);

				item.release(transaction);
			}
		} while (vct_undo->getNext());

		delete vct_undo;
		vct_undo = NULL;
	}

	// Now merge bitmap

	if (nextAction)
	{
		if (nextAction->vct_records)
		{
			RecordBitmap** bm_or = RecordBitmap::bit_or(&vct_records, &nextAction->vct_records);

			if (*bm_or == vct_records) // if next bitmap has been merged into this bitmap - swap them
			{
				RecordBitmap* temp = nextAction->vct_records;
				nextAction->vct_records = vct_records;
				vct_records = temp;
			}
		}
		else // just push current bitmap as is
		{
			nextAction->vct_records = vct_records;
			vct_records = NULL;
		}
	}

	release(transaction);
}

void VerbAction::undo(thread_db* tdbb, jrd_tra* transaction, bool preserveLocks, VerbAction* preserveAction)
{
	// Undo changes recorded for this verb action.
	// After that, clear the verb action and prepare it for later reuse.

	record_param rpb;
	rpb.rpb_relation = vct_relation;
	rpb.rpb_number.setValue(BOF_NUMBER);
	rpb.rpb_record = NULL;
	rpb.getWindow(tdbb).win_flags = 0;
	rpb.rpb_transaction_nr = transaction->tra_number;

	RecordBitmap::Accessor accessor(vct_records);
	if (accessor.getFirst())
	{
		do
		{
			rpb.rpb_number.setValue(accessor.current());

			const bool have_undo = vct_undo && vct_undo->locate(rpb.rpb_number.getValue());

			if (!DPM_get(tdbb, &rpb, LCK_read))
				BUGCHECK(186);	// msg 186 record disappeared

			if ((have_undo || preserveLocks) && !(rpb.rpb_flags & rpb_deleted))
				VIO_data(tdbb, &rpb, transaction->tra_pool);
			else
				CCH_RELEASE(tdbb, &rpb.getWindow(tdbb));

			if (rpb.rpb_transaction_nr != transaction->tra_number)
				BUGCHECK(185);	// msg 185 wrong record version

			if (!have_undo)
			{
				if (preserveLocks && rpb.rpb_b_page)
				{
					// Fetch previous record version and update in place current version with it
					record_param temp = rpb;
					temp.rpb_page = rpb.rpb_b_page;
					temp.rpb_line = rpb.rpb_b_line;
					temp.rpb_record = NULL;

					if (temp.rpb_flags & rpb_delta)
						fb_assert(temp.rpb_prior != NULL);
					else
						fb_assert(temp.rpb_prior == NULL);

					if (!DPM_fetch(tdbb, &temp, LCK_read))
						BUGCHECK(291);		// msg 291 cannot find record back version

					if (!(temp.rpb_flags & rpb_chained) || (temp.rpb_flags & (rpb_blob | rpb_fragment)))
						ERR_bugcheck_msg("invalid back version");

					VIO_data(tdbb, &temp, tdbb->getDefaultPool());

					Record* const save_record = rpb.rpb_record;
					if (rpb.rpb_flags & rpb_deleted)
						rpb.rpb_record = NULL;
					Record* const dead_record = rpb.rpb_record;

					VIO_update_in_place(tdbb, transaction, &rpb, &temp);

					if (dead_record)
					{
						rpb.rpb_record = NULL; // VIO_garbage_collect_idx will play with this record dirty tricks
						VIO_garbage_collect_idx(tdbb, transaction, &rpb, dead_record);
					}
					rpb.rpb_record = save_record;

					delete temp.rpb_record;

					if (preserveAction)
						RBM_SET(transaction->tra_pool, &preserveAction->vct_records, rpb.rpb_number.getValue());
				} 
				else
					VIO_backout(tdbb, &rpb, transaction);
			}
			else
			{
				AutoUndoRecord record(vct_undo->current().setupRecord(transaction));

				Record* const save_record = rpb.rpb_record;
				record_param new_rpb = rpb;

				if (rpb.rpb_flags & rpb_deleted)
					rpb.rpb_record = NULL;

				new_rpb.rpb_record = record;
				new_rpb.rpb_address = record->getData();
				new_rpb.rpb_length = record->getLength();
				new_rpb.rpb_flags = 0;

				Record* const dead_record = rpb.rpb_record;

				// This record will be in staying list twice. Ignorable overhead.
				VIO_update_in_place(tdbb, transaction, &rpb, &new_rpb);

				if (dead_record)
				{
					rpb.rpb_record = NULL; // VIO_garbage_collect_idx will play with this record dirty tricks
					VIO_garbage_collect_idx(tdbb, transaction, &rpb, dead_record);
				}

				rpb.rpb_record = save_record;
			}
		} while (accessor.getNext());

		delete rpb.rpb_record;
	}

	release(transaction);
}

void VerbAction::release(jrd_tra* transaction)
{
	// Release resources used by this verb action

	RecordBitmap::reset(vct_records);

	if (vct_undo)
	{
		if (vct_undo->getFirst())
		{
			do {
				vct_undo->current().release(transaction);
			} while (vct_undo->getNext());
		}

		delete vct_undo;
		vct_undo = NULL;
	}
}


// Savepoint implementation

VerbAction* Savepoint::createAction(jrd_rel* relation)
{
	// Create action for the given relation. If it already exists, just return.

	VerbAction* action = getAction(relation);

	if (!action)
	{
		if ( (action = m_freeActions) )
			m_freeActions = action->vct_next;
		else
			action = FB_NEW_POOL(*m_transaction->tra_pool) VerbAction();

		action->vct_next = m_actions;
		m_actions = action;

		action->vct_relation = relation;
	}

	return action;
}


void Savepoint::cleanupTempData()
{
	// Find all global temporary tables with DELETE ROWS action
	// and release their undo data

	for (VerbAction* action = m_actions; action; action = action->vct_next)
	{
		if (action->vct_relation->rel_flags & REL_temp_tran)
		{
			RecordBitmap::reset(action->vct_records);

			if (action->vct_undo)
			{
				if (action->vct_undo->getFirst())
				{
					do
					{
						action->vct_undo->current().release(m_transaction);
					} while (action->vct_undo->getNext());
				}

				delete action->vct_undo;
				action->vct_undo = NULL;
			}
		}
	}
}

Savepoint* Savepoint::rollback(thread_db* tdbb, Savepoint* prior, bool preserveLocks)
{
	// Undo changes made in this savepoint.
	// Perform index and BLOB cleanup if needed.
	// At the exit savepoint is clear and safe to reuse.

	jrd_tra* const old_tran = tdbb->getTransaction();

	try
	{
		DFW_delete_deferred(m_transaction, m_number);
		m_flags &= ~SAV_force_dfw;

		AutoSetRestoreFlag<ULONG> verbCleanupFlag(&tdbb->tdbb_flags, TDBB_verb_cleanup, true);

		tdbb->setTransaction(m_transaction);

		while (m_actions)
		{
			VerbAction* const action = m_actions;
			VerbAction* preserveAction = nullptr;

			if (preserveLocks && m_next)
				preserveAction = m_next->createAction(action->vct_relation);

			action->undo(tdbb, m_transaction, preserveLocks, preserveAction);

			m_actions = action->vct_next;
			action->vct_next = m_freeActions;
			m_freeActions = action;
		}

		tdbb->setTransaction(old_tran);
	}
	catch (const Exception& ex)
	{
		Arg::StatusVector error(ex);
		tdbb->setTransaction(old_tran);
		m_transaction->tra_flags |= TRA_invalidated;
		error.prepend(Arg::Gds(isc_savepoint_backout_err));
		error.raise();
	}

	return release(prior);
}

Savepoint* Savepoint::rollforward(thread_db* tdbb, Savepoint* prior)
{
	// Merge changes made in this savepoint into next one.
	// Perform index and BLOB cleanup if needed.
	// At the exit savepoint is clear and safe to reuse.

	jrd_tra* const old_tran = tdbb->getTransaction();

	try
	{
		// If the current to-be-cleaned-up savepoint is very big, and the next
		// level savepoint is the transaction level savepoint, then get rid of
		// the transaction level savepoint now (instead of after making the
		// transaction level savepoint very very big).

		if (m_next && m_next->isRoot() && this->isLarge())
		{
			fb_assert(!m_next->m_next); // check that transaction savepoint is the last in list
			// get rid of tx-level savepoint
			m_next->rollforward(tdbb);
			m_next = NULL;
		}

		// Cleanup/merge deferred work/event post

		if (m_actions || (m_flags & SAV_force_dfw))
		{
			DFW_merge_work(m_transaction, m_number, m_next ? m_next->m_number : 0);

			if (m_next && (m_flags & SAV_force_dfw))
				m_next->m_flags |= SAV_force_dfw;

			m_flags &= ~SAV_force_dfw;
		}

		tdbb->tdbb_flags |= TDBB_verb_cleanup;
		tdbb->setTransaction(m_transaction);

		while (m_actions)
		{
			VerbAction* const action = m_actions;
			VerbAction* nextAction = NULL;

			if (m_next)
			{
				nextAction = m_next->getAction(action->vct_relation);

				if (!nextAction) // next savepoint didn't touch this table yet - send whole action
				{
					m_actions = action->vct_next;
					action->vct_next = m_next->m_actions;
					m_next->m_actions = action;
					continue;
				}
			}

			// No luck, merge action in a slow way
			action->mergeTo(tdbb, m_transaction, nextAction);

			// and release it afterwards
			m_actions = action->vct_next;
			action->vct_next = m_freeActions;
			m_freeActions = action;
		}

		tdbb->setTransaction(old_tran);
		tdbb->tdbb_flags &= ~TDBB_verb_cleanup;
	}
	catch (...)
	{
		m_transaction->tra_flags |= TRA_invalidated;
		tdbb->setTransaction(old_tran);
		tdbb->tdbb_flags &= ~TDBB_verb_cleanup;
		throw;
	}

	// If the only remaining savepoint is the 'transaction-level' savepoint
	// that was started by TRA_start, then check if it hasn't grown out of
	// bounds yet.  If it has, then give up on this transaction-level savepoint.
	if (m_next && m_next->isRoot() && m_next->isLarge())
	{
		fb_assert(!m_next->m_next); // check that transaction savepoint is the last in list
		// get rid of tx-level savepoint
		m_next->rollforward(tdbb);
		m_next = NULL;
	}

	return release(prior);
}

bool Savepoint::isLarge() const
{
	// Returns whether the current savepoint is large enough (has many verbs posted).
	//
	// Notes:
	//
	// - This routine does not take into account the data allocated to 'vct_undo'.
	//   Why? Because this routine is used to estimate size of transaction-level
	//   savepoint and transaction-level savepoint may not contain undo data as it is
	//   always the first savepoint in transaction.
	//
	//  - We use U_IPTR, not ULONG to care of case when user savepoint gets very,
	//   very big on 64-bit machine. Its size may overflow 32 significant bits of
	//   ULONG in this case

	U_IPTR size = 0;

	// Iterate all tables changed under this savepoint

	for (VerbAction* action = m_actions; action; action = action->vct_next)
	{
		// Estimate size used for record backout bitmaps for this table

		if (action->vct_records)
		{
			size += action->vct_records->approxSize();

			if (size > SIZE_THRESHOLD)
				return true;
		}
	}

	return false;
}

Savepoint* Savepoint::release(Savepoint* prior)
{
	// Clear savepoint and prepare it for later reuse.
	// If prior savepoint is specified, relink its next pointer.
	// Return the next savepoint, if exists.

	m_flags = 0;
	m_count = 0;
	m_name = "";

	Savepoint* const next = m_next;

	if (prior)
	{
		fb_assert(prior != next);
		prior->m_next = next;
	}

	m_next = m_transaction->tra_save_free;
	m_transaction->tra_save_free = this;
	fb_assert(m_next != this);

	return next;
}


// AutoSavePoint implementation

AutoSavePoint::AutoSavePoint(thread_db* tdbb, jrd_tra* trans)
	: m_tdbb(tdbb), m_transaction(trans), m_number(0)
{
	const auto savepoint = trans->startSavepoint();
	m_number = savepoint->getNumber();
}

AutoSavePoint::~AutoSavePoint()
{
	if (m_number && !(m_tdbb->getDatabase()->dbb_flags & DBB_bugcheck))
	{
		fb_assert(m_transaction->tra_save_point);
		fb_assert(m_transaction->tra_save_point->getNumber() == m_number);
		m_transaction->rollbackSavepoint(m_tdbb);
	}
}

void AutoSavePoint::release()
{
	if (!m_number)
		return;

	fb_assert(m_transaction->tra_save_point);
	fb_assert(m_transaction->tra_save_point->getNumber() == m_number);
	m_transaction->rollforwardSavepoint(m_tdbb);
	m_number = 0;
}

void AutoSavePoint::rollback(bool preserveLocks)
{
	if (!m_number)
		return;

	fb_assert(m_transaction->tra_save_point);
	fb_assert(m_transaction->tra_save_point->getNumber() == m_number);
	m_transaction->rollbackSavepoint(m_tdbb, preserveLocks);
	m_number = 0;
}


// StableCursorSavePoint implementation

StableCursorSavePoint::StableCursorSavePoint(thread_db* tdbb, jrd_tra* trans, bool start)
	: m_tdbb(tdbb), m_transaction(trans), m_number(0)
{
	if (!start)
		return;

	if (trans->tra_flags & TRA_system)
		return;

	if (!trans->tra_save_point)
		return;

	const auto savepoint = trans->startSavepoint();
	m_number = savepoint->getNumber();
}


void StableCursorSavePoint::release()
{
	if (!m_number)
		return;

	while (m_transaction->tra_save_point &&
		m_transaction->tra_save_point->getNumber() >= m_number)
	{
		m_transaction->rollforwardSavepoint(m_tdbb);
	}

	m_number = 0;
}
