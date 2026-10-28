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
#include "../common/classes/alloc.h"
#include "../common/classes/GetPlugins.h"
#include "../jrd/EngineInterface.h"

using namespace Firebird;
using namespace Jrd;

namespace Burp {


struct FastPathBurpProvider::EngineHolder
{
	AutoPlugin<JProvider> provider{JProvider::getInstance()};
};


FastPathBurpProvider::FastPathBurpProvider(bool useFastPath)
	: provider(dispatcher)
{
	if (useFastPath)
	{
		engineHolder = FB_NEW EngineHolder();
		provider = engineHolder->provider;
	}
}

FastPathBurpProvider::~FastPathBurpProvider() = default;

IAttachment* FastPathBurpProvider::attachWorker(CheckStatusWrapper* status, const char* dbName,
	unsigned dpbLen, const unsigned char* dpb, ICryptKeyCallback* cryptCallback)
{
	AutoPlugin<JProvider> engineProvider(JProvider::getInstance());

	if (cryptCallback)
		engineProvider->setDbCryptCallback(status, cryptCallback);

	if (status->getState() & IStatus::STATE_ERRORS)
		return nullptr;

	return engineProvider->attachDatabase(status, dbName, dpbLen, dpb);
}


} // namespace Burp
