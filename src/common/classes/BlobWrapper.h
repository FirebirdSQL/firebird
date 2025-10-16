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

#ifndef FB_USER_BLOB_H
#define FB_USER_BLOB_H

#include "firebird/Interface.h"
#include <memory.h>
#include "../common/status.h"

class BlobWrapper
{
public:
	explicit BlobWrapper(Firebird::CheckStatusWrapper* status)
		: m_status(status ? status : &m_default_status), m_blob(nullptr), m_direction(dir_none)
	{ }

	~BlobWrapper()
	{
		close(true);
	}

	bool open(Firebird::IAttachment* db, Firebird::ITransaction* trans, ISC_QUAD& blobid,
				USHORT bpb_len = 0, const UCHAR* bpb = nullptr);
	bool create(Firebird::IAttachment* db, Firebird::ITransaction* trans, ISC_QUAD& blobid,
				USHORT bpb_len = 0, const UCHAR* bpb = nullptr);
	bool close(bool force_internal_SV = false);
	bool getSegment(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len);
	bool getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len, bool use_sep = false, const UCHAR separator = '\0');
	bool putSegment(FB_SIZE_T len, const void* buffer);
	bool putSegment(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len);
	bool putData(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len);
	bool putData(FB_SIZE_T len, const void* buffer)
	{
		FB_SIZE_T dummy;
		return putData(len, buffer, dummy);
	}

	bool isOpen() const
	{
		return m_blob != 0 && m_direction != dir_none;
	}

	ISC_STATUS getCode() const
	{
		return m_status->getErrors()[1];
	}

	bool getInfo(FB_SIZE_T items_size, const UCHAR* items, FB_SIZE_T info_size, UCHAR* blob_info) const;
	bool getSize(FB_UINT64* size, ULONG* seg_count, USHORT* max_seg) const;

	static bool blobIsNull(const ISC_QUAD& blobid)
	{
		return blobid.gds_quad_high == 0 && blobid.gds_quad_low == 0;
	}

private:
	enum b_direction
	{
		dir_none,
		dir_read,
		dir_write
	};

	Firebird::FbLocalStatus m_default_status;
	Firebird::CheckStatusWrapper* const m_status;
	Firebird::IBlob* m_blob;
	b_direction m_direction;
};



#endif // FB_USER_BLOB_H

