/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			GetPlugins.h
 *	DESCRIPTION:	Tools to help access plugins.
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_COMMON_CLASSES_GET_PLUGINS
#define FB_COMMON_CLASSES_GET_PLUGINS

#include "../common/classes/ImplementHelper.h"
#include "../common/classes/auto.h"
#include "../common/config/config.h"
#include "../common/StatusHolder.h"

namespace Firebird {

// Template to help with loop in the set of plugins
template <typename P>
class GetPlugins
{
public:
	GetPlugins(unsigned int interfaceType, const char* namesList = NULL)
		: masterInterface(), pluginInterface(),
		  pluginSet(NULL), currentPlugin(NULL),
		  ls(*getDefaultMemoryPool()), status(&ls)
	{
		pluginSet.assignRefNoIncr(pluginInterface->getPlugins(&status, interfaceType,
			(namesList ? namesList : Config::getDefaultConfig()->getPlugins(interfaceType)),
			NULL));
		check(&status);

		getPlugin();
	}

	GetPlugins(unsigned int interfaceType,
			   const Config* knownConfig, const char* namesList = NULL)
		: masterInterface(), pluginInterface(),
		  pluginSet(NULL), currentPlugin(NULL),
		  ls(*getDefaultMemoryPool()), status(&ls)
	{
		pluginSet.assignRefNoIncr(pluginInterface->getPlugins(&status, interfaceType,
			(namesList ? namesList : knownConfig->getPlugins(interfaceType)),
			FB_NEW FirebirdConf(knownConfig)));
		check(&status);

		getPlugin();
	}

	bool hasData() const
	{
		return currentPlugin;
	}

	const char* name() const
	{
		return hasData() ? pluginSet->getName() : NULL;
	}

	P* plugin() const
	{
		return currentPlugin;
	}

	P* makeInstance()
	{
		if (!hasData())
			return NULL;

		P* p = (P*) pluginSet->getPlugin(&status);
		check(&status);
		return p;
	}

	void next()
	{
		if (hasData())
		{
			pluginInterface->releasePlugin(currentPlugin);
			currentPlugin = NULL;

			pluginSet->next(&status);
			check(&status);
			getPlugin();
		}
	}

	void set(const char* newName)
	{
		if (hasData())
		{
			pluginInterface->releasePlugin(currentPlugin);
			currentPlugin = NULL;
		}

		pluginSet->set(&status, newName);
		check(&status);
		getPlugin();
	}

	~GetPlugins()
	{
		if (hasData())
		{
			pluginInterface->releasePlugin(currentPlugin);
			currentPlugin = NULL;
		}
	}

private:
	MasterInterfacePtr masterInterface;
	PluginManagerInterfacePtr pluginInterface;
	RefPtr<IPluginSet> pluginSet;
	P* currentPlugin;
	LocalStatus ls;
	CheckStatusWrapper status;

	void getPlugin()
	{
		currentPlugin = (P*) pluginSet->getPlugin(&status);
		check(&status);
	}
};

// template required to use AutoPtr for plugins

template <typename P>
class ReleasePlugin
{
public:
	static void clear(P* ptr)
	{
		if (ptr)
			PluginManagerInterfacePtr()->releasePlugin(ptr);
	}
};

template <typename P>
class AutoPlugin : public AutoPtr<P, ReleasePlugin>
{
public:
	AutoPlugin(P* p = nullptr)
		: AutoPtr<P, ReleasePlugin>(p)
	{ }
};

} // namespace Firebird


#endif // FB_COMMON_CLASSES_GET_PLUGINS
