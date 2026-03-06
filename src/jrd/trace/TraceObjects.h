/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceObjects.h
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

#ifndef JRD_TRACE_OBJECTS_H
#define JRD_TRACE_OBJECTS_H

#include <time.h>
#include "../../common/classes/array.h"
#include "../../common/classes/fb_string.h"
#include "../../dsql/dsql.h"
#include "../../jrd/ntrace.h"
#include "../../common/dsc.h"
#include "../../common/isc_s_proto.h"
#include "../../jrd/ods_proto.h"
#include "../../jrd/req.h"
#include "../../jrd/svc.h"
#include "../../jrd/tra.h"
#include "../../jrd/status.h"
#include "../../jrd/Function.h"
#include "../../jrd/RuntimeStatistics.h"
#include "../../jrd/Statement.h"
#include "../../jrd/trace/TraceSession.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../common/prett_proto.h"

//// TODO: DDL triggers, packages and external procedures and functions support
namespace Firebird::Jrd
{

class Database;
class Attachment;
class jrd_tra;


class StatementHolder
{
public:
	explicit StatementHolder(const Statement* statement)
		: m_statement(statement)
	{}

	explicit StatementHolder(const Request* request)
		: m_statement(request ? request->getStatement() : nullptr)
	{}

	SINT64 getId() const
	{
		return m_statement ? m_statement->getStatementId() : 0;
	}

	string getName() const;
	const char* ensurePlan(bool explained);

private:
	const Statement* const m_statement;
	string m_plan;
	bool m_planExplained = false;
};


class TraceRuntimeStats :
	public AutoIface<IPerformanceStatsImpl<TraceRuntimeStats, CheckStatusWrapper>>
{
	static constexpr unsigned GLOBAL_COUNTERS = 4; // PerformanceInfo::{FETCHES|READS|MARKS|WRITES}

	template <class T>
	class GenericCounters :
		public AutoIface<IPerformanceCountersImpl<GenericCounters<T>, CheckStatusWrapper>>
	{
	public:
		GenericCounters() = default;
		~GenericCounters() = default;

		void reset(const T* counters, std::function<string(unsigned)> getName)
		{
			m_counts = counters;
			m_names.clear();

			for (const auto& counts : *m_counts)
				m_names.add(getName(counts.getGroupId()));
		}

		// PerformanceCounts implementation
		unsigned getObjectCount()
		{
			return m_counts ? m_counts->getCount() : 0;
		}

		unsigned getMaxCounterIndex()
		{
			return T::getVectorCapacity() - 1;
		}

		unsigned getObjectId(unsigned index)
		{
			if (m_counts && index < m_counts->getCount())
			{
				const auto iter = m_counts->begin() + index;
				return iter->getGroupId();
			}

			return 0;
		}

		const char* getObjectName(unsigned index)
		{
			if (index < m_names.getCount())
				return m_names[index].c_str();

			return nullptr;
		}

		const SINT64* getObjectCounters(unsigned index)
		{
			if (m_counts && index < m_counts->getCount())
			{
				const auto iter = m_counts->begin() + index;
				return iter->getCounterVector();
			}

			return nullptr;
		}

	private:
		ObjectsArray<string> m_names;
		const T* m_counts = nullptr;
	};

	typedef GenericCounters<RuntimeStatistics::PageCounters> PageCounters;
	typedef GenericCounters<RuntimeStatistics::TableCounters> TableCounters;

public:
	TraceRuntimeStats(Attachment* att, RuntimeStatistics* baseline, RuntimeStatistics* stats,
		SINT64 clock, SINT64 recordsFetched);

	// PerformanceStats implementation
	FB_UINT64 getElapsedTime()
	{
		return m_info.pin_time;
	}

	FB_UINT64 getFetchedRecords()
	{
		return m_info.pin_records_fetched;
	}

	IPerformanceCounters* getCounters(unsigned group)
	{
		IPerformanceCounters* counters = nullptr;

		switch (group)
		{
			case IPerformanceStats::COUNTER_GROUP_PAGES:
				counters = &m_pageCounters;
				break;

			case IPerformanceStats::COUNTER_GROUP_TABLES:
				counters = &m_tableCounters;
				break;

			default:
				fb_assert(false);
		}

		return counters;
	}

	PerformanceInfo* getInfo()
	{
		return &m_info;
	}

private:
	PerformanceInfo m_info;
	PageCounters m_pageCounters;
	TableCounters m_tableCounters;
	SINT64 m_globalCounters[GLOBAL_COUNTERS];
	HalfStaticArray<TraceCounts, 16> m_legacyCounts;
};


class TraceConnectionImpl :
	public AutoIface<ITraceDatabaseConnectionImpl<TraceConnectionImpl, CheckStatusWrapper>>
{
public:
	TraceConnectionImpl(Attachment* att) :
		m_att(att)
	{}

	// TraceConnection implementation
	unsigned getKind();
	int getProcessID();
	const char* getUserName();
	const char* getRoleName();
	const char* getCharSet();
	const char* getRemoteProtocol();
	const char* getRemoteAddress();
	int getRemoteProcessID();
	const char* getRemoteProcessName();

	// TraceDatabaseConnection implementation
	ISC_INT64 getConnectionID();
	const char* getDatabaseName();
private:
	Attachment* const m_att;
};


class TraceTransactionImpl :
	public AutoIface<ITraceTransactionImpl<TraceTransactionImpl, CheckStatusWrapper>>
{
public:
	TraceTransactionImpl(const jrd_tra* tran, TraceRuntimeStats* stats = nullptr, ISC_INT64 prevID = 0) :
		m_tran(tran),
		m_prevID(prevID),
		m_stats(stats)
	{}

	// TraceTransaction implementation
	ISC_INT64 getTransactionID();
	FB_BOOLEAN getReadOnly();
	int getWait();
	unsigned getIsolation();
	ISC_INT64 getInitialID();

	ISC_INT64 getPreviousID()
	{
		return m_prevID;
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	}

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	}

private:
	const jrd_tra* const m_tran;
	const ISC_INT64 m_prevID;
	TraceRuntimeStats* const m_stats;
};


