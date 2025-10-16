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
 *  The Original Code was created by Roman Simakov
 *  for the RedDatabase project.
 *
 *  Copyright (c) 2018 <roman.simakov@red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
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

	class Tablespace
	{
	friend class Attachment;
	public:
		explicit Tablespace(MemoryPool& p, ULONG tsId, const MetaName& tsName)
			: id(tsId), name(p, tsName)
		{}

		~Tablespace();

		ULONG id;						// tablespace id = pagespace id
		MetaName name;					// tablespace name
		Lock* existenceLock = nullptr;	// existence lock, if any
		bool modified = false;

	private:
		int useCount = 0;

	public:
		void addRef(thread_db* tdbb);
		void release(thread_db* tdbb);

		bool isUsed() const
		{
			return (useCount > 0);
		}

	};
}

#endif // JRD_TABLESPACE_H
