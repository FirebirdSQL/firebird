/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		sqz.cpp
 *	DESCRIPTION:	Record compression/decompression
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

#include "firebird.h"
#include <string.h>
#include "../jrd/sqz.h"
#include "../jrd/req.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"

using namespace Jrd;

// Compression (run-length encoding aka RLE) scheme:
//
// positive length [1 .. 127] - up to 127 following bytes are copied "as is"
// negative length [-3 .. -128] - the following byte is repeated up to 128 times
// zero length bytes may be used as padding (if necessary), they are skipped during decoding
//
// In ODS 13.1, two priorly unused lengths (-1 and -2) are used as special markers
// for longer compressable runs:
//
// {-1 , two-byte counter, byte value} - repeating sequences between 129 bytes and 64KB in length
// {-2 , four-byte counter, byte value} - repeating sequences longer than 64KB in length
//
// Compressable runs of length 3 are in fact meaningless, if located between two
// non-compressable runs. Compressable runs between 4 and 8 bytes are somewhat border case, as
// they do not compress much but increase total number of runs thus affecting decompression speed.
// Starting from Firebird v5, we don't compress runs shorter than 8 bytes. But this rule is not
// set in stone, so let's not use lenghts between 4 and 7 bytes as some other special markers.

namespace
{
	const unsigned MIN_COMPRESS_RUN = 8; // minimal length of compressable run

	const int MAX_NONCOMP_RUN = MAX_SCHAR;	// 127

	const int MAX_SHORT_RUN = -MIN_SCHAR;	// 128
	const int MAX_MEDIUM_RUN = MAX_USHORT;	// 2^16
	const int MAX_LONG_RUN = MAX_SLONG;		// 2^31

	inline int adjustRunLength(unsigned length)
	{
		return (length <= MAX_SHORT_RUN) ? 0 :
			(length <= MAX_MEDIUM_RUN) ? sizeof(SSHORT) : sizeof(SLONG);
	}
};

unsigned Compressor::nonCompressableRun(unsigned length)
{
	fb_assert(length && length <= MAX_NONCOMP_RUN);

	auto result = length;

	// If the prior run was also non-compressable and it wasn't fully utilized,
	// merge our run there. Otherwise, create a new run.

	if (m_runs.hasData() && m_runs.back() > 0 && m_runs.back() < MAX_NONCOMP_RUN)
	{
		const auto max = MIN(MAX_NONCOMP_RUN - m_runs.back(), length);
		length -= max;
		m_runs.back() += max;
	}

	if (length)
	{
		m_runs.add(length);
		result++;
	}

	fb_assert(m_runs.back() > 0 && m_runs.back() <= MAX_NONCOMP_RUN);

	return result;
}

Compressor::Compressor(thread_db* tdbb, FB_SIZE_T length, const UCHAR* data)
	: m_runs(*tdbb->getDefaultPool())
{
	const auto dbb = tdbb->getDatabase();

	const auto end = data + length;
	const auto input = data;

	while (auto count = end - data)
	{
		auto start = data;

		// Find length of non-compressable run

		if (count >= MIN_COMPRESS_RUN)
		{
			auto max = count - 1;
			fb_assert(max > 1);

			do {
				if (data[0] == data[1] && data[0] == data[2])
				{
					count = data - start;
					break;
				}

				data++;

			} while (--max > 1);
		}

		data = start + count;

		// Store this run as non-compressable

		while (count)
		{
			const auto max = MIN(count, MAX_NONCOMP_RUN);
			m_length += nonCompressableRun(max);
			count -= max;
		}

		// Find a compressable run which is long enough.
		// Avoid compressing too short runs, this badly affects decompression speed.

		auto max = end - data;

		if (max < MIN_COMPRESS_RUN)
			continue;

		start = data;
		const auto c = *data;

		do
		{
			if (*data != c)
				break;

			++data;

		} while (--max);

		count = data - start;

		if (count < MIN_COMPRESS_RUN)
		{
			m_length += nonCompressableRun(count);
			continue;
		}

		// We have found a compressable run which is long enough

		if (dbb->getEncodedOdsVersion() >= ODS_13_1)
		{
			m_runs.add(-count);
			m_length += 2 + adjustRunLength(count);
		}
		else // older ODS
		{
			while (count)
			{
				const auto max = MIN(count, MAX_SHORT_RUN);
				m_runs.add(-max);
				m_length += 2;
				count -= max;
			}
		}
	}
}

