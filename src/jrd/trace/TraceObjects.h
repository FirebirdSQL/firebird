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
#include "../../jrd/trace/TraceSession.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../common/prett_proto.h"

//// TODO: DDL triggers, packages and external procedures and functions support
namespace Jrd {

class Database;
class Attachment;
class jrd_tra;

class TraceConnectionImpl :
	public Firebird::AutoIface<Firebird::ITraceDatabaseConnectionImpl<TraceConnectionImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceConnectionImpl(const Attachment* att) :
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
	const Attachment* const m_att;
};


class TraceTransactionImpl :
	public Firebird::AutoIface<Firebird::ITraceTransactionImpl<TraceTransactionImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceTransactionImpl(const jrd_tra* tran, PerformanceInfo* perf = NULL) :
		m_tran(tran),
		m_perf(perf)
	{}

	// TraceTransaction implementation
	ISC_INT64 getTransactionID();
	FB_BOOLEAN getReadOnly();
	int getWait();
	unsigned getIsolation();
	PerformanceInfo* getPerf()	{ return m_perf; }

private:
	const jrd_tra* const m_tran;
	PerformanceInfo* const m_perf;
};


template <class Final>
class BLRPrinter :
	public Firebird::AutoIface<Firebird::ITraceBLRStatementImpl<Final, Firebird::CheckStatusWrapper> >
{
public:
	BLRPrinter(const unsigned char* blr, unsigned length) :
		m_blr(blr),
		m_length(length),
		m_text(*getDefaultMemoryPool())
	{}

	// TraceBLRStatement implementation
	const unsigned char* getData()	{ return m_blr; }
	unsigned getDataLength()		{ return m_length; }
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

		Firebird::string temp;
		temp.printf("%4d %s\n", offset, line);
		blr->m_text.append(temp);
	}

	const unsigned char* const m_blr;
	const unsigned m_length;
	Firebird::string m_text;
};


class TraceBLRStatementImpl : public BLRPrinter<TraceBLRStatementImpl>
{
public:
	TraceBLRStatementImpl(const jrd_req* stmt, PerformanceInfo* perf) :
		BLRPrinter(stmt->getStatement()->blr.begin(), stmt->getStatement()->blr.getCount()),
		m_stmt(stmt),
		m_perf(perf)
	{}

	ISC_INT64 getStmtID()		{ return m_stmt->req_id; }
	PerformanceInfo* getPerf()	{ return m_perf; }

private:
	const jrd_req* const m_stmt;
	PerformanceInfo* const m_perf;
};


class TraceFailedBLRStatement : public BLRPrinter<TraceFailedBLRStatement>
{
public:
	TraceFailedBLRStatement(const unsigned char* blr, unsigned length) :
		BLRPrinter(blr, length)
	{}

	ISC_INT64 getStmtID()		{ return 0; }
	PerformanceInfo* getPerf()	{ return NULL; }
};


class TraceSQLStatementImpl :
	public Firebird::AutoIface<Firebird::ITraceSQLStatementImpl<TraceSQLStatementImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceSQLStatementImpl(const dsql_req* stmt, PerformanceInfo* perf) :
		m_stmt(stmt),
		m_perf(perf),
		m_planExplained(false),
		m_inputs(*getDefaultMemoryPool(), m_stmt)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID();
	PerformanceInfo* getPerf();
	Firebird::ITraceParams* getInputs();
	const char* getText();
	const char* getPlan();
	const char* getTextUTF8();
	const char* getExplainedPlan();

private:
	class DSQLParamsImpl :
		public Firebird::AutoIface<Firebird::ITraceParamsImpl<DSQLParamsImpl, Firebird::CheckStatusWrapper> >
	{
	public:
		DSQLParamsImpl(Firebird::MemoryPool& pool, const dsql_req* const stmt) :
			m_stmt(stmt),
			m_params(NULL),
			m_descs(pool)
		{
			const dsql_msg* msg = m_stmt->getStatement()->getSendMsg();
			if (msg)
				m_params = &msg->msg_parameters;
		}

		FB_SIZE_T getCount();
		const dsc* getParam(FB_SIZE_T idx);
		const char* getTextUTF8(Firebird::CheckStatusWrapper* status, FB_SIZE_T idx);

	private:
		void fillParams();

		const dsql_req* const m_stmt;
		const Firebird::Array<dsql_par*>* m_params;
		Firebird::HalfStaticArray<dsc, 16> m_descs;
		Firebird::string temp_utf8_text;
	};

	void fillPlan(bool explained);

	const dsql_req* const m_stmt;
	PerformanceInfo* const m_perf;
	Firebird::string m_plan;
	bool m_planExplained;
	DSQLParamsImpl m_inputs;
	Firebird::string m_textUTF8;
};


