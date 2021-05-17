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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef COMMON_MAC_UTILS_H
#define COMMON_MAC_UTILS_H

#ifdef DARWIN
extern "C"
{

const char* getTemporaryFolder();
bool isSandboxed();
void osLog(const char*);

} // extern "C"
#else // DARWIN

inline const char* getTemporaryFolder() { return NULL; }
inline bool isSandboxed() { return false; }

#endif // DARWIN

#endif // COMMON_MAC_UTILS_H
