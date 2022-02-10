/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceDSQLHelpers.h
 *	DESCRIPTION:	Trace API manager support
 *
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef JRD_TRACE_DSQL_HELPERS_H
#define JRD_TRACE_DSQL_HELPERS_H

#include "../../jrd/trace/TraceManager.h"
#include "../../jrd/trace/TraceObjects.h"

namespace Jrd {

using Firebird::ITracePlugin;
using Firebird::ITraceFactory;

class TraceDSQLPrepare
{
public:
	TraceDSQLPrepare(Attachment* attachment, jrd_tra* transaction,
				FB_SIZE_T string_length, const TEXT* string)
		: m_attachment(attachment),
		  m_transaction(transaction),
		  m_request(NULL),
		  m_string_len(string_length),
		  m_string(string)
	{
		m_need_trace = TraceManager::need_dsql_prepare(m_attachment);
		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();

		static const char empty_string[] = "";
		if (!m_string_len || !string)
		{
			m_string = empty_string;
			m_string_len = 0;
		}
	}

	~TraceDSQLPrepare()
	{
		prepare(ITracePlugin::RESULT_FAILED);
	}

	void setStatement(DsqlRequest* request)
	{
		m_request = request;
	}

	void prepare(ntrace_result_t result)
	{
		if (m_request) {
			m_need_trace = (m_need_trace && m_request->req_traced);
		}
		if (!m_need_trace)
			return;

		m_need_trace = false;
		const SINT64 millis = (fb_utils::query_performance_counter() - m_start_clock) * 1000 /
			fb_utils::query_performance_frequency();

		if ((result == ITracePlugin::RESULT_SUCCESS) && m_request)
		{
			TraceSQLStatementImpl stmt(m_request, NULL);
			TraceManager::event_dsql_prepare(m_attachment, m_transaction, &stmt, millis, result);
		}
		else
		{
			Firebird::string str(*getDefaultMemoryPool(), m_string, m_string_len);

			TraceFailedSQLStatement stmt(str);
			TraceManager::event_dsql_prepare(m_attachment, m_transaction, &stmt, millis, result);
		}
	}

private:
	bool m_need_trace;
	Attachment* m_attachment;
	jrd_tra* const m_transaction;
	DsqlRequest* m_request;
	SINT64 m_start_clock;
	FB_SIZE_T m_string_len;
	const TEXT* m_string;
};


class TraceDSQLExecute
{
public:
	TraceDSQLExecute(Attachment* attachment, DsqlRequest* dsqlRequest) :
		m_attachment(attachment),
		m_dsqlRequest(dsqlRequest)
	{
		m_need_trace = m_dsqlRequest->req_traced && TraceManager::need_dsql_execute(m_attachment);
		if (!m_need_trace)
			return;

		{	// scope
			TraceSQLStatementImpl stmt(dsqlRequest, NULL);
			TraceManager::event_dsql_execute(m_attachment, dsqlRequest->req_transaction, &stmt, true,
				ITracePlugin::RESULT_SUCCESS);
		}

		m_start_clock = fb_utils::query_performance_counter();

		m_dsqlRequest->req_fetch_elapsed = 0;
		m_dsqlRequest->req_fetch_rowcount = 0;
		fb_assert(!m_dsqlRequest->req_fetch_baseline);
		m_dsqlRequest->req_fetch_baseline = NULL;

		if (auto request = m_dsqlRequest->getRequest())
		{
			MemoryPool* pool = MemoryPool::getContextPool();
			m_dsqlRequest->req_fetch_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, request->req_stats);
		}
	}

	void finish(bool have_cursor, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;
		if (have_cursor)
		{
			m_dsqlRequest->req_fetch_elapsed = fb_utils::query_performance_counter() - m_start_clock;
			return;
		}

		TraceRuntimeStats stats(m_attachment, m_dsqlRequest->req_fetch_baseline,
			m_dsqlRequest->getRequest() ? &m_dsqlRequest->getRequest()->req_stats : NULL,
			fb_utils::query_performance_counter() - m_start_clock,
			m_dsqlRequest->req_fetch_rowcount);

		TraceSQLStatementImpl stmt(m_dsqlRequest, stats.getPerf());
		TraceManager::event_dsql_execute(m_attachment, m_dsqlRequest->req_transaction, &stmt, false, result);

		m_dsqlRequest->req_fetch_baseline = NULL;
	}

	~TraceDSQLExecute()
	{
		finish(false, ITracePlugin::RESULT_FAILED);
	}

private:
	bool m_need_trace;
	Attachment* const m_attachment;
	DsqlRequest* const m_dsqlRequest;
	SINT64 m_start_clock;
};

class TraceDSQLFetch
{
public:
	TraceDSQLFetch(Attachment* attachment, DsqlRequest* request) :
		m_attachment(attachment),
		m_dsqlRequest(request)
	{
		m_need_trace = m_dsqlRequest->req_traced && TraceManager::need_dsql_execute(m_attachment) &&
					   m_dsqlRequest->getRequest() && (m_dsqlRequest->getRequest()->req_flags & req_active);

		if (!m_need_trace)
		{
			m_dsqlRequest->req_fetch_baseline = NULL;
			return;
		}

		m_start_clock = fb_utils::query_performance_counter();
	}

	~TraceDSQLFetch()
	{
		fetch(true, ITracePlugin::RESULT_FAILED);
	}

	void fetch(bool eof, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;
		m_dsqlRequest->req_fetch_elapsed += fb_utils::query_performance_counter() - m_start_clock;
		if (!eof)
		{
			m_dsqlRequest->req_fetch_rowcount++;
			return;
		}

		TraceRuntimeStats stats(m_attachment, m_dsqlRequest->req_fetch_baseline,
			&m_dsqlRequest->getRequest()->req_stats, m_dsqlRequest->req_fetch_elapsed,
			m_dsqlRequest->req_fetch_rowcount);

		TraceSQLStatementImpl stmt(m_dsqlRequest, stats.getPerf());

		TraceManager::event_dsql_execute(m_attachment, m_dsqlRequest->req_transaction,
			&stmt, false, result);

		m_dsqlRequest->req_fetch_elapsed = 0;
		m_dsqlRequest->req_fetch_baseline = NULL;
	}

private:
	bool m_need_trace;
	Attachment* const m_attachment;
	DsqlRequest* const m_dsqlRequest;
	SINT64 m_start_clock;
};


} // namespace Jrd

#endif // JRD_TRACE_DSQL_HELPERS_H
