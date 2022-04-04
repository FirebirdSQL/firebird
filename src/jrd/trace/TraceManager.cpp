/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		TraceManager.cpp
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

#include "../../jrd/trace/TraceManager.h"
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

GlobalPtr<StorageInstance, InstanceControl::PRIORITY_DELETE_FIRST> TraceManager::storageInstance;
TraceManager::Factories* TraceManager::factories = NULL;
GlobalPtr<RWLock> TraceManager::init_factories_lock;
volatile bool TraceManager::init_factories;


bool TraceManager::check_result(ITracePlugin* plugin, const char* module, const char* function,
	bool result)
{
	if (result)
		return true;

	if (!plugin)
	{
		gds__log("Trace plugin %s returned error on call %s, "
			"did not create plugin and provided no additional details on reasons of failure",
			module, function);
		return false;
	}

	const char* errorStr = plugin->trace_get_error();

	if (!errorStr)
	{
		gds__log("Trace plugin %s returned error on call %s, "
			"but provided no additional details on reasons of failure", module, function);
		return false;
	}

	gds__log("Trace plugin %s returned error on call %s.\n\tError details: %s",
		module, function, errorStr);
	return false;
}

TraceManager::TraceManager(Attachment* in_att) :
	attachment(in_att),
	service(NULL),
	filename(NULL),
	trace_sessions(*in_att->att_pool),
	active(false)
{
	init();
}

TraceManager::TraceManager(Service* in_svc) :
	attachment(NULL),
	service(in_svc),
	filename(NULL),
	trace_sessions(in_svc->getPool()),
	active(true)
{
	init();
}

TraceManager::TraceManager(const char* in_filename) :
	attachment(NULL),
	service(NULL),
	filename(in_filename),
	trace_sessions(*getDefaultMemoryPool()),
	active(true)
{
	init();
}

TraceManager::~TraceManager()
{
}

void TraceManager::init()
{
	// ensure storage is initialized
	getStorage();
	load_plugins();
	changeNumber = 0;
}

void TraceManager::load_plugins()
{
	// Initialize all trace needs to false
	trace_needs = 0;

	if (init_factories)
		return;

	WriteLockGuard guard(init_factories_lock, FB_FUNCTION);
	if (init_factories)
		return;

	factories = FB_NEW_POOL(*getDefaultMemoryPool()) TraceManager::Factories(*getDefaultMemoryPool());
	for (GetPlugins<ITraceFactory> traceItr(IPluginManager::TYPE_TRACE); traceItr.hasData(); traceItr.next())
	{
		FactoryInfo info;
		info.factory = traceItr.plugin();
		info.factory->addRef();
		string name(traceItr.name());
		name.copyTo(info.name, sizeof(info.name));
		factories->add(info);
	}

	init_factories = true;
}


void TraceManager::shutdown()
{
	if (init_factories)
	{
		WriteLockGuard guard(init_factories_lock, FB_FUNCTION);

		if (init_factories)
		{
			init_factories = false;
			delete factories;
			factories = NULL;
		}
	}

	getStorage()->shutdown();
}


