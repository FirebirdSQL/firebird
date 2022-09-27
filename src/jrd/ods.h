/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ods.h
 *	DESCRIPTION:	On disk structure definitions
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
 *
 * 2000.11.29 Patrick J. P. Griffin: fixed bug SF #116733
 *	Add typedef struct gpg to properly document the layout of the generator page
 * 2002.08.26 Dmitry Yemanov: minor ODS change (new indices on system tables)
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef JRD_ODS_H
#define JRD_ODS_H

#include "../jrd/RecordNumber.h"
#include "../common/classes/fb_string.h"

// This macro enables the ability of the engine to connect to databases
// from ODS 8 up to the latest.  If this macro is undefined, the engine
// only opens a database of the current ODS major version.

//#define ODS_8_TO_CURRENT

/**********************************************************************
**
** NOTE:
**
**   ODS 5 was shipped with version 3.3 but no longer supported
**   ODS 6 and ODS 7 never went out the door
**   ODS 8 was shipped with version 4.0
**   ODS 9 was going to be shipped with version 4.5 but never was released,
**         thus it became v5.0's ODS.
**   ODS 10 was shipped with IB version 6.0
**   Here the Firebird history begins:
**   ODS 10.0 is for FB1.0 and ODS 10.1 is for FB1.5.
**   ODS 11.0 is for FB2.0, ODS11.1 is for FB2.1 and ODS11.2 is for FB2.5.
**   ODS 12.0 is for FB3, ODS 13.0 is for FB4.
**
***********************************************************************/

// ODS major version -- major versions are not compatible

//const USHORT ODS_VERSION6	= 6;		// on-disk structure as of v3.0
//const USHORT ODS_VERSION7	= 7;		// new on disk structure for fixing index bug
const USHORT ODS_VERSION8	= 8;		// new btree structure to support pc semantics
const USHORT ODS_VERSION9	= 9;		// btree leaf pages are always propagated up
const USHORT ODS_VERSION10	= 10;		// V6.0 features. SQL delimited idetifier,
										// SQLDATE, and 64-bit exact numeric type
const USHORT ODS_VERSION11	= 11;		// Firebird 2.x features
const USHORT ODS_VERSION12	= 12;		// Firebird 3.x features
const USHORT ODS_VERSION13	= 13;		// Firebird 4.x features

// ODS minor version -- minor versions ARE compatible, but may be
// increasingly functional.  Add new minor versions, but leave previous
// names intact

// Minor versions for ODS 6

//const USHORT ODS_GRANT6		= 1;	// adds fields for field level grant
//const USHORT ODS_INTEGRITY6	= 2;	// adds fields for referential integrity
//const USHORT ODS_FUNCTIONS6	= 3;	// modifies type of RDB$MODULE_NAME field
//const USHORT ODS_SQLNAMES6	= 4;	// permits SQL security on > 27 SCHAR names
//const USHORT ODS_CURRENT6		= 4;

// Minor versions for ODS 7

//const USHORT ODS_FUNCTIONS7	= 1;	// modifies type of RDB$MODULE_NAME field
//const USHORT ODS_SQLNAMES7	= 2;	// permits SQL security on > 27 SCHAR names
//const USHORT ODS_CURRENT7		= 2;

// Minor versions for ODS 8

//const USHORT ODS_CASCADE_RI8	= 1;	// permits cascading referential integrity
										// ODS 8.2 is the same as ODS 8.1
//const USHORT ODS_CURRENT8		= 2;

// Minor versions for ODS 9

//const USHORT ODS_CURRENT_9_0	= 0;	// SQL roles & Index garbage collection
//const USHORT ODS_SYSINDEX9	= 1;	// Index on RDB$CHECK_CONSTRAINTS (RDB$TRIGGER_NAME)
//const USHORT ODS_CURRENT9		= 1;

// Minor versions for ODS 10

//const USHORT ODS_CURRENT10_0	= 0;	// V6.0 features. SQL delimited identifier,
										// SQLDATE, and 64-bit exact numeric type
//const USHORT ODS_SYSINDEX10	= 1;	// New system indices
//const USHORT ODS_CURRENT10	= 1;

// Minor versions for ODS 11

//const USHORT ODS_CURRENT11_0	= 0;	// Firebird 2.0 features
//const USHORT ODS_CURRENT11_1	= 1;	// Firebird 2.1 features
//const USHORT ODS_CURRENT11_2	= 2;	// Firebird 2.5 features
//const USHORT ODS_CURRENT11	= 2;

// Minor versions for ODS 12

const USHORT ODS_CURRENT12_0	= 0;	// Firebird 3.0 features
const USHORT ODS_CURRENT12		= 0;

// Minor versions for ODS 13

const USHORT ODS_CURRENT13_0	= 0;	// Firebird 4.0 features
const USHORT ODS_CURRENT13_1	= 1;	// Firebird 4.1 features
const USHORT ODS_CURRENT13		= 1;

// useful ODS macros. These are currently used to flag the version of the
// system triggers and system indices in ini.e

inline USHORT ENCODE_ODS(USHORT major, USHORT minor)
{
	return ((major << 4) | minor);
}

const USHORT ODS_8_0		= ENCODE_ODS(ODS_VERSION8, 0);
const USHORT ODS_8_1		= ENCODE_ODS(ODS_VERSION8, 1);
const USHORT ODS_9_0		= ENCODE_ODS(ODS_VERSION9, 0);
const USHORT ODS_9_1		= ENCODE_ODS(ODS_VERSION9, 1);
const USHORT ODS_10_0		= ENCODE_ODS(ODS_VERSION10, 0);
const USHORT ODS_10_1		= ENCODE_ODS(ODS_VERSION10, 1);
const USHORT ODS_11_0		= ENCODE_ODS(ODS_VERSION11, 0);
const USHORT ODS_11_1		= ENCODE_ODS(ODS_VERSION11, 1);
const USHORT ODS_11_2		= ENCODE_ODS(ODS_VERSION11, 2);
const USHORT ODS_12_0		= ENCODE_ODS(ODS_VERSION12, 0);
const USHORT ODS_13_0		= ENCODE_ODS(ODS_VERSION13, 0);
const USHORT ODS_13_1		= ENCODE_ODS(ODS_VERSION13, 1);

