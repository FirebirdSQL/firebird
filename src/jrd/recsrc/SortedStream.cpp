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
#include "../jrd/jrd.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/optimizer/Optimizer.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// -----------------------------
// Data access: external sorting
// -----------------------------

SortedStream::SortedStream(CompilerScratch* csb, RecordSource* next, SortMap* map)
	: RecordSource(csb),
	  m_next(next),
	  m_map(map)
{
	fb_assert(m_next && m_map);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = next->getCardinality();

	if (m_map->flags & FLAG_PROJECT)
		m_cardinality *= DEFAULT_SELECTIVITY;
}

void SortedStream::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	// Get rid of the old sort areas if this request has been used already.
	// Null the pointer before calling init() because it may throw.
	delete impure->irsb_sort;
	impure->irsb_sort = nullptr;

	impure->irsb_sort = init(tdbb);
}

void SortedStream::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		delete impure->irsb_sort;
		impure->irsb_sort = nullptr;

		m_next->close(tdbb);
	}
}

bool SortedStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	UCHAR* const data = getData(tdbb);

	if (!data)
		return false;

	mapData(tdbb, request, data);

	return true;
}

bool SortedStream::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

WriteLockResult SortedStream::lockRecord(thread_db* tdbb, bool skipLocked) const
{
	return m_next->lockRecord(tdbb, skipLocked);
}

void SortedStream::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_next);
}

void SortedStream::print(thread_db* tdbb, string& plan,
						 bool detailed, unsigned level, bool recurse) const
{
	if (detailed)
	{
		string extras;
		extras.printf(" (record length: %" ULONGFORMAT", key length: %" ULONGFORMAT")",
					  m_map->length, m_map->keyLength);

		if (m_map->flags & FLAG_REFETCH)
			plan += printIndent(++level) + "Refetch";

		plan += printIndent(++level) +
			((m_map->flags & FLAG_PROJECT) ? "Unique Sort" : "Sort") + extras;
		printOptInfo(plan);

		if (recurse)
			m_next->print(tdbb, plan, true, level, recurse);
	}
	else
	{
		level++;
		plan += "SORT (";
		m_next->print(tdbb, plan, false, level, recurse);
		plan += ")";
	}
}

void SortedStream::markRecursive()
{
	m_next->markRecursive();
}

void SortedStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

void SortedStream::invalidateRecords(Request* request) const
{
	m_next->invalidateRecords(request);
}

void SortedStream::nullRecords(thread_db* tdbb) const
{
	m_next->nullRecords(tdbb);
}

