/*
 *  PROGRAM:        Firebird authentication.
 *  MODULE:         ChaCha.h
 *  DESCRIPTION:    ChaCha wire crypt plugin.
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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CRYPT_CHACHA_H
#define CRYPT_CHACHA_H

#include "firebird/Interface.h"

namespace Crypt
{
	void registerChaCha(Firebird::IPluginManager* iPlugin);
} // namespace Crypt

#endif // CRYPT_CHACHA_H
