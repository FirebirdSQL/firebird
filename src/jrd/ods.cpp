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
 *  Copyright (c) 2005 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/ods.h"
#include "../jrd/ods_proto.h"

using namespace Firebird;

namespace
{
	const FB_SIZE_T NEXT_INDEX = 0;
	const FB_SIZE_T OIT_INDEX = 1;
	const FB_SIZE_T OAT_INDEX = 2;
	const FB_SIZE_T OST_INDEX = 3;
}

namespace Ods {

bool isSupported(const header_page* hdr)
{
	USHORT majorVersion = hdr->hdr_ods_version;
	USHORT minorVersion = hdr->hdr_ods_minor;
	const bool isFirebird = (majorVersion & ODS_FIREBIRD_FLAG);
	majorVersion &= ~ODS_FIREBIRD_FLAG;

	if (!isFirebird)
		return false;

	// Support current ODS of the engine
	if (majorVersion == ODS_VERSION &&
		minorVersion >= ODS_RELEASED &&
		minorVersion <= ODS_CURRENT)
	{
		return true;
	}

	// Do not support anything else
	return false;
}

ULONG bytesBitPIP(ULONG page_size)
{
	return static_cast<ULONG>(page_size - offsetof(page_inv_page, pip_bits[0]));
}

ULONG pagesPerPIP(ULONG page_size)
{
	return bytesBitPIP(page_size) * 8;
}

ULONG pagesPerSCN(ULONG page_size)
{
	return pagesPerPIP(page_size) / BITS_PER_LONG;
}

// We must ensure that pagesPerSCN items can fit into scns_page::scn_pages array.
// We can't use fb_assert() here in ods.h so it is placed at pag.cpp

ULONG maxPagesPerSCN(ULONG page_size)
{
	return static_cast<ULONG>((page_size - offsetof(scns_page, scn_pages[0])) / sizeof(((scns_page*)NULL)->scn_pages));
}

ULONG transPerTIP(ULONG page_size)
{
	return static_cast<ULONG>((page_size - offsetof(tx_inv_page, tip_transactions[0])) * 4);
}

ULONG gensPerPage(ULONG page_size)
{
	return static_cast<ULONG>((page_size - offsetof(generator_page, gpg_values[0])) /
		sizeof(((generator_page*) NULL)->gpg_values));
}

ULONG dataPagesPerPP(ULONG page_size)
{
	// Compute the number of data pages per pointer page. Each data page requires
	// a 32 bit pointer (BITS_PER_LONG) and a 8 bit control field (PPG_DP_BITS_NUM).
	// Also, don't allow extent of data pages (8 pages) to cross PP boundary to
	// simplify code a bit.

	ULONG ret = static_cast<ULONG>((page_size - offsetof(pointer_page, ppg_page[0])) * 8 / (BITS_PER_LONG + PPG_DP_BITS_NUM));
	return ret & (~7);
}

ULONG maxRecsPerDP(ULONG page_size)
{
	// Compute the number of records that can fit on a page using the
	// size of the record index (dpb_repeat) and a record header.  This
	// gives an artificially high number, reducing the density of db_keys.

	ULONG max_records = static_cast<ULONG>(
		(page_size - sizeof(data_page)) / (sizeof(data_page::dpg_repeat) + offsetof(rhd, rhd_data[0])));

	// Artificially reduce density of records to test high bits of record number
	// max_records = 32000;

	// Optimize record numbers for new 64-bit sparse bitmap implementation
	// We need to measure if it is beneficial from performance point of view.
	// Price is slightly reduced density of record numbers, but for
	// ODS11 it doesn't matter because record numbers are 40-bit.
	// Benefit is ~1.5 times smaller sparse bitmaps on average and faster bitmap iteration.

	//max_records = FB_ALIGN(max_records, 64);

	return max_records;
}

ULONG maxIndices(ULONG page_size)
{
	// Compute the number of index roots that will fit on an index root page,
	// assuming that each index has only one key

	return static_cast<ULONG>((page_size - offsetof(index_root_page, irt_rpt[0])) /
		(sizeof(index_root_page::irt_repeat) + sizeof(irtd)));
}

Firebird::string pagtype(UCHAR type)
{
	// Print pretty name for database page type

	const char* nameArray[pag_max + 1] = {
		"purposely undefined",
		"database header",
		"page inventory",
		"transaction inventory",
		"pointer",
		"data",
		"index root",
		"index B-tree",
		"blob",
		"generators",
		"SCN inventory"
	};

	Firebird::string rc;
	if (type < FB_NELEM(nameArray))
		rc = nameArray[type];
	else
		rc.printf("unknown (%d)", type);

	return rc;
}

TraNumber getNT(const header_page* page)
{
	return (TraNumber) page->hdr_tra_high[NEXT_INDEX] << BITS_PER_LONG | page->hdr_next_transaction;
}

TraNumber getOIT(const header_page* page)
{
	return (TraNumber) page->hdr_tra_high[OIT_INDEX] << BITS_PER_LONG | page->hdr_oldest_transaction;
}

TraNumber getOAT(const header_page* page)
{
	return (TraNumber) page->hdr_tra_high[OAT_INDEX] << BITS_PER_LONG | page->hdr_oldest_active;
}

TraNumber getOST(const header_page* page)
{
	return (TraNumber) page->hdr_tra_high[OST_INDEX] << BITS_PER_LONG | page->hdr_oldest_snapshot;
}

void writeNT(header_page* page, TraNumber number)
{
	page->hdr_next_transaction = (ULONG) (number & MAX_ULONG);
	const SLONG high_word = number >> BITS_PER_LONG;
	fb_assert(high_word <= MAX_USHORT);
	page->hdr_tra_high[NEXT_INDEX] = (USHORT) high_word;
}

void writeOIT(header_page* page, TraNumber number)
{
	page->hdr_oldest_transaction = (ULONG) (number & MAX_ULONG);
	const SLONG high_word = number >> BITS_PER_LONG;
	fb_assert(high_word <= MAX_USHORT);
	page->hdr_tra_high[OIT_INDEX] = (USHORT) high_word;
}

void writeOAT(header_page* page, TraNumber number)
{
	page->hdr_oldest_active = (ULONG) (number & MAX_ULONG);
	const SLONG high_word = number >> BITS_PER_LONG;
	fb_assert(high_word <= MAX_USHORT);
	page->hdr_tra_high[OAT_INDEX] = (USHORT) high_word;
}

void writeOST(header_page* page, TraNumber number)
{
	page->hdr_oldest_snapshot = (ULONG) (number & MAX_ULONG);
	const SLONG high_word = number >> BITS_PER_LONG;
	fb_assert(high_word <= MAX_USHORT);
	page->hdr_tra_high[OST_INDEX] = (USHORT) high_word;
}

TraNumber getTraNum(const void* ptr)
{
	rhd* const record = (rhd*) ptr;
	USHORT high_word = 0;

	if (record->rhd_flags & rhd_long_tranum)
	{
		high_word = (record->rhd_flags & rhd_incomplete) ?
			((rhdf*) ptr)->rhdf_tra_high : ((rhde*) ptr)->rhde_tra_high;
	}

	return ((TraNumber) high_word << BITS_PER_LONG) | record->rhd_transaction;
}

void writeTraNum(void* ptr, TraNumber number, FB_SIZE_T header_size)
{
	rhd* const record = (rhd*) ptr;

	record->rhd_transaction = (ULONG) (number & MAX_ULONG);

	const SLONG high_word = number >> BITS_PER_LONG;
	fb_assert(high_word <= MAX_USHORT);

	if (high_word)
		record->rhd_flags |= rhd_long_tranum;

	if (record->rhd_flags & rhd_long_tranum)
	{
		if (record->rhd_flags & rhd_incomplete)
		{
			fb_assert(header_size == RHDF_SIZE);
			((rhdf*) ptr)->rhdf_tra_high = (USHORT) high_word;
		}
		else
		{
			fb_assert(header_size == RHDE_SIZE);
			((rhde*) ptr)->rhde_tra_high = (USHORT) high_word;
		}
	}
}

AttNumber getAttID(const header_page* page)
{
	return ((AttNumber)page->hdr_att_high << BITS_PER_LONG | page->hdr_attachment_id);
}

void writeAttID(header_page* page, AttNumber number)
{
	page->hdr_att_high = number >> BITS_PER_LONG;
	page->hdr_attachment_id = (ULONG) (number & MAX_ULONG);
}

} // namespace
