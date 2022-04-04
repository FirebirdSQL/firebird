/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		SecDbCache.h
 *	DESCRIPTION:	Cached security database connection
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
 *
 * 2003.02.02 Dmitry Yemanov: Implemented cached security database connection
 * 2011 - 2020	Alexander Peshkov
 */

#ifndef FB_SECDBCACHE_H
#define FB_SECDBCACHE_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/alloc.h"
#include "../common/classes/auto.h"


namespace Auth {

class VSecDb
{
public:
	VSecDb()
	{
	}

	virtual ~VSecDb()
	{
	}

	virtual bool lookup(void* inMsg, void* outMsg) = 0;
	virtual bool test() = 0;
};


class PluginDatabases;

class CachedSecurityDatabase final
	: public Firebird::RefCntIface<Firebird::ITimerImpl<CachedSecurityDatabase, Firebird::CheckStatusWrapper> >
{
public:
	char secureDbName[MAXPATHLEN + 1];

	CachedSecurityDatabase(PluginDatabases* l, const Firebird::PathName& nm)
		: secDb(nullptr), list(l)
	{
		nm.copyTo(secureDbName, sizeof secureDbName);
	}

	// ITimer implementation
	void handler();
	void close();

	Firebird::Mutex mutex;
	Firebird::AutoPtr<VSecDb> secDb;
	PluginDatabases* list;

public:
	// Related RAII holder
	class Instance : public Firebird::RefPtr<CachedSecurityDatabase>
	{
	public:
		Instance()
		{ }

		void set(CachedSecurityDatabase* db)
		{
			fb_assert(!hasData());
			fb_assert(db);

			assign(db);
			(*this)->mutex.enter(FB_FUNCTION);
		}

		void reset()
		{
			if (hasData())
			{
				(*this)->mutex.leave();
				(*this)->close();
				assign(nullptr);
			}
		}

		~Instance()
		{
			if (hasData())
			{
				(*this)->mutex.leave();
				(*this)->close();
			}
		}
	};
};

class PluginDatabases
{
public:
	PluginDatabases(MemoryPool& p)
		: dbArray(p)
	{ }

private:
	Firebird::HalfStaticArray<CachedSecurityDatabase*, 4> dbArray;
	Firebird::Mutex arrayMutex;

public:
	void getInstance(Firebird::IPluginConfig* pluginConfig, CachedSecurityDatabase::Instance& instance);
	int shutdown();
	void handler(CachedSecurityDatabase* tgt);
};

} // namespace Auth

#endif // FB_SECDBCACHE_H
