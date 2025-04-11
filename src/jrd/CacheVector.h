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

#include "../common/ThreadStart.h"
#include "../common/StatusArg.h"

#include "../jrd/SharedReadVector.h"
#include "../jrd/constants.h"
#include "../jrd/tra_proto.h"

namespace Jrd {

class thread_db;


class ObjectBase
{
public:
	typedef unsigned Flag;
	virtual void lockedExcl [[noreturn]] (thread_db* tdbb) /*const*/;
	virtual const char* c_name() const = 0;
};


class ElementBase
{
public:
	enum ResetType {Recompile, Mark, Commit, Rollback};

	typedef SLONG ReturnedId;	// enable '-1' as not found

public:
	virtual ~ElementBase();
	virtual void resetDependentObject(thread_db* tdbb, ResetType rt) = 0;
	virtual void cleanup(thread_db* tdbb) = 0;

public:
	void resetDependentObjects(thread_db* tdbb, TraNumber olderThan);
	void addDependentObject(thread_db* tdbb, ElementBase* dep);
	void removeDependentObject(thread_db* tdbb, ElementBase* dep);
	[[noreturn]] void busyError(thread_db* tdbb, MetaId id, const char* name, const char* family);
	void commitErase(thread_db* tdbb);
};


namespace CacheFlag
{
	static const ObjectBase::Flag COMMITTED =	0x01;		// version already committed
	static const ObjectBase::Flag ERASED =		0x02;		// object erased
	static const ObjectBase::Flag NOSCAN =		0x04;		// do not call Versioned::scan()
	static const ObjectBase::Flag AUTOCREATE =	0x08;		// create initial version automatically
	static const ObjectBase::Flag NOCOMMIT =	0x10;		// do not commit created version
	static const ObjectBase::Flag RET_ERASED =	0x20;		// return erased objects
	static const ObjectBase::Flag RETIRED = 	0x40;		// object is in a process of GC
	static const ObjectBase::Flag UPGRADE = 	0x80;		// create new versions for already existing in a cache objects
}


class VersionSupport
{
public:
	static MdcVersion next(thread_db* tdbb);
};


class CachePool
{
public:
	static MemoryPool& get(thread_db* tdbb);
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


template <class OBJ>
class ListEntry : public HazardObject
{
public:
	enum State { INITIAL, RELOAD, SCANNING, READY };

	ListEntry(OBJ* object, TraNumber traNumber, ObjectBase::Flag fl)
		: object(object), traNumber(traNumber), cacheFlags(fl), state(INITIAL)
	{ }

	~ListEntry()
	{
		fb_assert(!object);
	}

	void cleanup(thread_db* tdbb, bool withObject = true)
	{
		if (object)		// be careful with ERASED entries
		{
			if (withObject)
				OBJ::destroy(tdbb, object);
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
	static OBJ* getObject(thread_db* tdbb, HazardPtr<ListEntry>& listEntry, TraNumber currentTrans, ObjectBase::Flag fl)
	{
		auto entry = getEntry(tdbb, listEntry, currentTrans, fl);
		return entry ? entry->getObject() : nullptr;
	}

	// find appropriate object in cache
	static HazardPtr<ListEntry> getEntry(thread_db* tdbb, HazardPtr<ListEntry>& listEntry, TraNumber currentTrans, ObjectBase::Flag fl)
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

					if (fl & CacheFlag::ERASED)
						continue;

					return HazardPtr<ListEntry>(nullptr);		// object dropped
				}

				// required entry found in the list
				if (listEntry->object)
				{
					switch (listEntry->scan(tdbb, fl))
					{
					case ScanResult::COMPLETE:
					case ScanResult::REPEAT:		// scan complete but reload was requested - 
					case ScanResult::SKIP:			// scan skipped due to NOSCAN flag
						break;

					case ScanResult::MISS:			// no object
					default:
						return HazardPtr<ListEntry>(nullptr);
					}
				}
				return listEntry;
			}
		}

		return HazardPtr<ListEntry>(nullptr);	// object created (not by us) and not committed yet
	}

	bool isBusy(TraNumber currentTrans) const noexcept
	{
		return !((getFlags() & CacheFlag::COMMITTED) || (traNumber == currentTrans));
	}

	ObjectBase::Flag getFlags() const noexcept
	{
		return cacheFlags.load(atomics::memory_order_relaxed);
	}

	OBJ* getObject()
	{
		return object;
	}