class TraceFailedSQLStatement :
	public Firebird::AutoIface<Firebird::ITraceSQLStatementImpl<TraceFailedSQLStatement, Firebird::CheckStatusWrapper> >
{
public:
	TraceFailedSQLStatement(Firebird::string& text) :
		m_text(text)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID()		{ return 0; }
	PerformanceInfo* getPerf()	{ return NULL; }
	Firebird::ITraceParams* getInputs()	{ return NULL; }
	const char* getText()		{ return m_text.c_str(); }
	const char* getPlan()		{ return ""; }
	const char* getTextUTF8();
	const char* getExplainedPlan()	{ return ""; }

private:
	Firebird::string& m_text;
	Firebird::string m_textUTF8;
};


class TraceContextVarImpl :
	public Firebird::AutoIface<Firebird::ITraceContextVariableImpl<TraceContextVarImpl, Firebird::CheckStatusWrapper> >
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
	public Firebird::AutoIface<Firebird::ITraceParamsImpl<TraceParamsImpl, Firebird::CheckStatusWrapper> >
{
public:
	explicit TraceParamsImpl(TraceDescriptors *descs) :
		m_descs(descs)
	{}

	// TraceParams implementation
	FB_SIZE_T getCount();
	const dsc* getParam(FB_SIZE_T idx);
	const char* getTextUTF8(Firebird::CheckStatusWrapper* status, FB_SIZE_T idx);

private:
	TraceDescriptors* m_descs;
	Firebird::string temp_utf8_text;
};


class TraceDescriptors
{
public:
	explicit TraceDescriptors(Firebird::MemoryPool& pool) :
		m_descs(pool),
		m_traceParams(this)
	{
	}

	FB_SIZE_T getCount()
	{
		fillParams();
		return m_descs.getCount();
	}

	const dsc* getParam(FB_SIZE_T idx)
	{
		fillParams();

		if (/*idx >= 0 &&*/ idx < m_descs.getCount())
			return &m_descs[idx];

		return NULL;
	}

	operator Firebird::ITraceParams* ()
	{
		return &m_traceParams;
	}

protected:
	virtual void fillParams() = 0;

	Firebird::HalfStaticArray<dsc, 16> m_descs;

private:
	TraceParamsImpl	m_traceParams;
};


class TraceDscFromValues : public TraceDescriptors
{
public:
	TraceDscFromValues(Firebird::MemoryPool& pool, jrd_req* request, const ValueListNode* params) :
		TraceDescriptors(pool),
		m_request(request),
		m_params(params)
	{}

protected:
	void fillParams();

private:
	jrd_req* m_request;
	const ValueListNode* m_params;
};


class TraceDscFromMsg : public TraceDescriptors
{
public:
	TraceDscFromMsg(Firebird::MemoryPool& pool, const Format* format,
		const UCHAR* inMsg, ULONG inMsgLength) :
		TraceDescriptors(pool),
		m_format(format),
		m_inMsg(inMsg),
		m_inMsgLength(inMsgLength)
	{}

protected:
	void fillParams();

private:
	const Format* m_format;
	const UCHAR* m_inMsg;
	ULONG m_inMsgLength;
};


class TraceDscFromDsc : public TraceDescriptors
{
public:
	TraceDscFromDsc(Firebird::MemoryPool& pool, const dsc* desc) :
		TraceDescriptors(pool)
	{
		if (desc)
			m_descs.add(*desc);
		else
		{
			m_descs.grow(1);
			m_descs[0].setNull();
		}
	}

protected:
	void fillParams() {}
};


class TraceProcedureImpl :
	public Firebird::AutoIface<Firebird::ITraceProcedureImpl<TraceProcedureImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceProcedureImpl(jrd_req* request, PerformanceInfo* perf) :
		m_request(request),
		m_perf(perf),
		m_inputs(*getDefaultMemoryPool(), request->req_proc_caller, request->req_proc_inputs),
		m_name(m_request->getStatement()->procedure->getName().toString())

	{}

	// TraceProcedure implementation
	const char* getProcName()
	{
		return m_name.c_str();
	}

	Firebird::ITraceParams* getInputs()	{ return m_inputs; }
	PerformanceInfo* getPerf()	{ return m_perf; };

private:
	jrd_req* const m_request;
	PerformanceInfo* const m_perf;
	TraceDscFromValues m_inputs;
	Firebird::string m_name;
};


class TraceFunctionImpl :
	public Firebird::AutoIface<Firebird::ITraceFunctionImpl<TraceFunctionImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceFunctionImpl(jrd_req* request, Firebird::ITraceParams* inputs, PerformanceInfo* perf, const dsc* value) :
		m_request(request),
		m_perf(perf),
		m_inputs(inputs),
		m_value(*getDefaultMemoryPool(), value),
		m_name(m_request->getStatement()->function->getName().toString())
	{}

	// TraceFunction implementation
	const char* getFuncName()
	{
		return m_name.c_str();
	}

	Firebird::ITraceParams* getInputs()	{ return m_inputs; }
	Firebird::ITraceParams* getResult()	{ return m_value; }
	PerformanceInfo* getPerf()	{ return m_perf; };

