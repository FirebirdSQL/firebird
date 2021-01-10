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
 * Roman Simakov
 */

#ifndef JRD_TABLESPACE_H
#define JRD_TABLESPACE_H

#include "../common/classes/alloc.h"
#include "../jrd/MetaName.h"
#include "../jrd/pag.h"

namespace Jrd
{
	class thread_db;
	class CompilerScratch;
	class JrdStatement;
	class Lock;
	class Format;
	class Parameter;

	class Tablespace : public Firebird::PermanentStorage
	{
	friend class Attachment;
	public:
		explicit Tablespace(MemoryPool& p)
			: PermanentStorage(p),
			  id(INVALID_PAGE_SPACE),
			  name(p),
			  existenceLock(NULL),
		      useCount(0)
		{
		}

		USHORT id;					// tablespace id = pagespace id
		MetaName name;	// tablespace name
		Lock* existenceLock;		// existence lock, if any

	private:
		int useCount;

	public:
		void addRef(thread_db* tdbb);
		void release(thread_db* tdbb);
		bool isUsed() const
		{
			return useCount > 0;
		}

	};
}

#endif // JRD_TABLESPACE_H
