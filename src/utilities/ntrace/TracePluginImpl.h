/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		TracePluginImpl.h
 *	DESCRIPTION:	Plugin implementation
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  2008 Khorsun Vladyslav
 */

#ifndef TRACEPLUGINIMPL_H
#define TRACEPLUGINIMPL_H

#include "firebird.h"
#include "../../jrd/ntrace.h"
#include "TracePluginConfig.h"
#include "../../common/SimilarToRegex.h"
#include "../../common/classes/rwlock.h"
#include "../../common/classes/GenericMap.h"
#include "../../common/classes/locks.h"
#include "../../common/classes/ImplementHelper.h"

// Bring in off_t
#include <sys/types.h>

class TracePluginImpl FB_FINAL :
	public Firebird::RefCntIface<Firebird::ITracePluginImpl<TracePluginImpl, Firebird::CheckStatusWrapper> >
{
public:
	// Serialize exception to TLS buffer to return it to user
	static const char* marshal_exception(const Firebird::Exception& ex);

	// Data for tracked (active) connections
	struct ConnectionData
	{
		AttNumber id;
		Firebird::string* description;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const AttNumber& generate(const void* /*sender*/, const ConnectionData& item)
		{
			return item.id;
		}
	};

	typedef Firebird::BePlusTree<ConnectionData, AttNumber, Firebird::MemoryPool, ConnectionData>
		ConnectionsTree;

	// Data for tracked (active) transactions
	struct TransactionData
	{
		TraNumber id;
		Firebird::string* description;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const TraNumber& generate(const void* /*sender*/, const TransactionData& item)
		{
			return item.id;
		}
	};

	typedef Firebird::BePlusTree<TransactionData, TraNumber, Firebird::MemoryPool, TransactionData>
		TransactionsTree;

	// Data for tracked (active) statements
	struct StatementData
	{
		StmtNumber id;
		Firebird::string* description; // NULL in this field indicates that tracing of this statement is not desired

		static const StmtNumber& generate(const void* /*sender*/, const StatementData& item)
		{
			return item.id;
		}
	};

	typedef Firebird::BePlusTree<StatementData, StmtNumber, Firebird::MemoryPool, StatementData>
		StatementsTree;

	typedef void* ServiceId;
	struct ServiceData
	{
		ServiceId id;
		Firebird::string* description;
		bool enabled;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const ServiceId& generate(const void* /*sender*/, const ServiceData& item)
		{
			return item.id;
		}
	};

	typedef Firebird::BePlusTree<ServiceData, ServiceId, Firebird::MemoryPool, ServiceData>
		ServicesTree;

	TracePluginImpl(Firebird::IPluginBase* factory, const TracePluginConfig& configuration, Firebird::ITraceInitInfo* initInfo);

private:
	~TracePluginImpl();

	// Used to not allow to destroy plugin factory and unload dll\so if
	// instance of TracePluginImpl class exists
	Firebird::RefPtr<Firebird::IPluginBase> factory;

	bool operational; // Set if plugin is fully initialized and is ready for logging
					  // Keep this member field first to ensure its correctness
					  // when destructor is called
	const int session_id;				// trace session ID, set by Firebird
	Firebird::string session_name;		// trace session name, set by Firebird
	Firebird::ITraceLogWriter* logWriter;
	TracePluginConfig config;	// Immutable, thus thread-safe
	Firebird::string record;

	// Data for currently active connections, transactions, statements
	Firebird::RWLock connectionsLock;
	ConnectionsTree connections;

	Firebird::RWLock transactionsLock;
	TransactionsTree transactions;

	Firebird::RWLock statementsLock;
	StatementsTree statements;

	Firebird::RWLock servicesLock;
	ServicesTree services;

	// Lock for log rotation
	Firebird::RWLock renameLock;

	Firebird::AutoPtr<Firebird::SimilarToRegex> include_matcher, exclude_matcher;

	// Filters for gds error codes
	typedef Firebird::SortedArray<ISC_STATUS> GdsCodesArray;
	GdsCodesArray include_codes;
	GdsCodesArray exclude_codes;

	void appendGlobalCounts(const Firebird::PerformanceInfo* info);
	void appendTableCounts(const Firebird::PerformanceInfo* info);
	void appendParams(Firebird::ITraceParams* params);
	void appendServiceQueryParams(size_t send_item_length, const ntrace_byte_t* send_items,
								  size_t recv_item_length, const ntrace_byte_t* recv_items);
	void formatStringArgument(Firebird::string& result, const UCHAR* str, size_t len);
	bool filterStatus(const ISC_STATUS* status, GdsCodesArray& arr);
	void str2Array(const Firebird::string& str, GdsCodesArray& arr);

	// register various objects
	void register_connection(Firebird::ITraceDatabaseConnection* connection);
	void register_transaction(Firebird::ITraceTransaction* transaction);
	void register_sql_statement(Firebird::ITraceSQLStatement* statement);
	void register_blr_statement(Firebird::ITraceBLRStatement* statement);
	void register_service(Firebird::ITraceServiceConnection* service);

	bool checkServiceFilter(Firebird::ITraceServiceConnection* service, bool started);

	// Write message to text log file
	void logRecord(const char* action);
	void logRecordConn(const char* action, Firebird::ITraceDatabaseConnection* connection);
	void logRecordTrans(const char* action, Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction);
	void logRecordProcFunc(const char* action, Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, const char* obj_type, const char* obj_name);
	void logRecordStmt(const char* action, Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, Firebird::ITraceStatement* statement,
		bool isSQL);
	void logRecordServ(const char* action, Firebird::ITraceServiceConnection* service);
	void logRecordError(const char* action, Firebird::ITraceConnection* connection, Firebird::ITraceStatusVector* status);

	/* Methods which do logging of events to file */
	void log_init();
	void log_finalize();

	void log_event_attach(
		Firebird::ITraceDatabaseConnection* connection, FB_BOOLEAN create_db,
		unsigned att_result);
	void log_event_detach(
		Firebird::ITraceDatabaseConnection* connection, FB_BOOLEAN drop_db);

	void log_event_transaction_start(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		size_t tpb_length, const ntrace_byte_t* tpb, unsigned tra_result);
	void log_event_transaction_end(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		FB_BOOLEAN commit, FB_BOOLEAN retain_context, unsigned tra_result);

	void log_event_set_context(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceContextVariable* variable);

	void log_event_proc_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceProcedure* procedure, bool started, unsigned proc_result);

	void log_event_func_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceFunction* function, bool started, unsigned func_result);

	void log_event_trigger_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceTrigger* trigger, bool started, unsigned trig_result);

	void log_event_dsql_prepare(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceSQLStatement* statement, ntrace_counter_t time_millis, unsigned req_result);
	void log_event_dsql_free(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceSQLStatement* statement, unsigned short option);
	void log_event_dsql_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceSQLStatement* statement, bool started, unsigned req_result);

	void log_event_blr_compile(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceBLRStatement* statement, ntrace_counter_t time_millis, unsigned req_result);
	void log_event_blr_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceBLRStatement* statement, unsigned req_result);

	void log_event_dyn_execute(
		Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceDYNRequest* request, ntrace_counter_t time_millis,
		unsigned req_result);

	void log_event_service_attach(Firebird::ITraceServiceConnection* service, unsigned att_result);
	void log_event_service_start(Firebird::ITraceServiceConnection* service, size_t switches_length, const char* switches,
								 unsigned start_result);
	void log_event_service_query(Firebird::ITraceServiceConnection* service, size_t send_item_length,
								 const ntrace_byte_t* send_items, size_t recv_item_length,
								 const ntrace_byte_t* recv_items, unsigned query_result);
	void log_event_service_detach(Firebird::ITraceServiceConnection* service, unsigned detach_result);

	void log_event_error(Firebird::ITraceConnection* connection, Firebird::ITraceStatusVector* status, const char* function);

	void log_event_sweep(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state);

