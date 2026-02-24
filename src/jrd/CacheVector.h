/*
 *	PROGRAM:	Engine Code
 *	MODULE:		CacheVector.h
 *	DESCRIPTION:	Vector used in shared metadata cache.
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2021 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef JRD_CACHEVECTOR_H
#define JRD_CACHEVECTOR_H

#include <condition_variable>
#include <stdio.h>

#include "../common/ThreadStart.h"
#include "../common/StatusArg.h"

#include "../jrd/SharedReadVector.h"
#include "../jrd/constants.h"
#include "../jrd/tra_proto.h"
#include "../jrd/QualifiedName.h"

namespace Jrd {

class thread_db;
class Lock;
class MetadataCache;
enum lck_t : UCHAR;

class ObjectBase
{
public:
	typedef unsigned Flag;
};


class ConsistencyCheck
{
public:
	static bool commitNumber(thread_db* tdbb);
};


class ElementBase
{
public:
	ElementBase(thread_db* tdbb, MemoryPool& p, lck_t locktype, SINT64 key);
	ElementBase()
	{ }

private:
	virtual void reset(thread_db* tdbb, bool erase) = 0;
	static int blockingAst(void* ast_object);

public:
	static constexpr FB_UINT64 NO_METALOCK = ~0u;

public:
	void pingLock(thread_db* tdbb, ObjectBase::Flag flags, MetaId id, const char* family);
	void setLock(thread_db* tdbb, MetaId id, const char* family);
	void releaseLock(thread_db* tdbb);
	virtual void releaseLocks(thread_db* tdbb) = 0;

public:
	typedef SLONG ReturnedId;	// enable '-1' as not found

	virtual ~ElementBase();
	virtual void cleanup(thread_db* tdbb) = 0;

public:
	[[noreturn]] void busyError(thread_db* tdbb, MetaId id, const char* family);
	[[noreturn]] void newVersionBusy(thread_db* tdbb, MetaId id, const char* family, TraNumber traNum);
	[[noreturn]] void newVersionScan(thread_db* tdbb, MetaId id, const char* family);
	void commitErase(thread_db* tdbb);

	bool hasLock() const
	{
		return locked;
	}

private:
	Lock* lock = nullptr;
	std::atomic<bool> locked = false;
};


namespace CacheFlag
{

	static constexpr ObjectBase::Flag COMMITTED =	0x001;		// version already committed
	static constexpr ObjectBase::Flag ERASED =		0x002;		// object erased / return erased objects
	static constexpr ObjectBase::Flag NOSCAN =		0x004;		// do not call Versioned::scan()
	static constexpr ObjectBase::Flag AUTOCREATE =	0x008;		// create initial version automatically
	static constexpr ObjectBase::Flag NOCOMMIT =	0x010;		// do not commit created version
	static constexpr ObjectBase::Flag NOERASED =	0x020;		// never return erased version, skip till older object
	static constexpr ObjectBase::Flag RETIRED = 	0x040;		// object is in a process of GC
	static constexpr ObjectBase::Flag UPGRADE =		0x080;		// create new versions for already existing in a cache objects
	static constexpr ObjectBase::Flag MINISCAN =	0x100;		// perform minimum scan and set cache entry to reload state
	static constexpr ObjectBase::Flag DB_VERSION =	0x200;		// execute version upgrade in database

	// Useful combinations
	static constexpr ObjectBase::Flag TAG_FOR_UPDATE = NOCOMMIT | MINISCAN | DB_VERSION;
	static constexpr ObjectBase::Flag OLD_DROP = MINISCAN | AUTOCREATE;
	static constexpr ObjectBase::Flag OLD_ALTER = MINISCAN | AUTOCREATE;
}


// Controls growth of from & back values in metadata cache

class VersionIncr
{
public:
	VersionIncr(thread_db* tdbb);
	~VersionIncr();
	MdcVersion getVersion();

private:
	MetadataCache* mdc;
	MdcVersion current;
};


class TransactionNumber
{
public:
	static TraNumber current(thread_db* tdbb);
	static TraNumber oldestActive(thread_db* tdbb);
	static TraNumber next(thread_db* tdbb);
	static bool isNotActive(thread_db* tdbb, TraNumber traNumber);

	// Not related to number - but definitely related to transaction
	static ULONG* getFlags(thread_db* tdbb);
};


enum class ScanResult { COMPLETE, MISS, SKIP, REPEAT };


template <class Versioned>
class ListEntry : public HazardObject
{
public:
	enum State { INITIAL, RELOAD, MISSING, SCANNING, READY };

	ListEntry(Versioned* object, TraNumber traNumber, ObjectBase::Flag fl)
		: object(object), traNumber(traNumber), cacheFlags(fl), state(INITIAL)
	{
		if (fl & CacheFlag::ERASED)
			fb_assert(!object);
	}

	~ListEntry()
	{
		fb_assert(!object);
	}

	void cleanup(thread_db* tdbb, bool withObject = true)
	{
		if (object)		// be careful with ERASED entries
		{
			if (withObject)
				Versioned::destroy(tdbb, object);
			object = nullptr;
		}

		ListEntry* ptr = next.load(atomics::memory_order_relaxed);
		if (ptr)
		{
			ptr->cleanup(tdbb, withObject);
			delete ptr;
			next.store(nullptr, atomics::memory_order_relaxed);
		}
	}

	// find appropriate object in cache
	template <typename Permanent>
	static Versioned* getVersioned(thread_db* tdbb, HazardPtr<ListEntry>& listEntry, TraNumber currentTrans,
		ObjectBase::Flag fl, Permanent* permanent)
	{
		auto entry = getEntry(tdbb, listEntry, currentTrans, fl, permanent);
		return entry ? entry->getVersioned() : nullptr;
	}

	// find appropriate entry in cache
	template <typename Permanent>
	static HazardPtr<ListEntry> getEntry(thread_db* tdbb, HazardPtr<ListEntry>& listEntry, TraNumber currentTrans,
		ObjectBase::Flag fl, Permanent* permanent)
	{
		for (; listEntry; listEntry.set(listEntry->next))
		{
			ObjectBase::Flag f(listEntry->getFlags());

			if ((f & CacheFlag::COMMITTED) ||
					// committed (i.e. confirmed) objects are freely available
				(listEntry->traNumber == currentTrans))
					// transaction that created an object can always access it
			{
				if (f & CacheFlag::ERASED)
				{
					// object does not exist
					fb_assert(!listEntry->object);

					if (fl & CacheFlag::NOERASED)
						continue;	// to the next entry

					if (fl & CacheFlag::ERASED)
						return listEntry;	// return empty erased entry

					return HazardPtr<ListEntry>(nullptr);		// object dropped
				}

				// required entry found in the list
				switch (listEntry->scan(tdbb, fl, permanent))
				{
				case ScanResult::COMPLETE:
				case ScanResult::REPEAT:		// scan semi-complete but reload was requested
				case ScanResult::SKIP:			// scan skipped due to NOSCAN flag
					break;

				case ScanResult::MISS:			// no object
				default:
					return HazardPtr<ListEntry>(nullptr);
				}

				return listEntry;
			}
		}

		return HazardPtr<ListEntry>(nullptr);	// object created (not by us) and not committed yet
	}

	bool isBusy(TraNumber currentTrans, TraNumber* number = nullptr) const noexcept
	{
		bool busy = !((getFlags() & CacheFlag::COMMITTED) || (traNumber == currentTrans));
		if (busy && number)
			*number = traNumber;
		return busy;
	}

	ObjectBase::Flag getFlags() const noexcept
	{
		return cacheFlags.load(atomics::memory_order_relaxed);
	}

	Versioned* getVersioned()
	{
		return object;
	}

	// add new entry to the list
	static bool add(thread_db* tdbb, std::atomic<ListEntry*>& list, ListEntry* newVal)
	{
		HazardPtr<ListEntry> oldVal(list);

		do
		{
			while(oldVal && oldVal->isBusy(newVal->traNumber))
			{
				// modified in transaction oldVal->traNumber
				if (TransactionNumber::isNotActive(tdbb, oldVal->traNumber))
				{
					rollback(tdbb, list, oldVal->traNumber);
					oldVal = list;
				}
				else
					return false;
			}

			newVal->next.store(oldVal.getPointer());
		} while (!oldVal.replace(list, newVal));

		return true;
	}

	// insert newVal in the beginning of a list provided there is still oldVal at the top of the list
	static bool insert(std::atomic<ListEntry*>& list, ListEntry* newVal, ListEntry* oldVal) noexcept
	{
		if (oldVal && oldVal->isBusy(newVal->traNumber))	// modified in other transaction
			return false;

		newVal->next.store(oldVal, atomics::memory_order_release);
		return list.compare_exchange_strong(oldVal, newVal, std::memory_order_release, std::memory_order_acquire);
	}

	// remove too old objects - they are anyway can't be in use
	static TraNumber gc(thread_db* tdbb, std::atomic<ListEntry*>* list, const TraNumber oldest)
	{
		TraNumber rc = 0;
		for (HazardPtr<ListEntry> entry(*list); entry; list = &entry->next, entry.set(*list))
		{
			if (!(entry->getFlags() & CacheFlag::COMMITTED))
				continue;

			if (rc && entry->traNumber < oldest)
			{
				if (entry->cacheFlags.fetch_or(CacheFlag::RETIRED) & CacheFlag::RETIRED)
					break;	// someone else also performs GC

				// split remaining list off
				if (entry.replace(*list, nullptr))
				{
					while (entry)// && !(entry->cacheFlags.fetch_or(CacheFlag::RETIRED) & CacheFlag::RETIRED))
					{
						if (entry->object)
						{
							Versioned::destroy(tdbb, entry->object);
							entry->object = nullptr;
						}
						entry->retire();
						entry.set(entry->next);
						if (entry && (entry->cacheFlags.fetch_or(CacheFlag::RETIRED) & CacheFlag::RETIRED))
							break;
					}
				}
				break;
			}

			// store traNumber of last not removed list element
			rc = entry->traNumber;
		}

		return rc;		// 0 is returned in a case when list was empty
	}

	// created/modified/erased earlier object is OK and should become visible to the world
	// return object's flags for further analysis
	ObjectBase::Flag commit(thread_db* tdbb, TraNumber currentTrans, TraNumber nextTrans)
	{
		auto flags = cacheFlags.load(atomics::memory_order_acquire);
		for(;;)
		{
#ifdef DEV_BUILD
			if (ConsistencyCheck::commitNumber(tdbb))
			{
				if (flags & CacheFlag::COMMITTED)
					fb_assert(traNumber <= nextTrans);
				else
					fb_assert(traNumber == currentTrans);
			}
#endif
			if (flags & CacheFlag::COMMITTED)
				return flags;

			// RETIRED is set here to avoid illegal gc until traNumber correction
			auto newFlags = flags | CacheFlag::COMMITTED | CacheFlag::RETIRED;

			// NOCOMMIT cleared to avoid extra ASTs
			newFlags &= ~CacheFlag::NOCOMMIT;

			// Handle front & back of MDC
			VersionIncr incr(tdbb);

			// And finally make object version world-visible
			if (cacheFlags.compare_exchange_weak(flags, newFlags,
				atomics::memory_order_release, atomics::memory_order_acquire))
			{
				traNumber = nextTrans;
				version = incr.getVersion();
				cacheFlags &= ~CacheFlag::RETIRED;		// Enable GC

				break;
			}
		}


		return flags;
	}

	// created earlier object is bad and should be destroyed
	static void rollback(thread_db* tdbb, std::atomic<ListEntry*>& list, const TraNumber currentTran)
	{
		HazardPtr<ListEntry> entry(list);
		while (entry)
		{
			if (entry->getFlags() & CacheFlag::COMMITTED)
				break;

			// I keep this assertion for a while cause it's efficient finding bugs in a case of no races.
			// In general case it's wrong assertion.
			fb_assert(entry->traNumber == currentTran);
			if (entry->traNumber != currentTran)
				break;

			if (entry.replace(list, entry->next))
			{
				entry->next = nullptr;
				auto* obj = entry->object;
				if (obj)
					Versioned::destroy(tdbb, obj);
				entry->object = nullptr;
				entry->retire();

				entry = list;
			}
		}
	}

	void assertCommitted()
	{
		fb_assert(getFlags() & CacheFlag::COMMITTED);
	}

private:
	static ScanResult scan(thread_db* tdbb, Versioned* obj, ObjectBase::Flag& fl, bool rld)
	{
		fb_assert(!(fl & CacheFlag::NOSCAN));
		fb_assert(obj);

		return (!rld) ? obj->scan(tdbb, fl) :
			fl & CacheFlag::MINISCAN ? ScanResult::REPEAT : obj->reload(tdbb, fl);
	}

public:
	template <typename Permanent>
	ScanResult scan(thread_db* tdbb, ObjectBase::Flag fl, Permanent* permanent)
	{
		// no need opening barrier twice
		// explicitly done first cause will be done in 99.99%
		if (state == READY)
			return ScanResult::COMPLETE;

		if (fl & CacheFlag::NOSCAN)
			return ScanResult::SKIP;

		// enable recursive no-action pass by scanning thread
		// if thd is current thread state is not going to be changed - current thread holds mutex
		if ((thd == Thread::getCurrentThreadId()) && (state == SCANNING))
			return ScanResult::COMPLETE;

		permanent->setLock(tdbb, permanent->getId(), Versioned::objectFamily(permanent));

		std::unique_lock<std::mutex> g(mtx);

		for(;;)
		{
			bool reason = true;
			auto savedState = state;

			switch (state)
			{
			case INITIAL:
				if (cacheFlags.load(atomics::memory_order_relaxed) & CacheFlag::ERASED)
				{
					state = READY;
					return ScanResult::COMPLETE;
				}

				if (!object)
					object = Versioned::create(tdbb, permanent->getPool(), permanent);
				reason = false;
				// fall through...

			case RELOAD:
				thd = Thread::getCurrentThreadId();		// Our thread becomes scanning thread
				state = SCANNING;

				try
				{
					auto dbv = cacheFlags.load(atomics::memory_order_acquire) & CacheFlag::DB_VERSION;
					fl |= dbv;
					auto result = scan(tdbb, object, fl, reason);
					if (dbv && !(fl & CacheFlag::DB_VERSION))
						cacheFlags.fetch_and(~CacheFlag::DB_VERSION, atomics::memory_order_release);

					switch (result)
					{
					case ScanResult::COMPLETE:
						state = READY;
						break;

					case ScanResult::SKIP:
						if (savedState == MISSING)
							result = ScanResult::MISS;
						state = savedState;
						break;

					case ScanResult::REPEAT:	// scan complete but reload was requested
						state = RELOAD;
						break;

					case ScanResult::MISS:		// Hey, we scan existing object? What a hell...
						state = savedState;
						break;

					default:
						fb_assert(false);
						state = savedState;
						break;
					}

					thd = 0;
					cond.notify_all();			// other threads may proceed successfully
					return result;

				}
				catch(...)		// scan failed - give up
				{
					state = savedState;
					thd = 0;
					cond.notify_all();		// avoid deadlock in other threads

					throw;
				}

			case SCANNING:		// other thread is already scanning object
				cond.wait(g, [this]{ return state != SCANNING; });
				continue;		// repeat check of FLG value

			case READY:
				return ScanResult::COMPLETE;
			}
		}
	}

	bool isReady()
	{
		return (state == READY) || ((thd == Thread::getCurrentThreadId()) && (state == SCANNING));
	}

	bool isAvailable()
	{
		return isReady() || !(getFlags() & CacheFlag::NOCOMMIT);
	}

	bool scanInProgress() const
	{
		return state == READY ? false : (thd == Thread::getCurrentThreadId()) && (state == SCANNING);
	}

private:

	// object (nill/not nill) & ERASED bit in cacheFlags together control state of cache element
	//				|				 ERASED
	//----------------------------------|-----------------------------
	//		object	|		true		|			false
	//----------------------------------|-----------------------------
	//		nill	|	object dropped	|	cache to be loaded
	//	not nill	|	prohibited		|	cache is actual

	std::condition_variable cond;
	std::mutex mtx;
	Versioned* object;
	std::atomic<ListEntry*> next = nullptr;
	TraNumber traNumber;		// when COMMITTED not set - stores transaction that created this list element
								// when COMMITTED is set - stores transaction after which older elements are not needed
								// traNumber to be changed BEFORE setting COMMITTED

	MdcVersion version = 0;		// version of metadata cache when object was added
	ThreadId thd = 0;			// thread that performs object scan()
	std::atomic<ObjectBase::Flag> cacheFlags;
	State state;				// current entry state
};


enum class StoreResult { DUP, DONE, MISS, SKIP };

template <class V, class P>
class CacheElement : public ElementBase, public P
{
public:
	typedef V Versioned;
	typedef P Permanent;

	typedef std::atomic<CacheElement*> AtomicElementPointer;

	template <typename EXTEND>
	CacheElement(thread_db* tdbb, MemoryPool& p, MetaId id, EXTEND extend) :
		ElementBase(tdbb, p, Versioned::LOCKTYPE, makeId(id, extend)),
		Permanent(tdbb, p, id, extend)
	{ }

	CacheElement(thread_db* tdbb, MemoryPool& p, MetaId id) :
		ElementBase(),
		Permanent(tdbb, p, id, {})
	{ }

	CacheElement(MemoryPool& p) :
		ElementBase(),
		Permanent(p)
	{ }

	template <typename EXTEND>
	FB_UINT64 makeId(MetaId id, EXTEND extend)
	{
		return id;
	}

	static void cleanup(thread_db* tdbb, CacheElement* element)
	{
		auto* ptr = element->list.load(atomics::memory_order_relaxed);
		if (ptr)
		{
			ptr->cleanup(tdbb);
			delete ptr;
		}

		if (element->ptrToClean)
			*element->ptrToClean = nullptr;

		if (!Permanent::destroy(tdbb, element))
		{
			// destroy() returns true if it completed removal of permanent part (delete by pool)
			// if not - delete it ourself here
			delete element;
		}
	}

	void cleanup(thread_db* tdbb) override
	{
		cleanup(tdbb, this);
	}

	void releaseLocks(thread_db* tdbb) override
	{
		ElementBase::releaseLock(tdbb);
		Permanent::releaseLock(tdbb);
	}

	void setCleanup(AtomicElementPointer* clearPtr)
	{
		ptrToClean = clearPtr;
	}

	void reload(thread_db* tdbb, ObjectBase::Flag fl)
	{
		HazardPtr<ListEntry<Versioned>> listEntry(list);
		TraNumber cur = TransactionNumber::current(tdbb);
		if (listEntry)
		{
			fl &= ~CacheFlag::AUTOCREATE;
			Versioned* obj = ListEntry<Versioned>::getVersioned(tdbb, listEntry, cur, fl, this);
			if (obj)
				listEntry->scan(tdbb, fl, this);
		}
	}

	Versioned* getVersioned(thread_db* tdbb, ObjectBase::Flag fl)
	{
		return getVersioned(tdbb, TransactionNumber::current(tdbb), fl);
	}

	bool isReady(thread_db* tdbb)
	{
		auto entry = getEntry(tdbb, TransactionNumber::current(tdbb), CacheFlag::NOSCAN | CacheFlag::NOCOMMIT);
		return entry && entry->isReady();
	}

	enum Availability {
		MISSING,	// no entries in the list
		MODIFIED,	// entry modified by current transaction
		OCCUPIED,	// entry modified by other transaction
		READY		// entry scan completed or in progress
	};

	Availability isAvailable(thread_db* tdbb, TraNumber* number = nullptr)
	{
		HazardPtr<ListEntry<Versioned>> entry(list);

		for(;;)
		{
			if (!entry)
				return MISSING;

			TraNumber temp;
			if (entry->isBusy(TransactionNumber::current(tdbb), &temp))
			{
				// modified in transaction entry->traNumber
				if (TransactionNumber::isNotActive(tdbb, temp))
				{
					ListEntry<Versioned>::rollback(tdbb, list, temp);
					entry = list;
					continue;
				}

				if (number)
					*number = temp;
				return OCCUPIED;
			}

			return entry->isAvailable() ? READY : MODIFIED;
		}
	}

	ObjectBase::Flag getFlags(thread_db* tdbb)
	{
		auto entry = getEntry(tdbb, TransactionNumber::current(tdbb), CacheFlag::NOSCAN | CacheFlag::NOCOMMIT);
		return entry ? entry->getFlags() : 0;
	}

	Versioned* getVersioned(thread_db* tdbb, TraNumber traNum, ObjectBase::Flag fl)
	{
		auto entry = getEntry(tdbb, traNum, fl);
		return entry ? entry->getVersioned() : nullptr;
	}

	HazardPtr<ListEntry<Versioned>> getEntry(thread_db* tdbb, TraNumber traNum, ObjectBase::Flag fl)
	{
		HazardPtr<ListEntry<Versioned>> listEntry(list);
		if (!listEntry)
		{
			if (!(fl & CacheFlag::AUTOCREATE))
				return listEntry;		// nullptr

			fb_assert(tdbb);

			// create almost empty object ...
			Versioned* obj = Versioned::create(tdbb, this->getPool(), this);

			// make new entry to store it in cache
			ListEntry<Versioned>* newEntry = nullptr;
			try
			{
				newEntry = FB_NEW_POOL(*getDefaultMemoryPool())
					ListEntry<Versioned>(obj, traNum, fl & ~CacheFlag::ERASED);
			}
			catch (const Firebird::Exception&)
			{
				if (obj)	// Versioned::create() formally might return nullptr
					Versioned::destroy(tdbb, obj);
				throw;
			}

			if (ListEntry<Versioned>::insert(list, newEntry, nullptr))
			{
				auto sr = newEntry->scan(tdbb, fl, this);
				if (! (fl & CacheFlag::NOCOMMIT))
					newEntry->commit(tdbb, traNum, TransactionNumber::next(tdbb));

				switch (sr)
				{
				case ScanResult::COMPLETE:
				case ScanResult::REPEAT:
				case ScanResult::SKIP:
					break;

				case ScanResult::MISS:
				default:
					return listEntry;	// nullptr
				}

				return HazardPtr<ListEntry<Versioned>>(newEntry);
			}

			newEntry->cleanup(tdbb);
			delete newEntry;
			fb_assert(list.load());
			listEntry = list;
		}
		fl &= ~CacheFlag::AUTOCREATE;
		return ListEntry<Versioned>::getEntry(tdbb, listEntry, traNum, fl, this);
	}

	// return latest committed version or nullptr when does not exist
	Versioned* getLatestObject(thread_db* tdbb) const
	{
		HazardPtr<ListEntry<Versioned>> listEntry(list);
		if (!listEntry)
			return nullptr;

		return ListEntry<Versioned>::getVersioned(tdbb, listEntry, MAX_TRA_NUMBER, 0);
	}

	bool hasEntries() const
	{
		return list || hasLock();
	}

	StoreResult storeObject(thread_db* tdbb, Versioned* obj, ObjectBase::Flag fl)
	{
		TraNumber oldest = TransactionNumber::oldestActive(tdbb);
		TraNumber oldResetAt = resetAt.load(atomics::memory_order_acquire);
		if (oldResetAt && oldResetAt < oldest)
			setNewResetAt(oldResetAt, ListEntry<Versioned>::gc(tdbb, &list, oldest));

		TraNumber cur = TransactionNumber::current(tdbb);
		ListEntry<Versioned>* newEntry = FB_NEW_POOL(*getDefaultMemoryPool()) ListEntry<Versioned>(obj, cur, fl);
		if (!ListEntry<Versioned>::add(tdbb, list, newEntry))
		{
			newEntry->cleanup(tdbb, false);
			delete newEntry;
			return StoreResult::DUP;
		}
		setNewResetAt(oldResetAt, cur);

		auto rc = StoreResult::SKIP;
		if (obj && !(fl & CacheFlag::NOSCAN))
		{
			auto scanResult = newEntry->scan(tdbb, fl, this);
			switch (scanResult)
			{
			case ScanResult::COMPLETE:
			case ScanResult::REPEAT:
				rc = StoreResult::DONE;
				break;

			case ScanResult::MISS:
				rc = StoreResult::MISS;
				break;
			}
		}

		if (!(fl & CacheFlag::NOCOMMIT))
			commit(tdbb);

		return rc;
	}

	Versioned* makeObject(thread_db* tdbb, ObjectBase::Flag fl)
	{
		auto obj = Versioned::create(tdbb, Permanent::getPool(), this);
		if (!obj)
			(Firebird::Arg::Gds(isc_random) << "Object create failed in makeObject()").raise();

		switch (storeObject(tdbb, obj, fl & ~CacheFlag::ERASED))
		{
		case StoreResult::DUP:
			Versioned::destroy(tdbb, obj);
			break;

		case StoreResult::SKIP:
		case StoreResult::DONE:
			return obj;

		case StoreResult::MISS:
			break;
		}

		return nullptr;
	}

	void commit(thread_db* tdbb)
	{
		HazardPtr<ListEntry<Versioned>> current(list);
		if (current)
		{
			auto flags = current->commit(tdbb, TransactionNumber::current(tdbb), TransactionNumber::next(tdbb));

			if (flags & CacheFlag::NOCOMMIT)	// Committed newly created version in cache
				pingLock(tdbb, flags, this->getId(), Versioned::objectFamily(this));

			if (flags & CacheFlag::ERASED)
				commitErase(tdbb);
		}
	}

	void rollback(thread_db* tdbb)
	{
		ListEntry<Versioned>::rollback(tdbb, list, TransactionNumber::current(tdbb));
	}

/*
	void gc()
	{
		list.load()->assertCommitted();
		ListEntry<Versioned>::gc(&list, MAX_TRA_NUMBER);
	}
 */