Sort* SortedStream::init(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	m_next->open(tdbb);
	ULONG records = 0;

	// Initialize for sort. If this is really a project operation,
	// establish a callback routine to reject duplicate records.

	AutoPtr<Sort> scb(FB_NEW_POOL(request->req_sorts.getPool())
		Sort(tdbb->getDatabase(), &request->req_sorts,
			 m_map->length, m_map->keyItems.getCount(), m_map->keyItems.getCount(),
			 m_map->keyItems.begin(),
			 ((m_map->flags & FLAG_PROJECT) ? rejectDuplicate : nullptr), 0));

	// Pump the input stream dry while pushing records into sort. For
	// each record, map all fields into the sort record. The reverse
	// mapping is done in get_sort().

	dsc to, temp;

	while (m_next->getRecord(tdbb))
	{
		records++;

		// "Put" a record to sort. Actually, get the address of a place
		// to build a record.

		UCHAR* data = nullptr;
		scb->put(tdbb, reinterpret_cast<ULONG**>(&data));

		// Zero out the sort key. This solves a multitude of problems.

		memset(data, 0, m_map->length);

		// Loop thru all field (keys and hangers on) involved in the sort.
		// Be careful to null field all unused bytes in the sort key.

		const SortMap::Item* const end_item = m_map->items.begin() + m_map->items.getCount();
		for (const SortMap::Item* item = m_map->items.begin(); item < end_item; item++)
		{
			to = item->desc;
			to.dsc_address = data + (IPTR) to.dsc_address;
			bool flag = false;
			dsc* from = nullptr;

			if (item->node)
			{
				from = EVL_expr(tdbb, request, item->node);
				if (request->req_flags & req_null)
					flag = true;
			}
			else
			{
				from = &temp;

				record_param* const rpb = &request->req_rpb[item->stream];

				if (item->fieldId < 0)
				{
					switch (item->fieldId)
					{
					case ID_TRANS:
						*reinterpret_cast<SINT64*>(to.dsc_address) = rpb->rpb_transaction_nr;
						break;
					case ID_DBKEY:
						*reinterpret_cast<SINT64*>(to.dsc_address) = rpb->rpb_number.getValue();
						break;
					case ID_DBKEY_VALID:
						*to.dsc_address = (UCHAR) rpb->rpb_number.isValid();
						break;
					default:
						fb_assert(false);
					}
					continue;
				}

				if (!EVL_field(rpb->rpb_relation, rpb->rpb_record, item->fieldId, from))
					flag = true;
			}

			*(data + item->flagOffset) = flag ? TRUE : FALSE;

			if (!flag)
			{
				// If an INTL string is moved into the key portion of the sort record,
				// then we want to sort by language dependent order

				if (IS_INTL_DATA(&item->desc) && isKey(&item->desc))
				{
					INTL_string_to_key(tdbb, INTL_INDEX_TYPE(&item->desc), from, &to,
						(m_map->flags & FLAG_UNIQUE ? INTL_KEY_UNIQUE : INTL_KEY_SORT));
				}
				else
				{
					MOV_move(tdbb, from, &to);
				}
			}
		}
	}

	scb->sort(tdbb);

	return scb.release();
}

bool SortedStream::compareKeys(const UCHAR* p, const UCHAR* q) const
{
	if (!memcmp(p, q, m_map->keyLength))
		return true;

	if (!(m_map->flags & FLAG_KEY_VARY))
		return false;

	// Binary-distinct varying length string keys may in fact be equal.
	// Re-check the keys at the higher level. See CORE-4909.

	fb_assert(m_map->keyItems.getCount() % 2 == 0);
	const USHORT count = m_map->keyItems.getCount() / 2;
	thread_db* tdbb = JRD_get_thread_data();

	for (USHORT i = 0; i < count; i++)
	{
		const SortMap::Item* const item = &m_map->items[i];

		const UCHAR flag1 = *(p + item->flagOffset);
		const UCHAR flag2 = *(q + item->flagOffset);

		if (flag1 != flag2)
			return false;

		if (!flag1)
		{
			dsc desc1 = item->desc;
			desc1.dsc_address = const_cast<UCHAR*>(p) + (IPTR) desc1.dsc_address;

			dsc desc2 = item->desc;
			desc2.dsc_address = const_cast<UCHAR*>(q) + (IPTR) desc2.dsc_address;

			if (MOV_compare(tdbb, &desc1, &desc2))
				return false;
		}
	}

	return true;
}

UCHAR* SortedStream::getData(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	ULONG* data = nullptr;
	impure->irsb_sort->get(tdbb, &data);

	return reinterpret_cast<UCHAR*>(data);
}

