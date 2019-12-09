/*
 *	PROGRAM:	Operate lists of plugins
 *	MODULE:		ParsedList.cpp
 *	DESCRIPTION:	Parse, merge, etc. lists of plugins in firebird.conf format
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
 *  Copyright (c) 2010, 2019 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/ParsedList.h"
#include "../common/db_alias.h"


using namespace Firebird;

ParsedList::ParsedList(const PathName& list)
{
	parse(list, " \t,;");
}

void ParsedList::parse(PathName list, const char* sep)
{
	list.alltrim(" \t");

	for (;;)
	{
		PathName::size_type p = list.find_first_of(sep);
		if (p == PathName::npos)
		{
			if (list.hasData())
			{
				this->push(list);
			}
			break;
		}

		this->push(list.substr(0, p));
		list = list.substr(p + 1);
		list.ltrim(sep);
	}
}

ParsedList::ParsedList(const PathName& list, const char* delimiters)
{
	parse(list, delimiters);
}

void ParsedList::makeList(PathName& list) const
{
	fb_assert(this->hasData());
	list = (*this)[0];
	for (unsigned i = 1; i < this->getCount(); ++i)
	{
		list += ' ';
		list += (*this)[i];
	}
}

void ParsedList::mergeLists(PathName& list, const PathName& serverList, const PathName& clientList)
{
	ParsedList onClient(clientList), onServer(serverList), merged;

	// do not expect too long lists, therefore use double loop
	for (unsigned c = 0; c < onClient.getCount(); ++c)
	{
		for (unsigned s = 0; s < onServer.getCount(); ++s)
		{
			if (onClient[c] == onServer[s])
			{
				merged.push(onClient[c]);
				break;
			}
		}
	}

	merged.makeList(list);
}

PathName ParsedList::getNonLoopbackProviders(const PathName& aliasDb)
{
	PathName dummy;
	RefPtr<const Config> config;
	expandDatabaseName(aliasDb, dummy, &config);

	PathName providers(config->getPlugins(IPluginManager::TYPE_PROVIDER));
	ParsedList list(providers);
	for (unsigned n = 0; n < list.getCount(); ++n)
	{
		if (list[n] == "Loopback")
		{
			list.remove(n);
			break;
		}
	}
	list.makeList(providers);
	providers.insert(0, "Providers=");

	return providers;
}