const USHORT ODS_FIREBIRD_FLAG = 0x8000;

// Decode ODS version to Major and Minor parts. The 4 LSB's are minor and
// the next 11 bits are major version number. The highest significant bit
// is the Firebird database flag.
inline USHORT DECODE_ODS_MAJOR(USHORT ods_version)
{
	return ((ods_version & 0x7FF0) >> 4);
}

inline USHORT DECODE_ODS_MINOR(USHORT ods_version)
{
	return (ods_version & 0x000F);
}

// Set current ODS major and minor version

const USHORT ODS_VERSION = ODS_VERSION13;		// Current ODS major version -- always
												// the highest.

const USHORT ODS_RELEASED = ODS_CURRENT13_0;	// The lowest stable minor version
												// number for this ODS_VERSION!

const USHORT ODS_CURRENT = ODS_CURRENT13;		// The highest defined minor version
												// number for this ODS_VERSION!

const USHORT ODS_CURRENT_VERSION = ODS_13_1;	// Current ODS version in use which includes
												// both major and minor ODS versions!


//const USHORT USER_REL_INIT_ID_ODS8	= 31;	// ODS < 9 ( <= 8.2)
const USHORT USER_DEF_REL_INIT_ID		= 128;	// ODS >= 9


// Page types

const SCHAR pag_undefined		= 0;
const SCHAR pag_header			= 1;		// Database header page
const SCHAR pag_pages			= 2;		// Page inventory page
const SCHAR pag_transactions	= 3;		// Transaction inventory page
const SCHAR pag_pointer			= 4;		// Pointer page
const SCHAR pag_data			= 5;		// Data page
const SCHAR pag_root			= 6;		// Index root page
const SCHAR pag_index			= 7;		// Index (B-tree) page
const SCHAR pag_blob			= 8;		// Blob data page
const SCHAR pag_ids				= 9;		// Gen-ids
const SCHAR pag_scns			= 10;		// SCN's inventory page
const SCHAR pag_max				= 10;		// Max page type

// Pre-defined page numbers

const ULONG HEADER_PAGE		= 0;
const ULONG FIRST_PIP_PAGE	= 1;
const ULONG FIRST_SCN_PAGE	= 2;

// Page size limits

const USHORT MIN_PAGE_SIZE		= 4096;
const USHORT MAX_PAGE_SIZE		= 32768;

const USHORT DEFAULT_PAGE_SIZE	= 8192;

