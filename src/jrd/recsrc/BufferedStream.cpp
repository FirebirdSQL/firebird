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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// --------------------------
// Data access: record buffer
// --------------------------

BufferedStream::BufferedStream(CompilerScratch* csb, RecordSource* next)
	: BaseBufferedStream(csb),
	  m_next(next),
	  m_map(csb->csb_pool)
{
	fb_assert(m_next);

	m_impure = csb->allocImpure<Impure>();

	StreamList streams;
	m_next->findUsedStreams(streams);

	Array<dsc> fields;

	for (StreamList::iterator i = streams.begin(); i != streams.end(); ++i)
	{
		const StreamType stream = *i;
		CompilerScratch::csb_repeat* const tail = &csb->csb_rpt[stream];

		UInt32Bitmap::Accessor accessor(tail->csb_fields);

		if (accessor.getFirst())
		{
			do {
				const USHORT id = (USHORT) accessor.current();
				const Format* const format = tail->csb_format; // CMP_format(tdbb, csb, stream);
				const dsc* const desc = &format->fmt_desc[id];
				m_map.add(FieldMap(FieldMap::REGULAR_FIELD, stream, id));
				fields.add(*desc);
			} while (accessor.getNext());
		}
	}

	dsc desc;

	for (StreamList::iterator i = streams.begin(); i != streams.end(); ++i)
	{
		const StreamType stream = *i;

		desc.makeInt64(0);
		m_map.add(FieldMap(FieldMap::TRANSACTION_ID, stream, 0));
		fields.add(desc);

		desc.makeInt64(0);
		m_map.add(FieldMap(FieldMap::DBKEY_NUMBER, stream, 0));
		fields.add(desc);
	}

	for (StreamList::iterator i = streams.begin(); i != streams.end(); ++i)
	{
		const StreamType stream = *i;

		desc.makeText(1, CS_BINARY);
		m_map.add(FieldMap(FieldMap::DBKEY_VALID, stream, 0));
		fields.add(desc);
	}

	const FB_SIZE_T count = fields.getCount();
	Format* const format = Format::newFormat(csb->csb_pool, count);
	format->fmt_length = FLAG_BYTES(count);

	for (FB_SIZE_T i = 0; i < count; i++)
	{
		dsc& desc = format->fmt_desc[i] = fields[i];

		if (desc.dsc_dtype >= dtype_aligned)
			format->fmt_length = FB_ALIGN(format->fmt_length, type_alignments[desc.dsc_dtype]);

		desc.dsc_address = (UCHAR*)(IPTR) format->fmt_length;
		format->fmt_length += desc.dsc_length;
	}

	m_format = format;
}

void BufferedStream::internalOpen(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_mustread;

	m_next->open(tdbb);

	delete impure->irsb_buffer;
	MemoryPool& pool = *tdbb->getDefaultPool();
	impure->irsb_buffer = FB_NEW_POOL(pool) RecordBuffer(pool, m_format);

	impure->irsb_position = 0;
}

void BufferedStream::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		delete impure->irsb_buffer;
		impure->irsb_buffer = NULL;

		m_next->close(tdbb);
	}
}