private:
	// called by AST handler
	void reset(thread_db* tdbb, bool erase) override
	{
		storeObject(tdbb, nullptr, erase ? CacheFlag::ERASED : 0);
	}

public:
	CacheElement* erase(thread_db* tdbb)
	{
		HazardPtr<ListEntry<Versioned>> l(list);
		fb_assert(l);
		if (!l)
			return nullptr;

		if (storeObject(tdbb, nullptr, CacheFlag::ERASED | CacheFlag::NOCOMMIT) == StoreResult::DUP)
		{
			Versioned* oldObj = getVersioned(tdbb, 0);
			busyError(tdbb, this->getId(), V::objectFamily(this));
		}

		return this;
	}

	bool isDropped() const
	{
		HazardPtr<ListEntry<Versioned>> l(list);
		return l && (l->getFlags() & CacheFlag::ERASED);
	}

	bool scanInProgress() const
	{
		HazardPtr<ListEntry<Versioned>> listEntry(list);
		if (!listEntry)
			return false;

		return listEntry->scanInProgress();
	}

	static int getObjectType()
	{
		return Versioned::objectType();
	}

	void newVersion(thread_db* tdbb)
	{
		TraNumber traNum;

		switch (isAvailable(tdbb, &traNum))
		{
		case OCCUPIED:
			if (ConsistencyCheck::commitNumber(tdbb))
				newVersionBusy(tdbb, this->getId(), Versioned::objectFamily(this), traNum);
			break;

		case MODIFIED:
			break;

		case MISSING:
		case READY:
			if (scanInProgress())
				newVersionScan(tdbb, this->getId(), Versioned::objectFamily(this));

			storeObject(tdbb, nullptr, CacheFlag::TAG_FOR_UPDATE);
			break;
		}
	}