namespace Ods {

// Crypt page by type

const bool pag_crypt_page[pag_max + 1] = {false, false, false,
										  false, false, true,	// data
										  false, true, true,	// index, blob
										  true, false};			// generators

// pag_flags for any page type

const UCHAR crypted_page	= 0x80;		// Page on disk is encrypted (in memory cache it always isn't)

// Basic page header

struct pag
{
	UCHAR pag_type;
	UCHAR pag_flags;
	USHORT pag_reserved;		// not used but anyway present because of alignment rules
	ULONG pag_generation;
	ULONG pag_scn;
	ULONG pag_pageno;			// for validation
};

static_assert(sizeof(struct pag) == 16, "struct pag size mismatch");
static_assert(offsetof(struct pag, pag_type) == 0, "pag_type offset mismatch");
static_assert(offsetof(struct pag, pag_flags) == 1, "pag_flags offset mismatch");
static_assert(offsetof(struct pag, pag_reserved) == 2, "pag_reserved offset mismatch");
static_assert(offsetof(struct pag, pag_generation) == 4, "pag_generation offset mismatch");
static_assert(offsetof(struct pag, pag_scn) == 8, "pag_scn offset mismatch");
static_assert(offsetof(struct pag, pag_pageno) == 12, "pag_pageno offset mismatch");

typedef pag* PAG;


// Blob page

struct blob_page
{
	pag blp_header;
	ULONG blp_lead_page;		// First page of blob (for redundancy only)
	ULONG blp_sequence;			// Sequence within blob
	USHORT blp_length;			// Bytes on page
	USHORT blp_pad;				// Unused
	ULONG blp_page[1];			// Page number if level 1
};

static_assert(sizeof(struct blob_page) == 32, "struct blob_page size mismatch");
static_assert(offsetof(struct blob_page, blp_header) == 0, "blp_header offset mismatch");
static_assert(offsetof(struct blob_page, blp_lead_page) == 16, "blp_lead_page offset mismatch");
static_assert(offsetof(struct blob_page, blp_sequence) == 20, "blp_sequence offset mismatch");
static_assert(offsetof(struct blob_page, blp_length) == 24, "blp_length offset mismatch");
static_assert(offsetof(struct blob_page, blp_pad) == 26, "blp_pag offset mismatch");
static_assert(offsetof(struct blob_page, blp_page) == 28, "blp_page offset mismatch");

#define BLP_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::blob_page, blp_page[0]))

// pag_flags
const UCHAR blp_pointers	= 0x01;		// Blob pointer page, not data page


// B-tree page ("bucket")
struct btree_page
{
	pag btr_header;
	ULONG btr_sibling;			// right sibling page
	ULONG btr_left_sibling;		// left sibling page
	SLONG btr_prefix_total;		// sum of all prefixes on page
	USHORT btr_relation;		// relation id for consistency
	USHORT btr_length;			// length of data in bucket
	UCHAR btr_id;				// index id for consistency
	UCHAR btr_level;			// index level (0 = leaf)
	USHORT btr_jump_interval;	// interval between jump nodes
	USHORT btr_jump_size;		// size of the jump table
	UCHAR btr_jump_count;		// number of jump nodes
	UCHAR btr_nodes[1];
};

static_assert(sizeof(struct btree_page) == 40, "struct btree_page size mismatch");
static_assert(offsetof(struct btree_page, btr_header) == 0, "btr_header offset mismatch");
static_assert(offsetof(struct btree_page, btr_sibling) == 16, "btr_sibling offset mismatch");
static_assert(offsetof(struct btree_page, btr_left_sibling) == 20, "btr_left_sibling offset mismatch");
static_assert(offsetof(struct btree_page, btr_prefix_total) == 24, "btr_prefix_total offset mismatch");
static_assert(offsetof(struct btree_page, btr_relation) == 28, "btr_relation offset mismatch");
static_assert(offsetof(struct btree_page, btr_length) == 30, "btr_length offset mismatch");
static_assert(offsetof(struct btree_page, btr_id) == 32, "btr_id offset mismatch");
static_assert(offsetof(struct btree_page, btr_level) == 33, "btr_level offset mismatch");
static_assert(offsetof(struct btree_page, btr_jump_interval) == 34, "btr_jump_interval offset mismatch");
static_assert(offsetof(struct btree_page, btr_jump_size) == 36, "btr_jump_size offset mismatch");
static_assert(offsetof(struct btree_page, btr_jump_count) == 38, "btr_jump_count offset mismatch");
static_assert(offsetof(struct btree_page, btr_nodes) == 39, "btr_nodes offset mismatch");

// NS 2014-07-17: You can define this thing as "const FB_SIZE_t ...", and it works
// for standards-conforming compilers (recent GCC and MSVC will do)
// But older versions might have a problem, so I leave #define in place for now
#define BTR_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::btree_page, btr_nodes[0]))

// pag_flags
//const UCHAR btr_dont_gc			= 1;	// Don't garbage-collect this page
//const UCHAR btr_descending		= 2;	// Page/bucket is part of a descending index
//const UCHAR btr_jump_info			= 16;	// AB: 2003-index-structure enhancement
const UCHAR btr_released			= 32;	// Page was released from b-tree

// Data Page

struct data_page
{
	pag dpg_header;
	ULONG dpg_sequence;			// Sequence number in relation
	USHORT dpg_relation;		// Relation id
	USHORT dpg_count;			// Number of record segments on page
	struct dpg_repeat
	{
		USHORT dpg_offset;		// Offset of record fragment
		USHORT dpg_length;		// Length of record fragment
	} dpg_rpt[1];
};

static_assert(sizeof(struct data_page) == 28, "struct data_page size mismatch");
static_assert(offsetof(struct data_page, dpg_header) == 0, "dpg_header offset mismatch");
static_assert(offsetof(struct data_page, dpg_sequence) == 16, "gpg_sequence offset mismatch");
static_assert(offsetof(struct data_page, dpg_relation) == 20, "dpg_relation offset mismatch");
static_assert(offsetof(struct data_page, dpg_count) == 22, "dpg_count offset mismatch");
static_assert(offsetof(struct data_page, dpg_rpt) == 24, "dpg_rpt offset mismatch");

static_assert(sizeof(struct data_page::dpg_repeat) == 4, "struct dpg_repeat size mismatch");
static_assert(offsetof(struct data_page::dpg_repeat, dpg_offset) == 0, "dpg_offset offset mismatch");
static_assert(offsetof(struct data_page::dpg_repeat, dpg_length) == 2, "dpg_length offset mismatch");

#define DPG_SIZE	(sizeof (Ods::data_page) - sizeof (Ods::data_page::dpg_repeat))

// pag_flags
const UCHAR dpg_orphan		= 0x01;		// Data page is NOT in pointer page
const UCHAR dpg_full		= 0x02;		// Pointer page is marked FULL
const UCHAR dpg_large		= 0x04;		// Large object is on page
const UCHAR dpg_swept		= 0x08;		// Sweep has nothing to do on this page
const UCHAR dpg_secondary	= 0x10;	// Primary record versions not stored on this page
									// Set in dpm.epp's extend_relation() but never tested.


// Index root page

struct index_root_page
{
	pag irt_header;
	USHORT irt_relation;			// relation id (for consistency)
	USHORT irt_count;				// Number of indices
	struct irt_repeat
	{
	private:
		friend struct index_root_page; // to allow offset check for private members
		ULONG irt_root;				// page number of index root if irt_in_progress is NOT set, or
									// highest 32 bit of transaction if irt_in_progress is set
		ULONG irt_transaction;		// transaction in progress (lowest 32 bits)
	public:
		USHORT irt_desc;			// offset to key descriptions
		UCHAR irt_keys;				// number of keys in index
		UCHAR irt_flags;

		ULONG getRoot() const;
		void setRoot(ULONG root_page);

		TraNumber getTransaction() const;
		void setTransaction(TraNumber traNumber);

		bool isUsed() const;

	} irt_rpt[1];

