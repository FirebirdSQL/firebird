/*
 *	PROGRAM:	Server Access Methods
 *	MODULE:		TraceManager.h
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

#ifndef TRACEMANAGER_H
#define TRACEMANAGER_H

#include "../../jrd/ntrace.h"
#include "../../common/classes/array.h"
#include "../../common/classes/fb_string.h"
#include "../../common/classes/init.h"
#include "../../common/classes/rwlock.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../jrd/trace/TraceConfigStorage.h"
#include "../../jrd/trace/TraceSession.h"

class TraceManager
{
public:
	/* Initializes plugins. */
	explicit TraceManager(Firebird::ITraceDatabaseConnection* conn, const char* in_filename);
	explicit TraceManager(const char* in_filename, MemoryPool& pool);
	void initServerTrace();

	/* Finalize plugins. Called when database is closed by the engine */
	~TraceManager();

	static Jrd::ConfigStorage* getStorage()
	{ return storageInstance->getStorage(); }

	static size_t pluginsCount()
	{ return factories->getCount(); }

	void event_attach(Firebird::ITraceDatabaseConnection* connection, bool create_db,
		ntrace_result_t att_result);

	void event_detach(Firebird::ITraceDatabaseConnection* connection, bool drop_db);

	typedef ntrace_mask_t NotificationNeeds;

	inline bool needs(unsigned e)
	{
		if (!init_factories)
			return false;

		if (changeNumber != getStorage()->getChangeNumber())
			update_sessions();

		return trace_needs & (FB_CONST64(1) << e);
	}

	static void shutdown();
	static Firebird::ITraceLogWriter* createSessionLogWriter(const Firebird::TraceSession& session);


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

protected:
	void reload_sessions_lists(Firebird::HalfStaticArray<Firebird::TraceSession*, 64>& newSessions);

private:
	Firebird::ITraceDatabaseConnection* connection;

protected:
	const char* filename;
	NotificationNeeds trace_needs, new_needs;

	// This structure should be POD-like to be stored in Array
	struct FactoryInfo
	{
		FactoryInfo() : factory(NULL)
		{
			memset(name, 0, sizeof(name));
		}

		Firebird::ITraceFactory* factory;
		char name[MAXPATHLEN];
	};

	class Factories : public Firebird::Array<FactoryInfo>
	{
	public:
		explicit Factories(Firebird::MemoryPool& p)
			: Firebird::Array<FactoryInfo>(p)
		{ }

		~Factories()
		{
			Firebird::PluginManagerInterfacePtr pi;

			for (unsigned int i = 0; i < getCount(); ++i)
				pi->releasePlugin(getElement(i).factory);
		}
	};


	static Factories* factories;
	static Firebird::GlobalPtr<Firebird::RWLock> init_factories_lock;
	static volatile bool init_factories;

	struct SessionInfo
	{
		FactoryInfo* factory_info;
		Firebird::ITracePlugin* plugin;
		ULONG ses_id;

		static ULONG generate(const SessionInfo& item)
		{ return item.ses_id; }
	};
	class Sessions : public Firebird::SortedArray<SessionInfo, Firebird::EmptyStorage<SessionInfo>, ULONG, SessionInfo>
	{
	public:
		explicit Sessions(MemoryPool& p)
			: Firebird::SortedArray<SessionInfo, Firebird::EmptyStorage<SessionInfo>, ULONG, SessionInfo>(p)
		{ }

		~Sessions()
		{
			for (unsigned int i = 0; i < getCount(); ++i)
			{
				getElement(i).plugin->release();
			}
		}
	};

	Sessions trace_sessions;

	void load_plugins();
	void update_sessions();
	void update_session(const Firebird::TraceSession& session);

	bool check_result(Firebird::ITracePlugin* plugin, const char* module, const char* function, bool result);

	static Firebird::GlobalPtr<Jrd::StorageInstance, Firebird::InstanceControl::PRIORITY_DELETE_FIRST> storageInstance;

	ULONG changeNumber;
};

#endif // !TRACEMANAGER_H
