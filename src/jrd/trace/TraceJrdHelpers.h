/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceJrdHelpers.h
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

#ifndef JRD_TRACE_JRD_HELPERS_H
#define JRD_TRACE_JRD_HELPERS_H

#include "../../jrd/jrd.h"
#include "../../jrd/trace/TraceManager.h"
#include "../../jrd/trace/TraceObjects.h"

namespace Jrd {

using Firebird::ITracePlugin;
using Firebird::ITraceFactory;

class TraceTransactionEnd
{
public:
	TraceTransactionEnd(jrd_tra* transaction, bool commit, bool retain) :
		m_commit(commit),
		m_retain(retain),
		m_transaction(transaction),
		m_prevID(transaction->tra_number),
		m_baseline(NULL)
	{
		Attachment* attachment = m_transaction->tra_attachment;
		m_need_trace = attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_TRANSACTION_END);
		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();
		MemoryPool* pool = m_transaction->tra_pool;
		m_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, m_transaction->tra_stats);
	}

	~TraceTransactionEnd()
	{
		finish(ITracePlugin::RESULT_FAILED);
	}

	void finish(ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		Attachment* attachment = m_transaction->tra_attachment;

		TraceRuntimeStats stats(attachment, m_baseline, &m_transaction->tra_stats,
			fb_utils::query_performance_counter() - m_start_clock, 0);

		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(m_transaction, stats.getPerf(), m_prevID);

		attachment->att_trace_manager->event_transaction_end(&conn, &tran, m_commit, m_retain, result);
		m_baseline = NULL;
	}

private:
	bool m_need_trace;
	const bool m_commit;
	const bool m_retain;
	jrd_tra* const m_transaction;
	const ISC_INT64 m_prevID;
	SINT64 m_start_clock;
	Firebird::AutoPtr<RuntimeStatistics> m_baseline;
};


class TraceProcCompile
{
public:
	TraceProcCompile(thread_db* tdbb, const Firebird::string& name) :
		m_tdbb(tdbb), m_name(name)
	{
		const auto attachment = m_tdbb->getAttachment();

		const auto trace_mgr = attachment->att_trace_manager;
		m_need_trace = trace_mgr->needs(ITraceFactory::TRACE_EVENT_PROC_COMPILE);

		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();
	}

	~TraceProcCompile()
	{
		finish(nullptr, ITracePlugin::RESULT_FAILED);
	}

	void finish(Statement* statement, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		const auto time = (fb_utils::query_performance_counter() - m_start_clock) * 1000 /
			fb_utils::query_performance_frequency();

		const auto attachment = m_tdbb->getAttachment();

		TraceConnectionImpl conn(attachment);
		TraceProcedureImpl proc(m_name, statement);

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_proc_compile(&conn, &proc, time, result);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	const Firebird::string m_name;
	SINT64 m_start_clock;
};

class TraceProcExecute
{
public:
	TraceProcExecute(thread_db* tdbb, Request* request, Request* caller, const ValueListNode* inputs) :
		m_tdbb(tdbb),
		m_request(request)
	{
		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		const auto trace_mgr = attachment->att_trace_manager;
		m_need_trace = trace_mgr->needs(ITraceFactory::TRACE_EVENT_PROC_EXECUTE);

		if (!m_need_trace)
			return;

		m_request->req_proc_inputs = inputs;
		m_request->req_proc_caller = caller;

		{	// scope
			TraceConnectionImpl conn(attachment);
			TraceTransactionImpl tran(transaction);
			TraceProcedureImpl proc(m_request, nullptr);

			trace_mgr->event_proc_execute(&conn, &tran, &proc, true, ITracePlugin::RESULT_SUCCESS);
		}

		m_start_clock = fb_utils::query_performance_counter();

		m_request->req_fetch_elapsed = 0;
		m_request->req_fetch_rowcount = 0;
		fb_assert(!m_request->req_fetch_baseline);
		m_request->req_fetch_baseline = NULL;

		MemoryPool* pool = m_request->req_pool;
		m_request->req_fetch_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, m_request->req_stats);
	}

	~TraceProcExecute()
	{
		finish(false, ITracePlugin::RESULT_FAILED);
	}

	void finish(bool have_cursor, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		if (have_cursor)
		{
			m_request->req_fetch_elapsed = fb_utils::query_performance_counter() - m_start_clock;
			return;
		}

		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		TraceRuntimeStats stats(attachment, m_request->req_fetch_baseline, &m_request->req_stats,
			fb_utils::query_performance_counter() - m_start_clock,
			m_request->req_fetch_rowcount);

		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);
		TraceProcedureImpl proc(m_request, stats.getPerf());

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_proc_execute(&conn, &tran, &proc, false, result);

