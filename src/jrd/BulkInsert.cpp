/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BulkInsert.cpp
 *	DESCRIPTION:	Support for faster inserting of bunch of rows into table
 *
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
 *  The Original Code was created by Vladyslav Khorsun for the
 *  Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "../jrd/BulkInsert.h"
#include "../jrd/sqz.h"
#include "../jrd/tra.h"
#include "../jrd/cch_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/ods_proto.h"


using namespace Firebird;
using namespace Ods;

static inline data_page* nextPage(data_page* ptr, ULONG pageSize)
{
	UCHAR* p = reinterpret_cast<UCHAR*>(ptr);
	p += pageSize;
	return reinterpret_cast<data_page*>(p);
};

namespace Jrd
{

// How many bytes per record should be reserved, see SPACE_FUDGE in dpm.epp
constexpr unsigned RESERVE_SIZE = (ROUNDUP(RHDF_SIZE, ODS_ALIGNMENT) + sizeof(data_page::dpg_repeat));

BulkInsert::BulkInsert(MemoryPool& pool, thread_db* tdbb, jrd_rel* relation) :
	PermanentStorage(pool),
	m_request(tdbb->getRequest())
{
	Database* dbb = tdbb->getDatabase();

	m_primary = FB_NEW_POOL(getPool())
		Buffer(getPool(), dbb->dbb_page_size, (dbb->dbb_flags & DBB_no_reserve) ? 0 : RESERVE_SIZE, true, relation);
}

void BulkInsert::putRecord(thread_db* tdbb, record_param* rpb, jrd_tra* transaction)
{
	m_primary->putRecord(tdbb, rpb, transaction);
}

RecordNumber BulkInsert::putBlob(thread_db* tdbb, blb* blob, Record* record)
{
	if (!m_other)
		m_other = FB_NEW_POOL(getPool()) Buffer(getPool(), m_primary->m_pageSize, 0, false, m_primary->m_relation);

	return m_other->putBlob(tdbb, blob, record);
}

void BulkInsert::flush(thread_db* tdbb)
{
	if (m_other)
		m_other->flush(tdbb);
	m_primary->flush(tdbb);
}


BulkInsert::Buffer::Buffer(MemoryPool& pool, ULONG pageSize, ULONG spaceReserve, bool primary, jrd_rel* relation) :
	PermanentStorage(pool),
	m_pageSize(pageSize),
	m_spaceReserve(spaceReserve),
	m_isPrimary(primary),
	m_relation(relation),
	m_buffer(pool),
	m_highPages(getPool())
{
}

void BulkInsert::Buffer::putRecord(thread_db* tdbb, record_param* rpb, jrd_tra* transaction)
{
	transaction->tra_flags |= TRA_write;

	rpb->rpb_b_page = 0;
	rpb->rpb_b_line = 0;
	rpb->rpb_flags = 0;
	rpb->rpb_transaction_nr = transaction->tra_number;

	Compressor dcc(getPool(), true, true, rpb->rpb_length, rpb->rpb_address);
	const ULONG packed = dcc.getPackedLength();

	const ULONG header_size = (transaction->tra_number > MAX_ULONG) ? RHDE_SIZE : RHD_SIZE;
	const ULONG max_data = m_pageSize - sizeof(data_page) - header_size;

	if (packed > max_data)
	{
		// store big
		fragmentRecord(tdbb, rpb, &dcc);
		return;
	}

	SLONG fill = (RHDF_SIZE - header_size) - packed;
	if (fill < 0)
		fill = 0;

	rhd* header = (rhd*) findSpace(tdbb, rpb, header_size + packed + fill);

	if (auto* record = rpb->rpb_record)
	{
		auto& stack = record->getPrecedence();
		while (stack.hasData())
			m_highPages[m_index].push(stack.pop());
	}

	rpb->rpb_flags &= ~rpb_not_packed;

	header->rhd_flags = rpb->rpb_flags;
	Ods::writeTraNum(header, rpb->rpb_transaction_nr, header_size);
	header->rhd_format = rpb->rpb_format_number;

	fb_assert(rpb->rpb_b_page == 0);
	header->rhd_b_page = rpb->rpb_b_page;
	header->rhd_b_line = rpb->rpb_b_line;

	if (!dcc.isPacked())
		header->rhd_flags |= rhd_not_packed;

	UCHAR* const data = (UCHAR*) header + header_size;

	dcc.pack(rpb->rpb_address, data);

	if (fill)
		memset(data + packed, 0, fill);
}

RecordNumber BulkInsert::Buffer::putBlob(thread_db* tdbb, blb* blob, Record* record)
{
	//fb_assert(blob->blb_relation == m_relation);

	Database* dbb = tdbb->getDatabase();

	// Figure out length of blob on page.  Remember that blob can either
	// be a clump of data or a vector of page pointers.
	USHORT length;
	const UCHAR* q;
	PageStack stack;
	Array<UCHAR> buffer;

	blob->storeToPage(&length, buffer, &q, &stack);

	// Locate space to store blob

	record_param rpb;
	//rpb.getWindow(tdbb).win_flags = 0; redundant.

	rpb.rpb_relation = m_relation;	//blob->blb_relation;
	rpb.rpb_transaction_nr = tdbb->getTransaction()->tra_number;
	rpb.rpb_flags = rpb_blob;

	blh* header = (blh*) findSpace(tdbb, &rpb, (BLH_SIZE + length));

	while (stack.hasData())
		m_highPages[m_index].push(stack.pop());

	header->blh_flags = rhd_blob;

	if (blob->blb_flags & BLB_stream)
		header->blh_flags |= rhd_stream_blob;

	if (blob->getLevel())
	{
		header->blh_flags |= rhd_large;
		markLarge();
	}

	blob->toPageHeader(header);

	if (length)
		memcpy(header->blh_page, q, length);

	if (record)
	{
		RelationPages* relPages = rpb.rpb_relation->getPages(tdbb);
		record->pushPrecedence(PageNumber(relPages->rel_pg_space_id, m_current->dpg_header.pag_pageno));
	}

	return rpb.rpb_number;
}

void BulkInsert::Buffer::fragmentRecord(thread_db* tdbb, record_param* rpb, Compressor* dcc)
{
	Database* dbb = tdbb->getDatabase();

	// Start compression from the end.

	const UCHAR* in = rpb->rpb_address + rpb->rpb_length;
	RelationPages* relPages = rpb->rpb_relation->getPages(tdbb);
	PageNumber prior(relPages->rel_pg_space_id, 0);

	// The last fragment should have rhd header because rhd_incomplete flag won't be set for it.
	// It's important for get_header() function which relies on rhd_incomplete flag to determine header size.
	ULONG header_size = RHD_SIZE;
	ULONG max_data = dbb->dbb_page_size - sizeof(data_page) - header_size;

	// Fill up data pages tail first until what's left fits on a single page.

	auto size = dcc->getPackedLength();
	fb_assert(size > max_data);

	do
	{
		// Allocate and format data page and fragment header

		data_page* page = (data_page*) DPM_allocate(tdbb, &rpb->getWindow(tdbb));

		page->dpg_header.pag_type = pag_data;
		page->dpg_header.pag_flags = dpg_orphan | dpg_full;
		page->dpg_relation = rpb->rpb_relation->getId();
		page->dpg_count = 1;

		const auto inLength = dcc->truncateTail(max_data);
		in -= inLength;
		size = dcc->getPackedLength();

		const Compressor tailDcc(tdbb, inLength, in);
		const auto tail_size = tailDcc.getPackedLength();
		fb_assert(tail_size <= max_data);

		// Cast to (rhdf*) but use only rhd fields for the last fragment
		rhdf* header = (rhdf*) &page->dpg_rpt[1];
		page->dpg_rpt[0].dpg_offset = (UCHAR*) header - (UCHAR*) page;
		page->dpg_rpt[0].dpg_length = tail_size + header_size;
		header->rhdf_flags = rhd_fragment;

		if (prior.getPageNum())
		{
			// This is not the last fragment
			header->rhdf_flags |= rhd_incomplete;
			header->rhdf_f_page = prior.getPageNum();
		}

		if (!tailDcc.isPacked())
			header->rhdf_flags |= rhd_not_packed;

		const auto out = (UCHAR*) header + header_size;
		tailDcc.pack(in, out);

		if (prior.getPageNum())
			CCH_precedence(tdbb, &rpb->getWindow(tdbb), prior);

		CCH_RELEASE(tdbb, &rpb->getWindow(tdbb));
		prior = rpb->getWindow(tdbb).win_page;

		// Other fragments except the last one should have rhdf header
		header_size = RHDF_SIZE;
		max_data = dbb->dbb_page_size - sizeof(data_page) - header_size;
	} while (size > max_data);

	// What's left fits on a page. Store it somewhere.

	const auto inLength = in - rpb->rpb_address;

	rhdf* header = (rhdf*) findSpace(tdbb, rpb, RHDF_SIZE + size);

	rpb->rpb_flags &= ~rpb_not_packed;

	header->rhdf_flags = rhd_incomplete | rhd_large | rpb->rpb_flags;
	Ods::writeTraNum(header, rpb->rpb_transaction_nr, RHDF_SIZE);
	header->rhdf_format = rpb->rpb_format_number;
	header->rhdf_b_page = rpb->rpb_b_page;
	header->rhdf_b_line = rpb->rpb_b_line;
	header->rhdf_f_page = prior.getPageNum();
	header->rhdf_f_line = 0;

	if (!dcc->isPacked())
		header->rhdf_flags |= rhd_not_packed;

	dcc->pack(rpb->rpb_address, header->rhdf_data);

	markLarge();

	m_highPages[m_index].push(prior);
}

void BulkInsert::Buffer::markLarge()
{
	m_current->dpg_header.pag_flags |= dpg_large;
}

UCHAR* BulkInsert::Buffer::findSpace(thread_db* tdbb, record_param* rpb, USHORT size)
{
	// record (with header) size, aligned up to ODS_ALIGNMENT
	const ULONG aligned = ROUNDUP(size, ODS_ALIGNMENT);

	// size to allocate
	const ULONG alloc = aligned + sizeof(data_page::dpg_repeat);

	// already used slots
	const ULONG used = (m_current ? m_current->dpg_count : 0);

	if (alloc + m_spaceReserve * (used + 1) > m_freeSpace)
	{
		if (m_current)
		{
			m_current->dpg_header.pag_flags |= dpg_full;

			// Get next reserved page, or reserve a new set of pages.

			UCHAR* const ptr = reinterpret_cast<UCHAR*>(nextPage(m_current, m_pageSize));
			if (ptr + m_pageSize < m_buffer.end())
			{
				m_current = reinterpret_cast<data_page*>(ptr);
				m_index++;
			}
			else
			{
				flush(tdbb);
				m_current = nullptr;
			}
		}

		if (!m_current)
		{
			m_current = allocatePages(tdbb);
			m_index = 0;
		}

		m_current->dpg_header.pag_flags |= (m_isPrimary ? dpg_swept : dpg_secondary);
		m_freeSpace = m_pageSize - sizeof(data_page) + sizeof(data_page::dpg_repeat);

		m_highPages[m_index].push(PageNumber(TRANS_PAGE_SPACE, rpb->rpb_transaction_nr));
	}

	fb_assert(alloc <= m_freeSpace);

	data_page::dpg_repeat* index = m_current->dpg_rpt + m_current->dpg_count;
	index->dpg_length = size;

	index->dpg_offset = (m_current->dpg_count > 0) ? index[-1].dpg_offset : m_pageSize;
	index->dpg_offset -= aligned;

	m_current->dpg_count++;
	m_freeSpace -= alloc;

	Database* dbb = tdbb->getDatabase();
	rpb->rpb_number.setValue(((SINT64) m_current->dpg_sequence) * dbb->dbb_max_records + m_current->dpg_count - 1);

	return reinterpret_cast<UCHAR*>(m_current) + index->dpg_offset;
}

data_page* BulkInsert::Buffer::allocatePages(thread_db* tdbb)
{
	Database* dbb = tdbb->getDatabase();
	RelationPages* relPages = m_relation->getPages(tdbb);

	WIN window(relPages->rel_pg_space_id, 0);

	const auto reserved = DPM_reserve_pages(tdbb, m_relation, &window);

	if (!m_pages || m_reserved != reserved)
	{
		m_pages = reinterpret_cast<data_page*> (m_buffer.getAlignedBuffer(m_pageSize * reserved, ODS_ALIGNMENT));
		m_highPages.resize(reserved);
		m_reserved = reserved;
	}

	fb_assert(m_reserved == reserved);

	auto dpage = reinterpret_cast<data_page*>(window.win_buffer);

	// format data pages in the buffer
	auto ptr = m_pages;
	for (auto i = 0; i < m_reserved; i++)
	{
		*ptr = *dpage;

		ptr->dpg_header.pag_pageno += i;
		ptr->dpg_sequence += i;

		ptr = nextPage(ptr, m_pageSize);
	}

	CCH_RELEASE(tdbb, &window);

	m_current = m_pages;
	return m_current;
}

void BulkInsert::Buffer::flush(thread_db* tdbb)
{
	if (!m_current)
		return;

	Database* dbb = tdbb->getDatabase();
	RelationPages* relPages = m_relation->getPages(tdbb);

	const ULONG pp_sequence = m_current->dpg_sequence / dbb->dbb_dp_per_pp;

	// copy buffered data into buffers in page cache
	m_current = m_pages;
	for (auto i = 0; i < m_reserved; i++)
	{
		if (m_current->dpg_count == 0)
			break;

		win dpWindow(relPages->rel_pg_space_id, m_current->dpg_header.pag_pageno);

		auto dpage = CCH_FETCH(tdbb, &dpWindow, LCK_write, pag_data);

		while (m_highPages[i].hasData())
			CCH_precedence(tdbb, &dpWindow, m_highPages[i].pop());

		CCH_MARK(tdbb, &dpWindow);

		// don't overwrite pag_scn
		m_current->dpg_header.pag_scn = dpage->pag_scn;
		memcpy(dpage, m_current, m_pageSize);

		CCH_RELEASE(tdbb, &dpWindow);

		if (m_isPrimary)
			tdbb->bumpStats(RecordStatType::INSERTS, m_relation->getId(), m_current->dpg_count);

		m_current = nextPage(m_current, m_pageSize);
	}

	win ppWindow(relPages->rel_pg_space_id, (*relPages->rel_pages)[pp_sequence]);
	pointer_page* ppage = (pointer_page*) CCH_FETCH(tdbb, &ppWindow, LCK_write, pag_pointer);

	m_current = m_pages;
	for (auto i = 0; i < m_reserved; i++)
	{
		if (m_current->dpg_count == 0)
			break;

		CCH_precedence(tdbb, &ppWindow, m_current->dpg_header.pag_pageno + i);
		m_current = nextPage(m_current, m_pageSize);
	}

	m_current = m_pages;

	CCH_MARK(tdbb, &ppWindow);

	UCHAR* bits = (UCHAR*) (ppage->ppg_page + dbb->dbb_dp_per_pp);
	const USHORT firstSlot = m_current->dpg_sequence % dbb->dbb_dp_per_pp;

	for (USHORT slot = firstSlot; slot < firstSlot + m_reserved; slot++)
	{
		PPG_DP_BIT_CLEAR(bits, slot, ppg_dp_reserved);

		if (m_current->dpg_count > 0)
		{
			PPG_DP_BIT_CLEAR(bits, slot, ppg_dp_empty);

			if (m_isPrimary)
			{
				PPG_DP_BIT_SET(bits, slot, ppg_dp_swept);
				PPG_DP_BIT_CLEAR(bits, slot, ppg_dp_secondary);
			}
			else
			{
				PPG_DP_BIT_CLEAR(bits, slot, ppg_dp_swept);
				PPG_DP_BIT_SET(bits, slot, ppg_dp_secondary);
			}

			if (m_current->dpg_header.pag_flags & dpg_full)
				PPG_DP_BIT_SET(bits, slot, ppg_dp_full);

			if (m_current->dpg_header.pag_flags & dpg_large)
				PPG_DP_BIT_SET(bits, slot, ppg_dp_large);
		}

		m_current = nextPage(m_current, m_pageSize);
	}

	CCH_RELEASE(tdbb, &ppWindow);
}

};	// namespace Jrd
