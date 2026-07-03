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
#include "../../../yvalve/utl_proto.h"

using namespace Firebird;


BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
			hDllInst = h;
			break;

		case DLL_THREAD_DETACH:
		{
			Why::threadCleanup();
			break;
		}

		case DLL_PROCESS_DETACH:
			bDllProcessExiting = (reserved != NULL);
			dDllUnloadTID = GetCurrentThreadId();
			break;
	}

	return TRUE;
}
