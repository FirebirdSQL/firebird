/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pag.h
 *	DESCRIPTION:	Page interface definitions
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

/*
 * Modified by: Patrick J. P. Griffin
 * Date: 11/29/2000
 * Problem:   Bug 116733 Too many generators corrupt database.
 *            DPM_gen_id was not calculating page and offset correctly.
 * Change:    Add pgc_gpg, number of generators per page,
 *            for use in DPM_gen_id.
 */


#ifndef JRD_PAG_H
#define JRD_PAG_H

#include "../include/fb_blk.h"
#include "../common/classes/array.h"
#include "../common/classes/locks.h"
#include "../common/classes/rwlock.h"
#include "../jrd/ods.h"
#include "../jrd/lls.h"
#include "../jrd/PageNumber.h"

namespace Jrd {

const PageNumber ZERO_PAGE_NUMBER(INVALID_PAGE_SPACE, 0);
const PageNumber HEADER_PAGE_NUMBER(DB_PAGE_SPACE, HEADER_PAGE);

inline constexpr USHORT PAGES_IN_EXTENT	= 8;

class jrd_file;
class Database;
class thread_db;
class PageManager;

class PageSpace : public pool_alloc<type_PageSpace>
{
public:
	explicit PageSpace(Database* aDbb, ULONG aPageSpaceID)
	{
		pageSpaceID = aPageSpaceID;
		pipHighWater = 0;
		pipWithExtent = 0;
		pipFirst = 0;
		scnFirst = 0;
		file = 0;
		dbb = aDbb;
		maxPageNumber = 0;
		pipMaxKnown = 0;
	}

	~PageSpace();

	ULONG pageSpaceID;
	Firebird::AtomicCounter pipHighWater;		// Lowest PIP with space
	Firebird::AtomicCounter pipWithExtent;		// Lowest PIP with free extent
	ULONG pipFirst;								// First pointer page
	ULONG scnFirst;								// First SCN's page

	jrd_file*	file;

	static inline bool isTemporary(ULONG aPageSpaceID) noexcept
	{
		return (aPageSpaceID >= TEMP_PAGE_SPACE);
	}

	static inline bool isTemporary(const PageNumber& page) noexcept
	{
		return isTemporary(page.getPageSpaceID());
	}

	inline bool isTemporary() const noexcept
	{
		return isTemporary(pageSpaceID);
	}

	static inline bool isTablespace(ULONG aPageSpaceID)
	{
		return (aPageSpaceID > DB_PAGE_SPACE && aPageSpaceID <= MAX_TABLESPACE_ID);
	}

	static inline bool isTablespace(const PageNumber& page) noexcept
	{
		return isTablespace(page.getPageSpaceID());
	}

	inline bool isTablespace() const
	{
		return isTablespace(pageSpaceID);
	}

	static inline ULONG generate(const PageSpace* Item) noexcept
	{
		return Item->pageSpaceID;
	}

	// how many pages allocated
	ULONG actAlloc();
	static ULONG actAlloc(const Database* dbb);

	// number of last allocated page
	ULONG maxAlloc();
	static ULONG maxAlloc(const Database* dbb);

	// number of last used page
	ULONG lastUsedPage();
	static ULONG lastUsedPage(const Database* dbb);

	// number of used pages
	ULONG usedPages();
	static ULONG usedPages(const Database* dbb);

	// extend page space
	ULONG extend(thread_db* tdbb, ULONG pageNum, bool forceSize);

	// get SCN's page number
	ULONG getSCNPageNum(ULONG sequence) const noexcept;

	// is pagespace on raw device
	bool onRawDevice() const noexcept;

private:
	ULONG	maxPageNumber;
	Database* dbb;
	ULONG	pipMaxKnown;
};

class PageManager : public pool_alloc<type_PageManager>
{
public:
	explicit PageManager(Database* aDbb, Firebird::MemoryPool& aPool);

	~PageManager()
	{
		while (pageSpaces.hasData())
			delete pageSpaces.pop();

		delete pageSpacesLock;
	}

	PageSpace* findPageSpace(const ULONG pageSpaceID) const;

	void initTempPageSpace(thread_db* tdbb);
	ULONG getTempPageSpaceID(thread_db* tdbb);

	void allocatePageSpace(thread_db* tdbb, ULONG pageSpaceID, bool create, const Firebird::PathName& fileName);
	void deletePageSpace(const ULONG pageSpaceID, bool deleteFile = false);

	void closeAll();

	ULONG pagesPerPIP;			// Pages per pip
	ULONG bytesBitPIP;			// Number of bytes of bit in PIP
	ULONG transPerTIP;			// Transactions per TIP
	ULONG gensPerPage;			// Generators per generator page
	ULONG pagesPerSCN;			// Slots per SCN's page

private:
	typedef Firebird::SortedArray<PageSpace*, Firebird::EmptyStorage<PageSpace*>,
		ULONG, PageSpace> PageSpaceArray;

	PageSpace* addPageSpace(const ULONG pageSpaceID);

	Database* dbb;
	PageSpaceArray pageSpaces;
	Firebird::RWLock* pageSpacesLock;
	Firebird::MemoryPool& pool;
	Firebird::Mutex	initTmpMtx;
	USHORT tempPageSpaceID;
	bool tempFileCreated;
};

} //namespace Jrd

#endif // JRD_PAG_H