void TraceManager::update_sessions()
{
	// Let be inactive until database is creating
	if (attachment && (attachment->att_database->dbb_flags & DBB_creating))
		return;

	MemoryPool& pool = *getDefaultMemoryPool();
	SortedArray<ULONG, InlineStorage<ULONG, 64> > liveSessions(pool);
	HalfStaticArray<TraceSession*, 64> newSessions(pool);

	{	// scope
		ConfigStorage* storage = getStorage();

		StorageGuard guard(storage);
		storage->restart();

		TraceSession session(pool);
		while (storage->getNextSession(session, ConfigStorage::FLAGS))
		{
			if ((session.ses_flags & trs_active) && !(session.ses_flags & trs_log_full))
			{
				FB_SIZE_T pos;
				if (trace_sessions.find(session.ses_id, pos))
					liveSessions.add(session.ses_id);
				else
				{
					storage->getSession(session, ConfigStorage::ALL);
					newSessions.add(FB_NEW_POOL(pool) TraceSession(pool, session));
				}
			}
		}

		changeNumber = storage->getChangeNumber();
	}

	// remove sessions not present in storage
	FB_SIZE_T i = 0;
	while (i < trace_sessions.getCount())
	{
		FB_SIZE_T pos;
		if (liveSessions.find(trace_sessions[i].ses_id, pos)) {
			i++;
		}
		else
		{
			trace_sessions[i].plugin->release();
			trace_sessions.remove(i);
		}
	}

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

void TraceManager::update_session(const TraceSession& session)
{
	// if this session is already known, nothing to do
	FB_SIZE_T pos;
	if (trace_sessions.find(session.ses_id, pos)) {
		return;
	}

	// if this session is not from administrator, it may trace connections
	// only created by the same user
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
				if ((!attachment->att_user) || (attachment->att_flags & ATT_mapping))
					return;

				curr_user = attachment->getUserName().c_str();

				if (session.ses_auth.hasData())
				{ // scope
					AutoSetRestoreFlag<ULONG> autoRestore(&attachment->att_flags, ATT_mapping, true);

					Database* dbb = attachment->att_database;
					fb_assert(dbb);
					Mapping mapping(Mapping::MAP_NO_FLAGS, dbb->dbb_callback);
					mapping.needSystemPrivileges(priv);
					mapping.setAuthBlock(session.ses_auth);
					mapping.setSqlRole(session.ses_role);
					mapping.setSecurityDbAlias(dbb->dbb_config->getSecurityDatabase(), dbb->dbb_filename.c_str());
					mapping.setDb(attachment->att_filename.c_str(), dbb->dbb_filename.c_str(),
						attachment->getInterface());
					mapResult = mapping.mapUser(s_user, t_role);
				}
			}
			else if (service)
			{
				curr_user = service->getUserName().c_str();

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
			s_user != curr_user && (!priv.test(TRACE_ANY_ATTACHMENT)))
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

bool TraceManager::need_dsql_prepare(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_PREPARE);
}

bool TraceManager::need_dsql_free(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_FREE);
}

bool TraceManager::need_dsql_execute(Attachment* att)
{
	return att->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_DSQL_EXECUTE);
}

void TraceManager::event_dsql_prepare(Attachment* att, jrd_tra* transaction,
		ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	TraceConnectionImpl conn(att);
	TraceTransactionImpl tran(transaction);

	att->att_trace_manager->event_dsql_prepare(&conn, transaction ? &tran : NULL, statement,
											   time_millis, req_result);
}

void TraceManager::event_dsql_free(Attachment* att,	ITraceSQLStatement* statement,
		unsigned short option)
{
	TraceConnectionImpl conn(att);

	att->att_trace_manager->event_dsql_free(&conn, statement, option);
}

void TraceManager::event_dsql_execute(Attachment* att, jrd_tra* transaction,
	ITraceSQLStatement* statement, bool started, ntrace_result_t req_result)
{
	TraceConnectionImpl conn(att);
	TraceTransactionImpl tran(transaction);

	att->att_trace_manager->event_dsql_execute(&conn, transaction ? &tran : NULL, statement, 
											   started, req_result);
}


#define EXECUTE_HOOKS(METHOD, PARAMS) \
	FB_SIZE_T i = 0; \
	while (i < trace_sessions.getCount()) \
	{ \
		SessionInfo* plug_info = &trace_sessions[i]; \
		if (check_result(plug_info->plugin, plug_info->factory_info->name, #METHOD, \
			plug_info->plugin->METHOD PARAMS)) \
		{ \
			i++; /* Move to next plugin */ \
		} \
		else { \
			trace_sessions.remove(i); /* Remove broken plugin from the list */ \
		} \
	}


void TraceManager::event_attach(ITraceDatabaseConnection* connection,
		bool create_db, ntrace_result_t att_result)
{
	EXECUTE_HOOKS(trace_attach,
		(connection, create_db, att_result));
}

void TraceManager::event_detach(ITraceDatabaseConnection* connection, bool drop_db)
{
	EXECUTE_HOOKS(trace_detach, (connection, drop_db));
}

