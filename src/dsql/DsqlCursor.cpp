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

using namespace Firebird;
using namespace Jrd;

static const char* const SCRATCH = "fb_cursor_";
static const ULONG PREFETCH_SIZE = 65536; // 64 KB

DsqlCursor::DsqlCursor(DsqlDmlRequest* req, ULONG flags)
	: m_dsqlRequest(req), m_message(req->getDsqlStatement()->getReceiveMsg()),
	  m_resultSet(NULL), m_flags(flags),
	  m_space(req->getPool(), SCRATCH),
	  m_state(BOS), m_eof(false), m_position(0), m_cachedCount(0)
{
	TRA_link_cursor(m_dsqlRequest->req_transaction, this);
}

DsqlCursor::~DsqlCursor()
{
	if (m_resultSet)
		m_resultSet->resetHandle();
}

jrd_tra* DsqlCursor::getTransaction() const
{
	return m_dsqlRequest->req_transaction;
}

Attachment* DsqlCursor::getAttachment() const
{
	return m_dsqlRequest->req_dbb->dbb_attachment;
}

void DsqlCursor::setInterfacePtr(JResultSet* interfacePtr) throw()
{
	fb_assert(!m_resultSet);
	m_resultSet = interfacePtr;
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
				TraceSQLStatementImpl stmt(dsqlRequest, NULL);
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
			cacheInput(tdbb);
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
					cacheInput(tdbb);
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
		if (m_eof || !cacheInput(tdbb, position))
		{
			m_state = EOS;
			return 1;
		}
	}

	fb_assert(position < m_cachedCount);

	UCHAR* const msgBuffer = m_dsqlRequest->req_msg_buffers[m_message->msg_buffer_number];

	const FB_UINT64 offset = position * m_message->msg_length;
	const FB_UINT64 readBytes = m_space.read(offset, msgBuffer, m_message->msg_length);
	fb_assert(readBytes == m_message->msg_length);

	m_dsqlRequest->mapInOut(tdbb, true, m_message, NULL, buffer);

	m_position = position;
	m_state = POSITIONED;
	return 0;
}

bool DsqlCursor::cacheInput(thread_db* tdbb, FB_UINT64 position)
{
	fb_assert(!m_eof);

	const ULONG prefetchCount = MAX(PREFETCH_SIZE / m_message->msg_length, 1);
	const UCHAR* const msgBuffer = m_dsqlRequest->req_msg_buffers[m_message->msg_buffer_number];

	while (position >= m_cachedCount)
	{
		for (ULONG count = 0; count < prefetchCount; count++)
		{
			if (!m_dsqlRequest->fetch(tdbb, NULL))
			{
				m_eof = true;
				break;
			}

			const FB_UINT64 offset = m_cachedCount * m_message->msg_length;
			const FB_UINT64 writtenBytes = m_space.write(offset, msgBuffer, m_message->msg_length);
			fb_assert(writtenBytes == m_message->msg_length);
			m_cachedCount++;
		}

		if (m_eof)
			break;
	}

	return (position < m_cachedCount);
}
