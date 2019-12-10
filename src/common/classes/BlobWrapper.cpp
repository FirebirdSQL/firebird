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
 *  The Original Code was created by Claudio Valderrama on 16-Mar-2007
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Claudio Valderrama
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *	Alex Peshkoff, 2017
 *
 */

#include "firebird.h"
#include "BlobWrapper.h"
#include "ibase.h"
#include "firebird/Interface.h"

static const USHORT SEGMENT_LIMIT = 65535;

using namespace Firebird;

bool BlobWrapper::open(IAttachment* db, ITransaction* trans, ISC_QUAD& blobid,
					USHORT bpb_len, const UCHAR* bpb)
{
	if (m_direction != dir_none)
		return false;

	if ((bpb_len > 0 && !bpb) || blobIsNull(blobid))
		return false;

	m_blob = db->openBlob(m_status, trans, &blobid, bpb_len, bpb);
	if (m_status->isEmpty())
	{
		m_direction = dir_read;
		return true;
	}
	return false;
}

bool BlobWrapper::create(IAttachment* db, ITransaction* trans, ISC_QUAD& blobid,
			USHORT bpb_len, const UCHAR* bpb)
{
	if (m_direction != dir_none)
		return false;

	if (bpb_len > 0 && !bpb)
		return false;

	blobid.gds_quad_high = blobid.gds_quad_low = 0;
	m_blob = db->createBlob(m_status, trans, &blobid, bpb_len, bpb);
	if (m_status->isEmpty())
	{
		m_direction = dir_write;
		return true;
	}
	return false;
}

bool BlobWrapper::close(bool force_internal_SV)
{
	bool rc = false;
	if (m_blob)
	{
		m_blob->close(force_internal_SV ? &m_default_status : m_status);
		rc = (force_internal_SV ? &m_default_status : m_status)->isEmpty();
		if (rc)
			m_blob = nullptr;
		m_direction = dir_none;
	}
	return rc;
}

bool BlobWrapper::getSegment(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len)
{
	real_len = 0;

	if (!m_blob || m_direction != dir_read)
		return false;

	if (len && !buffer)
		return false;

	unsigned ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<unsigned>(len);
	unsigned olen = 0;
	bool eof = m_blob->getSegment(m_status, ilen, buffer, &olen) == Firebird::IStatus::RESULT_NO_DATA;
	if (m_status->isEmpty() && !eof)
	{
		real_len = olen;
		return true;
	}
	return false;
}

bool BlobWrapper::getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len,
						bool use_sep, const UCHAR separator)
{
#ifdef DEV_BUILD
	if (!m_blob || m_direction != dir_read)
		return false;

	if (!len || !buffer)
		return false;
#endif

	bool rc = false;
	real_len = 0;
	char* buf2 = static_cast<char*>(buffer);
	while (len)
	{
		unsigned olen = 0;
		bool eof = m_blob->getSegment(m_status, len, buf2, &olen) == Firebird::IStatus::RESULT_NO_DATA;
		if (m_status->isEmpty() && !eof)
		{
			len -= olen;
			buf2 += olen;
			real_len += olen;

			if (len && use_sep) // Append the segment separator.
			{
				--len;
				*buf2++ = separator;
				++real_len;
			}

			rc = true;
		}
		else
			break;
	}

	return rc;
}

bool BlobWrapper::putSegment(FB_SIZE_T len, const void* buffer)
{
#ifdef DEV_BUILD
	if (!m_blob || m_direction != dir_write)
		return false;

	if (len > 0 && !buffer)
		return false;
#endif

	unsigned ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<unsigned>(len);
	m_blob->putSegment(m_status, ilen, buffer);
	return m_status->isEmpty();
}

bool BlobWrapper::putSegment(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len)
{
#ifdef DEV_BUILD
	if (!m_blob || m_direction == dir_read)
		return false;

	if (len > 0 && !buffer)
		return false;
#endif

	real_len = 0;
	unsigned ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<unsigned>(len);
	m_blob->putSegment(m_status, ilen, buffer);

	if (!m_status->isEmpty())
		return false;

	real_len = ilen;
	return true;
}

bool BlobWrapper::putData(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len)
{
	if (!m_blob || m_direction == dir_read)
		return false;

	if (len > 0 && !buffer)
		return false;

	real_len = 0;
	m_blob->putSegment(m_status, len, buffer);

	if (!m_status->isEmpty())
		return false;

	real_len = len;
	return true;
}

bool BlobWrapper::getInfo(FB_SIZE_T items_size, const UCHAR* items,
					   FB_SIZE_T info_size, UCHAR* blob_info) const
{
	if (!m_blob || m_direction != dir_read)
		return false;

	m_blob->getInfo(m_status, items_size, items, info_size, blob_info);

	return m_status->isEmpty();
}

bool BlobWrapper::getSize(SLONG* size, SLONG* seg_count, SLONG* max_seg) const
{
/**************************************
 *
 *	g e t B l o b S i z e
 *
 **************************************
 *
 * Functional description
 *	Get the size, number of segments, and max
 *	segment length of a blob.  Return true
 *	if it happens to succeed.
 *	This is a clone of gds__blob_size.
 *
 **************************************/
	static const UCHAR blob_items[] =
	{
		isc_info_blob_max_segment,
		isc_info_blob_num_segments,
		isc_info_blob_total_length
	};

	UCHAR buffer[64];

	if (!getInfo(sizeof(blob_items), blob_items, sizeof(buffer), buffer))
		return false;

	const UCHAR* p = buffer;
	const UCHAR* const end = buffer + sizeof(buffer);

	for (UCHAR item = *p++; item != isc_info_end && p < end; item = *p++)
	{
		const USHORT l = gds__vax_integer(p, 2);
		p += 2;
		const SLONG n = gds__vax_integer(p, l);
		p += l;

		switch (item)
		{
		case isc_info_blob_max_segment:
			if (max_seg)
				*max_seg = n;
			break;

		case isc_info_blob_num_segments:
			if (seg_count)
				*seg_count = n;
			break;

		case isc_info_blob_total_length:
			if (size)
				*size = n;
			break;

		default:
			return false;
		}
	}

	return true;
}
