/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		SecDbCache.cpp
 *	DESCRIPTION:	Cached security database connection
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2003.02.02 Dmitry Yemanov: Implemented cached security database connection
 * 2011 - 2020	Alexander Peshkov
 */

#include "firebird.h"

#include "../auth/SecDbCache.h"
#include "../common/status.h"
#include "../common/isc_proto.h"

#include <string.h>


using namespace Firebird;

namespace Auth {

void CachedSecurityDatabase::close()
{
	FbLocalStatus s;
	TimerInterfacePtr()->start(&s, this, 10 * 1000 * 1000);
	if (s->getState() & IStatus::STATE_ERRORS)
		handler();
}

void CachedSecurityDatabase::handler()
{
	list->handler(this);
}


void PluginDatabases::getInstance(IPluginConfig* pluginConfig, CachedSecurityDatabase::Instance& instance)
{
	// Determine sec.db name based on existing config
	PathName secDbName;
	{ // config scope
		FbLocalStatus s;
		RefPtr<IFirebirdConf> config(REF_NO_INCR, pluginConfig->getFirebirdConf(&s));
		check(&s);

		static GlobalPtr<ConfigKeys> keys;
		unsigned int secDbKey = keys->getKey(config, "SecurityDatabase");
		const char* tmp = config->asString(secDbKey);
		if (!tmp)
			Arg::Gds(isc_secdb_name).raise();

		secDbName = tmp;
	}

	{ // guard scope
		MutexLockGuard g(arrayMutex, FB_FUNCTION);
		for (unsigned int i = 0; i < dbArray.getCount(); )
		{
			if (secDbName == dbArray[i]->secureDbName)
			{
				CachedSecurityDatabase* fromCache = dbArray[i];
				// if element is just created or test passed we can use it
				if ((!fromCache->secDb) || fromCache->secDb->test())
				{
					instance.set(fromCache);
					break;
				}
				else
				{
					dbArray.remove(i);
					continue;
				}
			}
			++i;
		}

		if (!instance)
		{
			instance.set(FB_NEW CachedSecurityDatabase(this, secDbName));
			instance->addRef();
			secDbName.copyTo(instance->secureDbName, sizeof(instance->secureDbName));
			dbArray.add(instance);
		}
	}
}

int PluginDatabases::shutdown()
{
	try
	{
		MutexLockGuard g(arrayMutex, FB_FUNCTION);
		for (unsigned int i = 0; i < dbArray.getCount(); ++i)
		{
			if (dbArray[i])
			{
				FbLocalStatus s;
				TimerInterfacePtr()->stop(&s, dbArray[i]);
				check(&s);
				dbArray[i]->release();
				dbArray[i] = NULL;
			}
		}
		dbArray.clear();
	}
	catch (Exception &ex)
	{
 		StaticStatusVector st;
 		ex.stuffException(st);
		const ISC_STATUS* status = st.begin();
		if (status[0] == 1 && status[1] != isc_att_shutdown)
		{
			iscLogStatus("Legacy security database shutdown", status);
		}

		return FB_FAILURE;
	}

	return FB_SUCCESS;
}

void PluginDatabases::handler(CachedSecurityDatabase* tgt)
{
	try
	{
		MutexLockGuard g(arrayMutex, FB_FUNCTION);

		for (unsigned int i = 0; i < dbArray.getCount(); ++i)
		{
			if (dbArray[i] == tgt)
			{
				dbArray.remove(i);
				tgt->release();
				break;
			}
		}
	}
	catch (Exception &ex)
	{
 		StaticStatusVector st;
 		ex.stuffException(st);
		const ISC_STATUS* status = st.begin();
		if (status[0] == 1 && status[1] != isc_att_shutdown)
		{
			iscLogStatus("Security database timer handler", status);
		}
	}
}

} // namespace Auth