	static_assert(sizeof(struct irt_repeat) == 12, "struct irt_repeat size mismatch");
	static_assert(offsetof(struct irt_repeat, irt_root) == 0, "irt_root offset mismatch");
	static_assert(offsetof(struct irt_repeat, irt_transaction) == 4, "irt_transaction offset mismatch");
	static_assert(offsetof(struct irt_repeat, irt_desc) == 8, "irt_desc offset mismatch");
	static_assert(offsetof(struct irt_repeat, irt_keys) == 10, "irt_keys offset mismatch");
	static_assert(offsetof(struct irt_repeat, irt_flags) == 11, "irt_flags offset mismatch");
};

static_assert(sizeof(struct index_root_page) == 32, "struct index_root_page size mismatch");
static_assert(offsetof(struct index_root_page, irt_header) == 0, "irt_header offset mismatch");
static_assert(offsetof(struct index_root_page, irt_relation) == 16, "irt_relation offset mismatch");
static_assert(offsetof(struct index_root_page, irt_count) == 18, "irt_count offset mismatch");
static_assert(offsetof(struct index_root_page, irt_rpt) == 20, "irt_rpt offset mismatch");

// key descriptor

struct irtd
{
	USHORT irtd_field;
	USHORT irtd_itype;
	float irtd_selectivity;
};

static_assert(sizeof(struct irtd) == 8, "struct irtd size mismatch");
static_assert(offsetof(struct irtd, irtd_field) == 0, "irtd_field offset mismatch");
static_assert(offsetof(struct irtd, irtd_itype) == 2, "irtd_itype offset mismatch");
static_assert(offsetof(struct irtd, irtd_selectivity) == 4, "irtd_selectivity offset mismatch");

// irt_flags, must match the idx_flags (see btr.h)
const USHORT irt_unique			= 1;
const USHORT irt_descending		= 2;
const USHORT irt_in_progress	= 4;
const USHORT irt_foreign		= 8;
const USHORT irt_primary		= 16;
const USHORT irt_expression		= 32;

inline ULONG index_root_page::irt_repeat::getRoot() const
{
	return (irt_flags & irt_in_progress) ? 0 : irt_root;
}

inline void index_root_page::irt_repeat::setRoot(ULONG root_page)
{
	irt_root = root_page;
	irt_flags &= ~irt_in_progress;
}

inline TraNumber index_root_page::irt_repeat::getTransaction() const
{
	return (irt_flags & irt_in_progress) ? ((TraNumber) irt_root << BITS_PER_LONG) | irt_transaction : 0;
}

inline void index_root_page::irt_repeat::setTransaction(TraNumber traNumber)
{
	irt_root = ULONG(traNumber >> BITS_PER_LONG);
	irt_transaction = ULONG(traNumber);
	irt_flags |= irt_in_progress;
}

inline bool index_root_page::irt_repeat::isUsed() const
{
	return (irt_flags & irt_in_progress) || (irt_root != 0);
}


const int STUFF_COUNT		= 4;

const ULONG END_LEVEL		= ~0;
const ULONG END_BUCKET		= (~0u) << 1;

// Header page

struct header_page
{
	pag hdr_header;
	USHORT hdr_page_size;			// Page size of database
	USHORT hdr_ods_version;			// Version of on-disk structure
	ULONG hdr_PAGES;				// Page number of PAGES relation
	ULONG hdr_next_page;			// Page number of next hdr page
	ULONG hdr_oldest_transaction;	// Oldest interesting transaction
	ULONG hdr_oldest_active;		// Oldest transaction thought active
	ULONG hdr_next_transaction;		// Next transaction id
	USHORT hdr_sequence;			// sequence number of file
	USHORT hdr_flags;				// Flag settings, see below
	SLONG hdr_creation_date[2];		// Date/time of creation
	ULONG hdr_attachment_id;		// Next attachment id
	SLONG hdr_shadow_count;			// Event count for shadow synchronization
	UCHAR hdr_cpu;					// CPU database was created on
	UCHAR hdr_os;					// OS database was created under
	UCHAR hdr_cc;					// Compiler of engine on which database was created
	UCHAR hdr_compatibility_flags;	// Cross-platform database transfer compatibility flags
	USHORT hdr_ods_minor;			// Update version of ODS
	USHORT hdr_end;					// offset of HDR_end in page
	ULONG hdr_page_buffers;			// Page buffers for database cache
	ULONG hdr_oldest_snapshot;		// Oldest snapshot of active transactions
	SLONG hdr_backup_pages; 		// The amount of pages in files locked for backup
	ULONG hdr_crypt_page;			// Page at which processing is in progress
	TEXT hdr_crypt_plugin[32];		// Name of plugin used to crypt this DB
	SLONG hdr_att_high;				// High word of the next attachment counter
	USHORT hdr_tra_high[4];			// High words of the transaction counters
	UCHAR hdr_data[1];				// Misc data
};

static_assert(sizeof(struct header_page) == 132, "struct header_page size mismatch");
static_assert(offsetof(struct header_page, hdr_header) == 0, "hdr_header offset mismatch");
static_assert(offsetof(struct header_page, hdr_page_size) == 16, "hdr_page_size offset mismatch");
static_assert(offsetof(struct header_page, hdr_ods_version) == 18, "hdr_ods_version offset mismatch");
static_assert(offsetof(struct header_page, hdr_PAGES) == 20, "hdr_PAGES offset mismatch");
static_assert(offsetof(struct header_page, hdr_next_page) == 24, "hdr_next_page offset mismatch");
static_assert(offsetof(struct header_page, hdr_oldest_transaction) == 28, "hdr_oldest_transaction offset mismatch");
static_assert(offsetof(struct header_page, hdr_oldest_active) == 32, "hdr_oldest_active offset mismatch");
static_assert(offsetof(struct header_page, hdr_next_transaction) == 36, "hdr_next_transaction offset mismatch");
static_assert(offsetof(struct header_page, hdr_sequence) == 40, "hdr_sequence offset mismatch");
static_assert(offsetof(struct header_page, hdr_flags) == 42, "hdr_flags offset mismatch");
static_assert(offsetof(struct header_page, hdr_creation_date) == 44, "hdr_creation_date offset mismatch");
static_assert(offsetof(struct header_page, hdr_attachment_id) == 52, "hdr_attachment_id offset mismatch");
static_assert(offsetof(struct header_page, hdr_shadow_count) == 56, "hdr_shadow_count offset mismatch");
static_assert(offsetof(struct header_page, hdr_cpu) == 60, "hdr_cpu offset mismatch");
static_assert(offsetof(struct header_page, hdr_os) == 61, "hdr_os offset mismatch");
static_assert(offsetof(struct header_page, hdr_cc) == 62, "hdr_cc offset mismatch");
static_assert(offsetof(struct header_page, hdr_compatibility_flags) == 63, "hdr_compatibility_flags offset mismatch");
static_assert(offsetof(struct header_page, hdr_ods_minor) == 64, "hdr_ods_minor offset mismatch");
static_assert(offsetof(struct header_page, hdr_end) == 66, "hdr_end offset mismatch");
static_assert(offsetof(struct header_page, hdr_page_buffers) == 68, "hdr_page_buffers offset mismatch");
static_assert(offsetof(struct header_page, hdr_oldest_snapshot) == 72, "hdr_oldest_snapshot offset mismatch");
static_assert(offsetof(struct header_page, hdr_backup_pages) == 76, "hdr_backup_pages offset mismatch");
static_assert(offsetof(struct header_page, hdr_crypt_page) == 80, "hdr_crypt_page offset mismatch");
static_assert(offsetof(struct header_page, hdr_crypt_plugin) == 84, "hdr_crypt_plugin offset mismatch");
static_assert(offsetof(struct header_page, hdr_att_high) == 116, "hdr_att_high offset mismatch");
static_assert(offsetof(struct header_page, hdr_tra_high) == 120, "hdr_tra_high offset mismatch");
static_assert(offsetof(struct header_page, hdr_data) == 128, "hdr_data offset mismatch");

#define HDR_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::header_page, hdr_data[0]))

// Header page clumplets

// Data items have the format
//
//	<type_byte> <length_byte> <data...>

const UCHAR HDR_end					= 0;
const UCHAR HDR_root_file_name		= 1;	// Original name of root file
const UCHAR HDR_file				= 2;	// Secondary file
const UCHAR HDR_last_page			= 3;	// Last logical page number of file
const UCHAR HDR_sweep_interval		= 4;	// Transactions between sweeps
const UCHAR HDR_crypt_checksum		= 5;	// Checksum of critical crypt parameters
const UCHAR HDR_difference_file		= 6;	// Delta file that is used during backup lock
const UCHAR HDR_backup_guid			= 7;	// GUID generated on each switch into backup mode
const UCHAR HDR_crypt_key			= 8;	// Name of a key used to crypt database
const UCHAR HDR_crypt_hash			= 9;	// Validator of key correctness
const UCHAR HDR_db_guid				= 10;	// Database GUID
const UCHAR HDR_repl_seq			= 11;	// Replication changelog sequence
const UCHAR HDR_max					= 11;	// Maximum HDR_clump value

// Header page flags

const USHORT hdr_active_shadow		= 0x1;		// 1	file is an active shadow file
const USHORT hdr_force_write		= 0x2;		// 2	database is forced write
const USHORT hdr_crypt_process		= 0x4;		// 4	Encryption status is changing now
const USHORT hdr_no_reserve			= 0x8;		// 8	don't reserve space for versions
const USHORT hdr_SQL_dialect_3		= 0x10;		// 16	database SQL dialect 3
const USHORT hdr_read_only			= 0x20;		// 32	Database is ReadOnly. If not set, DB is RW
const USHORT hdr_encrypted			= 0x40;		// 64	Database is encrypted

const USHORT hdr_backup_mask		= 0xC00;
const USHORT hdr_shutdown_mask		= 0x1080;
const USHORT hdr_replica_mask		= 0x6000;

// Values for backup mask
const USHORT hdr_nbak_normal		= 0x000;	// Normal mode. Changes are simply written to main files
const USHORT hdr_nbak_stalled		= 0x400;	// 1024 Main files are locked. Changes are written to diff file
const USHORT hdr_nbak_merge			= 0x800;	// 2048 Merging changes from diff file into main files
const USHORT hdr_nbak_unknown		= USHORT(~0);	// State is unknown. Needs to be read from disk

// Values for shutdown mask
const USHORT hdr_shutdown_none		= 0x0;
const USHORT hdr_shutdown_multi		= 0x80;
const USHORT hdr_shutdown_full		= 0x1000;
const USHORT hdr_shutdown_single	= 0x1080;

// Values for replica mask
const USHORT hdr_replica_none		= 0x0000;
const USHORT hdr_replica_read_only	= 0x2000;
const USHORT hdr_replica_read_write	= 0x4000;


// Page Inventory Page

struct page_inv_page
{
	pag pip_header;
	ULONG pip_min;				// Lowest (possible) free page
	ULONG pip_extent;			// Lowest free extent
	ULONG pip_used;				// Number of pages allocated from this PIP page
	UCHAR pip_bits[1];
};

static_assert(sizeof(struct page_inv_page) == 32, "struct page_inv_page size mismatch");
static_assert(offsetof(struct page_inv_page, pip_header) == 0, "pip_header offset mismatch");
static_assert(offsetof(struct page_inv_page, pip_min) == 16, "pip_min offset mismatch");
static_assert(offsetof(struct page_inv_page, pip_extent) == 20, "pip_extent offset mismatch");
static_assert(offsetof(struct page_inv_page, pip_used) == 24, "pip_used offset mismatch");
static_assert(offsetof(struct page_inv_page, pip_bits) == 28, "pip_bits offset mismatch");


// SCN's Page

struct scns_page
{
	pag scn_header;
	ULONG scn_sequence;			// Sequence number in page space
	ULONG scn_pages[1];			// SCN's vector
};

static_assert(sizeof(struct scns_page) == 24, "struct scns_page size mismatch");
static_assert(offsetof(struct scns_page, scn_header) == 0, "scn_header offset mismatch");
static_assert(offsetof(struct scns_page, scn_sequence) == 16, "scn_sequence offset mismatch");
static_assert(offsetof(struct scns_page, scn_pages) == 20, "scn_pages offset mismatch");


// Important note !
// pagesPerPIP value must be multiply of pagesPerSCN value !
//
// Nth PIP page number is : pagesPerPIP * N - 1
// Nth SCN page number is : pagesPerSCN * N
// Numbers of first PIP and SCN pages (N = 0) is fixed and not interesting here.
//
// Generally speaking it is possible that exists N and M that
//   pagesPerSCN * N == pagesPerPIP * M - 1,
// i.e. we can't guarantee that some SCN page will not have the same number as
// some PIP page. We can implement checks for this case and put corresponding
// SCN page at the next position but it will complicate code a lot.
//
// The much more easy solution is to make pagesPerPIP multiply of pagesPerSCN.
// The fact that page_inv_page::pip_bits array is LONG aligned and occupy less
// size (in bytes) than scns_page::scn_pages array allow us to use very simple
// formula for pagesPerSCN : pagesPerSCN = pagesPerPIP / BITS_PER_LONG.
// Please, consider above when changing page_inv_page or scns_page definition.
//
// Table below show numbers for different page sizes using current (ODS12)
// definitions of page_inv_page and scns_page
//
// PageSize  pagesPerPIP  maxPagesPerSCN    pagesPerSCN
//     4096        32544            1019           1017
//     8192        65312            2043           2041
//    16384       130848            4091           4089
//    32768       261920            8187           8185
//    65536       524064           16379          16377

// Pointer Page

struct pointer_page
{
	pag ppg_header;
	ULONG ppg_sequence;			// Sequence number in relation
	ULONG ppg_next;				// Next pointer page in relation
	USHORT ppg_count;			// Number of slots active
	USHORT ppg_relation;		// Relation id
	USHORT ppg_min_space;		// Lowest slot with space available
	ULONG ppg_page[1];			// Data page vector
};

static_assert(sizeof(struct pointer_page) == 36, "struct pointer_page size mismatch");
static_assert(offsetof(struct pointer_page, ppg_header) == 0, "ppg_header offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_sequence) == 16, "ppg_sequence offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_next) == 20, "ppg_next offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_count) == 24, "ppg_count offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_relation) == 26, "ppg_relation offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_min_space) == 28, "ppg_min_space offset mismatch");
static_assert(offsetof(struct pointer_page, ppg_page) == 32, "ppg_page offset mismatch");