private:
	void setNewResetAt(TraNumber oldVal, TraNumber newVal)
	{
		resetAt.compare_exchange_strong(oldVal, newVal,
			atomics::memory_order_release, atomics::memory_order_relaxed);
	}

private:
	std::atomic<ListEntry<Versioned>*> list = nullptr;
	std::atomic<TraNumber> resetAt = 0;
	AtomicElementPointer* ptrToClean = nullptr;
};


struct NoData
{
	NoData() { }
};

template <class StoredElement, unsigned SUBARRAY_SHIFT = 8, typename EXTEND = NoData>
class CacheVector : public Firebird::PermanentStorage
{
public:
	static const unsigned SUBARRAY_SIZE = 1 << SUBARRAY_SHIFT;
	static const unsigned SUBARRAY_MASK = SUBARRAY_SIZE - 1;

	typedef typename StoredElement::Versioned Versioned;
	typedef typename StoredElement::Permanent Permanent;
	typedef typename StoredElement::AtomicElementPointer SubArrayData;
	typedef std::atomic<SubArrayData*> ArrayData;
	typedef SharedReadVector<ArrayData, 4> Storage;

	explicit CacheVector(MemoryPool& pool, EXTEND extend = NoData())
		: Firebird::PermanentStorage(pool),
		  m_objects(),
		  m_extend(extend)
	{}

private:
	static FB_SIZE_T getCount(const HazardPtr<typename Storage::Generation>& v)
	{
		return v->getCount() << SUBARRAY_SHIFT;
	}

