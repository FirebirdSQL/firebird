/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		JrdTraceManager.cpp
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

#include "firebird.h"

#include "JrdTraceManager.h"
#include "../../jrd/trace/TraceObjects.h"
#include "../../jrd/Mapping.h"
#include "../../common/os/path_utils.h"
#include "../../common/ScanDir.h"
#include "../../common/isc_proto.h"
#include "../../common/classes/GetPlugins.h"
#include "../../common/db_alias.h"

#ifdef WIN_NT
#include <process.h>
#endif

using namespace Firebird;

namespace
{
	static const char* const NTRACE_PREFIX = "fbtrace";
}

namespace Jrd {


JrdTraceManager::JrdTraceManager(Attachment* in_att) :
	TraceManager(NULL, *in_att->att_pool),
	attachment(in_att),
	service(NULL),
	callback(NULL),
	active(false)
{
	init();
}

JrdTraceManager::JrdTraceManager(Service* in_svc) :
	TraceManager(NULL, in_svc->getPool()),
	attachment(NULL),
	service(in_svc),
	callback(NULL),
	active(true)
{
	init();
}

JrdTraceManager::JrdTraceManager(const char* in_filename, ICryptKeyCallback* cb, bool failed) :
	TraceManager(in_filename, *getDefaultMemoryPool()),
	attachment(NULL),
	service(NULL),
	callback(cb),
	active(true),
	failedAttach(failed)
{
	init();
}

JrdTraceManager::~JrdTraceManager()
{
}

void JrdTraceManager::init()
{
	// ensure storage is initialized
	getStorage();
	load_plugins();
	changeNumber = 0;
}



void JrdTraceManager::update_sessions()
{
	// Let be inactive until database is creating
	if (attachment && (attachment->att_database->dbb_flags & DBB_creating))
		return;

	MemoryPool& pool = *getDefaultMemoryPool();
	HalfStaticArray<TraceSession*, 64> newSessions(pool);

	reload_sessions_lists(newSessions);


	// add new sessions
	new_needs = trace_needs;
	trace_needs = 0;
	while (newSessions.hasData())
	{
		TraceSession* s = newSessions.pop();
		update_session(*s);
		delete s;
	}

	// nothing to trace, clear needs
	if (trace_sessions.getCount() == 0)
	{
		trace_needs = 0;
	}
	else
	{
		trace_needs = new_needs;
	}
}

void JrdTraceManager::update_session(const TraceSession& session)
{
	// if this session is already known, nothing to do
	FB_SIZE_T pos;
	if (trace_sessions.find(session.ses_id, pos)) {
		return;
	}

	// if this session is not from administrator, it may trace connections
	// only created by the same user, or when it has TRACE_ANY_ATTACHMENT
	// privilege in current context
	if (!(session.ses_flags & (trs_admin | trs_system)))
	{
		const char* curr_user = nullptr;
		string s_user = session.ses_user;
		string t_role;
		UserId::Privileges priv;

		try
		{
			ULONG mapResult = 0;

			if (attachment)
			{
				if (attachment->att_flags & ATT_mapping)
					return;

				if (attachment->att_user)
					curr_user = attachment->att_user->getUserName().c_str();

				if (session.ses_auth.hasData())
				{
					AutoSetRestoreFlag<ULONG> autoRestore(&attachment->att_flags, ATT_mapping, true);

					Database* dbb = attachment->att_database;
					fb_assert(dbb);
					Mapping mapping(Mapping::MAP_NO_FLAGS, dbb->dbb_callback);
					mapping.needSystemPrivileges(priv);
					mapping.setAuthBlock(session.ses_auth);
					mapping.setSqlRole(session.ses_role);
					mapping.setSecurityDbAlias(dbb->dbb_config->getSecurityDatabase(), dbb->dbb_filename.c_str());

					fb_assert(attachment->getInterface());
					mapping.setDb(attachment->att_filename.c_str(), dbb->dbb_filename.c_str(),
						attachment->getInterface());

					EngineCheckout guard(attachment, FB_FUNCTION);
					mapResult = mapping.mapUser(s_user, t_role);
				}
			}
			else if (service)
			{
				curr_user = service->getUserName().nullStr();

				if (session.ses_auth.hasData())
				{
					PathName dummy;
					RefPtr<const Config> config;
					expandDatabaseName(service->getExpectedDb(), dummy, &config);

					Mapping mapping(Mapping::MAP_NO_FLAGS, service->getCryptCallback());
					mapping.needSystemPrivileges(priv);
					mapping.setAuthBlock(session.ses_auth);
					mapping.setErrorMessagesContextName("services manager");
					mapping.setSqlRole(session.ses_role);
					mapping.setSecurityDbAlias(config->getSecurityDatabase(), nullptr);

					mapResult = mapping.mapUser(s_user, t_role);
				}
			}
			else if (filename)
			{
				if (session.ses_auth.hasData())
				{
					Mapping mapping(Mapping::MAP_NO_FLAGS, callback);
					mapping.needSystemPrivileges(priv);
					mapping.setAuthBlock(session.ses_auth);
					mapping.setSqlRole(session.ses_role);

					RefPtr<const Config> config;
					PathName org_filename(filename), expanded_name;
					if (! expandDatabaseName(org_filename, expanded_name, &config))
						expanded_name = filename;

					mapping.setSecurityDbAlias(config->getSecurityDatabase(), expanded_name.c_str());
					if (!failedAttach)
						mapping.setDb(filename, expanded_name.c_str(), nullptr);

					mapResult = mapping.mapUser(s_user, t_role);
				}
			}
			else
			{
				// failed attachment attempts traced by admin trace only
				return;
			}

			if (mapResult & Mapping::MAP_ERROR_NOT_THROWN)
			{
				// Error in mapUser() means missing context, therefore...
				return;
			}
		}
		catch (const Exception&)
		{
			return;
		}

		t_role.upper();
		if (s_user != DBA_USER_NAME && t_role != ADMIN_ROLE &&
			((!curr_user) || (s_user != curr_user)) && (!priv.test(TRACE_ANY_ATTACHMENT)))
		{
			return;
		}
	}

	ReadLockGuard guard(init_factories_lock, FB_FUNCTION);
	if (!factories)
		return;

	for (FactoryInfo* info = factories->begin(); info != factories->end(); ++info)
	{
		TraceInitInfoImpl attachInfo(session, attachment, filename);
		FbLocalStatus status;
		ITracePlugin* plugin = info->factory->trace_create(&status, &attachInfo);

		if (plugin)
		{
			plugin->addRef();
			SessionInfo sesInfo;
			sesInfo.plugin = plugin;
			sesInfo.factory_info = info;
			sesInfo.ses_id = session.ses_id;
			trace_sessions.add(sesInfo);

			new_needs |= info->factory->trace_needs();
		}
		else if (status->getState() & IStatus::STATE_ERRORS)
		{
			string header;
			header.printf("Trace plugin %s returned error on call trace_create.", info->name);
			iscLogStatus(header.c_str(), &status);
		}
	}
}

bool JrdTraceManager::need_dsql_prepare(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_PREPARE);
}