// pag_flags
const UCHAR ppg_eof		= 1;	// Last pointer page in relation

// After array of physical page numbers (ppg_page) there is also array of bit
// flags per every data page. These flags describes state of corresponding data
// page. Definitions below used to deal with these bits.
const int PPG_DP_BITS_NUM		= 8;		// Number of additional flag bits per data page

const UCHAR ppg_dp_full			= 0x01;		// Data page is FULL
const UCHAR ppg_dp_large		= 0x02;		// Large object is on data page
const UCHAR ppg_dp_swept		= 0x04;		// Sweep has nothing to do on data page
const UCHAR ppg_dp_secondary	= 0x08;		// Primary record versions not stored on data page
const UCHAR ppg_dp_empty		= 0x10;		// Data page is empty

const UCHAR PPG_DP_ALL_BITS	= (1 << PPG_DP_BITS_NUM) - 1;

#define PPG_DP_BIT_MASK(slot, bit)		(bit)
#define PPG_DP_BITS_BYTE(bits, slot)	((bits)[(slot)])

#define PPG_DP_BIT_TEST(flags, slot, bit)	(PPG_DP_BITS_BYTE((flags), (slot)) & PPG_DP_BIT_MASK((slot), (bit)))
#define PPG_DP_BIT_SET(flags, slot, bit)	(PPG_DP_BITS_BYTE((flags), (slot)) |= PPG_DP_BIT_MASK((slot), (bit)))
#define PPG_DP_BIT_CLEAR(flags, slot, bit)	(PPG_DP_BITS_BYTE((flags), (slot)) &= ~PPG_DP_BIT_MASK((slot), (bit)))