public:
	// TracePlugin implementation
	const char* trace_get_error();

	// Create/close attachment
	FB_BOOLEAN trace_attach(Firebird::ITraceDatabaseConnection* connection, FB_BOOLEAN create_db, unsigned att_result);
	FB_BOOLEAN trace_detach(Firebird::ITraceDatabaseConnection* connection, FB_BOOLEAN drop_db);

	// Start/end transaction
	FB_BOOLEAN trace_transaction_start(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			unsigned tpb_length, const ntrace_byte_t* tpb, unsigned tra_result);
	FB_BOOLEAN trace_transaction_end(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			FB_BOOLEAN commit, FB_BOOLEAN retain_context, unsigned tra_result);

	// Stored procedures, functions and triggers execution
	FB_BOOLEAN trace_proc_execute (Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceProcedure* procedure, FB_BOOLEAN started, unsigned proc_result);
	FB_BOOLEAN trace_func_execute (Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceFunction* function, FB_BOOLEAN started, unsigned func_result);
	FB_BOOLEAN trace_trigger_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceTrigger* trigger,
			FB_BOOLEAN started, unsigned trig_result);

	// Assignment to context variables
	FB_BOOLEAN trace_set_context(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceContextVariable* variable);

	// DSQL statement lifecycle
	FB_BOOLEAN trace_dsql_prepare(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceSQLStatement* statement, ISC_INT64 time_millis, unsigned req_result);
	FB_BOOLEAN trace_dsql_free(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceSQLStatement* statement,
			unsigned option);
	FB_BOOLEAN trace_dsql_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceSQLStatement* statement, FB_BOOLEAN started, unsigned req_result);

	// BLR requests
	FB_BOOLEAN trace_blr_compile(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceBLRStatement* statement, ISC_INT64 time_millis, unsigned req_result);
	FB_BOOLEAN trace_blr_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceBLRStatement* statement, unsigned req_result);

	// DYN requests
	FB_BOOLEAN trace_dyn_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
			Firebird::ITraceDYNRequest* request, ISC_INT64 time_millis, unsigned req_result);

	// Using the services
	FB_BOOLEAN trace_service_attach(Firebird::ITraceServiceConnection* service, unsigned att_result);
	FB_BOOLEAN trace_service_start(Firebird::ITraceServiceConnection* service, unsigned switches_length, const char* switches,
			unsigned start_result);
	FB_BOOLEAN trace_service_query(Firebird::ITraceServiceConnection* service, unsigned send_item_length,
			const ntrace_byte_t* send_items, unsigned recv_item_length,
			const ntrace_byte_t* recv_items, unsigned query_result);
	FB_BOOLEAN trace_service_detach(Firebird::ITraceServiceConnection* service, unsigned detach_result);

	// Errors happened
	virtual FB_BOOLEAN trace_event_error(Firebird::ITraceConnection* connection, Firebird::ITraceStatusVector* status,
			const char* function);

	// Sweep activity
	virtual FB_BOOLEAN trace_event_sweep(Firebird::ITraceDatabaseConnection* connection,
			Firebird::ITraceSweepInfo* sweep, ntrace_process_state_t sweep_state);
};


#endif // TRACEPLUGINIMPL_H
