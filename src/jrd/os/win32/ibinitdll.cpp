/*
 *      PROGRAM:        Firebird Windows platforms
 *      MODULE:         ibinitdll.cpp
 *      DESCRIPTION:    DLL entry/exit function
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
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include <windows.h>
#include "../../../common/dllinst.h"


using namespace Firebird;


// Per-thread cleanup used to run from the DLL_THREAD_DETACH case here, but
// that only ever fires when this code is actually linked into a real DLL.
// It is now handled uniformly (DLL or static build) via Fiber-Local Storage -
// see ThreadCleanup in yvalve/utl.cpp. This DllMain is therefore not compiled
// into the static client library (see fbclient_static.vcxproj) - hDllInst,
// bDllProcessExiting and dDllUnloadTID simply keep their safe default values
// (0 / false) in that build, which is the correct behavior for code linked
// directly into the host application (see doc/README.StaticClient.md).
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
			hDllInst = h;
			break;

		case DLL_PROCESS_DETACH:
			bDllProcessExiting = (reserved != NULL);
			dDllUnloadTID = GetCurrentThreadId();
			break;
	}

	return TRUE;
}
