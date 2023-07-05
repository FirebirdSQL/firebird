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

#include "TraceManager.h"
#include "../../common/os/path_utils.h"
#include "../../common/ScanDir.h"
#include "../../common/isc_proto.h"
#include "../../common/classes/GetPlugins.h"
#include "../../common/db_alias.h"
#include "../../jrd/trace/TraceLog.h"

#ifdef WIN_NT
#include <process.h>
#endif

using namespace Firebird;


class TraceLogWriterImpl final :
	public RefCntIface<ITraceLogWriterImpl<TraceLogWriterImpl, CheckStatusWrapper> >
{
public:
	TraceLogWriterImpl(const TraceSession& session) :
		m_log(*getDefaultMemoryPool(), session.ses_logfile, false),
		m_sesId(session.ses_id)
	{
		string s;
		s.printf("\n--- Session %d is suspended as its log is full ---\n", session.ses_id);
		m_log.setFullMsg(s.c_str());
	}

	// TraceLogWriter implementation

	FB_SIZE_T write(const void* buf, FB_SIZE_T size)
	{
		const FB_SIZE_T written = m_log.write(buf, size);
		if (written == size)
			return size;

		if (!m_log.isFull())
			return written;

		Jrd::ConfigStorage* storage = TraceManager::getStorage();
		Jrd::StorageGuard guard(storage);

		TraceSession session(*getDefaultMemoryPool());
		session.ses_id = m_sesId;
		if (storage->getSession(session, Jrd::ConfigStorage::FLAGS))
		{
			if (!(session.ses_flags & trs_log_full))
			{
				// suspend session
				session.ses_flags |= trs_log_full;
				storage->updateFlags(session);
			}
		}

		// report successful write
		return size;
	}

	FB_SIZE_T write_s(CheckStatusWrapper* status, const void* buf, FB_SIZE_T size)
	{
		try
		{
			return write(buf, size);
		}
		catch (Exception &ex)
		{
			ex.stuffException(status);
		}

		return 0;
	}

	int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

private:
	Jrd::TraceLog m_log;
	ULONG m_sesId;
};


class ServerTraceInitInfoImpl :
	public AutoIface<ITraceInitInfoImpl<ServerTraceInitInfoImpl, CheckStatusWrapper> >
{
public:
	ServerTraceInitInfoImpl(const TraceSession& session, ITraceDatabaseConnection* conn,
		const char* dbName) :
		session(session),
		dbName(dbName),
		logWriter(),
		connection(conn)
	{ }

	// TraceInitInfo implementation
	const char* getConfigText()			{ return session.ses_config.c_str(); }
	int getTraceSessionID()				{ return session.ses_id; }
	const char* getTraceSessionName()	{ return session.ses_name.c_str(); }
	int getTraceSessionFlags()			{ return session.ses_flags; }

	const char* getFirebirdRootDirectory() { return Config::getRootDirectory(); }
	const char* getDatabaseName()		{ return dbName; }

	Firebird::ITraceDatabaseConnection* getConnection()
	{
		return connection;
	}

	Firebird::ITraceServiceConnection* getService()
	{
		return NULL;
	}

	Firebird::ITraceLogWriter* getLogWriter()
	{
		if (!logWriter && !session.ses_logfile.empty())
		{
			logWriter = FB_NEW TraceLogWriterImpl(session);
		}
		if (logWriter)
		{
			logWriter->addRef();
		}
		return logWriter;
	}

private:
	const Firebird::TraceSession& session;
	const char* dbName;
	Firebird::RefPtr<Firebird::ITraceLogWriter> logWriter;
	ITraceDatabaseConnection* connection;
};


GlobalPtr<Jrd::StorageInstance, InstanceControl::PRIORITY_DELETE_FIRST> TraceManager::storageInstance;
TraceManager::Factories* TraceManager::factories = NULL;
GlobalPtr<Firebird::RWLock> TraceManager::init_factories_lock;
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