	SubArrayData* getDataPointer(MetaId id) const
	{
		auto up = m_objects.readAccessor();
		if (id >= getCount(up))
			return nullptr;

		auto sub = up->value(id >> SUBARRAY_SHIFT).load(atomics::memory_order_acquire);
		fb_assert(sub);
		return &sub[id & SUBARRAY_MASK];
	}

	void grow(FB_SIZE_T reqSize)
	{
		fb_assert(reqSize > 0);
		reqSize = ((reqSize - 1) >> SUBARRAY_SHIFT) + 1;

		Firebird::MutexLockGuard g(m_objectsGrowMutex, FB_FUNCTION);

		m_objects.grow(reqSize, false);
		auto wa = m_objects.writeAccessor();
		fb_assert(wa->getCapacity() >= reqSize);
		while (wa->getCount() < reqSize)
		{
			SubArrayData* sub = FB_NEW_POOL(getPool()) SubArrayData[SUBARRAY_SIZE];
			memset(sub, 0, sizeof(SubArrayData) * SUBARRAY_SIZE);
			wa->addStart()->store(sub, atomics::memory_order_release);
			wa->addComplete();
		}
	}

public:
	StoredElement* getDataNoChecks(MetaId id) const
	{
		SubArrayData* ptr = getDataPointer(id);
		if (!ptr)
			return nullptr;

		auto* data = ptr->load(atomics::memory_order_relaxed);
		if (!data)
			return nullptr;

		return data->hasEntries() ? data : nullptr;
	}

