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
#include "../jrd/tra_proto.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceDSQLHelpers.h"

#include "../dsql/DsqlCursor.h"

using namespace Firebird;
using namespace Jrd;

static const char* const SCRATCH = "fb_cursor_";
static const ULONG PREFETCH_SIZE = 65536; // 64 KB

DsqlCursor::DsqlCursor(DsqlDmlRequest* req, ULONG flags)
	: m_request(req), m_message(req->getStatement()->getReceiveMsg()),
	  m_resultSet(NULL), m_flags(flags),
	  m_space(req->getPool(), SCRATCH),
	  m_state(BOS), m_eof(false), m_position(0), m_cachedCount(0)
{
	TRA_link_cursor(m_request->req_transaction, this);
}

DsqlCursor::~DsqlCursor()
{
	if (m_resultSet)
		m_resultSet->resetHandle();
}

jrd_tra* DsqlCursor::getTransaction() const
{
	return m_request->req_transaction;
}

Attachment* DsqlCursor::getAttachment() const
{
	return m_request->req_dbb->dbb_attachment;
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
	const auto request = cursor->m_request;

	if (request->getJrdRequest())
	{
		ThreadStatusGuard status_vector(tdbb);
		try
		{
			// Report some remaining fetches if any
			if (request->req_fetch_baseline)
			{
				TraceDSQLFetch trace(attachment, request);
				trace.fetch(true, ITracePlugin::RESULT_SUCCESS);
			}

			if (request->req_traced && TraceManager::need_dsql_free(attachment))
			{
				TraceSQLStatementImpl stmt(request, NULL);
				TraceManager::event_dsql_free(attachment, &stmt, DSQL_close);
			}

			JRD_unwind_request(tdbb, request->getJrdRequest());
		}
		catch (Firebird::Exception&)
		{} // no-op
	}

	request->req_cursor = NULL;
	TRA_unlink_cursor(request->req_transaction, cursor);
	delete cursor;
}

int DsqlCursor::fetchNext(thread_db* tdbb, UCHAR* buffer)
{
	if (!(m_flags & IStatement::CURSOR_TYPE_SCROLLABLE))
	{
		m_eof = !m_request->fetch(tdbb, buffer);

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

	UCHAR* const msgBuffer = m_request->req_msg_buffers[m_message->msg_buffer_number];

	const FB_UINT64 offset = position * m_message->msg_length;
	const FB_UINT64 readBytes = m_space.read(offset, msgBuffer, m_message->msg_length);
	fb_assert(readBytes == m_message->msg_length);

	m_request->mapInOut(tdbb, true, m_message, NULL, buffer);

	m_position = position;
	m_state = POSITIONED;
	return 0;
}

bool DsqlCursor::cacheInput(thread_db* tdbb, FB_UINT64 position)
{
	fb_assert(!m_eof);

	const ULONG prefetchCount = MAX(PREFETCH_SIZE / m_message->msg_length, 1);
	const UCHAR* const msgBuffer = m_request->req_msg_buffers[m_message->msg_buffer_number];

	while (position >= m_cachedCount)
	{
		for (ULONG count = 0; count < prefetchCount; count++)
		{
			if (!m_request->fetch(tdbb, NULL))
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
