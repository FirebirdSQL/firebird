/*
 *	PROGRAM:		Firebird authentication
 *	MODULE:			Auth.h
 *	DESCRIPTION:	Implementation of interfaces, passed to plugins
 *					Plugins loader
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

#ifndef FB_AUTHDBG_H
#define FB_AUTHDBG_H

#define AUTH_DEBUG

#ifdef AUTH_DEBUG

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace Firebird::Auth
{


// The idea of debug plugin is to send some data from server to client,
// modify them on client and return result (which becomes login name) to the server

class DebugServer final :
	public StdPlugin<IServerImpl<DebugServer, CheckStatusWrapper>>
{
public:
	explicit DebugServer(IPluginConfig*);

    int authenticate(CheckStatusWrapper* status, IServerBlock* sBlock,
    				 IWriter* writerInterface);
	void setDbCryptCallback(CheckStatusWrapper*, ICryptKeyCallback*);

private:
	string str;
	RefPtr<IConfig> config;
};

class DebugClient final :
	public StdPlugin<IClientImpl<DebugClient, CheckStatusWrapper>>
{
public:
	DebugClient(IPluginConfig*);

    int authenticate(CheckStatusWrapper* status, IClientBlock* sBlock);

private:
	string str;
};


} // namespace Firebird::Auth

#endif // AUTH_DEBUG

#endif // FB_AUTHDBG_H
