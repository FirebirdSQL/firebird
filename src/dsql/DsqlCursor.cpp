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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Dmitry Yemanov <dimitrf@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/ClumpletWriter.h"
#include "../jrd/tra_proto.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceDSQLHelpers.h"

#include "../dsql/dsql_proto.h"
#include "../dsql/DsqlCursor.h"
#include "../dsql/StmtNodes.h"

using namespace Firebird;
using namespace Jrd;

static const char* const SCRATCH = "fb_cursor_";
static const ULONG PREFETCH_SIZE = 65536; // 64 KB

DsqlCursor::DsqlCursor(DsqlDmlRequest* req, ULONG flags)
	: m_dsqlRequest(req),
	  m_message(req->getDsqlStatement()->getReceiveMsg()->msg_number),
	  m_flags(flags),
	  m_space(req->getPool(), SCRATCH)
{
	TRA_link_cursor(m_dsqlRequest->req_transaction, this);
}

DsqlCursor::~DsqlCursor()
{
	if (m_resultSet)
		m_resultSet->resetHandle();

	delete[] m_keyBuffer;
}

jrd_tra* DsqlCursor::getTransaction() const
{
	return m_dsqlRequest->req_transaction;
}

Attachment* DsqlCursor::getAttachment() const
{
	return m_dsqlRequest->req_dbb->dbb_attachment;
}

void DsqlCursor::setInterfacePtr(JResultSet* interfacePtr) noexcept
{
	fb_assert(!m_resultSet);
	m_resultSet = interfacePtr;
}

bool DsqlCursor::getCurrentRecordKey(USHORT context, RecordKey& key) const
{
	if (m_keyBuffer == nullptr || context * sizeof(RecordKey) >= m_keyBufferLength)
	{
		fb_assert(false);
		return false;
	}

	if (m_state != POSITIONED)
	{
		return false;
	}

	key = m_keyBuffer[context];
	return key.recordNumber.bid_relation_id != 0;
}

void DsqlCursor::close(thread_db* tdbb, DsqlCursor* cursor)
{
	if (!cursor)
		return;

	const auto attachment = cursor->getAttachment();
	const auto dsqlRequest = cursor->m_dsqlRequest;

	if (dsqlRequest->getRequest())
	{
		ThreadStatusGuard status_vector(tdbb);
		try
		{
			// Report some remaining fetches if any
			if (dsqlRequest->req_fetch_baseline)
			{
				TraceDSQLFetch trace(attachment, dsqlRequest);
				trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
			}

			if (dsqlRequest->req_traced && TraceManager::need_dsql_free(attachment))
			{
				TraceSQLStatementImpl stmt(dsqlRequest, nullptr, nullptr);
				TraceManager::event_dsql_free(attachment, &stmt, DSQL_close);
			}

			JRD_unwind_request(tdbb, dsqlRequest->getRequest());
		}
		catch (Firebird::Exception&)
		{} // no-op
	}

	dsqlRequest->req_cursor = NULL;
	TRA_unlink_cursor(dsqlRequest->req_transaction, cursor);
	delete cursor;
}

int DsqlCursor::fetchNext(thread_db* tdbb, UCHAR* buffer)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
	{
		m_eof = !m_dsqlRequest->fetch(tdbb, buffer);

		if (m_eof)
		{
			m_state = EOS;
			return 1;
		}

		if (m_keyBufferLength == 0)
		{
			Request* req = m_dsqlRequest->getRequest();
			m_keyBufferLength = req->req_rpb.getCount() * sizeof(RecordKey);
			fb_assert(m_keyBufferLength > 0);
			m_keyBuffer = FB_NEW_POOL(m_dsqlRequest->getPool()) RecordKey[req->req_rpb.getCount()];
		}

		m_dsqlRequest->gatherRecordKey(m_keyBuffer);
		m_state = POSITIONED;
		return 0;
	}

	return fetchRelative(tdbb, buffer, 1);
}

int DsqlCursor::fetchPrior(thread_db* tdbb, UCHAR* buffer)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
		(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("PRIOR")).raise();

	return fetchRelative(tdbb, buffer, -1);
}

int DsqlCursor::fetchFirst(thread_db* tdbb, UCHAR* buffer)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
		(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("FIRST")).raise();

	return fetchAbsolute(tdbb, buffer, 1);
}

int DsqlCursor::fetchLast(thread_db* tdbb, UCHAR* buffer)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
		(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("LAST")).raise();

	return fetchAbsolute(tdbb, buffer, -1);
}

int DsqlCursor::fetchAbsolute(thread_db* tdbb, UCHAR* buffer, SLONG position)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
		(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("ABSOLUTE")).raise();

	if (!position)
	{
		m_state = BOS;
		return -1;
	}

	SINT64 offset = -1;

	if (position < 0)
	{
		if (!m_eof)
		{
			cacheInput(tdbb, buffer);
			fb_assert(m_eof);
		}

		offset = m_cachedCount;
	}

	if (position + offset < 0)
	{
		m_state = BOS;
		return -1;
	}

	return fetchFromCache(tdbb, buffer, position + offset);
}