	StoredElement* getData(thread_db* tdbb, MetaId id, ObjectBase::Flag fl)
	{
		SubArrayData* ptr = getDataPointer(id);

		if (ptr)
		{
			StoredElement* rc = ptr->load(atomics::memory_order_relaxed);
			if (rc && rc->getEntry(tdbb, TransactionNumber::current(tdbb), fl))
				return rc;
		}

		if (fl & CacheFlag::AUTOCREATE)
		{
			StoredElement* data = ensurePermanent(tdbb, id);
			data->makeObject(tdbb, fl);
			return data;
		}

		return nullptr;
	}

	FB_SIZE_T getCount() const
	{
		return getCount(m_objects.readAccessor());
	}

	Versioned* getVersioned(thread_db* tdbb, MetaId id, ObjectBase::Flag fl)
	{

//		In theory that should be endless cycle - object may arrive/disappear again and again.
//		But in order to faster find devel problems we run it very limited number of times.
#ifdef DEV_BUILD
		for (int i = 0; i < 2; ++i)
#else
		for (;;)
#endif
		{
			auto ptr = getDataPointer(id);
			if (ptr)
			{
				StoredElement* data = ptr->load(atomics::memory_order_acquire);
				if (data)
				{
					if (fl & CacheFlag::UPGRADE)
					{
						auto val = makeObject(tdbb, id, fl);
						if (val)
							return val;
						continue;
					}

					return data->getVersioned(tdbb, fl);
				}
			}

			if (!(fl & CacheFlag::AUTOCREATE))
				return nullptr;

			auto val = makeObject(tdbb, id, fl);
			if (val)
				return val;
		}
#ifdef DEV_BUILD
		(Firebird::Arg::Gds(isc_random) << "Object suddenly disappeared").raise();
#endif
	}

