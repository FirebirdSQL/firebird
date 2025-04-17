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
 *
 */

#ifndef FB_USER_BLOB_H
#define FB_USER_BLOB_H

#include "firebird.h"
#include <memory.h>

class UserBlob
{
public:
	explicit UserBlob(ISC_STATUS* status);
	~UserBlob();
	bool open(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid);
	bool open(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid,
				USHORT bpb_len, const UCHAR* bpb);
	bool create(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid);
	bool create(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid,
				USHORT bpb_len, const UCHAR* bpb);
	bool close(bool force_internal_SV = false);
	bool getSegment(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len);
	bool getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len);
	bool getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len, bool use_sep, const UCHAR separator);
	bool putSegment(FB_SIZE_T len, const void* buffer);
	bool putSegment(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len);
	bool putData(FB_SIZE_T len, const void* buffer);
	bool putData(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len);
	bool isOpen() const;
	ISC_STATUS getCode() const;
//	FB_API_HANDLE& getHandle();
	bool getInfo(FB_SIZE_T items_size, const UCHAR* items, FB_SIZE_T info_size, UCHAR* blob_info) const;
	static bool blobIsNull(const ISC_QUAD& blobid);
private:
	enum b_direction
	{
		dir_none,
		dir_read,
		dir_write
	};
	ISC_STATUS* const m_status;
	FB_API_HANDLE m_blob;
	b_direction m_direction;
	ISC_STATUS_ARRAY m_default_status;
};


inline UserBlob::UserBlob(ISC_STATUS* status)
	: m_status(status ? status : m_default_status), m_blob(0), m_direction(dir_none)
{
	memset(m_default_status, 0, sizeof(m_default_status));
}

inline UserBlob::~UserBlob()
{
	close(true);
}

inline bool UserBlob::isOpen() const
{
	return m_blob != 0 && m_direction != dir_none;
}

// Do not call it after close(true) unless you did open() or create() again!!!
inline ISC_STATUS UserBlob::getCode() const
{
	return m_status[1];
}

//inline FB_API_HANDLE& UserBlob::getHandle()
//{
//	return m_blob;
//}

inline bool UserBlob::blobIsNull(const ISC_QUAD& blobid)
{
	return blobid.gds_quad_high == 0 && blobid.gds_quad_low == 0;
}

inline bool UserBlob::putData(FB_SIZE_T len, const void* buffer)
{
	FB_SIZE_T dummy;
	return putData(len, buffer, dummy);
}

inline bool UserBlob::getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len)
{
	return getData(len, buffer, real_len, false, '\0');
}

bool getBlobSize(	const UserBlob& b,
					FB_UINT64* size,
					ULONG* seg_count,
					USHORT* max_seg);

#endif // FB_USER_BLOB_H

