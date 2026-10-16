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

#ifndef BURP_FAST_PATH_BURP_PROVIDER_H
#define BURP_FAST_PATH_BURP_PROVIDER_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/auto.h"

namespace Burp {

class FastPathBurpProvider
{
private:
	struct EngineHolder;

public:
	explicit FastPathBurpProvider(bool useFastPath);
	~FastPathBurpProvider();

	FastPathBurpProvider(const FastPathBurpProvider&) = delete;
	FastPathBurpProvider& operator=(const FastPathBurpProvider&) = delete;

public:
	Firebird::IProvider* get() noexcept
	{
		return provider;
	}

	static Firebird::IAttachment* attachWorker(Firebird::CheckStatusWrapper* status,
		const char* dbName, unsigned dpbLen, const unsigned char* dpb,
		Firebird::ICryptKeyCallback* cryptCallback);

private:
	Firebird::DispatcherPtr dispatcher;
	Firebird::AutoPtr<EngineHolder> engineHolder;
	Firebird::IProvider* provider;
};

} // namespace Burp

#endif // BURP_FAST_PATH_BURP_PROVIDER_H