template <class Final>
class BLRPrinter :
	public AutoIface<ITraceBLRStatementImpl<Final, CheckStatusWrapper>>
{
public:
	BLRPrinter(const unsigned char* blr, unsigned length) :
		m_blr(blr),
		m_length(length),
		m_text(*getDefaultMemoryPool())
	{}

	// TraceBLRStatement implementation
	const unsigned char* getData()
	{
		return m_blr;
	}

	unsigned getDataLength()
	{
		return m_length;
	}

	const char* getText()
	{
		if (m_text.empty() && getDataLength())
			fb_print_blr(getData(), (ULONG) getDataLength(), print_blr, this, 0);
		return m_text.c_str();
	}

private:
	static void print_blr(void* arg, SSHORT offset, const char* line)
	{
		BLRPrinter* blr = (BLRPrinter*) arg;

		string temp;
		temp.printf("%4d %s\n", offset, line);
		blr->m_text.append(temp);
	}

	const unsigned char* const m_blr;
	const unsigned m_length;
	string m_text;
};


class TraceBLRStatementImpl : public BLRPrinter<TraceBLRStatementImpl>
{
public:
	TraceBLRStatementImpl(const Statement* stmt, TraceRuntimeStats* stats) :
		BLRPrinter(stmt->blr.begin(), stmt->blr.getCount()),
		m_stmt(stmt),
		m_stats(stats)
	{}

	ISC_INT64 getStmtID()
	{
		return m_stmt->getStatementId();
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	}

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	}

private:
	const Statement* const m_stmt;
	TraceRuntimeStats* const m_stats;
};


class TraceFailedBLRStatement : public BLRPrinter<TraceFailedBLRStatement>
{
public:
	TraceFailedBLRStatement(const unsigned char* blr, unsigned length) :
		BLRPrinter(blr, length)
	{}

	ISC_INT64 getStmtID() { return 0; }
	PerformanceInfo* getPerf() { return nullptr; }
	IPerformanceStats* getPerfStats() { return nullptr; }
};