bool JrdTraceManager::need_dsql_free(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_FREE);
}

bool JrdTraceManager::need_dsql_execute(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_EXECUTE);
}

void JrdTraceManager::event_dsql_prepare(Attachment* att, jrd_tra* transaction,
		ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	TraceConnectionImpl conn(att);
	TraceTransactionImpl tran(transaction);

	att->att_trace_manager->event_dsql_prepare(&conn, transaction ? &tran : NULL, statement,
											   time_millis, req_result);
}

void JrdTraceManager::event_dsql_free(Attachment* att,	ITraceSQLStatement* statement,
		unsigned short option)
{
	TraceConnectionImpl conn(att);

	att->att_trace_manager->event_dsql_free(&conn, statement, option);
}

void JrdTraceManager::event_dsql_execute(Attachment* att, jrd_tra* transaction,
	ITraceSQLStatement* statement, bool started, ntrace_result_t req_result)
{
	TraceConnectionImpl conn(att);
	TraceTransactionImpl tran(transaction);

	att->att_trace_manager->event_dsql_execute(&conn, transaction ? &tran : NULL, statement,
											   started, req_result);
}

void JrdTraceManager::event_dsql_restart(Attachment* att, jrd_tra* transaction,
	DsqlRequest* statement, int number)
{
	TraceConnectionImpl conn(att);
	TraceTransactionImpl tran(transaction);
	TraceSQLStatementImpl stmt(statement, NULL);

	att->att_trace_manager->event_dsql_restart(&conn, transaction ? &tran : NULL, &stmt,
											   (unsigned) number);
}


