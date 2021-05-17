/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			MasterImplementation.h
 *	DESCRIPTION:	Main firebird interface, used to get other interfaces.
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
 *  Copyright (c) 2011 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef YVALVE_MASTER_IMPLEMENTATION_H
#define YVALVE_MASTER_IMPLEMENTATION_H

#include "firebird.h"
#include "firebird/Interface.h"
#include "../yvalve/YObjects.h"
#include "../yvalve/DistributedTransaction.h"
#include "../common/classes/ImplementHelper.h"

namespace Firebird
{
	class Mutex;
}

namespace Why
{
	class MasterImplementation :
		public Firebird::AutoIface<Firebird::IMasterImpl<MasterImplementation, Firebird::CheckStatusWrapper> >
	{
	public:
		static Firebird::Static<Dtc> dtc;

	public:
		// IMaster implementation
		Firebird::IStatus* getStatus();
		Firebird::IProvider* getDispatcher();
		Firebird::IPluginManager* getPluginManager();
		Firebird::ITimerControl* getTimerControl();
		Firebird::IAttachment* registerAttachment(Firebird::IProvider* provider,
			Firebird::IAttachment* attachment);
		Firebird::ITransaction* registerTransaction(Firebird::IAttachment* attachment,
			Firebird::ITransaction* transaction);
		Dtc* getDtc();
		Firebird::IMetadataBuilder* getMetadataBuilder(Firebird::CheckStatusWrapper* status, unsigned fieldCount);
		int serverMode(int mode);
		Firebird::IUtil* getUtilInterface();
		Firebird::IConfigManager* getConfigManager();
		FB_BOOLEAN getProcessExiting();
	};

	// Should be called if process must be terminated wihout any regular shutdown routines
	void abortShutdown();

	void shutdownTimers();

	Firebird::Mutex& pauseTimer();
} // namespace Why

#endif // YVALVE_MASTER_IMPLEMENTATION_H
