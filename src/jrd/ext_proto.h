/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ext_proto.h
 *	DESCRIPTION:	Prototype header file for ext.cpp & extvms.cpp
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

#include <stdio.h>
#include <string.h>

#include "fb_blk.h"
#include "../common/classes/alloc.h"
#include "../common/classes/locks.h"

#ifndef JRD_EXT_PROTO_H
#define JRD_EXT_PROTO_H

namespace Jrd {

class jrd_tra;
class RecordSource;
class jrd_rel;
struct record_param;
struct bid;
class Database;
class thread_db;

// External file access block

class ExternalFile : public pool_alloc_rpt<SCHAR, type_ext>
{
private:
	ExternalFile()
		: ext_flags(0), ext_tra_cnt(0), ext_ifi(nullptr)
	{ }

	void open(Database* dbb);
	void checkOpened();

public:
	static ExternalFile* create(MemoryPool& pool, const char* name)
	{
		ExternalFile* file = FB_NEW_RPT(pool, (strlen(name) + 1)) ExternalFile();
		strcpy(file->ext_filename, name);
		return file;
	}

	~ExternalFile()
	{
		fb_assert(!ext_ifi);
	}

	FILE* getFile()
	{
		return ext_ifi;
	}

	void traAttach(thread_db* tdbb);
	void traDetach() noexcept;
	double getCardinality(thread_db* tdbb, jrd_rel* relation) noexcept;
	void erase(record_param*, jrd_tra*);
	bool get(thread_db* tdbb, record_param* rpb, FB_UINT64& position);
	void modify(record_param*, record_param*, jrd_tra*);
	void store(thread_db*, record_param*);
	void release();

private:
	Firebird::Mutex	ext_sync;
	USHORT			ext_flags;		// Misc and cruddy flags
	USHORT			ext_tra_cnt;	// How many transactions used the file
	FILE*			ext_ifi;		// Internal file identifier
	char			ext_filename[1];
};

// ext_flags
const USHORT EXT_readonly	= 1;	// File could only be opened for read
const USHORT EXT_last_read	= 2;	// last operation was read
const USHORT EXT_last_write	= 4;	// last operation was write

} //namespace Jrd

#endif // JRD_EXT_PROTO_H