class TraceSQLStatementImpl :
	public AutoIface<ITraceSQLStatementImpl<TraceSQLStatementImpl, CheckStatusWrapper>>,
	public StatementHolder
{
public:
	TraceSQLStatementImpl(DsqlRequest* stmt, TraceRuntimeStats* stats, const UCHAR* inputBuffer) :
		StatementHolder(stmt ? stmt->getStatement() : nullptr),
		m_stmt(stmt),
		m_inputs(stmt, inputBuffer),
		m_stats(stats)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID();
	const char* getText();
	const char* getTextUTF8();

	ITraceParams* getInputs()
	{
		return &m_inputs;
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	}

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	}

private:
	class DSQLParamsImpl :
		public AutoIface<ITraceParamsImpl<DSQLParamsImpl, CheckStatusWrapper>>
	{
	public:
		explicit DSQLParamsImpl(DsqlRequest* const stmt, const UCHAR* const inputBuffer) :
			m_stmt(stmt), m_buffer(inputBuffer)
		{}

		FB_SIZE_T getCount();
		const paramdsc* getParam(FB_SIZE_T idx);
		const char* getTextUTF8(CheckStatusWrapper* status, FB_SIZE_T idx);

	private:
		void fillParams();

		DsqlRequest* const m_stmt;
		const UCHAR* m_buffer;
		HalfStaticArray<paramdsc, 16> m_descs;
		string m_tempUTF8;
	};

	DsqlRequest* const m_stmt;
	DSQLParamsImpl m_inputs;
	string m_textUTF8;
	TraceRuntimeStats* const m_stats;
};


class TraceFailedSQLStatement :
	public AutoIface<ITraceSQLStatementImpl<TraceFailedSQLStatement, CheckStatusWrapper>>
{
public:
	TraceFailedSQLStatement(string& text) :
		m_text(text)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID() {	return 0; }
	ITraceParams* getInputs() { return nullptr; }
	const char* getText() { return m_text.c_str(); }
	const char* getPlan() { return ""; }
	const char* getTextUTF8();
	const char* getExplainedPlan() { return ""; }
	PerformanceInfo* getPerf() { return nullptr; }
	IPerformanceStats* getPerfStats() { return nullptr; }

private:
	string& m_text;
	string m_textUTF8;
};


class TraceContextVarImpl :
	public AutoIface<ITraceContextVariableImpl<TraceContextVarImpl, CheckStatusWrapper>>
{
public:
	TraceContextVarImpl(const char* ns, const char* name, const char* value) :
		m_namespace(ns),
		m_name(name),
		m_value(value)
	{}

	// TraceContextVariable implementation
	const char* getNameSpace()	{ return m_namespace; }
	const char* getVarName()	{ return m_name; }
	const char* getVarValue()	{ return m_value; }

private:
	const char* const m_namespace;
	const char* const m_name;
	const char* const m_value;
};


// forward declaration
class TraceDescriptors;

class TraceParamsImpl :
	public AutoIface<ITraceParamsImpl<TraceParamsImpl, CheckStatusWrapper>>
{
public:
	explicit TraceParamsImpl(TraceDescriptors *descs) :
		m_descs(descs)
	{}

	// TraceParams implementation
	FB_SIZE_T getCount();
	const paramdsc* getParam(FB_SIZE_T idx);
	const char* getTextUTF8(CheckStatusWrapper* status, FB_SIZE_T idx);

private:
	TraceDescriptors* m_descs;
	string m_tempUTF8;
};


class TraceDescriptors
{
public:
	TraceDescriptors() :
		m_traceParams(this)
	{
	}

	FB_SIZE_T getCount()
	{
		fillParams();
		return m_descs.getCount();
	}

	const paramdsc* getParam(FB_SIZE_T idx)
	{
		fillParams();

		if (idx < m_descs.getCount())
			return &m_descs[idx];

		return NULL;
	}

	operator ITraceParams* ()
	{
		return &m_traceParams;
	}

protected:
	virtual void fillParams() = 0;

	HalfStaticArray<paramdsc, 16> m_descs;

private:
	TraceParamsImpl	m_traceParams;
};