void Compressor::pack(const UCHAR* input, UCHAR* output) const
{
/**************************************
 *
 *	Compress a string into a sufficiently large area.
 *	Don't check nuttin' -- go for speed, man, raw SPEED!
 *
 **************************************/
	for (const auto length : m_runs)
	{
		if (length < 0)
		{
			const auto zipLength = (unsigned) -length;

			if (zipLength <= MAX_SHORT_RUN)
			{
				*output++ = (UCHAR) length;
			}
			else if (zipLength <= MAX_MEDIUM_RUN)
			{
				*output++ = (UCHAR) -1;
				put_short(output, (SSHORT) zipLength);
				output += sizeof(SSHORT);
			}
			else if (zipLength <= MAX_LONG_RUN)
			{
				*output++ = (UCHAR) -2;
				put_long(output, (SLONG) zipLength);
				output += sizeof(SLONG);
			}
			else
				fb_assert(false);

			*output++ = *input;
			input += zipLength;
		}
		else
		{
			fb_assert(length > 0 && length <= MAX_NONCOMP_RUN);

			*output++ = (UCHAR) length;
			memcpy(output, input, length);
			output += length;
			input += length;
		}
	}
}

FB_SIZE_T Compressor::truncate(FB_SIZE_T outLength)
{
/**************************************
 *
 *	Reset to pack only a leading fragment of the input, restricted by the output length.
 *	Return the number of leading input bytes that fit the given output length.
 *
 **************************************/
	auto space = (int) outLength;
	FB_SIZE_T inLength = 0;
	FB_SIZE_T keepRuns = 0;

	for (auto& length : m_runs)
	{
		m_length = outLength - space;

		if (--space <= 0)
			break;

		if (length < 0)
		{
			const auto zipLength = (unsigned) -length;
			space -= adjustRunLength(zipLength);

			if (--space < 0)
				break;

			inLength += zipLength;
		}
		else
		{
			fb_assert(length > 0 && length <= MAX_NONCOMP_RUN);

			if ((space -= length) < 0)
			{
				length += space; // how many bytes fit

				m_length += length;
				inLength += length;
				keepRuns++;
				break;
			}

			inLength += length;
		}

		keepRuns++;
	}

	if (m_length > outLength)
		BUGCHECK(178);	// msg 178 record length inconsistent

	m_runs.resize(keepRuns);
	return inLength;
}

FB_SIZE_T Compressor::truncateTail(FB_SIZE_T outLength)
{
/**************************************
 *
 *	Reset to pack the input excluding a trailing fragment, restricted by the output length.
 *	Return the number of trailing input bytes that fit the given output length.
 *
 **************************************/
	auto space = (int) outLength;
	FB_SIZE_T inLength = 0;

	while (m_runs.hasData())
	{
		if (--space <= 0)
			break;

		auto length = m_runs.back();

		if (length < 0)
		{
			const auto zipLength = (unsigned) -length;
			length = 1 + adjustRunLength(zipLength);

			if ((space -= length) < 0)
				break;

			m_length -= length + 1;
			inLength += zipLength;
		}
		else
		{
			fb_assert(length > 0 && length <= MAX_NONCOMP_RUN);

			if ((space -= length) < 0)
			{
				length += space; // how many bytes fit

				m_runs.back() -= length;
				m_length -= length;
				inLength += length;
				break;
			}

			m_length -= length + 1;
			inLength += length;
		}

		m_runs.pop();
	}

	fb_assert(m_runs.hasData());

	return inLength;
}

UCHAR* Compressor::unpack(FB_SIZE_T inLength,
						  const UCHAR* input,
						  FB_SIZE_T outLength,
						  UCHAR* output)
{
/**************************************
 *
 *	Decompress a compressed string into a buffer.
 *	Return the address where the output stopped.
 *
 **************************************/
	const auto end = input + inLength;
	const auto output_end = output + outLength;

	while (input < end)
	{
		const int length = (signed char) *input++;

		if (length < 0)
		{
			auto zipLength = (unsigned) -length;

			if (length == -1)
			{
				zipLength = (USHORT) get_short(input);
				input += sizeof(SSHORT);
			}
			else if (length == -2)
			{
				zipLength = (ULONG) get_long(input);
				input += sizeof(SLONG);
			}

			if (input >= end || (output + zipLength) > output_end)
				BUGCHECK(179);	// msg 179 decompression overran buffer

			const auto c = *input++;
			memset(output, c, zipLength);
			output += zipLength;
		}
		else
		{
			if (output + length > output_end)
				BUGCHECK(179);	// msg 179 decompression overran buffer

			memcpy(output, input, length);
			output += length;
			input += length;
		}
	}

	if (output > output_end)
		BUGCHECK(179);			// msg 179 decompression overran buffer

	return output;
}