// Transaction Inventory Page

struct tx_inv_page
{
	pag tip_header;
	ULONG tip_next;				// Next transaction inventory page
	UCHAR tip_transactions[1];
};

static_assert(sizeof(struct tx_inv_page) == 24, "struct tx_inv_page size mismatch");
static_assert(offsetof(struct tx_inv_page, tip_header) == 0, "tip_header offset mismatch");
static_assert(offsetof(struct tx_inv_page, tip_next) == 16, "tip_next offset mismatch");
static_assert(offsetof(struct tx_inv_page, tip_transactions) == 20, "tip_transactions offset mismatch");


// Generator Page

struct generator_page
{
	pag gpg_header;
	ULONG gpg_sequence;			// Sequence number
	ULONG gpg_dummy1;			// Alignment enforced
	SINT64 gpg_values[1];		// Generator vector
};

static_assert(sizeof(struct generator_page) == 32, "struct generator_page size mismatch");
static_assert(offsetof(struct generator_page, gpg_header) == 0, "gpg_header offset mismatch");
static_assert(offsetof(struct generator_page, gpg_sequence) == 16, "gpg_sequence offset mismatch");
static_assert(offsetof(struct generator_page, gpg_dummy1) == 20, "gpg_dummy1 offset mismatch");
static_assert(offsetof(struct generator_page, gpg_values) == 24, "gpg_values offset mismatch");