void TraceManager::event_transaction_start(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, unsigned tpb_length, const ntrace_byte_t* tpb,
		ntrace_result_t tra_result)
{
	EXECUTE_HOOKS(trace_transaction_start,
		(connection, transaction, tpb_length, tpb, tra_result));
}

void TraceManager::event_transaction_end(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, bool commit, bool retain_context,
		ntrace_result_t tra_result)
{
	EXECUTE_HOOKS(trace_transaction_end,
		(connection, transaction, commit, retain_context, tra_result));
}

void TraceManager::event_set_context(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceContextVariable* variable)
{
	EXECUTE_HOOKS(trace_set_context,
		(connection, transaction, variable));
}

 void TraceManager::event_proc_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceProcedure* procedure, bool started, ntrace_result_t proc_result)
{
	EXECUTE_HOOKS(trace_proc_execute,
		(connection, transaction, procedure, started, proc_result));
}

void TraceManager::event_func_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceFunction* function, bool started, ntrace_result_t func_result)
{
	EXECUTE_HOOKS(trace_func_execute,
		(connection, transaction, function, started, func_result));
}

void TraceManager::event_trigger_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceTrigger* trigger, bool started, ntrace_result_t trig_result)
{
	EXECUTE_HOOKS(trace_trigger_execute,
		(connection, transaction, trigger, started, trig_result));
}

void TraceManager::event_dsql_prepare(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceSQLStatement* statement, ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dsql_prepare,
		(connection, transaction, statement,
		 time_millis, req_result));
}

void TraceManager::event_dsql_free(ITraceDatabaseConnection* connection,
		ITraceSQLStatement* statement, unsigned short option)
{
	EXECUTE_HOOKS(trace_dsql_free,
		(connection, statement, option));
}

void TraceManager::event_dsql_execute(ITraceDatabaseConnection* connection, ITraceTransaction* transaction,
		ITraceSQLStatement* statement, bool started, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dsql_execute,
		(connection, transaction, statement, started, req_result));
}


void TraceManager::event_blr_compile(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceBLRStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_blr_compile,
		(connection, transaction, statement,
		 time_millis, req_result));
}

void TraceManager::event_blr_execute(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceBLRStatement* statement,
		ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_blr_execute,
		(connection, transaction, statement, req_result));
}

void TraceManager::event_dyn_execute(ITraceDatabaseConnection* connection,
		ITraceTransaction* transaction, ITraceDYNRequest* request,
		ntrace_counter_t time_millis, ntrace_result_t req_result)
{
	EXECUTE_HOOKS(trace_dyn_execute,
		(connection, transaction, request, time_millis,
			req_result));
}

void TraceManager::event_service_attach(ITraceServiceConnection* service, ntrace_result_t att_result)
{
	EXECUTE_HOOKS(trace_service_attach,
		(service, att_result));
}

void TraceManager::event_service_start(ITraceServiceConnection* service,
		unsigned switches_length, const char* switches,
		ntrace_result_t start_result)
{
	EXECUTE_HOOKS(trace_service_start,
		(service, switches_length, switches, start_result));
}

void TraceManager::event_service_query(ITraceServiceConnection* service,
		unsigned send_item_length, const ntrace_byte_t* send_items,
		unsigned recv_item_length, const ntrace_byte_t* recv_items,
		ntrace_result_t query_result)
{
	EXECUTE_HOOKS(trace_service_query,
		(service, send_item_length, send_items,
		 recv_item_length, recv_items, query_result));
}

void TraceManager::event_service_detach(ITraceServiceConnection* service, ntrace_result_t detach_result)
{
	EXECUTE_HOOKS(trace_service_detach,
		(service, detach_result));
}

void TraceManager::event_error(ITraceConnection* connection, ITraceStatusVector* status, const char* function)
{
	EXECUTE_HOOKS(trace_event_error,
		(connection, status, function));
}


void TraceManager::event_sweep(ITraceDatabaseConnection* connection, ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state)
{
	EXECUTE_HOOKS(trace_event_sweep,
		(connection, sweep, sweep_state));
}

}