		m_request->req_proc_inputs = nullptr;
		m_request->req_proc_caller = nullptr;
		m_request->req_fetch_baseline = nullptr;
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Request* const m_request;
	SINT64 m_start_clock;
};

class TraceProcFetch
{
public:
	TraceProcFetch(thread_db* tdbb, Request* request) :
		m_tdbb(tdbb),
		m_request(request)
	{
		const auto attachment = m_tdbb->getAttachment();
		const auto trace_mgr = attachment->att_trace_manager;

		m_need_trace = (request->req_flags & req_proc_fetch) &&
			trace_mgr->needs(ITraceFactory::TRACE_EVENT_PROC_EXECUTE);

		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();
	}

	~TraceProcFetch()
	{
		fetch(true, ITracePlugin::RESULT_FAILED);
	}

	void fetch(bool eof, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		m_request->req_fetch_elapsed += fb_utils::query_performance_counter() - m_start_clock;
		if (!eof)
		{
			m_request->req_fetch_rowcount++;
			return;
		}

		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		TraceRuntimeStats stats(attachment, m_request->req_fetch_baseline, &m_request->req_stats,
			m_request->req_fetch_elapsed, m_request->req_fetch_rowcount);

		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);
		TraceProcedureImpl proc(m_request, stats.getPerf());

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_proc_execute(&conn, &tran, &proc, false, result);

		m_request->req_proc_inputs = nullptr;
		m_request->req_proc_caller = nullptr;
		m_request->req_fetch_elapsed = 0;
		m_request->req_fetch_baseline = nullptr;
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Request* const m_request;
	SINT64 m_start_clock;
};


class TraceFuncCompile
{
public:
	TraceFuncCompile(thread_db* tdbb, const Firebird::string& name) :
		m_tdbb(tdbb),
		m_name(name)
	{
		const auto attachment = m_tdbb->getAttachment();

		const auto trace_mgr = attachment->att_trace_manager;
		m_need_trace = trace_mgr->needs(ITraceFactory::TRACE_EVENT_FUNC_COMPILE);

		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();
	}

	~TraceFuncCompile()
	{
		finish(nullptr, ITracePlugin::RESULT_FAILED);
	}

	void finish(Statement* statement, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		const auto time = (fb_utils::query_performance_counter() - m_start_clock) * 1000 /
			fb_utils::query_performance_frequency();

		const auto attachment = m_tdbb->getAttachment();

		TraceConnectionImpl conn(attachment);
		TraceFunctionImpl func(m_name, statement);

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_func_compile(&conn, &func, time, result);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	const Firebird::string m_name;
	SINT64 m_start_clock;
};

class TraceFuncExecute
{
public:
	TraceFuncExecute(thread_db* tdbb, Request* request, Request* caller,
					 const UCHAR* inMsg, ULONG inMsgLength) :
		m_tdbb(tdbb),
		m_request(request),
		m_inMsg(inMsg),
		m_inMsgLength(inMsgLength)
	{
		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		const auto trace_mgr = attachment->att_trace_manager;
		m_need_trace = trace_mgr->needs(ITraceFactory::TRACE_EVENT_FUNC_EXECUTE);

		if (!m_need_trace)
			return;

		//m_request->req_proc_inputs = inputs;
		m_request->req_proc_caller = caller;

		{	// scope
			TraceConnectionImpl conn(attachment);
			TraceTransactionImpl tran(transaction);

			TraceDscFromMsg inputs(request->getStatement()->function->getInputFormat(), m_inMsg, m_inMsgLength);
			TraceFunctionImpl func(m_request, nullptr, inputs, nullptr);

			trace_mgr->event_func_execute(&conn, &tran, &func, true, ITracePlugin::RESULT_SUCCESS);
		}

		m_start_clock = fb_utils::query_performance_counter();

		m_request->req_fetch_elapsed = 0;
		m_request->req_fetch_rowcount = 0;
		fb_assert(!m_request->req_fetch_baseline);
		m_request->req_fetch_baseline = nullptr;

		MemoryPool* pool = m_request->req_pool;
		m_request->req_fetch_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, m_request->req_stats);
	}

	~TraceFuncExecute()
	{
		finish(ITracePlugin::RESULT_FAILED);
	}

	void finish(ntrace_result_t result, const dsc* value = NULL)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		TraceRuntimeStats stats(attachment, m_request->req_fetch_baseline, &m_request->req_stats,
			fb_utils::query_performance_counter() - m_start_clock,
			m_request->req_fetch_rowcount);

		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);

		TraceDscFromMsg inputs(m_request->getStatement()->function->getInputFormat(), m_inMsg, m_inMsgLength);
		TraceFunctionImpl func(m_request, stats.getPerf(), inputs, value);

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_func_execute(&conn, &tran, &func,  false, result);

		m_request->req_proc_inputs = nullptr;
		m_request->req_proc_caller = nullptr;
		m_request->req_fetch_baseline = nullptr;
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Request* const m_request;
	const UCHAR* m_inMsg;
	ULONG m_inMsgLength;
	SINT64 m_start_clock;
};