int DsqlCursor::fetchRelative(thread_db* tdbb, UCHAR* buffer, SLONG offset)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
		(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("RELATIVE")).raise();

	SINT64 position = m_position + offset;

	if (m_state == BOS)
	{
		if (offset <= 0)
			return -1;

		position = offset - 1;
	}
	else if (m_state == EOS)
	{
		if (offset >= 0)
			return 1;

		fb_assert(m_eof);

		position = m_cachedCount + offset;
	}

	if (position < 0)
	{
		m_state = BOS;
		return -1;
	}

	return fetchFromCache(tdbb, buffer, position);
}

void DsqlCursor::getInfo(thread_db* tdbb,
						 unsigned int itemsLength, const unsigned char* items,
						 unsigned int bufferLength, unsigned char* buffer)
{
	if (bufferLength < 7) // isc_info_error + 2-byte length + 4-byte error code
	{
		if (bufferLength)
			*buffer = isc_info_truncated;
		return;
	}

	const bool isScrollable = (m_flags & IStatement::CURSOR_TYPE_SCROLLABLE);

	ClumpletWriter response(ClumpletReader::InfoResponse, bufferLength - 1); // isc_info_end
	ISC_STATUS errorCode = 0;
	bool needLength = false, completed = false;

	try
	{
		ClumpletReader infoItems(ClumpletReader::InfoItems, items, itemsLength);
		for (infoItems.rewind(); !errorCode && !infoItems.isEof(); infoItems.moveNext())
		{
			const auto tag = infoItems.getClumpTag();

			switch (tag)
			{
			case isc_info_end:
				break;

			case isc_info_length:
				needLength = true;
				break;

			case IResultSet::INF_RECORD_COUNT:
				if (isScrollable && !m_eof)
				{
					cacheInput(tdbb, nullptr);
					fb_assert(m_eof);
				}
				response.insertInt(tag, isScrollable ? m_cachedCount : -1);
				break;

			default:
				errorCode = isc_infunk;
				break;
			}
		}

		completed = infoItems.isEof();

		if (needLength && completed)
		{
			response.rewind();
			response.insertInt(isc_info_length, response.getBufferLength() + 1); // isc_info_end
		}
	}
	catch (const Exception&)
	{
		if (!response.hasOverflow())
			throw;
	}

	if (errorCode)
	{
		response.clear();
		response.insertInt(isc_info_error, (SLONG) errorCode);
	}

	fb_assert(response.getBufferLength() <= bufferLength);
	memcpy(buffer, response.getBuffer(), response.getBufferLength());
	buffer += response.getBufferLength();

	*buffer = completed ? isc_info_end : isc_info_truncated;
}

int DsqlCursor::fetchFromCache(thread_db* tdbb, UCHAR* buffer, FB_UINT64 position)
{
	if (position >= m_cachedCount)
	{
		if (m_eof || !cacheInput(tdbb, buffer, position))
		{
			m_state = EOS;
			return 1;
		}
	}

	fb_assert(position < m_cachedCount);
	fb_assert(m_messageLength > 0); // At this point m_messageLength must be set by cacheInput

	FB_UINT64 offset = position * (m_messageLength + m_keyBufferLength);
	FB_UINT64 readBytes = m_space.read(offset, buffer, m_messageLength);
	offset += m_messageLength;
	readBytes += m_space.read(offset, m_keyBuffer, m_keyBufferLength);
	fb_assert(readBytes == m_messageLength + m_keyBufferLength);

	m_position = position;
	m_state = POSITIONED;
	return 0;
}

bool DsqlCursor::cacheInput(thread_db* tdbb, UCHAR* buffer, FB_UINT64 position)
{
	fb_assert(!m_eof);

	// It could not be done before: user buffer length may be unknown until call setDelayedOutputMetadata()
	if (m_messageLength == 0)
	{
		Request* req = m_dsqlRequest->getRequest();
		const MessageNode* msg = req->getStatement()->getMessage(m_message);
		m_messageLength = msg->getFormat(req)->fmt_length;
		// Save record key unconditionally because setCursorName() can be called after openCursor()
		m_keyBufferLength = req->req_rpb.getCount() * sizeof(RecordKey);
		m_keyBuffer = FB_NEW_POOL(m_dsqlRequest->getPool()) RecordKey[req->req_rpb.getCount()];
	}

	std::unique_ptr<UCHAR[]> ownBuffer;
	if (buffer == nullptr)
	{
		// We are called from getInfo() and there is no user-provided buffer for data.
		// Create a temporary one.
		// This code cannot be moved into getInfo() itself because it is most likely called before fetch()
		// so m_messageLength is still unknown there.
		ownBuffer.reset(buffer = new UCHAR[m_messageLength]);
	}

	const ULONG prefetchCount = MAX(PREFETCH_SIZE / (m_messageLength + m_keyBufferLength), 1);

	while (position >= m_cachedCount)
	{
		for (ULONG count = 0; count < prefetchCount; count++)
		{
			if (!m_dsqlRequest->fetch(tdbb, buffer))
			{
				m_eof = true;
				break;
			}

			m_dsqlRequest->gatherRecordKey(m_keyBuffer);

			FB_UINT64 offset = m_cachedCount * (m_messageLength + m_keyBufferLength);
			FB_UINT64 writtenBytes = m_space.write(offset, buffer, m_messageLength);
			offset += m_messageLength;
			writtenBytes += m_space.write(offset, m_keyBuffer, m_keyBufferLength);
			fb_assert(writtenBytes == m_messageLength + m_keyBufferLength);
			m_cachedCount++;
		}

		if (m_eof)
			break;
	}

	return (position < m_cachedCount);
}