TraceManager::TraceManager(ITraceDatabaseConnection* conn, const char* in_filename) :
	filename(in_filename),
	trace_needs(0),
	new_needs(0),
	connection(conn),
	trace_sessions(*getDefaultMemoryPool()),
	changeNumber(0)
{
}

TraceManager::TraceManager(const char* in_filename, MemoryPool& pool) :
	filename(in_filename),
	trace_needs(0),
	new_needs(0),
	connection(NULL),
	trace_sessions(pool),
	changeNumber(0)
{ }

TraceManager::~TraceManager()
{
}

void TraceManager::initServerTrace()
{
	// ensure storage is initialized

	// In Classic mode this is the first getStorage call, so it always creates the storage
	// but it may not reread the config
	getStorage(); // Open storage

	load_plugins();
	changeNumber = 0;
}

void TraceManager::load_plugins()
{
	// Initialize all trace needs to false
	trace_needs = 0;

	if (init_factories)
		return;

	Firebird::WriteLockGuard guard(init_factories_lock, FB_FUNCTION);
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
		Firebird::WriteLockGuard guard(init_factories_lock, FB_FUNCTION);

		if (init_factories)
		{
			init_factories = false;
			delete factories;
			factories = NULL;
		}
	}

	getStorage()->shutdown();
}

ITraceLogWriter* TraceManager::createSessionLogWriter(const TraceSession& session)
{
	return FB_NEW TraceLogWriterImpl(session);
}

void TraceManager::reload_sessions_lists(HalfStaticArray<TraceSession*, 64>& newSessions)
{
	MemoryPool& pool = *getDefaultMemoryPool();
	SortedArray<ULONG, InlineStorage<ULONG, 64> > liveSessions(pool);

	{	// scope
		Jrd::ConfigStorage* storage = getStorage();

		Jrd::StorageGuard guard(storage);
		storage->restart();

		TraceSession session(pool);
		while (storage->getNextSession(session, Jrd::ConfigStorage::FLAGS))
		{
			if ((session.ses_flags & trs_active) && !(session.ses_flags & trs_log_full))
			{
				FB_SIZE_T pos;
				if (trace_sessions.find(session.ses_id, pos))
					liveSessions.add(session.ses_id);
				else
				{
					storage->getSession(session, Jrd::ConfigStorage::ALL);
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
}

void TraceManager::update_sessions()
{
	MemoryPool& pool = *getDefaultMemoryPool();
	HalfStaticArray<TraceSession*, 64> newSessions(pool);

	reload_sessions_lists(newSessions);

	// add new sessions
	new_needs = trace_needs;
	trace_needs = 0;
	while (newSessions.hasData())
	{
		// Preventing memory leaks in case of an exception
		Firebird::AutoPtr<TraceSession> s(newSessions.pop());
		update_session(*s.get());
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

	Firebird::ReadLockGuard guard(init_factories_lock, FB_FUNCTION);
	if (!factories)
		return;

	for (FactoryInfo* info = factories->begin(); info != factories->end(); ++info)
	{
		Firebird::LocalStatus localStatus;
		Firebird::CheckStatusWrapper status(&localStatus);

		ServerTraceInitInfoImpl attachInfo(session, connection, filename);
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
		else if (status.getState() & IStatus::STATE_ERRORS)
		{
			string header;
			header.printf("Trace plugin %s returned error on call trace_create.", info->name);
			iscLogStatus(header.c_str(), &status);
		}
	}
}

void TraceManager::event_attach(Firebird::ITraceDatabaseConnection* connection, bool create_db,
		ntrace_result_t att_result)
{
	EXECUTE_HOOKS(trace_attach,
		(connection, create_db, att_result));

	trace_needs &= ~(FB_CONST64(1) << ITraceFactory::TRACE_EVENT_ATTACH);
}

void TraceManager::event_detach(ITraceDatabaseConnection* connection, bool drop_db)
{
	EXECUTE_HOOKS(trace_detach, (connection, drop_db));

	trace_needs &= ~(FB_CONST64(1) << ITraceFactory::TRACE_EVENT_DETACH);
}