void SortedStream::mapData(thread_db* tdbb, Request* request, UCHAR* data) const
{
	StreamType stream = INVALID_STREAM;
	dsc from, to;
	StreamList refetchStreams;

	for (const auto& item : m_map->items)
	{
		const auto flag = (*(data + item.flagOffset) == TRUE);
		from = item.desc;
		from.dsc_address = data + (IPTR) from.dsc_address;

		if (item.node && !nodeIs<FieldNode>(item.node))
			continue;

		// Some fields may have volatile keys, so that their value
		// can be possibly changed before or during the sorting.
		// We should ignore such an item, because there is a later field
		// in the item list that contains the original value to send back
		// (or the record will be refetched as a whole).

		if (hasVolatileKey(&item.desc) && isKey(&item.desc))
			continue;

		const auto rpb = &request->req_rpb[item.stream];
		const auto relation = rpb->rpb_relation;
		const auto id = item.fieldId;

		if (id < 0)
		{
			switch (id)
			{
			case ID_TRANS:
				rpb->rpb_transaction_nr = *reinterpret_cast<SINT64*>(from.dsc_address);
				break;
			case ID_DBKEY:
				rpb->rpb_number.setValue(*reinterpret_cast<SINT64*>(from.dsc_address));
				break;
			case ID_DBKEY_VALID:
				rpb->rpb_number.setValid(*from.dsc_address != 0);
				break;
			default:
				fb_assert(false);
			}

			rpb->rpb_runtime_flags &= ~RPB_CLEAR_FLAGS;

			// If transaction ID is present, then fields from this stream are accessed.
			// So we need to refetch the stream, either immediately or on demand.
			const auto refetch = (id == ID_TRANS);

			if (refetch && relation &&
				!relation->rel_file &&
				!relation->rel_view_rse &&
				!relation->isVirtual())
			{
				if (m_map->flags & FLAG_REFETCH)
				{
					// Prepare this stream for an immediate refetch
					if (!refetchStreams.exist(item.stream))
						refetchStreams.add(item.stream);
				}
				else // delay refetch until really necessary
					rpb->rpb_runtime_flags |= RPB_refetch;
			}

			continue;
		}

		fb_assert(!(rpb->rpb_stream_flags & RPB_s_no_data));

		if (item.stream != stream)
		{
			stream = item.stream;

			// For the sake of prudence, set all record parameter blocks to contain
			// the most recent format. This will guarantee that all fields mapped
			// back to records have homes in the target record.

			// dimitr:	I've added the check for !isValid to ensure that we don't overwrite
			//			the format for an active rpb (i.e. the one having some record fetched).
			//			See CORE-3806 for example.
			// BEWARE:	This check depends on the fact that ID_DBKEY_VALID flags are stored
			//			*after* real fields and ID_TRANS / ID_DBKEY values.
			if (relation && !rpb->rpb_number.isValid())
				VIO_record(tdbb, rpb, MET_current(tdbb, relation), tdbb->getDefaultPool());
		}

		const auto record = rpb->rpb_record;
		record->reset();

		if (flag)
			record->setNull(id);
		else
		{
			EVL_field(relation, record, id, &to);
			MOV_move(tdbb, &from, &to);
			record->clearNull(id);
		}
	}

	// If necessary, refetch records from the underlying streams

	UCharBuffer keyBuffer;

	for (const auto stream : refetchStreams)
	{
		fb_assert(m_map->flags & FLAG_REFETCH);

		const auto rpb = &request->req_rpb[stream];
		const auto relation = rpb->rpb_relation;

		// Ensure the record is still in the most recent format
		const auto record =
			VIO_record(tdbb, rpb, MET_current(tdbb, relation), tdbb->getDefaultPool());

		// Set all fields to NULL if the stream was originally marked as invalid
		if (!rpb->rpb_number.isValid())
		{
			rpb->rpb_record->nullify();
			continue;
		}

		// Refetch the record to make sure all fields are present.
		// It should always succeed for SNAPSHOT and READ CONSISTENCY transactions.

		const auto transaction = tdbb->getTransaction();
		fb_assert(transaction);
		const auto selfTraNum = transaction->tra_number;

		const auto orgTraNum = rpb->rpb_transaction_nr;

		// Code underneath is a slightly customized version of VIO_refetch_record,
		// because in the case of deleted record followed by a commit we should
		// find the original version (or die trying).

		auto temp = *rpb;
		temp.rpb_record = nullptr;
		AutoPtr<Record> tempRecord;

		// If the primary record version disappeared, we cannot proceed

		if (!DPM_get(tdbb, &temp, LCK_read))
			Arg::Gds(isc_no_cur_rec).raise();

		tdbb->bumpRelStats(RuntimeStatistics::RECORD_RPT_READS, relation->rel_id);

		if (VIO_chase_record_version(tdbb, &temp, transaction, tdbb->getDefaultPool(), false, false))
		{
			if (!(temp.rpb_runtime_flags & RPB_undo_data))
				VIO_data(tdbb, &temp, tdbb->getDefaultPool());

			tempRecord = temp.rpb_record;

			VIO_copy_record(tdbb, relation, tempRecord, record);

			if (temp.rpb_transaction_nr == orgTraNum && orgTraNum != selfTraNum)
				continue; // we surely see the original record version
		}
		else if (!(temp.rpb_flags & rpb_deleted))
			Arg::Gds(isc_no_cur_rec).raise();

		if (temp.rpb_transaction_nr != selfTraNum)
		{
			// Ensure that somebody really touched this record
			fb_assert(temp.rpb_transaction_nr != orgTraNum);
			// and we discovered it in READ COMMITTED transaction
			fb_assert(transaction->tra_flags & TRA_read_committed);
			// and our transaction isn't READ CONSISTENCY one
			fb_assert(!(transaction->tra_flags & TRA_read_consistency));
		}

		// We have found a more recent record version. Unless it's a delete stub,
		// validate whether it's still suitable for the result set.

		if (!(temp.rpb_flags & rpb_deleted))
		{
			fb_assert(temp.rpb_length != 0);
			fb_assert(temp.rpb_address != nullptr);

			// Record can be safely returned only if it has no fields
			// acting as sort keys or they haven't been changed

			bool keysChanged = false;

			for (const auto& item : m_map->items)
			{
				// Stop comparing at the first non-key field (if any)

				if (!isKey(&item.desc))
					break;

				if (item.node && !nodeIs<FieldNode>(item.node))
					continue;

				if (item.stream != stream || item.fieldId < 0)
					continue;

				const auto null1 = (*(data + item.flagOffset) == TRUE);
				const auto null2 = !EVL_field(relation, temp.rpb_record, item.fieldId, &from);

				if (null1 != null2)
				{
					keysChanged = true;
					break;
				}

				if (!null1)
				{
					dsc sortDesc = item.desc;
					sortDesc.dsc_address += (IPTR) data;

					const dsc* recDesc = &from;

					if (IS_INTL_DATA(&item.desc))
					{
						// For an INTL string, compute the language dependent key

						to = item.desc;
						to.dsc_address = keyBuffer.getBuffer(to.dsc_length);
						memset(to.dsc_address, 0, to.dsc_length);

						INTL_string_to_key(tdbb, INTL_INDEX_TYPE(&item.desc), &from, &to,
							(m_map->flags & FLAG_UNIQUE ? INTL_KEY_UNIQUE : INTL_KEY_SORT));

						recDesc = &to;
					}

					if (MOV_compare(tdbb, &sortDesc, recDesc))
					{
						keysChanged = true;
						break;
					}
				}
			}

			if (!keysChanged)
				continue;
		}

		// If it's our transaction who updated/deleted this record, punt.
		// We don't know how to find the original record version.

		if (temp.rpb_transaction_nr == selfTraNum)
			Arg::Gds(isc_no_cur_rec).raise();

		// We have to find the original record version, sigh.
		// Scan version chain backwards until it's done.

		RuntimeStatistics::Accumulator backversions(tdbb, relation,
													RuntimeStatistics::RECORD_BACKVERSION_READS);

		while (temp.rpb_transaction_nr != orgTraNum)
		{
			// If the backversion has been garbage collected, punt.
			// We can do nothing in this situation.

			if (!temp.rpb_b_page)
				Arg::Gds(isc_no_cur_rec).raise();

			// Fetch the backversion. Punt if it's unexpectedly disappeared.

			temp.rpb_page = temp.rpb_b_page;
			temp.rpb_line = temp.rpb_b_line;

			if (!DPM_fetch(tdbb, &temp, LCK_read))
				Arg::Gds(isc_no_cur_rec).raise();

			VIO_data(tdbb, &temp, tdbb->getDefaultPool());

			tempRecord.reset(temp.rpb_record);

			++backversions;
		}

		VIO_copy_record(tdbb, relation, tempRecord, record);
	}
}
