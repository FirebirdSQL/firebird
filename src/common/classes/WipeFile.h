/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		WipeFile.h
 *	DESCRIPTION:	Wipe and delete file
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
 *  The Original Code was created by Dmitry Starodubov
 *   <dmitry.starodubov (at) red-soft.biz> for Red Soft Corporation.
 *
 *  Copyright (c) 2010 Red Soft Corporation <info (at) red-soft.biz>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 */

#ifndef CLASSES_WIPE_FILE_H
#define CLASSES_WIPE_FILE_H

#include "firebird.h"

#ifdef WIN_NT
#include <windows.h>
#else
#define HANDLE int
#endif

int WipeFile(HANDLE handle);

#endif // CLASSES_WIPE_FILE_H
