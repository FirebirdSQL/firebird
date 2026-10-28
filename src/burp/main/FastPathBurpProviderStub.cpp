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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../burp/FastPathBurpProvider.h"
#include "../common/utils_proto.h"

using namespace Firebird;

namespace Burp {


// Complete type for Firebird::AutoPtr<EngineHolder>. The real definition lives in
// burp/engine/FastPathBurpUtil.cpp, which is not linked into standalone gbak.
struct FastPathBurpProvider::EngineHolder
{
};


FastPathBurpProvider::FastPathBurpProvider(bool useFastPath)
	: provider(dispatcher)
{
	if (useFastPath)
	{
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH mode requires service manager")).raise();
	}
}

FastPathBurpProvider::~FastPathBurpProvider() = default;

IAttachment* FastPathBurpProvider::attachWorker(CheckStatusWrapper* status, const char* dbName,
	unsigned dpbLen, const unsigned char* dpb, ICryptKeyCallback*)
{
	(Arg::Gds(isc_random) <<
		Arg::Str("FAST_PATH mode requires service manager")).raise();
}


} // namespace Burp
