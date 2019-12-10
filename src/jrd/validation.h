/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		validation.h
 *	DESCRIPTION:	Validation and garbage collection
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
 */

#ifndef JRD_VALIDATION_H
#define JRD_VALIDATION_H

#include "firebird.h"
#include "fb_types.h"

#include "../common/classes/array.h"
#include "../common/SimilarToRegex.h"
#include "../jrd/ods.h"
#include "../jrd/cch.h"
#include "../jrd/sbm.h"
#include "../jrd/RecordNumber.h"


namespace Firebird
{
class UtilSvc;
}

namespace Jrd
{

class Attachment;
class Database;
class jrd_rel;
class thread_db;


// Validation/garbage collection/repair control block

class Validation
{
public:
	// vdr_flags

	static const USHORT VDR_online = 0x01;		// online validation (no exclusive attachment)
	static const USHORT VDR_update = 0x02;		// fix simple things
	static const USHORT VDR_repair = 0x04;		// fix non-simple things (-mend)
	static const USHORT VDR_records = 0x08;		// Walk all records
	static const USHORT VDR_partial = 0x10;		// Walk only (some) relations

private:

	enum FETCH_CODE
	{
		fetch_ok,
		//fetch_checksum,
		fetch_type,
		fetch_duplicate
	};

	enum RTN
	{
		rtn_ok,
		rtn_corrupt,
		rtn_eof
	};

	enum VAL_ERRORS
	{
		VAL_PAG_WRONG_TYPE			= 0,
		VAL_PAG_CHECKSUM_ERR		= 1,
		VAL_PAG_DOUBLE_ALLOC		= 2,
		VAL_PAG_IN_USE				= 3,
		VAL_PAG_ORPHAN				= 4,
		VAL_BLOB_INCONSISTENT		= 5,
		VAL_BLOB_CORRUPT			= 6,
		VAL_BLOB_TRUNCATED			= 7,
		VAL_REC_CHAIN_BROKEN		= 8,
		VAL_DATA_PAGE_CONFUSED		= 9,
		VAL_DATA_PAGE_LINE_ERR		= 10,
		VAL_INDEX_PAGE_CORRUPT		= 11,
		VAL_P_PAGE_LOST				= 12,
		VAL_P_PAGE_INCONSISTENT		= 13,
		VAL_REC_DAMAGED				= 14,
		VAL_REC_BAD_TID				= 15,
		VAL_REC_FRAGMENT_CORRUPT	= 16,
		VAL_REC_WRONG_LENGTH		= 17,
		VAL_INDEX_ROOT_MISSING		= 18,
		VAL_TIP_LOST				= 19,
		VAL_TIP_LOST_SEQUENCE		= 20,
		VAL_TIP_CONFUSED			= 21,
		VAL_REL_CHAIN_ORPHANS		= 22,
		VAL_INDEX_MISSING_ROWS		= 23,
		VAL_INDEX_ORPHAN_CHILD		= 24,
		VAL_INDEX_CYCLE				= 25,
		VAL_SCNS_PAGE_INCONSISTENT	= 26,
		VAL_PAG_WRONG_SCN			= 27,
		VAL_BLOB_UNKNOWN_LEVEL		= 28,
		VAL_INDEX_BAD_LEFT_SIBLING	= 29,
		VAL_INDEX_MISSES_NODE		= 30,
		VAL_PIP_WRONG_MIN			= 31,
		VAL_PIP_WRONG_EXTENT		= 32,
		VAL_PIP_WRONG_USED			= 33,
		VAL_P_PAGE_WRONG_BITS		= 34,
		VAL_DATA_PAGE_ISNT_IN_PIP   = 35,
		VAL_DATA_PAGE_SLOT_NOT_FOUND= 36,
		VAL_DATA_PAGE_SLOT_BAD_VAL  = 37,
		VAL_DATA_PAGE_HASNO_PP      = 38,
		VAL_DATA_PAGE_SEC_PRI		= 39,

		VAL_MAX_ERROR				= 40
	};

	struct MSG_ENTRY
	{
		bool error;
		UCHAR info_item;
		const TEXT* msg;
	};

	static const MSG_ENTRY vdr_msg_table[VAL_MAX_ERROR];

	thread_db* vdr_tdbb;
	ULONG vdr_max_page;
	USHORT vdr_flags;
	int vdr_errors;
	int vdr_warns;
	int vdr_fixed;
	TraNumber vdr_max_transaction;
	FB_UINT64 vdr_rel_backversion_counter;	// Counts slots w/rhd_chain
	PageBitmap* vdr_backversion_pages;      // 1 bit per visited table page
	FB_UINT64 vdr_rel_chain_counter;		// Counts chains w/rdr_chain
	PageBitmap* vdr_chain_pages;    // 1 bit per visited record chain page
	RecordBitmap* vdr_rel_records;			// 1 bit per valid record
	RecordBitmap* vdr_idx_records;			// 1 bit per index item
	PageBitmap* vdr_page_bitmap;
	ULONG vdr_err_counts[VAL_MAX_ERROR];

	Firebird::UtilSvc* vdr_service;
	Firebird::AutoPtr<Firebird::SimilarToRegex> vdr_tab_incl;
	Firebird::AutoPtr<Firebird::SimilarToRegex> vdr_tab_excl;
	Firebird::AutoPtr<Firebird::SimilarToRegex> vdr_idx_incl;
	Firebird::AutoPtr<Firebird::SimilarToRegex> vdr_idx_excl;
	int vdr_lock_tout;
	void checkDPinPP(jrd_rel *relation, SLONG page_number);
	void checkDPinPIP(jrd_rel *relation, SLONG page_number);

public:
	explicit Validation(thread_db*, Firebird::UtilSvc* uSvc = NULL);
	~Validation();

	bool run(thread_db* tdbb, USHORT flags);
	ULONG getInfo(UCHAR item);

private:
	struct UsedBdb
	{
		explicit UsedBdb(BufferDesc* _bdb) : bdb(_bdb), count(1) {}

		BufferDesc* bdb;
		int count;

		static const ULONG generate(const UsedBdb& p)
		{
			return p.bdb ? p.bdb->bdb_page.getPageNum() : 0;
		}
	};

	typedef Firebird::SortedArray<
				UsedBdb,
				Firebird::EmptyStorage<UsedBdb>,
				ULONG,
				UsedBdb> UsedBdbs;

	UsedBdbs vdr_used_bdbs;

	void cleanup();
	RTN corrupt(int, const jrd_rel*, ...);
	FETCH_CODE fetch_page(bool mark, ULONG, USHORT, WIN*, void*);
	void release_page(WIN*);
	void garbage_collect();

	void parse_args(thread_db*);
	void output(const char*, ...);

	RTN walk_blob(jrd_rel*, const Ods::blh*, USHORT, RecordNumber);
	RTN walk_chain(jrd_rel*, const Ods::rhd*, RecordNumber);
	RTN walk_data_page(jrd_rel*, ULONG, ULONG, UCHAR&);
	void walk_database();
	void walk_generators();
	void walk_header(ULONG);
	RTN walk_index(jrd_rel*, Ods::index_root_page&, USHORT);
	void walk_pip();
	RTN walk_pointer_page(jrd_rel*, ULONG);
	RTN walk_record(jrd_rel*, const Ods::rhd*, USHORT, RecordNumber, bool);
	RTN walk_relation(jrd_rel*);
	RTN walk_root(jrd_rel*);
	RTN walk_scns();
	RTN walk_tip(TraNumber);
};

} // namespace Jrd

#endif
