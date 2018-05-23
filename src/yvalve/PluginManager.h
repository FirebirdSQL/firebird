/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef YVALVE_PLUGIN_MANAGER_H
#define YVALVE_PLUGIN_MANAGER_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"

#include "../common/os/mod_loader.h"
#include "../common/classes/array.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/locks.h"
#include "../common/config/config_file.h"
#include "../common/config/config.h"

namespace Firebird {

class PluginManager : public AutoIface<IPluginManagerImpl<PluginManager, CheckStatusWrapper> >
{
public:
	// IPluginManager implementation
	IPluginSet* getPlugins(CheckStatusWrapper* status, unsigned int interfaceType,
					const char* namesList, IFirebirdConf* firebirdConf);
	void registerPluginFactory(unsigned int interfaceType, const char* defaultName,
					IPluginFactory* factory);
	IConfig* getConfig(CheckStatusWrapper* status, const char* filename);
	void releasePlugin(IPluginBase* plugin);
	void registerModule(IPluginModule* module);
	void unregisterModule(IPluginModule* module);

	PluginManager();

	static void shutdown();
	static void waitForType(unsigned int typeThatMustGoAway);
	static void threadDetach();
};

}	// namespace Firebird

#endif	// YVALVE_PLUGIN_MANAGER_H