void JrdTraceManager::event_transaction_start(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, unsigned tpb_length, const ntrace_byte_t* tpb,
		ntrace_result_t tra_result)
{
	EXECUTE_HOOKS(trace_transaction_start,
		(connection, transaction, tpb_length, tpb, tra_result));
}

void JrdTraceManager::event_transaction_end(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, bool commit, bool retain_context,
		ntrace_result_t tra_result)
{
	EXECUTE_HOOKS(trace_transaction_end,
		(connection, transaction, commit, retain_context, tra_result));
}

void JrdTraceManager::event_set_context(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceContextVariable* variable)
{
	EXECUTE_HOOKS(trace_set_context,
		(connection, transaction, variable));
}

 void JrdTraceManager::event_proc_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceProcedure* procedure, bool started, ntrace_result_t proc_result)
{
	EXECUTE_HOOKS(trace_proc_execute,
		(connection, transaction, procedure, started, proc_result));
}

void JrdTraceManager::event_func_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceFunction* function, bool started, ntrace_result_t func_result)
{
	EXECUTE_HOOKS(trace_func_execute,
		(connection, transaction, function, started, func_result));
}

void JrdTraceManager::event_trigger_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceTrigger* trigger, bool started, ntrace_result_t trig_result)
{
	EXECUTE_HOOKS(trace_trigger_execute,
		(connection, transaction, trigger, started, trig_result));
}

void JrdTraceManager::event_dsql_prepare(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceSQLStatement* statement, ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dsql_prepare,
		(connection, transaction, statement,
		 time_millis, req_result));
}

void JrdTraceManager::event_dsql_free(ITraceDatabaseConnection* connection,
		ITraceSQLStatement* statement, unsigned short option)
{
	EXECUTE_HOOKS(trace_dsql_free,
		(connection, statement, option));
}

void JrdTraceManager::event_dsql_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceSQLStatement* statement, bool started, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dsql_execute,
		(connection, transaction, statement, started, req_result));
}

void JrdTraceManager::event_dsql_restart(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceSQLStatement* statement, unsigned number)
{
	EXECUTE_HOOKS(trace_dsql_restart,
		(connection, transaction, statement, number));
}

void JrdTraceManager::event_blr_compile(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceBLRStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_blr_compile,
		(connection, transaction, statement,
		 time_millis, req_result));
}

void JrdTraceManager::event_blr_execute(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceBLRStatement* statement,
		ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_blr_execute,
		(connection, transaction, statement, req_result));
}

void JrdTraceManager::event_dyn_execute(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceDYNRequest* request,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dyn_execute,
		(connection, transaction, request, time_millis,
			req_result));
}

void JrdTraceManager::event_service_attach(ITraceServiceConnection* service, ntrace_result_t att_result)
{
	EXECUTE_HOOKS(trace_service_attach,
		(service, att_result));
}

void JrdTraceManager::event_service_start(ITraceServiceConnection* service,
		unsigned switches_length, const char* switches,
		ntrace_result_t start_result)
{
	EXECUTE_HOOKS(trace_service_start,
		(service, switches_length, switches, start_result));
}

void JrdTraceManager::event_service_query(ITraceServiceConnection* service,
		unsigned send_item_length, const ntrace_byte_t* send_items,
		unsigned recv_item_length, const ntrace_byte_t* recv_items,
		ntrace_result_t query_result)
{
	EXECUTE_HOOKS(trace_service_query,
		(service, send_item_length, send_items,
		 recv_item_length, recv_items, query_result));
}

void JrdTraceManager::event_service_detach(ITraceServiceConnection* service, ntrace_result_t detach_result)
{
	EXECUTE_HOOKS(trace_service_detach,
		(service, detach_result));
}

void JrdTraceManager::event_error(ITraceConnection* connection, ITraceStatusVector* status, const char* function)
{
	EXECUTE_HOOKS(trace_event_error,
		(connection, status, function));
}


void JrdTraceManager::event_sweep(ITraceDatabaseConnection* connection, ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state)
{
	EXECUTE_HOOKS(trace_event_sweep,
		(connection, sweep, sweep_state));
}

}