	StoredElement* erase(thread_db* tdbb, MetaId id)
	{
		auto ptr = getDataPointer(id);
		if (ptr)
		{
			StoredElement* data = ptr->load(atomics::memory_order_acquire);
			if (data)
				return data->erase(tdbb);
		}

		return nullptr;
	}

	StoredElement* ensurePermanent(thread_db* tdbb, MetaId id)
	{
		if (id >= getCount())
			grow(id + 1);

		auto ptr = getDataPointer(id);
		fb_assert(ptr);

		StoredElement* data = ptr->load(atomics::memory_order_acquire);
		if (!data)
		{
			StoredElement* newData = FB_NEW_POOL(getPool()) StoredElement(tdbb, getPool(), id, m_extend);
			if (ptr->compare_exchange_strong(data, newData,
				atomics::memory_order_release, atomics::memory_order_acquire))
			{
				newData->setCleanup(ptr);
				data = newData;
			}
			else
			{
				newData->releaseLocks(tdbb);
				StoredElement::cleanup(tdbb, newData);
			}
		}

		return data;
	}

	Versioned* makeObject(thread_db* tdbb, MetaId id, ObjectBase::Flag fl)
	{
		StoredElement* data = ensurePermanent(tdbb, id);
		return data->makeObject(tdbb, fl);
	}

