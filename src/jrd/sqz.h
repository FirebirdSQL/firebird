/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		sqz.h
 *	DESCRIPTION:	Data compression control block
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

#ifndef JRD_SQZ_H
#define JRD_SQZ_H

#include "../include/fb_blk.h"
#include "../../common/classes/array.h"

namespace Jrd
{
	class thread_db;

	class Compressor
	{
	public:
		Compressor(thread_db* tdbb, ULONG length, const UCHAR* data);

		ULONG getPackedLength() const
		{
			return m_length;
		}

		bool isPacked() const
		{
			return m_runs.hasData();
		}

		void pack(const UCHAR* input, UCHAR* output) const;
		ULONG truncate(ULONG outLength);
		ULONG truncateTail(ULONG outLength);

		static ULONG getUnpackedLength(ULONG inLength, const UCHAR* input);
		static UCHAR* unpack(ULONG inLength, const UCHAR* input,
							 ULONG outLength, UCHAR* output);

	private:
		unsigned nonCompressableRun(unsigned length);

		Firebird::HalfStaticArray<int, 256> m_runs;
		ULONG m_length = 0;

		// Compatibility options
		bool m_allowLongRuns = true;
		bool m_allowUnpacked = true;
	};

	class Difference
	{
		// Max length of generated differences string between two records
		static const unsigned MAX_DIFFERENCES = 1024;

	public:
		UCHAR* getData()
		{
			return m_differences;
		}

		const UCHAR* getData() const
		{
			return m_differences;
		}

		ULONG getCapacity() const
		{
			return MAX_DIFFERENCES;
		}

		ULONG apply(ULONG diffLength, ULONG outLength, UCHAR* output);
		ULONG make(ULONG length1, const UCHAR* rec1,
				   ULONG length2, const UCHAR* rec2);
		ULONG makeNoDiff(ULONG length);

	private:
		UCHAR m_differences[MAX_DIFFERENCES];
	};

} //namespace Jrd

#endif // JRD_SQZ_H