// Record header

struct rhd
{
	ULONG rhd_transaction;		// transaction id (lowest 32 bits)
	ULONG rhd_b_page;			// back pointer
	USHORT rhd_b_line;			// back line
	USHORT rhd_flags;			// flags, etc
	UCHAR rhd_format;			// format version
	UCHAR rhd_data[1];			// record data
};

static_assert(sizeof(struct rhd) == 16, "struct rhd size mismatch");
static_assert(offsetof(struct rhd, rhd_transaction) == 0, "rhd_transaction offset mismatch");
static_assert(offsetof(struct rhd, rhd_b_page) == 4, "rhd_b_page offset mismatch");
static_assert(offsetof(struct rhd, rhd_b_line) == 8, "rhd_b_line offset mismatch");
static_assert(offsetof(struct rhd, rhd_flags) == 10, "rhd_flags offset mismatch");
static_assert(offsetof(struct rhd, rhd_format) == 12, "rhd_format offset mismatch");
static_assert(offsetof(struct rhd, rhd_data) == 13, "rhd_data offset mismatch");

#define RHD_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::rhd, rhd_data[0]))

// Record header extended to hold long transaction id

struct rhde
{
	ULONG rhde_transaction;		// transaction id (lowest 32 bits)
	ULONG rhde_b_page;			// back pointer
	USHORT rhde_b_line;			// back line
	USHORT rhde_flags;			// flags, etc
	UCHAR rhde_format;			// format version	// until here, same as rhd
	USHORT rhde_tra_high;		// higher bits of transaction id
	UCHAR rhde_data[1];			// record data
};

static_assert(sizeof(struct rhde) == 20, "struct rhde size mismatch");
static_assert(offsetof(struct rhde, rhde_transaction) == 0, "rhde_transaction offset mismatch");
static_assert(offsetof(struct rhde, rhde_b_page) == 4, "rhde_b_page offset mismatch");
static_assert(offsetof(struct rhde, rhde_b_line) == 8, "rhde_b_line offset mismatch");
static_assert(offsetof(struct rhde, rhde_flags) == 10, "rhde_flags offset mismatch");
static_assert(offsetof(struct rhde, rhde_format) == 12, "rhde_formats offset mismatch");
static_assert(offsetof(struct rhde, rhde_tra_high) == 14, "rhde_tra_high offset mismatch");
static_assert(offsetof(struct rhde, rhde_data) == 16, "rhde_data offset mismatch");

#define RHDE_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::rhde, rhde_data[0]))

// Record header for fragmented record

struct rhdf
{
	ULONG rhdf_transaction;		// transaction id (lowest 32 bits)
	ULONG rhdf_b_page;			// back pointer
	USHORT rhdf_b_line;			// back line
	USHORT rhdf_flags;			// flags, etc
	UCHAR rhdf_format;			// format version    // until here, same as rhd
	USHORT rhdf_tra_high;		// higher bits of transaction id
	ULONG rhdf_f_page;			// next fragment page
	USHORT rhdf_f_line;			// next fragment line
	UCHAR rhdf_data[1];			// record data
};

static_assert(sizeof(struct rhdf) == 24, "struct rhdf size mismatch");
static_assert(offsetof(struct rhdf, rhdf_transaction) == 0, "rhdf_transaction offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_b_page) == 4, "rhdf_b_page offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_b_line) == 8, "rhdf_b_line offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_flags) == 10, "rhdf_flags offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_format) == 12, "rhdf_format offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_tra_high) == 14, "rhdf_tra_high offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_f_page) == 16, "rhdf_f_page offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_f_line) == 20, "rhdf_f_line offset mismatch");
static_assert(offsetof(struct rhdf, rhdf_data) == 22, "rhdf_data offset mismatch");

#define RHDF_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::rhdf, rhdf_data[0]))


// Record header for blob header

struct blh
{
	ULONG blh_lead_page;		// First data page number
	ULONG blh_max_sequence;		// Number of data pages
	USHORT blh_max_segment;		// Longest segment
	USHORT blh_flags;			// flags, etc
	UCHAR blh_level;			// Number of address levels, see blb_level in blb.h
	ULONG blh_count;			// Total number of segments
	ULONG blh_length;			// Total length of data
	USHORT blh_sub_type;		// Blob sub-type
	UCHAR blh_charset;			// Blob charset (since ODS 11.1)
// Macro CHECK_BLOB_FIELD_ACCESS_FOR_SELECT is never defined, code under it was left for a case
// we would like to have that check in a future.
#ifdef CHECK_BLOB_FIELD_ACCESS_FOR_SELECT
	USHORT blh_fld_id;			// Field ID
#endif
	UCHAR blh_unused;
	ULONG blh_page[1];			// Page vector for blob pages
};

static_assert(sizeof(struct blh) == 32, "struct blh size mismatch");
static_assert(offsetof(struct blh, blh_lead_page) == 0, "blh_lead_page offset mismatch");
static_assert(offsetof(struct blh, blh_max_sequence) == 4, "blh_max_sequence offset mismatch");
static_assert(offsetof(struct blh, blh_max_segment) == 8, "blh_max_segment offset mismatch");
static_assert(offsetof(struct blh, blh_flags) == 10, "blh_flags offset mismatch");
static_assert(offsetof(struct blh, blh_level) == 12, "blh_level offset mismatch");
static_assert(offsetof(struct blh, blh_count) == 16, "blh_count offset mismatch");
static_assert(offsetof(struct blh, blh_length) == 20, "blh_length offset mismatch");
static_assert(offsetof(struct blh, blh_sub_type) == 24, "blh_sub_type offset mismatch");
static_assert(offsetof(struct blh, blh_charset) == 26, "blh_charset offset mismatch");
static_assert(offsetof(struct blh, blh_unused) == 27, "blh_unused offset mismatch");
static_assert(offsetof(struct blh, blh_page) == 28, "blh_page offset mismatch");