	StoredElement* newVersion(thread_db* tdbb, MetaId id)
	{
		if (id < getCount())
		{
			auto ptr = getDataPointer(id);
			if (ptr)
			{
				StoredElement* data = ptr->load(atomics::memory_order_acquire);
				if (data)
				{
					data->newVersion(tdbb);
					return data;
				}
			}
		}

		StoredElement* data = ensurePermanent(tdbb, id);
		data->newVersion(tdbb);
		return data;
	}

	template <typename F>
	StoredElement* lookup(thread_db* tdbb, F&& cmp, ObjectBase::Flag fl) const
	{
		auto a = m_objects.readAccessor();
		for (FB_SIZE_T i = 0; i < a->getCount(); ++i)
		{
			SubArrayData* const sub = a->value(i).load(atomics::memory_order_relaxed);
			if (!sub)
				continue;

			for (SubArrayData* end = &sub[SUBARRAY_SIZE]; sub < end--;)
			{
				StoredElement* ptr = end->load(atomics::memory_order_relaxed);
				if (ptr)
				{
					auto listEntry = ptr->getEntry(tdbb, TransactionNumber::current(tdbb), fl | CacheFlag::MINISCAN);
					if (listEntry && cmp(ptr))
					{
						if (!(fl & CacheFlag::ERASED))
							ptr->reload(tdbb, fl);		// found object to be reloaded w/o MINISCAN flag
						return ptr;
					}
				}
			}
		}

		return nullptr;
	}

