/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BulkInsert.h
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

#ifndef JRD_BULKINSERT
#define JRD_BULKINSERT

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/pag.h"
#include "../jrd/RecordNumber.h"


namespace Jrd
{

class Compressor;
class jrd_rel;
class jrd_tra;
struct record_param;
class Record;
class Request;

class BulkInsert : public Firebird::PermanentStorage
{
public:
	BulkInsert(Firebird::MemoryPool& pool, thread_db* tdbb, jrd_rel* relation);

	void putRecord(thread_db* tdbb, record_param* rpb, jrd_tra* transaction);
	RecordNumber putBlob(thread_db* tdbb, blb* blob, Record* record);
	void flush(thread_db* tdbb);

	Request* getRequest() const
	{
		return m_request;
	}

	jrd_rel* getRelation() const
	{
		return m_primary->m_relation;
	}

private:
	struct Buffer : public Firebird::PermanentStorage
	{
		Buffer(Firebird::MemoryPool& pool, ULONG pageSize, ULONG spaceReserve, bool primary,
			jrd_rel* relation);

		void putRecord(thread_db* tdbb, record_param* rpb, jrd_tra* transaction);
		RecordNumber putBlob(thread_db* tdbb, blb* blob, Record* record);
		void flush(thread_db* tdbb);

		// allocate and reserve data pages
		Ods::data_page* allocatePages(thread_db* tdbb);
		UCHAR* findSpace(thread_db* tdbb, record_param* rpb, USHORT size);
		void fragmentRecord(thread_db* tdbb, record_param* rpb, Compressor* dcc);
		void markLarge();

		const ULONG m_pageSize;
		const ULONG m_spaceReserve;
		const bool m_isPrimary;
		jrd_rel* const m_relation;

		Firebird::Array<UCHAR> m_buffer;				// buffer for data pages
		Ods::data_page* m_pages = nullptr;				// first DP in buffer
		Ods::data_page* m_current = nullptr;			// current DP to put records
		unsigned m_index = 0;							// index of the current page [0..m_reserved)
		Firebird::ObjectsArray<PageStack> m_highPages;	// high precedence pages, per data page
		ULONG m_freeSpace = 0;							// free space on current DP
		USHORT m_reserved = 0;							// count of reserved pages
	};

	Request* const m_request;		// "owner" request that will destroy this object on unwind

	Firebird::AutoPtr<Buffer> m_primary;
	Firebird::AutoPtr<Buffer> m_other;
};

};	// namespace Jrd

#endif // JRD_BULKINSERT