class TraceDscFromValues : public TraceDescriptors
{
public:
	TraceDscFromValues(Request* request, const ValueListNode* params) :
		m_request(request),
		m_params(params)
	{}

protected:
	void fillParams();

private:
	Request* const m_request;
	const ValueListNode* const m_params;
};


class TraceDscFromMsg : public TraceDescriptors
{
public:
	TraceDscFromMsg(const Format* format, const UCHAR* inMsg, ULONG inMsgLength) :
		m_format(format),
		m_inMsg(inMsg),
		m_inMsgLength(inMsgLength)
	{}

protected:
	void fillParams();

private:
	const Format* const m_format;
	const UCHAR* const m_inMsg;
	const ULONG m_inMsgLength;
};


class TraceDscFromDsc : public TraceDescriptors
{
public:
	TraceDscFromDsc(const dsc* desc)
	{
		if (desc)
			m_descs.add(*desc);
		else
		{
			m_descs.grow(1);
			m_descs[0].dsc_flags |= DSC_null;
		}
	}

protected:
	void fillParams() {}
};


class TraceProcedureImpl :
	public AutoIface<ITraceProcedureImpl<TraceProcedureImpl, CheckStatusWrapper>>,
	public StatementHolder
{
public:
	TraceProcedureImpl(const string& name, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_inputs(nullptr, nullptr),
		m_stats(nullptr)
	{}

	TraceProcedureImpl(Request* request, TraceRuntimeStats* stats) :
		StatementHolder(request),
		m_name(getName()),
		m_inputs(request->req_proc_caller, request->req_proc_inputs),
		m_stats(stats)
	{}

	// TraceProcedure implementation
	const char* getProcName()
	{
		return m_name.c_str();
	}

	ITraceParams* getInputs()
	{
		return m_inputs;
	}

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	}

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	}

private:
	const string m_name;
	TraceDscFromValues m_inputs;
	TraceRuntimeStats* const m_stats;
};


class TraceFunctionImpl :
	public AutoIface<ITraceFunctionImpl<TraceFunctionImpl, CheckStatusWrapper>>,
	public StatementHolder
{
public:
	TraceFunctionImpl(const string& name, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_inputs(nullptr),
		m_value(nullptr),
		m_stats(nullptr)
	{}

	TraceFunctionImpl(Request* request, TraceRuntimeStats* stats,
					  ITraceParams* inputs, const dsc* value) :
		StatementHolder(request),
		m_name(getName()),
		m_inputs(inputs),
		m_value(value),
		m_stats(stats)
	{}

	// TraceFunction implementation
	const char* getFuncName()
	{
		return m_name.c_str();
	}

	ITraceParams* getInputs()
	{
		return m_inputs;
	}

	ITraceParams* getResult()
	{
		return m_value;
	}

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	};

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	};

private:
	string m_name;
	ITraceParams* const m_inputs;
	TraceDscFromDsc m_value;
	TraceRuntimeStats* const m_stats;
};


class TraceTriggerImpl :
	public AutoIface<ITraceTriggerImpl<TraceTriggerImpl, CheckStatusWrapper>>,
	public StatementHolder
{
public:
	TraceTriggerImpl(const string& name, const string& relationName,
		int which, int action, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_relationName(relationName),
		m_which(which),
		m_action(action),
		m_stats(nullptr)
	{}

	TraceTriggerImpl(int which, const Request* request, TraceRuntimeStats* stats) :
		StatementHolder(request),
		m_name(getName()),
		m_relationName((request->req_rpb.hasData() && request->req_rpb[0].rpb_relation) ?
			request->req_rpb[0].rpb_relation->getName().toQuotedString() : ""),
		m_which(which),
		m_action(request->req_trigger_action),
		m_stats(stats)
	{}

	// TraceTrigger implementation
	const char* getTriggerName()
	{
		return m_name.nullStr();
	}

	const char* getRelationName()
	{
		return m_relationName.nullStr();
	}

	int getWhich()
	{
		return m_which;
	}

	int getAction()
	{
		return m_action;
	}

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	};

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	};

private:
	const string m_name;
	const string m_relationName;
	const int m_which;
	const int m_action;
	TraceRuntimeStats* const m_stats;
};