	bool lookup(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag fl,
		StoredElement** element, Versioned** versioned)
	{
		auto a = m_objects.readAccessor();
		for (FB_SIZE_T i = 0; i < a->getCount(); ++i)
		{
			SubArrayData* sub = a->value(i).load(atomics::memory_order_relaxed);
			if (!sub)
				continue;

			for (SubArrayData* const end = &sub[SUBARRAY_SIZE]; sub < end; ++sub)
			{
				StoredElement* ptr = sub->load(atomics::memory_order_relaxed);
				if (ptr)
				{
					auto listEntry = ptr->getEntry(tdbb, TransactionNumber::current(tdbb), fl | CacheFlag::MINISCAN);
					if (listEntry && ptr->getName() == name)
					{
						if (!(fl & CacheFlag::ERASED))
							ptr->reload(tdbb, fl);		// found object to be reloaded w/o MINISCAN flag
						if (versioned)
							*versioned = listEntry->getVersioned();
						if (element)
							*element = ptr;

						return true;
					}
				}
			}
		}

		if (!(fl & CacheFlag::AUTOCREATE))
			return false;

		auto id = Versioned::getIdByName(tdbb, name);
		if (!id.has_value())
			return false;

		auto* v = getVersioned(tdbb, id.value(), fl);
		if (!v)
			return false;

		if (versioned)
			*versioned = v;
		if (element)
			*element = v->getPermanent();

		return true;
	}

	~CacheVector()
	{
		fb_assert(!m_objects.hasData());
	}

	void cleanup(thread_db* tdbb)
	{
		auto a = m_objects.writeAccessor();
		for (FB_SIZE_T i = 0; i < a->getCount(); ++i)
		{
			SubArrayData* const sub = a->value(i).load(atomics::memory_order_relaxed);
			if (!sub)
				continue;

			for (SubArrayData* end = &sub[SUBARRAY_SIZE]; sub < end--;)
			{
				StoredElement* elem = end->load(atomics::memory_order_relaxed);
				if (!elem)
					continue;

				StoredElement::cleanup(tdbb, elem);
				fb_assert(!end->load(atomics::memory_order_relaxed));
			}

			delete[] sub;		// no need using retire() here in CacheVector's cleanup
			a->value(i).store(nullptr, atomics::memory_order_relaxed);
		}

		m_objects.clear();
	}

	HazardPtr<typename Storage::Generation> readAccessor() const
	{
		return m_objects.readAccessor();
	}

	class Iterator
	{
		static const FB_SIZE_T eof = ~0u;
		static const FB_SIZE_T endloop = ~0u;

	public:
		StoredElement* operator*()
		{
			return get();
		}

		Iterator& operator++()
		{
			index = locateData(index + 1);
			return *this;
		}

		bool operator==(const Iterator& itr) const
		{
			fb_assert(data == itr.data);
			return index == itr.index ||
				(index == endloop && itr.index == eof) ||
				(itr.index == endloop && index == eof);
		}

		bool operator!=(const Iterator& itr) const
		{
			fb_assert(data == itr.data);
			return !operator==(itr);
		}

	private:
		void* operator new(size_t) = delete;
		void* operator new[](size_t) = delete;

	public:
		enum class Location {Begin, End};
		Iterator(const CacheVector* v, Location loc)
			: data(v),
			  index(loc == Location::Begin ? locateData(0) : endloop)
		{ }

		StoredElement* get()
		{
			fb_assert(index != eof);
			if (index == eof)
				return nullptr;
			StoredElement* ptr = data->getDataNoChecks(index);
			fb_assert(ptr);
			return ptr;
		}

	private:
		FB_SIZE_T locateData(FB_SIZE_T i)
		{
			for (auto cnt = data->getCount(); i < cnt; ++i)
			{
				if (data->getDataNoChecks(i))
					return i;
			}

			return eof;
		}

		const CacheVector* data;
		FB_SIZE_T index;		// should be able to store MAX_META_ID + 1 value
	};

	Iterator begin() const
	{
		return Iterator(this, Iterator::Location::Begin);
	}

	Iterator end() const
	{
		return Iterator(this, Iterator::Location::End);
	}

private:
	Storage m_objects;
	Firebird::Mutex m_objectsGrowMutex;
	EXTEND m_extend;
};

template <typename T>
auto getPermanent(T* t) -> decltype(t->getPermanent())
{
	return t ? t->getPermanent() : nullptr;
}

} // namespace Jrd

#endif // JRD_CACHEVECTOR_H
