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
		Compressor(thread_db* tdbb, FB_SIZE_T length, const UCHAR* data);

		FB_SIZE_T getPackedLength() const
		{
			return m_length;
		}

		bool isCompressable() const
		{
			for (const auto length : m_runs)
			{
				if (length < 0)
					return true;
			}

			return false;
		}

		void pack(const UCHAR* input, UCHAR* output) const;
		FB_SIZE_T truncate(FB_SIZE_T outLength);
		FB_SIZE_T truncateTail(FB_SIZE_T outLength);

		static UCHAR* unpack(FB_SIZE_T, const UCHAR*, FB_SIZE_T, UCHAR*);

		static FB_SIZE_T applyDiff(FB_SIZE_T, const UCHAR*, FB_SIZE_T, UCHAR* const);
		static FB_SIZE_T makeDiff(FB_SIZE_T, const UCHAR*, FB_SIZE_T, UCHAR*, FB_SIZE_T, UCHAR*);
		static FB_SIZE_T makeNoDiff(FB_SIZE_T, UCHAR*);

	private:
		Compressor();
		unsigned nonCompressableRun(unsigned length);

		Firebird::HalfStaticArray<int, 256> m_runs;
		FB_SIZE_T m_length = 0;
	};

} //namespace Jrd

#endif // JRD_SQZ_H