class TraceTrigCompile
{
public:
	TraceTrigCompile(thread_db* tdbb, const Trigger* trigger) :
		m_tdbb(tdbb)
	{
		const auto attachment = m_tdbb->getAttachment();
		const auto trace_mgr = attachment->att_trace_manager;

		m_need_trace = !trigger->sysTrigger &&
			trace_mgr->needs(ITraceFactory::TRACE_EVENT_TRIGGER_COMPILE);

		if (!m_need_trace)
			return;

		m_name = trigger->name.toQuotedString();
		m_relationName = trigger->relation ? trigger->relation->rel_name.toQuotedString() : "";

		const auto type = (trigger->type & ~TRIGGER_TYPE_MASK);

		switch (trigger->type & TRIGGER_TYPE_MASK)
		{
			case TRIGGER_TYPE_DML:
				{
					// TYPE_BEFORE == 1, TYPE_AFTER == 2
					m_which = ((type + 1) & 1) + 1;
					m_action = (type + 1) >> 1;

					fb_assert(m_action == TRIGGER_INSERT ||
							  m_action == TRIGGER_UPDATE ||
							  m_action == TRIGGER_DELETE);
				}
				break;

			case TRIGGER_TYPE_DB:
				{
					m_action = type + DB_TRIGGER_MAX - 1;

					fb_assert(m_action == TRIGGER_CONNECT ||
							  m_action == TRIGGER_DISCONNECT ||
							  m_action == TRIGGER_TRANS_START ||
							  m_action == TRIGGER_TRANS_COMMIT ||
							  m_action == TRIGGER_TRANS_ROLLBACK);
				}
				break;

			case TRIGGER_TYPE_DDL:
				{
					// TYPE_BEFORE == 1, TYPE_AFTER == 2
					m_which = (type & 1) + 1;
					m_action = TRIGGER_DDL;
				}
				break;

			default:
				fb_assert(false);
		}

		m_start_clock = fb_utils::query_performance_counter();
	}

	~TraceTrigCompile()
	{
		finish(nullptr, ITracePlugin::RESULT_FAILED);
	}

	void finish(Statement* statement, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		const auto time = (fb_utils::query_performance_counter() - m_start_clock) * 1000 /
			fb_utils::query_performance_frequency();

		const auto attachment = m_tdbb->getAttachment();

		TraceConnectionImpl conn(attachment);
		TraceTriggerImpl trig(m_name, m_relationName, m_which, m_action, statement);

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_trigger_compile(&conn, &trig, time, result);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Firebird::string m_name;
	Firebird::string m_relationName;
	int m_which = 0;
	int m_action = 0;
	SINT64 m_start_clock;
};

class TraceTrigExecute
{
public:
	TraceTrigExecute(thread_db* tdbb, Request* request, int which) :
		m_tdbb(tdbb),
		m_request(request),
		m_which(which)
	{
		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();
		const auto trace_mgr = attachment->att_trace_manager;

		m_need_trace = !(m_request->getStatement()->flags & Statement::FLAG_SYS_TRIGGER) &&
			trace_mgr->needs(ITraceFactory::TRACE_EVENT_TRIGGER_EXECUTE);

		if (!m_need_trace)
			return;

		{	// scope
			TraceConnectionImpl conn(attachment);
			TraceTransactionImpl tran(transaction);
			TraceTriggerImpl trig(m_which, m_request, nullptr);

			trace_mgr->event_trigger_execute(&conn, &tran, &trig, true, ITracePlugin::RESULT_SUCCESS);
		}

		fb_assert(!m_request->req_fetch_baseline);
		m_request->req_fetch_baseline = nullptr;

		MemoryPool* pool = m_request->req_pool;
		m_request->req_fetch_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, m_request->req_stats);
		m_start_clock = fb_utils::query_performance_counter();
	}

	void finish(ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		const auto attachment = m_tdbb->getAttachment();
		const auto transaction = m_tdbb->getTransaction();

		TraceRuntimeStats stats(attachment, m_request->req_fetch_baseline, &m_request->req_stats,
			fb_utils::query_performance_counter() - m_start_clock, 0);

		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);
		TraceTriggerImpl trig(m_which, m_request, stats.getPerf());

		const auto trace_mgr = attachment->att_trace_manager;
		trace_mgr->event_trigger_execute(&conn, &tran, &trig, false, result);

