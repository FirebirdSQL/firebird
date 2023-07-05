/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		JrdTraceManager.h
 *	DESCRIPTION:	Trace API manager
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

#ifndef JRD_TRACEMANAGER_H
#define JRD_TRACEMANAGER_H

#include "TraceManager.h"

namespace Firebird {

class ICryptKeyCallback;

}

namespace Jrd {

class Database;
class Attachment;
class jrd_tra;
class DsqlRequest;
class Service;

class JrdTraceManager : public TraceManager
{
public:
    /* Initializes plugins. */
	explicit JrdTraceManager(Attachment* in_att);
	explicit JrdTraceManager(Service* in_svc);
	JrdTraceManager(const char* in_filename, Firebird::ICryptKeyCallback* callback, bool failedAttach);

	/* Finalize plugins. Called when database is closed by the engine */
	~JrdTraceManager();


	/* Start/end transaction */
	void event_transaction_start(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		unsigned tpb_length, const ntrace_byte_t* tpb, ntrace_result_t tra_result);

	void event_transaction_end(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		bool commit, bool retain_context, ntrace_result_t tra_result);

	void event_set_context(Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, Firebird::ITraceContextVariable* variable);

	void event_proc_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceProcedure* procedure, bool started, ntrace_result_t proc_result);

	void event_func_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceFunction* function, bool started, ntrace_result_t func_result);

	void event_trigger_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceTrigger* trigger, bool started, ntrace_result_t trig_result);

	void event_blr_compile(Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, Firebird::ITraceBLRStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_blr_execute(Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, Firebird::ITraceBLRStatement* statement,
		ntrace_result_t req_result);

	void event_dyn_execute(Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceTransaction* transaction, Firebird::ITraceDYNRequest* request,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_service_attach(Firebird::ITraceServiceConnection* service, ntrace_result_t att_result);

	void event_service_start(Firebird::ITraceServiceConnection* service,
		unsigned switches_length, const char* switches,
		ntrace_result_t start_result);

	void event_service_query(Firebird::ITraceServiceConnection* service,
		unsigned send_item_length, const ntrace_byte_t* send_items,
		unsigned recv_item_length, const ntrace_byte_t* recv_items,
		ntrace_result_t query_result);

	void event_service_detach(Firebird::ITraceServiceConnection* service, ntrace_result_t detach_result);

	void event_error(Firebird::ITraceConnection* connection, Firebird::ITraceStatusVector* status, const char* function);

	void event_sweep(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state);

	typedef ntrace_mask_t NotificationNeeds;

	inline bool needs(unsigned e)
	{
		if (!active || !init_factories)
			return false;

		if (changeNumber != getStorage()->getChangeNumber())
			update_sessions();

		return trace_needs & (FB_CONST64(1) << e);
	}

	// should be called after attachment user is authenticated
	void activate()
	{
		active = true;
	}

	// helps avoid early use
	bool isActive()
	{
		return active;
	}

	// external access to stored attachment
	Attachment* getAttachment()
	{
		return attachment;
	}

	/* DSQL-friendly routines to call Trace API hooks.
       Needed because DSQL cannot include JRD for the current engine */
	static bool need_dsql_prepare(Attachment* att);
	static bool need_dsql_free(Attachment* att);
	static bool need_dsql_execute(Attachment* att);

	static void event_dsql_prepare(Attachment* att, jrd_tra* transaction, Firebird::ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	static void event_dsql_free(Attachment* att, Firebird::ITraceSQLStatement* statement,
		unsigned short option);

	static void event_dsql_execute(Attachment* att, jrd_tra* transaction, Firebird::ITraceSQLStatement* statement,
		bool started, ntrace_result_t req_result);

	static void event_dsql_restart(Attachment* att, jrd_tra* transaction, DsqlRequest* statement,
		int number);


private:
	Attachment*	attachment;
	Service* service;
	Firebird::ICryptKeyCallback* callback;



	void init();
	void update_sessions();
	void update_session(const Firebird::TraceSession& session);

	/* DSQL statement lifecycle. To be moved to public and used directly when DSQL becomes a part of JRD */
	void event_dsql_prepare(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_dsql_free(Firebird::ITraceDatabaseConnection* connection,
		Firebird::ITraceSQLStatement* statement, unsigned short option);

	void event_dsql_execute(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction,
		Firebird::ITraceSQLStatement* statement,
		bool started, ntrace_result_t req_result);

	void event_dsql_restart(Firebird::ITraceDatabaseConnection* connection, Firebird::ITraceTransaction* transaction, Firebird::ITraceSQLStatement* statement,
		unsigned number);

	bool active, failedAttach;
};

}

#endif