private:
	jrd_req* const m_request;
	PerformanceInfo* const m_perf;
	Firebird::ITraceParams* m_inputs;
	TraceDscFromDsc m_value;
	Firebird::string m_name;
};


class TraceTriggerImpl :
	public Firebird::AutoIface<Firebird::ITraceTriggerImpl<TraceTriggerImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceTriggerImpl(const jrd_req* trig, SSHORT which, PerformanceInfo* perf) :
	  m_trig(trig),
	  m_which(which),
	  m_perf(perf)
	{}

	// TraceTrigger implementation
	const char* getTriggerName();
	const char* getRelationName();
	int getAction()				{ return m_trig->req_trigger_action; }
	int getWhich()				{ return m_which; }
	PerformanceInfo* getPerf()	{ return m_perf; }

private:
	const jrd_req* const m_trig;
	const SSHORT m_which;
	PerformanceInfo* const m_perf;
};


class TraceServiceImpl :
	public Firebird::AutoIface<Firebird::ITraceServiceConnectionImpl<TraceServiceImpl, Firebird::CheckStatusWrapper> >
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


class TraceRuntimeStats
{
public:
	TraceRuntimeStats(Attachment* att, RuntimeStatistics* baseline, RuntimeStatistics* stats,
		SINT64 clock, SINT64 records_fetched);

	PerformanceInfo* getPerf()	{ return &m_info; }

private:
	PerformanceInfo m_info;
	TraceCountsArray m_counts;
	static SINT64 m_dummy_counts[RuntimeStatistics::TOTAL_ITEMS];	// Zero-initialized array with zero counts
};


class TraceInitInfoImpl :
	public Firebird::AutoIface<Firebird::ITraceInitInfoImpl<TraceInitInfoImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceInitInfoImpl(const Firebird::TraceSession& session, const Attachment* att,
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

	Firebird::ITraceDatabaseConnection* getConnection()
	{
		if (m_attachment)
			return &m_trace_conn;

		return NULL;
	}

	Firebird::ITraceLogWriter* getLogWriter();

private:
	const Firebird::TraceSession& m_session;
	Firebird::RefPtr<Firebird::ITraceLogWriter> m_logWriter;
	TraceConnectionImpl m_trace_conn;
	const char* m_filename;
	const Attachment* const m_attachment;
};


class TraceStatusVectorImpl :
	public Firebird::AutoIface<Firebird::ITraceStatusVectorImpl<TraceStatusVectorImpl, Firebird::CheckStatusWrapper> >
{
public:
	enum Kind {TS_ERRORS, TS_WARNINGS};

	TraceStatusVectorImpl(FbStatusVector* status, Kind k) :
		m_status(status), kind(k)
	{
	}

	FB_BOOLEAN hasError()
	{
		return m_status->getState() & Firebird::IStatus::STATE_ERRORS;
	}

	FB_BOOLEAN hasWarning()
	{
		return m_status->getState() & Firebird::IStatus::STATE_WARNINGS;
	}

	Firebird::IStatus* getStatus()
	{
		return m_status;
	}

	const char* getText();

private:
	Firebird::string m_error;
	FbStatusVector* m_status;
	Kind kind;
};

class TraceSweepImpl :
	public Firebird::AutoIface<Firebird::ITraceSweepInfoImpl<TraceSweepImpl, Firebird::CheckStatusWrapper> >
{
public:
	TraceSweepImpl()
	{
		m_oit = 0;
		m_ost = 0;
		m_oat = 0;
		m_next = 0;
		m_perf = 0;
	}

	void update(const Ods::header_page* header)
	{
		m_oit = Ods::getOIT(header);
		m_ost = Ods::getOST(header);
		m_oat = Ods::getOAT(header);
		m_next = Ods::getNT(header);
	}

	void setPerf(PerformanceInfo* perf)
	{
		m_perf = perf;
	}

	ISC_INT64 getOIT()			{ return m_oit; };
	ISC_INT64 getOST()			{ return m_ost; };
	ISC_INT64 getOAT()			{ return m_oat; };
	ISC_INT64 getNext()			{ return m_next; };
	PerformanceInfo* getPerf()	{ return m_perf; };

private:
	TraNumber m_oit;
	TraNumber m_ost;
	TraNumber m_oat;
	TraNumber m_next;
	PerformanceInfo* m_perf;
};

} // namespace Jrd

#endif // JRD_TRACE_OBJECTS_H