	// add new entry to the list
	static bool add(thread_db* tdbb, atomics::atomic<ListEntry*>& list, ListEntry* newVal)
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
					oldVal.set(list);
				}
				else
					return false;
			}

			newVal->next.store(oldVal.getPointer());
		} while (!oldVal.replace(list, newVal));

		return true;
	}

	// insert newVal in the beginning of a list provided there is still oldVal at the top of the list
	static bool replace(atomics::atomic<ListEntry*>& list, ListEntry* newVal, ListEntry* oldVal) noexcept
	{
		if (oldVal && oldVal->isBusy(newVal->traNumber))	// modified in other transaction
			return false;

		newVal->next.store(oldVal, atomics::memory_order_acquire);
		return list.compare_exchange_strong(oldVal, newVal, std::memory_order_release, std::memory_order_acquire);
	}

	// remove too old objects - they are anyway can't be in use
	static TraNumber gc(thread_db* tdbb, atomics::atomic<ListEntry*>* list, const TraNumber oldest)
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
							OBJ::destroy(tdbb, entry->object);
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

	// created (erased) earlier object is OK and should become visible to the world
	// return true if object was erased
	bool commit(thread_db* tdbb, TraNumber currentTrans, TraNumber nextTrans)
	{
		TraNumber oldNumber = traNumber;

		traNumber = nextTrans;
		version = VersionSupport::next(tdbb);
		auto flags = cacheFlags.fetch_or(CacheFlag::COMMITTED);

		fb_assert((flags & CacheFlag::COMMITTED ? nextTrans : currentTrans) == oldNumber);

		return flags & CacheFlag::ERASED;
	}

	// created earlier object is bad and should be destroyed
	static void rollback(thread_db* tdbb, atomics::atomic<ListEntry*>& list, const TraNumber currentTran)
	{
		// Take into an account that no other transaction except current (i.e. object creator)
		// can access uncommitted objects, only list entries may be accessed as hazard pointers.
		// Therefore rollback can retire such entries at once, a kind of pop() from stack.

		HazardPtr<ListEntry> entry(list);
		while (entry)
		{
			if (entry->getFlags() & CacheFlag::COMMITTED)
				break;
			fb_assert(entry->traNumber == currentTran);

			if (entry.replace(list, entry->next))
			{
				entry->next = nullptr;
				OBJ::destroy(tdbb, entry->object);
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
	static ScanResult scan(thread_db* tdbb, OBJ* obj, ObjectBase::Flag fl, bool rld = false)
	{
		if ((fl & CacheFlag::NOSCAN) || (!obj))
			return ScanResult::SKIP;

		auto* flags = TransactionNumber::getFlags(tdbb);
		Firebird::AutoSetRestoreFlag readCommitted(flags,
			(*flags) & TRA_degree3 ? 0 : TRA_read_committed | TRA_rec_version, true);

		return rld ? obj->reload(tdbb, fl) : obj->scan(tdbb, fl);
	}

public:
	ScanResult scan(thread_db* tdbb, ObjectBase::Flag fl)
	{
		// no need opening barrier twice
		// explicitly done first cause will be done in 99.99%
		if (state == READY)
			return ScanResult::COMPLETE;

		// enable recursive no-action pass by scanning thread
		// if thd is current thread state is not going to be changed - current thread holds mutex
		if ((thd == Thread::getId()) && (state == SCANNING))
			return ScanResult::COMPLETE;

		std::unique_lock<std::mutex> g(mtx);
		for(;;)
		{
			bool reason = true;
			auto savedState = state;

			switch (state)
			{
			case INITIAL: 		// Our thread becomes scanning thread
				reason = false;
				// fall through...
			case RELOAD: 		// may be because object body reload required.
				thd = Thread::getId();
				state = SCANNING;

				try
				{
					auto result = scan(tdbb, object, fl, reason);
					switch (result)
					{
					case ScanResult::COMPLETE:
						state = READY;
						break;

					case ScanResult::SKIP:
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
		return (state == READY) || ((thd == Thread::getId()) && (state == SCANNING));
	}

	bool scanInProgress() const
	{
		return state == READY ? false : (thd == Thread::getId()) && (state == SCANNING);
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
	OBJ* object;
	atomics::atomic<ListEntry*> next = nullptr;
	TraNumber traNumber;		// when COMMITTED not set - stores transaction that created this list element
								// when COMMITTED is set - stores transaction after which older elements are not needed
								// traNumber to be changed BEFORE setting COMMITTED

	MdcVersion version = 0;		// version of metadata cache when object was added
	ThreadId thd = 0;			// thread that performs object scan()
	atomics::atomic<ObjectBase::Flag> cacheFlags;
	State state;				// current entry state
};


enum class StoreResult { DUP, DONE, MISS, SKIP };


typedef class Lock* MakeLock(thread_db*, MemoryPool&);

template <class V, class P>
class CacheElement : public ElementBase, public P
{
public:
	typedef V Versioned;
	typedef P Permanent;

	typedef atomics::atomic<CacheElement*> AtomicElementPointer;

	template <typename EXTEND>
	CacheElement(thread_db* tdbb, MemoryPool& p, MetaId id, MakeLock* makeLock, EXTEND extend) :
		Permanent(tdbb, p, id, makeLock, extend), list(nullptr), resetAt(0), ptrToClean(nullptr)
	{ }

	CacheElement(MemoryPool& p) :
		Permanent(p), list(nullptr), resetAt(0), ptrToClean(nullptr)
	{ }

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
			// destroy() returns true if it completed removal of permamnet part (delete by pool)
			// if not - delete it ourself here
			delete element;
		}
	}

	void cleanup(thread_db* tdbb) override
	{
		cleanup(tdbb, this);
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
			Versioned* obj = ListEntry<Versioned>::getObject(tdbb, listEntry, cur, fl);
			if (obj)
				listEntry->scan(tdbb, fl);
		}
	}

	Versioned* getObject(thread_db* tdbb, ObjectBase::Flag fl)
	{
		return getObject(tdbb, TransactionNumber::current(tdbb), fl);
	}

	bool isReady(thread_db* tdbb)
	{
		auto entry = getEntry(tdbb, TransactionNumber::current(tdbb), CacheFlag::NOSCAN | CacheFlag::NOCOMMIT);
		return entry && entry->isReady();
	}

	ObjectBase::Flag getFlags(thread_db* tdbb)
	{
		auto entry = getEntry(tdbb, TransactionNumber::current(tdbb), CacheFlag::NOSCAN | CacheFlag::NOCOMMIT);
		return entry ? entry->getFlags() : 0;
	}

	Versioned* getObject(thread_db* tdbb, TraNumber traNum, ObjectBase::Flag fl)
	{
		auto entry = getEntry(tdbb, traNum, fl);
		return entry ? entry->getObject() : nullptr;
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
				newEntry = FB_NEW_POOL(*getDefaultMemoryPool()) ListEntry<Versioned>(obj, traNum, fl);
			}
			catch (const Firebird::Exception&)
			{
				if (obj)
					Versioned::destroy(tdbb, obj);
				throw;
			}

			if (ListEntry<Versioned>::replace(list, newEntry, nullptr))
			{
				auto sr = newEntry->scan(tdbb, fl);
				if (! (fl & CacheFlag::NOCOMMIT))
					newEntry->commit(tdbb, traNum, TransactionNumber::next(tdbb));

				switch (sr)
				{
				case ScanResult::COMPLETE:
				case ScanResult::REPEAT:
					break;

				case ScanResult::MISS:
				case ScanResult::SKIP:
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
		return ListEntry<Versioned>::getEntry(tdbb, listEntry, traNum, fl);
	}

	// return latest committed version or nullptr when does not exist
	Versioned* getLatestObject(thread_db* tdbb) const
	{
		HazardPtr<ListEntry<Versioned>> listEntry(list);
		if (!listEntry)
			return nullptr;

		return ListEntry<Versioned>::getObject(tdbb, listEntry, MAX_TRA_NUMBER, 0);
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
			auto sr = newEntry->scan(tdbb, fl);
			switch (sr)
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
			newEntry->commit(tdbb, cur, TransactionNumber::next(tdbb));

		return rc;
	}

	Versioned* makeObject(thread_db* tdbb, ObjectBase::Flag fl)
	{
		auto obj = Versioned::create(tdbb, Permanent::getPool(), this);
		if (!obj)
			(Firebird::Arg::Gds(isc_random) << "Object create failed in makeObject()").raise();

		switch (storeObject(tdbb, obj, fl))
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
			if (current->commit(tdbb, TransactionNumber::current(tdbb), TransactionNumber::next(tdbb)))
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
	void resetDependentObject(thread_db* tdbb, ResetType rt) override
	{
		switch (rt)
		{
		case ElementBase::ResetType::Recompile:
			{
				Versioned* newObj = Versioned::create(tdbb, CachePool::get(tdbb), this);
				if (storeObject(tdbb, newObj, CacheFlag::NOCOMMIT) == StoreResult::DUP)
				{
					Versioned::destroy(tdbb, newObj);
					busyError(tdbb, this->getId(), this->c_name(), V::objectFamily(this));
				}
			}
			break;

		case ElementBase::ResetType::Mark:
			// used in AST, therefore ignore error when saving empty object
			storeObject(tdbb, nullptr, 0);
			break;

		case ElementBase::ResetType::Commit:
			commit(tdbb);
			break;

		case ElementBase::ResetType::Rollback:
			rollback(tdbb);
			break;
		}
	}

	CacheElement* erase(thread_db* tdbb)
	{
		HazardPtr<ListEntry<Versioned>> l(list);
		fb_assert(l);
		if (!l)
			return nullptr;

		if (storeObject(tdbb, nullptr, CacheFlag::ERASED | CacheFlag::NOCOMMIT) == StoreResult::DUP)
		{
			Versioned* oldObj = getObject(tdbb, 0);
			busyError(tdbb, this->getId(), this->c_name(), V::objectFamily(this));
		}

		return this;
	}

	bool isDropped() const
	{
		auto* l = list.load(atomics::memory_order_relaxed);
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

private:
	void setNewResetAt(TraNumber oldVal, TraNumber newVal)
	{
		resetAt.compare_exchange_strong(oldVal, newVal,
			atomics::memory_order_release, atomics::memory_order_relaxed);
	}

private:
	atomics::atomic<ListEntry<Versioned>*> list;
	atomics::atomic<TraNumber> resetAt;
	AtomicElementPointer* ptrToClean;
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
	typedef atomics::atomic<SubArrayData*> ArrayData;
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

		Firebird::MutexLockGuard g(objectsGrowMutex, FB_FUNCTION);

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
		return ptr ? ptr->load(atomics::memory_order_relaxed) : nullptr;
	}

	StoredElement* getData(thread_db* tdbb, MetaId id, ObjectBase::Flag fl) const
	{
		SubArrayData* ptr = getDataPointer(id);

		if (ptr)
		{
			StoredElement* rc = ptr->load(atomics::memory_order_relaxed);
			if (rc && rc->getObject(tdbb, fl))
				return rc;
		}

		return nullptr;
	}

	FB_SIZE_T getCount() const
	{
		return getCount(m_objects.readAccessor());
	}

	Versioned* getObject(thread_db* tdbb, MetaId id, ObjectBase::Flag fl)
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

					return data->getObject(tdbb, fl);
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

	Versioned* makeObject(thread_db* tdbb, MetaId id, ObjectBase::Flag fl)
	{
		if (id >= getCount())
			grow(id + 1);

		auto ptr = getDataPointer(id);
		fb_assert(ptr);

		StoredElement* data = ptr->load(atomics::memory_order_acquire);
		if (!data)
		{
			StoredElement* newData = FB_NEW_POOL(getPool())
				StoredElement(tdbb, getPool(), id, Versioned::makeLock, m_extend);
			if (ptr->compare_exchange_strong(data, newData,
				atomics::memory_order_release, atomics::memory_order_acquire))
			{
				newData->setCleanup(ptr);
				data = newData;
			}
			else
				StoredElement::cleanup(tdbb, newData);
		}

		return data->makeObject(tdbb, fl);
	}

	void tagForUpdate(thread_db* tdbb, MetaId id)
	{
		auto constexpr fl = CacheFlag::NOCOMMIT | CacheFlag::NOSCAN;

		fb_assert(id < getCount());
		if (id < getCount())
		{
			auto ptr = getDataPointer(id);
			fb_assert(ptr);

			StoredElement* data = ptr->load(atomics::memory_order_acquire);
			if (data)
			{
				if (data->isReady(tdbb) && !data->scanInProgress())
					data->makeObject(tdbb, fl);
				return;
			}
		}

		makeObject(tdbb, id, fl);
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
					auto listEntry = ptr->getEntry(tdbb, TransactionNumber::current(tdbb), CacheFlag::NOSCAN);
					if (listEntry && cmp(ptr))
					{
						// Optimize ??????????????
						ptr->reload(tdbb, fl);
						return ptr;
					}
				}
			}
		}

		return nullptr;
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

	bool clear(MetaId id)
	{
		if (id >= getCount())
			return false;

		auto a = m_objects.readAccessor();
		SubArrayData* sub = a->value(id >> SUBARRAY_SHIFT).load(atomics::memory_order_acquire);
		fb_assert(sub);
		sub = &sub[id & SUBARRAY_MASK];

		sub->store(nullptr, atomics::memory_order_release);
		return true;
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
			while (i < data->getCount())
			{
				if (data->getDataNoChecks(i))
					return i;
				++i;
			}

			return eof;
		}

		const CacheVector* data;
		FB_SIZE_T index;		// should be able to store MAX_METAID + 1 value
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
	Firebird::Mutex objectsGrowMutex;
	EXTEND m_extend;
};

template <typename T>
auto getPermanent(T* t) -> decltype(t->getPermanent())
{
	return t ? t->getPermanent() : nullptr;
}

} // namespace Jrd

#endif // JRD_CACHEVECTOR_H