FB_SIZE_T Compressor::applyDiff(FB_SIZE_T diffLength,
								const UCHAR* differences,
								FB_SIZE_T outLength,
								UCHAR* const output)
{
/**************************************
 *
 *	Apply a differences (delta) to a record.
 *  Return the length.
 *
 **************************************/
	if (diffLength > MAX_DIFFERENCES)
		BUGCHECK(176);			// msg 176 bad difference record

	const auto end = differences + diffLength;
	auto p = output;
	const auto p_end = output + outLength;

	while (differences < end && p < p_end)
	{
		const int l = (signed char) *differences++;

		if (l > 0)
		{
			if (p + l > p_end)
				BUGCHECK(177);	// msg 177 applied differences will not fit in record

			if (differences + l > end)
				BUGCHECK(176);	// msg 176 bad difference record

			memcpy(p, differences, l);
			p += l;
			differences += l;
		}
		else
		{
			p += -l;
		}
	}

	const auto length = p - output;

	if (length > outLength || differences < end)
		BUGCHECK(177);			// msg 177 applied differences will not fit in record

	return length;
}

FB_SIZE_T Compressor::makeNoDiff(FB_SIZE_T outLength, UCHAR* output)
{
/**************************************
 *
 *  Generates differences record marking that there are no differences.
 *
 **************************************/
	auto temp = output;
	auto length = (int) outLength;

	while (length > 127)
	{
		*temp++ = -127;
		length -= 127;
	}

	if (length)
		*temp++ = (UCHAR) -length;

	return temp - output;
}

FB_SIZE_T Compressor::makeDiff(FB_SIZE_T length1,
							   const UCHAR* rec1,
							   FB_SIZE_T length2,
							   UCHAR* rec2,
							   FB_SIZE_T outLength,
							   UCHAR* output)
{
/**************************************
 *
 *	Compute differences between two records. The difference
 *	record, when applied to the first record, produces the
 *	second record.
 *
 *	    difference_record := <control_string>...
 *
 *	    control_string := <positive_integer> <positive_integer data bytes>
 *				:= <negative_integer>
 *
 *	Return the total length of the differences string.
 *
 **************************************/
	UCHAR *p;

#define STUFF(val)	if (output < end) *output++ = val; else return MAX_ULONG;

	/* WHY IS THIS RETURNING MAX_ULONG ???
	* It returns a large positive value to indicate to the caller that we ran out
	* of buffer space in the 'out' argument. Thus we could not create a
	* successful differences record. Now it is upto the caller to check the
	* return value of this function and figure out whether the differences record
	* was created or not. Check prepare_update() (JRD/vio.c) for further
	* information. Of course, the size for a 'differences' record is not expected
	* to go near 2^32 in the future.
	*
	* This was investigated as a part of solving bug 10206, bsriram - 25-Feb-1999.
	*/

	const auto start = output;
	const auto end = output + outLength;
	const auto end1 = rec1 + MIN(length1, length2);
	const auto end2 = rec2 + length2;

	while (end1 - rec1 > 2)
	{
		if (rec1[0] != rec2[0] || rec1[1] != rec2[1])
		{
			p = output++;

			// cast this to LONG to take care of OS/2 pointer arithmetic
			// when rec1 is at the end of a segment, to avoid wrapping around

			const auto yellow = (UCHAR*) MIN((U_IPTR) end1, ((U_IPTR) rec1 + 127)) - 1;
			while (rec1 <= yellow && (rec1[0] != rec2[0] || (rec1 < yellow && rec1[1] != rec2[1])))
			{
				STUFF(*rec2++);
				++rec1;
			}

			*p = output - p - 1;
			continue;
		}

		for (p = rec2; rec1 < end1 && *rec1 == *rec2; rec1++, rec2++)
			; // no-op

		// This "l" could be more than 32K since the Old and New records
		// could be the same for more than 32K characters.
		// MAX record size is currently 64K. Hence it is defined as "int".
		int l = p - rec2;

		while (l < -127)
		{
			STUFF(-127);
			l += 127;
		}

		if (l)
			STUFF(l);
	}

	while (rec2 < end2)
	{
		p = output++;

		// cast this to LONG to take care of OS/2 pointer arithmetic
		// when rec1 is at the end of a segment, to avoid wrapping around

		const auto yellow = (UCHAR*) MIN((U_IPTR) end2, ((U_IPTR) rec2 + 127));
		while (rec2 < yellow)
			STUFF(*rec2++);

		*p = output - p - 1;
	}

	return output - start;
#undef STUFF
}