class TraceServiceImpl :
	public AutoIface<ITraceServiceConnectionImpl<TraceServiceImpl, CheckStatusWrapper>>
{
public:
	TraceServiceImpl(const Service* svc) :
		m_svc(svc)
	{}

	// TraceConnection implementation
	unsigned getKind();
	const char* getUserName();
	const char* getRoleName();
	const char* getCharSet();
	int getProcessID();
	const char* getRemoteProtocol();
	const char* getRemoteAddress();
	int getRemoteProcessID();
	const char* getRemoteProcessName();

	// TraceServiceConnection implementation
	void* getServiceID();
	const char* getServiceMgr();
	const char* getServiceName();
private:
	const Service* const m_svc;
};


class TraceInitInfoImpl :
	public AutoIface<ITraceInitInfoImpl<TraceInitInfoImpl, CheckStatusWrapper>>
{
public:
	TraceInitInfoImpl(const TraceSession& session, Attachment* att,
					const char* filename) :
		m_session(session),
		m_trace_conn(att),
		m_filename(filename),
		m_attachment(att)
	{
		if (m_attachment && !m_attachment->att_filename.empty()) {
			m_filename = m_attachment->att_filename.c_str();
		}
	}

	// TraceInitInfo implementation
	const char* getConfigText()			{ return m_session.ses_config.c_str(); }
	int getTraceSessionID()				{ return m_session.ses_id; }
	const char* getTraceSessionName()	{ return m_session.ses_name.c_str(); }

	const char* getFirebirdRootDirectory();
	const char* getDatabaseName()		{ return m_filename; }

	ITraceDatabaseConnection* getConnection()
	{
		if (m_attachment)
			return &m_trace_conn;

		return NULL;
	}

	ITraceLogWriter* getLogWriter();

private:
	const TraceSession& m_session;
	RefPtr<ITraceLogWriter> m_logWriter;
	TraceConnectionImpl m_trace_conn;
	const char* m_filename;
	const Attachment* const m_attachment;
};


class TraceStatusVectorImpl :
	public AutoIface<ITraceStatusVectorImpl<TraceStatusVectorImpl, CheckStatusWrapper>>
{
public:
	enum Kind {TS_ERRORS, TS_WARNINGS};

	TraceStatusVectorImpl(FbStatusVector* status, Kind k) :
		m_status(status), kind(k)
	{
	}

	FB_BOOLEAN hasError()
	{
		return m_status->getState() & IStatus::STATE_ERRORS;
	}

	FB_BOOLEAN hasWarning()
	{
		return m_status->getState() & IStatus::STATE_WARNINGS;
	}

	IStatus* getStatus()
	{
		return m_status;
	}

	const char* getText();

private:
	string m_error;
	FbStatusVector* m_status;
	Kind kind;
};

class TraceSweepImpl :
	public AutoIface<ITraceSweepInfoImpl<TraceSweepImpl, CheckStatusWrapper>>
{
public:
	TraceSweepImpl() = default;

	void update(const Ods::header_page* header)
	{
		m_oit = header->hdr_oldest_transaction;
		m_ost = header->hdr_oldest_snapshot;
		m_oat = header->hdr_oldest_active;
		m_next = header->hdr_next_transaction;
	}

	void setStats(TraceRuntimeStats* stats)
	{
		m_stats = stats;
	}

	ISC_INT64 getOIT()			{ return m_oit; };
	ISC_INT64 getOST()			{ return m_ost; };
	ISC_INT64 getOAT()			{ return m_oat; };
	ISC_INT64 getNext()			{ return m_next; };

	PerformanceInfo* getPerf()
	{
		return m_stats ? m_stats->getInfo() : nullptr;
	};

	IPerformanceStats* getPerfStats()
	{
		return m_stats;
	};

private:
	TraNumber m_oit = 0;
	TraNumber m_ost = 0;
	TraNumber m_oat = 0;
	TraNumber m_next = 0;
	TraceRuntimeStats* m_stats = nullptr;
};


} // namespace Firebird::Jrd

#endif // JRD_TRACE_OBJECTS_H
