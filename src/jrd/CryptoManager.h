/*
 *	PROGRAM:		JRD access method
 *	MODULE:			CryptoManager.h
 *	DESCRIPTION:	Database encryption
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
 *  Copyright (c) 2012 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef JRD_CRYPTO_MANAGER
#define JRD_CRYPTO_MANAGER

#include "../common/classes/alloc.h"
#include "../common/classes/fb_atomic.h"
#include "../common/classes/SyncObject.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/condition.h"
#include "../jrd/MetaName.h"
#include "../common/classes/GetPlugins.h"
#include "../common/ThreadStart.h"
#include "../jrd/ods.h"
#include "../jrd/status.h"
#include "firebird/Interface.h"

// forward

namespace Ods {

struct pag;

}

namespace Firebird {

class ClumpletReader;

}

namespace Jrd {

class Database;
class Attachment;
class jrd_file;
class BufferDesc;
class thread_db;
class Lock;
class PageSpace;


//
// This very specific locking class makes it possible to perform traditional read/write locks,
// but in addition it can on special request perform some predefined action or (in a case when
// >=1 read lock is taken) set barrier for new locks (new locks will not be granted) and
// at the moment when last existing lock is released execute that predefined action in context
// of a thread, releasing last lock.
//
// In our case special request is done from AST handler - and therefore called ast.
// Read locks are done when performing IO+crypt activity - and called ioBegin/ioEnd.
// Write locks are done when some exclusive activity like changing crypt state is
// needed - they are full locks and called lockBegin/lockEnd.
//

class BarSync
{
public:
	class IBar
	{
	public:
		virtual void doOnTakenWriteSync(Jrd::thread_db* tdbb) = 0;
		virtual void doOnAst(Jrd::thread_db* tdbb) = 0;
	};

	BarSync(IBar* i)
		: callback(i), counter(0), lockMode(0), flagWriteLock(false)
	{ }

	class IoGuard
	{
	public:
		IoGuard(Jrd::thread_db* p_tdbb, BarSync& p_bs)
			: tdbb(p_tdbb), bs(p_bs)
		{
			bs.ioBegin(tdbb);
		}

		~IoGuard()
		{
			bs.ioEnd(tdbb);
		}

	private:
		Jrd::thread_db* tdbb;
		BarSync& bs;
	};

	class LockGuard
	{
	public:
		LockGuard(Jrd::thread_db* p_tdbb, BarSync& p_bs)
			: tdbb(p_tdbb), bs(p_bs), flagLocked(false)
		{ }

		void lock()
		{
			fb_assert(!flagLocked);
			if (!flagLocked)
			{
				bs.lockBegin(tdbb);
				flagLocked = true;
			}
		}

		~LockGuard()
		{
			if (flagLocked)
			{
				bs.lockEnd(tdbb);
			}
		}

	private:
		Jrd::thread_db* tdbb;
		BarSync& bs;
		bool flagLocked;
	};

	void ioBegin(Jrd::thread_db* tdbb)
	{
		Firebird::MutexLockGuard g(mutex, FB_FUNCTION);

		if (counter < 0)
		{
			if (!(flagWriteLock && (thread == getThreadId())))
			{
				if ((counter % BIG_VALUE == 0) && (!flagWriteLock))
				{
					if (lockMode)
					{
						// Someone is waiting for write lock
						lockCond.notifyOne();
						barCond.wait(mutex);
					}
					else
					{
						// Ast done
						callWriteLockHandler(tdbb);
						counter = 0;
					}
				}
				else
					barCond.wait(mutex);
			}
		}
		++counter;
	}

	void ioEnd(Jrd::thread_db* tdbb)
	{
		Firebird::MutexLockGuard g(mutex, FB_FUNCTION);

		if (--counter < 0 && counter % BIG_VALUE == 0)
		{
			if (!(flagWriteLock && (thread == getThreadId())))
			{
				if (lockMode)
					lockCond.notifyOne();
				else
				{
					callWriteLockHandler(tdbb);
					finishWriteLock();
				}
			}
		}
	}

	void ast(Jrd::thread_db* tdbb)
	{
		Firebird::MutexLockGuard g(mutex, FB_FUNCTION);
		if (counter >= 0)
		{
			counter -= BIG_VALUE;
		}
		callback->doOnAst(tdbb);
	}

	void lockBegin(Jrd::thread_db* tdbb)
	{
		Firebird::MutexLockGuard g(mutex, FB_FUNCTION);

		if ((counter -= BIG_VALUE) != -BIG_VALUE)
		{
			++lockMode;
			try
			{
				lockCond.wait(mutex);
			}
			catch (const Firebird::Exception&)
			{
				--lockMode;
				throw;
			}
			--lockMode;
		}

		thread = getThreadId();
		flagWriteLock = true;
	}

	void lockEnd(Jrd::thread_db* tdbb)
	{
		Firebird::MutexLockGuard g(mutex, FB_FUNCTION);

		flagWriteLock = false;
		finishWriteLock();
	}

private:
	void callWriteLockHandler(Jrd::thread_db* tdbb)
	{
		thread = getThreadId();
		flagWriteLock = true;
		callback->doOnTakenWriteSync(tdbb);
		flagWriteLock = false;
	}

	void finishWriteLock()
	{
		if ((counter += BIG_VALUE) == 0)
			barCond.notifyAll();
		else
			lockCond.notifyOne();
	}

	Firebird::Condition barCond, lockCond;
	Firebird::Mutex mutex;
	IBar* callback;
	ThreadId thread;
	int counter;
	int lockMode;
	bool flagWriteLock;

	static const int BIG_VALUE = 1000000;
};

class CryptoManager final : public Firebird::PermanentStorage, public BarSync::IBar
{
public:
	typedef Firebird::GetPlugins<Firebird::IDbCryptPlugin> Factory;
	typedef Firebird::HalfStaticArray<Attachment*, 16> AttVector;

	explicit CryptoManager(thread_db* tdbb);
	~CryptoManager();

	void shutdown(thread_db* tdbb);

	void prepareChangeCryptState(thread_db* tdbb, const MetaName& plugName,
		const MetaName& key);
	void changeCryptState(thread_db* tdbb, const Firebird::string& plugName);
	void attach(thread_db* tdbb, Attachment* att);
	void detach(thread_db* tdbb, Attachment* att);

	void startCryptThread(thread_db* tdbb);
	void terminateCryptThread(thread_db* tdbb, bool wait);
	void stopThreadUsing(thread_db* tdbb, Attachment* att);

	class IOCallback
	{
	public:
		virtual bool callback(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page) = 0;
	};

	bool read(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io);
	bool write(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io);

	void cryptThread();

	bool checkValidation(Firebird::IDbCryptPlugin* crypt);
	void setDbInfo(Firebird::IDbCryptPlugin* cp);

	ULONG getCurrentPage(thread_db* tdbb) const;
	UCHAR getCurrentState(thread_db* tdbb) const;
	const char* getKeyName() const;
	const char* getPluginName() const;

private:
	enum IoResult {SUCCESS_ALL, FAILED_CRYPT, FAILED_IO};
	IoResult internalRead(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io);
	IoResult internalWrite(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io);

	class Buffer
	{
	public:
		operator Ods::pag*()
		{
			return reinterpret_cast<Ods::pag*>(FB_ALIGN(buf, PAGE_ALIGNMENT));
		}

		Ods::pag* operator->()
		{
			return reinterpret_cast<Ods::pag*>(FB_ALIGN(buf, PAGE_ALIGNMENT));
		}

	private:
		char buf[MAX_PAGE_SIZE + PAGE_ALIGNMENT - 1];
	};

	class DbInfo;
	friend class DbInfo;

	class DbInfo final : public Firebird::RefCntIface<Firebird::IDbCryptInfoImpl<DbInfo, Firebird::CheckStatusWrapper> >
	{
	public:
		DbInfo(CryptoManager* cm)
			: cryptoManager(cm)
		{ }

		void destroy()
		{
			cryptoManager = NULL;
		}

		// IDbCryptInfo implementation
		const char* getDatabaseFullPath(Firebird::CheckStatusWrapper* status);

	private:
		CryptoManager* cryptoManager;
	};

	static int blockingAstChangeCryptState(void*);
	void blockingAstChangeCryptState();

	// IBar's pure virtual functions are implemented here
	void doOnTakenWriteSync(thread_db* tdbb);
	void doOnAst(thread_db* tdbb);

	void loadPlugin(thread_db* tdbb, const char* pluginName);
	bool validateAttachment(thread_db* tdbb, Attachment* att, bool consume);
	ULONG getLastPage(thread_db* tdbb);
	void writeDbHeader(thread_db* tdbb, ULONG runpage);
	void calcValidation(Firebird::string& valid, Firebird::IDbCryptPlugin* plugin);
	void checkValidation();
	void shutdownConsumers(thread_db* tdbb);

	void lockAndReadHeader(thread_db* tdbb, unsigned flags = 0);
	static const unsigned CRYPT_HDR_INIT =		0x01;
	static const unsigned CRYPT_HDR_NOWAIT =	0x02;

	void addClumplet(Firebird::string& value, Firebird::ClumpletReader& block, UCHAR tag);
	void calcDigitalSignature(thread_db* tdbb, Firebird::string& signature, const class Header& hdr);
	void digitalySignDatabase(thread_db* tdbb, class CchHdr& hdr);
	void checkDigitalSignature(thread_db* tdbb, const class Header& hdr);

	BarSync sync;
	MetaName keyName, pluginName;
	ULONG currentPage;
	Firebird::Mutex pluginLoadMtx, cryptThreadMtx, holdersMutex;
	AttVector keyProviders, keyConsumers;
	Firebird::string hash;
	Firebird::RefPtr<DbInfo> dbInfo;
	Thread::Handle cryptThreadId;
	Firebird::IDbCryptPlugin* cryptPlugin;
	Factory* checkFactory;
	Database& dbb;
	Lock* stateLock;
	Lock* threadLock;
	Attachment* cryptAtt;

	// This counter works only in a case when database encryption is changed.
	// Traditional processing of AST can not be used for crypto manager.
	// The problem is with taking state lock after AST.
	// That should be done before next IO operation to guarantee database
	// consistency, but IO operation may be requested from another AST
	// (database cache blocking) when 'wait for a lock' operations are
	// prohibited. I.e. we can't proceed with normal IO without a lock
	// but at the same time can't take it.

	// The solution is to use crypt versions counter in a lock (incremented
	// by one each time one issues ALTER DATABASE ENCRYPT/DECRYPT), read
	// it when lock can't be taken, store in slowIO variable, perform IO
	// and compare stored value with current lock data. In case when values
	// differ encryption status of database was changed during IO operation
	// and such operation should be repeated. When data in lock does not
	// change during IO that means that crypt rules remained the same even
	// without state lock taken by us and therefore result of IO operation
	// is correct. As soon as non-waiting attempt to take state lock succeeds
	// slowIO mode is off (slowIO counter becomes zero) and we return to
	// normal operation.

	SINT64 slowIO;
	bool crypt, process, flDown, run;

	bool down() const;
};

} // namespace Jrd


#endif // JRD_CRYPTO_MANAGER