bool BufferedStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	dsc from, to;

	Record* const buffer_record = impure->irsb_buffer->getTempRecord();

	if (impure->irsb_flags & irsb_mustread)
	{
		if (!m_next->getRecord(tdbb))
		{
			// ASF: There is nothing more to read, so remove irsb_mustread flag.
			// That's important if m_next is reused in another stream and our caller
			// relies on this BufferedStream being non-lazy, like WindowedStream does.
			impure->irsb_flags &= ~irsb_mustread;
			return false;
		}

		buffer_record->nullify();

		// Assign the fields to the record to be stored
		for (FB_SIZE_T i = 0; i < m_map.getCount(); i++)
		{
			const FieldMap& map = m_map[i];

			record_param* const rpb = &request->req_rpb[map.map_stream];
			Record* const record = rpb->rpb_record;

			if (map.map_type == FieldMap::REGULAR_FIELD)
			{
				if (!EVL_field(rpb->rpb_relation, record, map.map_id, &from))
					continue;
			}

			buffer_record->clearNull(i);

			if (!EVL_field(rpb->rpb_relation, buffer_record, (USHORT) i, &to))
				fb_assert(false);

			switch (map.map_type)
			{
			case FieldMap::REGULAR_FIELD:
				MOV_move(tdbb, &from, &to);
				break;

			case FieldMap::TRANSACTION_ID:
				*reinterpret_cast<SINT64*>(to.dsc_address) = rpb->rpb_transaction_nr;
				break;

			case FieldMap::DBKEY_NUMBER:
				*reinterpret_cast<SINT64*>(to.dsc_address) = rpb->rpb_number.getValue();
				break;

			case FieldMap::DBKEY_VALID:
				*to.dsc_address = (UCHAR) rpb->rpb_number.isValid();
				break;

			default:
				fb_assert(false);
			}
		}

		// Put the record into the buffer
		impure->irsb_buffer->store(buffer_record);
	}
	else
	{
		// Read the record from the buffer
		if (!impure->irsb_buffer->fetch(impure->irsb_position, buffer_record))
			return false;

		StreamType stream = INVALID_STREAM;

		// Assign fields back to their original streams
		for (FB_SIZE_T i = 0; i < m_map.getCount(); i++)
		{
			const FieldMap& map = m_map[i];

			record_param* const rpb = &request->req_rpb[map.map_stream];
			jrd_rel* const relation = rpb->rpb_relation;

			if (relation &&
				!relation->rel_file &&
				!relation->rel_view_rse &&
				!relation->isVirtual())
			{
				rpb->rpb_runtime_flags |= RPB_refetch;
			}

			if (map.map_stream != stream)
			{
				stream = map.map_stream;

				// See SortedStream::mapData() for explanations why we need
				// to upgrade the record format

				if (relation && !rpb->rpb_number.isValid())
					VIO_record(tdbb, rpb, MET_current(tdbb, relation), tdbb->getDefaultPool());
			}

			Record* const record = rpb->rpb_record;
			record->reset();

			if (!EVL_field(relation, buffer_record, (USHORT) i, &from))
			{
				fb_assert(map.map_type == FieldMap::REGULAR_FIELD);
				record->setNull(map.map_id);
				continue;
			}

			switch (map.map_type)
			{
			case FieldMap::REGULAR_FIELD:
				{
					EVL_field(relation, record, map.map_id, &to);
					MOV_move(tdbb, &from, &to);
					record->clearNull(map.map_id);
				}
				break;

			case FieldMap::TRANSACTION_ID:
				rpb->rpb_transaction_nr = *reinterpret_cast<SINT64*>(from.dsc_address);
				break;

			case FieldMap::DBKEY_NUMBER:
				rpb->rpb_number.setValue(*reinterpret_cast<SINT64*>(from.dsc_address));
				break;

			case FieldMap::DBKEY_VALID:
				rpb->rpb_number.setValid(*from.dsc_address != 0);
				break;

			default:
				fb_assert(false);
			}
		}
	}

	impure->irsb_position++;
	return true;
}

bool BufferedStream::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

bool BufferedStream::lockRecord(thread_db* tdbb) const
{
	return m_next->lockRecord(tdbb);
}

void BufferedStream::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_next);
}

void BufferedStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
{
	if (detailed)
	{
		string extras;
		extras.printf(" (record length: %" ULONGFORMAT")", m_format->fmt_length);

		plan += printIndent(++level) + "Record Buffer" + extras;
	}

	if (recurse)
		m_next->print(tdbb, plan, detailed, level, recurse);
}

void BufferedStream::markRecursive()
{
	m_next->markRecursive();
}

void BufferedStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

void BufferedStream::invalidateRecords(jrd_req* request) const
{
	m_next->invalidateRecords(request);
}

void BufferedStream::nullRecords(thread_db* tdbb) const
{
	m_next->nullRecords(tdbb);
}

void BufferedStream::locate(thread_db* tdbb, FB_UINT64 position) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// If we haven't fetched and cached the underlying stream completely, do it now
	if (impure->irsb_flags & irsb_mustread)
	{
		while (this->getRecord(tdbb))
			; // no-op
		fb_assert(!(impure->irsb_flags & irsb_mustread));
	}

	impure->irsb_position = position;
}

FB_UINT64 BufferedStream::getCount(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// If we haven't fetched and cached the underlying stream completely, do it now
	if (impure->irsb_flags & irsb_mustread)
	{
		while (this->getRecord(tdbb))
			; // no-op
		fb_assert(!(impure->irsb_flags & irsb_mustread));
	}

	return impure->irsb_buffer ? impure->irsb_buffer->getCount() : 0;
}