#define BLH_SIZE static_cast<FB_SIZE_T>(offsetof(Ods::blh, blh_page[0]))
// rhd_flags, rhdf_flags and blh_flags

// record_param flags in req.h must be an exact replica of ODS record header flags

const USHORT rhd_deleted		= 1;		// record is logically deleted
const USHORT rhd_chain			= 2;		// record is an old version
const USHORT rhd_fragment		= 4;		// record is a fragment
const USHORT rhd_incomplete		= 8;		// record is incomplete
const USHORT rhd_blob			= 16;		// isn't a record but a blob
const USHORT rhd_stream_blob	= 32;		// blob is a stream mode blob
const USHORT rhd_delta			= 32;		// prior version is differences only
const USHORT rhd_large			= 64;		// object is large
const USHORT rhd_damaged		= 128;		// object is known to be damaged
const USHORT rhd_gc_active		= 256;		// garbage collecting dead record version
const USHORT rhd_uk_modified	= 512;		// record key field values are changed
const USHORT rhd_long_tranum	= 1024;		// transaction number is 64-bit
const USHORT rhd_not_packed		= 2048;		// record (or delta) is stored "as is"


// This (not exact) copy of class DSC is used to store descriptors on disk.
// Hopefully its binary layout is common for 32/64 bit CPUs.
struct Descriptor
{
	UCHAR	dsc_dtype;
	SCHAR	dsc_scale;
	USHORT	dsc_length;
	SSHORT	dsc_sub_type;
	USHORT	dsc_flags;
	ULONG	dsc_offset;
};

static_assert(sizeof(struct Descriptor) == 12, "struct Descriptor size mismatch");
static_assert(offsetof(struct Descriptor, dsc_dtype) == 0, "dsc_dtype offset mismatch");
static_assert(offsetof(struct Descriptor, dsc_scale) == 1, "dsc_scale offset mismatch");
static_assert(offsetof(struct Descriptor, dsc_length) == 2, "dsc_length offset mismatch");
static_assert(offsetof(struct Descriptor, dsc_sub_type) == 4, "dsc_sub_type offset mismatch");
static_assert(offsetof(struct Descriptor, dsc_flags) == 6, "dsc_flags offset mismatch");
static_assert(offsetof(struct Descriptor, dsc_offset) == 8, "dsc_offset offset mismatch");

// Array description, "internal side" used by the engine.
// And stored on the disk, in the relation summary blob.

struct InternalArrayDesc
{
	UCHAR iad_version;			// Array descriptor version number
	UCHAR iad_dimensions;		// Dimensions of array
	USHORT iad_struct_count;	// Number of struct elements
	USHORT iad_element_length;	// Length of array element
	USHORT iad_length;			// Length of array descriptor
	ULONG iad_count;			// Total number of elements
	ULONG iad_total_length;		// Total length of array
	struct iad_repeat
	{
		Descriptor iad_desc;	// Element descriptor
		ULONG iad_length;		// Length of "vector" element
		SLONG iad_lower;		// Lower bound
		SLONG iad_upper;		// Upper bound
	};
	iad_repeat iad_rpt[1];

	static_assert(sizeof(struct iad_repeat) == 24, "struct iad_repeat size mismatch");
	static_assert(offsetof(struct iad_repeat, iad_desc) == 0, "iad_desc offset mismatch");
	static_assert(offsetof(struct iad_repeat, iad_length) == 12, "iad_length offset mismatch");
	static_assert(offsetof(struct iad_repeat, iad_lower) == 16, "iad_lower offset mismatch");
	static_assert(offsetof(struct iad_repeat, iad_upper) == 20, "iad_upper offset mismatch");
};

static_assert(sizeof(struct InternalArrayDesc) == 40, "struct InternalArrayDesc size mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_version) == 0, "iad_version offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_dimensions) == 1, "iad_dimension offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_struct_count) == 2, "iad_struct_count offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_element_length) == 4, "iad_element_length offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_length) == 6, "iad_length offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_count) == 8, "iad_count offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_total_length) == 12, "iad_total_length offset mismatch");
static_assert(offsetof(struct InternalArrayDesc, iad_rpt) == 16, "iad_rpt offset mismatch");

const UCHAR IAD_VERSION_1		= 1;

/*
inline int IAD_LEN(int count)
{
	if (!count)
		count = 1;
	return sizeof (InternalArrayDesc) +
		(count - 1) * sizeof (InternalArrayDesc::iad_repeat);
}
*/
#define IAD_LEN(count)	(sizeof (Ods::InternalArrayDesc) + \
	(count ? count - 1: count) * sizeof (Ods::InternalArrayDesc::iad_repeat))

Firebird::string pagtype(UCHAR type);

} //namespace Ods

// alignment for raw page access
const USHORT PAGE_ALIGNMENT = 1024;

// size of raw I/O operation for header page
const USHORT RAW_HEADER_SIZE = 1024;	// ROUNDUP(HDR_SIZE, PAGE_ALIGNMENT);
//static_assert(RAW_HEADER_SIZE >= HDR_SIZE, "RAW_HEADER_SIZE is less than HDR_SIZE");

// max number of table formats (aka versions), limited by "UCHAR rhd_format"
const int MAX_TABLE_VERSIONS = 255;

// max number of view formats (aka versions), limited by "SSHORT RDB$FORMAT"
const int MAX_VIEW_VERSIONS = MAX_SSHORT;

#endif // JRD_ODS_H