		m_request->req_fetch_baseline = nullptr;
	}

	~TraceTrigExecute()
	{
		finish(ITracePlugin::RESULT_FAILED);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Request* const m_request;
	SINT64 m_start_clock;
	const int m_which;
};


class TraceBlrCompile
{
public:
	TraceBlrCompile(thread_db* tdbb, unsigned blr_length, const UCHAR* blr) :
		m_tdbb(tdbb),
		m_blr_length(blr_length),
		m_blr(blr)
	{
		Attachment* attachment = m_tdbb->getAttachment();

		m_need_trace = attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_BLR_COMPILE) &&
			m_blr_length && m_blr && !attachment->isUtility();

		if (!m_need_trace)
			return;

		m_start_clock = fb_utils::query_performance_counter();
	}

	void finish(Statement* statement, ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		m_start_clock = (fb_utils::query_performance_counter() - m_start_clock) * 1000 /
						 fb_utils::query_performance_frequency();
		TraceManager* trace_mgr = m_tdbb->getAttachment()->att_trace_manager;

		TraceConnectionImpl conn(m_tdbb->getAttachment());
		TraceTransactionImpl tran(m_tdbb->getTransaction());

		if (statement)
		{
			TraceBLRStatementImpl stmt(statement, NULL);
			trace_mgr->event_blr_compile(&conn, m_tdbb->getTransaction() ? &tran : NULL, &stmt,
				m_start_clock, result);
		}
		else
		{
			TraceFailedBLRStatement stmt(m_blr, m_blr_length);
			trace_mgr->event_blr_compile(&conn, m_tdbb->getTransaction() ? &tran : NULL, &stmt,
				m_start_clock, result);
		}
	}

	~TraceBlrCompile()
	{
		finish(NULL, ITracePlugin::RESULT_FAILED);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	SINT64 m_start_clock;
	const unsigned m_blr_length;
	const UCHAR* const m_blr;
};


class TraceBlrExecute
{
public:
	TraceBlrExecute(thread_db* tdbb, Request* request) :
		m_tdbb(tdbb),
		m_request(request)
	{
		Attachment* attachment = m_tdbb->getAttachment();
		Statement* statement = m_request->getStatement();

		m_need_trace = attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_BLR_EXECUTE) &&
			!statement->sqlText &&
			!(statement->flags & Statement::FLAG_INTERNAL) &&
			!attachment->isUtility();

		if (!m_need_trace)
			return;

		fb_assert(!m_request->req_fetch_baseline);
		m_request->req_fetch_baseline = NULL;

		MemoryPool* pool = m_request->req_pool;
		m_request->req_fetch_baseline = FB_NEW_POOL(*pool) RuntimeStatistics(*pool, m_request->req_stats);

		m_start_clock = fb_utils::query_performance_counter();
	}

	void finish(ntrace_result_t result)
	{
		if (!m_need_trace)
			return;

		m_need_trace = false;

		TraceRuntimeStats stats(m_tdbb->getAttachment(), m_request->req_fetch_baseline, &m_request->req_stats,
			fb_utils::query_performance_counter() - m_start_clock,
			m_request->req_fetch_rowcount);

		TraceConnectionImpl conn(m_tdbb->getAttachment());
		TraceTransactionImpl tran(m_tdbb->getTransaction());
		TraceBLRStatementImpl stmt(m_request->getStatement(), stats.getPerf());

		TraceManager* trace_mgr = m_tdbb->getAttachment()->att_trace_manager;
		trace_mgr->event_blr_execute(&conn, &tran, &stmt, result);

		m_request->req_fetch_baseline = NULL;
	}

	~TraceBlrExecute()
	{
		finish(ITracePlugin::RESULT_FAILED);
	}

private:
	bool m_need_trace;
	thread_db* const m_tdbb;
	Request* const m_request;
	SINT64 m_start_clock;
};


class TraceSweepEvent	// implementation is in tra.cpp
{
public:
	explicit TraceSweepEvent(thread_db* tdbb);

	~TraceSweepEvent();

	void update(const Ods::header_page* header)
	{
		m_sweep_info.update(header);
	}

	void beginSweepRelation(jrd_rel* relation);
	void endSweepRelation(jrd_rel* relation);

	void finish()
	{
		report(ITracePlugin::SWEEP_STATE_FINISHED);
	}

private:
	void report(ntrace_process_state_t state);

	bool m_need_trace;
	thread_db* m_tdbb;
	TraceSweepImpl m_sweep_info;
	SINT64 m_start_clock;
	SINT64 m_relation_clock;
	RuntimeStatistics m_base_stats;
};


} // namespace Jrd

#endif // JRD_TRACE_JRD_HELPERS_H
