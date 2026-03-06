/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		traceplugin.cpp
 *	DESCRIPTION:	Exported entrypoints for the plugin
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
#include "../common/classes/ImplementHelper.h"

#include "TraceConfiguration.h"
#include "TracePluginImpl.h"

namespace Firebird::Ntrace
{


class TraceFactoryImpl final :
	public StdPlugin<ITraceFactoryImpl<TraceFactoryImpl, CheckStatusWrapper>>
{
public:
	explicit TraceFactoryImpl(IPluginConfig*)
	{ }

	// TraceFactory implementation
	ntrace_mask_t trace_needs();
	ITracePlugin* trace_create(CheckStatusWrapper* status,
		ITraceInitInfo* init_info);
};

ntrace_mask_t TraceFactoryImpl::trace_needs()
{
	return (1 << ITraceFactory::TRACE_EVENT_MAX) - 1;
}

ITracePlugin* TraceFactoryImpl::trace_create(CheckStatusWrapper* status,
	ITraceInitInfo* initInfo)
{
	MasterInterfacePtr master;
	const char* dbname = NULL;
	try
	{
		dbname = initInfo->getDatabaseName();
		if (!dbname)
			dbname = "";

		TracePluginConfig config;
		TraceCfgReader::readTraceConfiguration(initInfo->getConfigText(), dbname, config);

		ITraceDatabaseConnection* connection = initInfo->getConnection();

		if (!config.enabled ||
			(config.connection_id && connection &&
				(connection->getConnectionID() != config.connection_id)))
		{
			return NULL; // Plugin is not needed, no error happened.
		}

		AutoPtr<ITraceLogWriter, SimpleRelease>
			logWriter(initInfo->getLogWriter());

		if (logWriter)
			config.log_filename = "";

		return FB_NEW TracePluginImpl(this, config, initInfo);	// Everything is ok, we created a plugin

	}
	catch (Exception& ex)
	{
		// put error into trace log
		ITraceLogWriter* logWriter = initInfo->getLogWriter();
		if (logWriter)
		{
			const char* strEx = TracePluginImpl::marshal_exception(ex);
			string err;
			if (dbname && dbname[0])
				err.printf("Error creating trace session for database \"%s\":\n%s\n", dbname, strEx);
			else
				err.printf("Error creating trace session for service manager attachment:\n%s\n", strEx);

			logWriter->write(err.c_str(), err.length());
			logWriter->release();
		}
		else
			ex.stuffException(status);
	}

	return NULL;
}


static SimpleFactory<TraceFactoryImpl> traceFactory;

void registerTrace(IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(IPluginManager::TYPE_TRACE, "fbtrace", &traceFactory);
	getUnloadDetector()->registerMe();
}


} // namespace Firebird::Ntrace


using namespace Firebird;

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	CachedMasterInterface::set(master);
	Ntrace::registerTrace(PluginManagerInterfacePtr());
}
