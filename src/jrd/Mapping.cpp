/*
 *	PROGRAM:		JRD access method
 *	MODULE:			Mapping.cpp
 *	DESCRIPTION:	Maps names in authentication block
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
 *  Copyright (c) 2014 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "firebird/Interface.h"
#include "../auth/SecureRemotePassword/Message.h"
#include "iberror.h"

#include "../jrd/constants.h"
#include "../common/classes/init.h"
#include "../common/classes/RefMutex.h"
#include "../common/classes/SyncObject.h"
#include "../jrd/MetaName.h"
#include "../common/isc_s_proto.h"
#include "../common/isc_proto.h"
#include "../common/ThreadStart.h"
#include "../common/db_alias.h"
#include "../common/classes/ParsedList.h"

#include "../jrd/Mapping.h"
#include "../jrd/tra.h"
#include "../jrd/ini.h"
#include "../jrd/status.h"
#include "../jrd/ids.h"

#ifdef WIN_NT
#include <process.h>
#define getpid _getpid
#endif

#define MAP_DEBUG(A)

using namespace Firebird;
using namespace Jrd;

namespace {

// internalFlags bits
const ULONG FLAG_DB =		1;
const ULONG FLAG_SEC =		2;
const ULONG FLAG_DOWN_DB =	4;
const ULONG FLAG_DOWN_SEC =	8;

// flagRolUsr values
const ULONG FLAG_USER = 1;
const ULONG FLAG_ROLE = 2;

const char* NM_ROLE = "Role";
const char* NM_USER = "User";
const char* TYPE_SEEN = "Seen";

void check(const char* s, IStatus* st)
{
	if (!(st->getState() & IStatus::STATE_ERRORS))
		return;

	Arg::StatusVector newStatus(st);
	newStatus << Arg::Gds(isc_map_load) << s;
	newStatus.raise();
}

} // anonymous namespace

namespace Jrd {

class AuthWriter : public ClumpletWriter
{
public:
	AuthWriter()
		: ClumpletWriter(WideUnTagged, MAX_DPB_SIZE), sequence(0)
	{ }

	void append(AuthWriter& w)
	{
		internalAppend(w);
	}

	void append(const AuthReader::AuthBlock& b)
	{
		ClumpletReader r(WideUnTagged, b.begin(), b.getCount());
		internalAppend(r);
	}

	void add(const AuthReader::Info& info)
	{
		ClumpletWriter to(WideUnTagged, MAX_DPB_SIZE);

		add(to, AuthReader::AUTH_TYPE, info.type);
		add(to, AuthReader::AUTH_NAME, info.name);
		add(to, AuthReader::AUTH_PLUGIN, info.plugin);
		add(to, AuthReader::AUTH_SECURE_DB, info.secDb);
		add(to, AuthReader::AUTH_ORIG_PLUG, info.origPlug);

		if (to.getBufferLength())
		{
			moveNext();
			insertBytes(sequence++, to.getBuffer(), to.getBufferLength());
		}
	}

private:
	void add(ClumpletWriter& to, const unsigned char tag, const NoCaseString& str)
	{
		if (str.hasData())
		{
			to.insertString(tag, str.c_str(), str.length());
		}
	}

	void internalAppend(ClumpletReader& w)
	{
		while (!isEof())
		{
			moveNext();
		}

		for(w.rewind(); !w.isEof(); w.moveNext())
		{
			SingleClumplet sc = w.getClumplet();
			sc.tag = sequence++;
			insertClumplet(sc);
			moveNext();
		}
	}

	unsigned char sequence;
};

} // namespace Jrd


Mapping::DbHandle::DbHandle()
{ }

void Mapping::DbHandle::setAttachment(IAttachment* att)
{
	if (att)
	{
		MAP_DEBUG(fprintf(stderr, "Using existing db handle %p\n", att));
		assign(att);
	}
}

void Mapping::DbHandle::clear()
{
	assign(nullptr);
}

bool Mapping::DbHandle::attach(const char* aliasDb, ICryptKeyCallback* cryptCb)
{
	FbLocalStatus st;
	bool down = false;		// true if on attach db is shutdown

	if (hasData())
	{
		MAP_DEBUG(fprintf(stderr, "Already attached %s\n", aliasDb));
		return down;
	}

	DispatcherPtr prov;
	if (cryptCb)
	{
		prov->setDbCryptCallback(&st, cryptCb);
		check("IProvider::setDbCryptCallback", &st);
	}

	ClumpletWriter embeddedSysdba(ClumpletWriter::dpbList, MAX_DPB_SIZE);
	embeddedSysdba.insertString(isc_dpb_user_name, DBA_USER_NAME, fb_strlen(DBA_USER_NAME));
	embeddedSysdba.insertByte(isc_dpb_sec_attach, TRUE);
	embeddedSysdba.insertString(isc_dpb_config, ParsedList::getNonLoopbackProviders(aliasDb));
	embeddedSysdba.insertByte(isc_dpb_map_attach, TRUE);
	embeddedSysdba.insertByte(isc_dpb_no_db_triggers, TRUE);

	MAP_DEBUG(fprintf(stderr, "Attach %s\n", aliasDb));
	IAttachment* att = prov->attachDatabase(&st, aliasDb,
		embeddedSysdba.getBufferLength(), embeddedSysdba.getBuffer());

	if (st->getState() & IStatus::STATE_ERRORS)
	{
		const ISC_STATUS* s = st->getErrors();
		MAP_DEBUG(isc_print_status(s));
		bool missing = fb_utils::containsErrorCode(s, isc_io_error);
		down = fb_utils::containsErrorCode(s, isc_shutdown);
		if (!(missing || down))
			check("IProvider::attachDatabase", &st);

		// down/missing DB is not a reason to fail mapping
	}
	else
		assignRefNoIncr(att);

	MAP_DEBUG(fprintf(stderr, "Att=%p\n", att));

	return down;
}


FB_SIZE_T Mapping::Map::hash(const Map& value, FB_SIZE_T hashSize)
{
	NoCaseString key = value.makeHashKey();
	return DefaultHash<Map>::hash(key.c_str(), key.length(), hashSize);
}

NoCaseString Mapping::Map::makeHashKey() const
{
	NoCaseString key;
	key += usng;
	MAP_DEBUG(key += ':');
	key += plugin;
	MAP_DEBUG(key += ':');
	key += db;
	MAP_DEBUG(key += ':');
	key += fromType;
	MAP_DEBUG(key += ':');
	key += from;

	key.upper();
	return key;
}

Mapping::Map::Map(const char* aUsing, const char* aPlugin, const char* aDb,
	const char* aFromType, const char* aFrom,
	SSHORT aRole, const char* aTo)
	: plugin(getPool()), db(getPool()), fromType(getPool()),
	  from(getPool()), to(getPool()), toRole(aRole ? true : false), usng(aUsing[0])
{
	plugin = aPlugin;
	db = aDb;
	fromType = aFromType;
	from = aFrom;
	to = aTo;

	trimAll();
}

Mapping::Map::Map(AuthReader::Info& info)   //type, name, plugin, secDb
	: plugin(getPool()), db(getPool()),
	  fromType(getPool()), from(getPool()), to(getPool()),
	  toRole(false), usng(info.plugin.hasData() ? 'P' : 'M')
{
	plugin = info.plugin.hasData() ? info.plugin.c_str() : "*";
	db = info.secDb.hasData() ? info.secDb.c_str() : "*";
	fromType = info.type;
	from = info.name.hasData() ? info.name.c_str() : "*";

	trimAll();
}

void Mapping::Map::trimAll()
{
	plugin.rtrim();
	db.rtrim();
	fromType.rtrim();
	from.rtrim();
	to.rtrim();
}

bool Mapping::Map::isEqual(const Map& k) const
{
	return usng == k.usng &&
		plugin == k.plugin &&
		db == k.db &&
		fromType == k.fromType &&
		from == k.from ;
}

Mapping::Map* Mapping::Map::get()
{
	return this;
}


Mapping::Cache::Cache(const NoCaseString& aliasDb, const NoCaseString& db)
	: alias(getPool(), aliasDb), name(getPool(), db),
	  dataFlag(false)
{
	enableDuplicates();
}

Mapping::Cache::~Cache()
{
	cleanup(eraseEntry);
}

bool Mapping::Cache::populate(IAttachment *att)
{
	FbLocalStatus st;

	if (dataFlag)
	{
		return false;
	}

	if (!att)
	{
		dataFlag = true;
		return false;
	}

	MAP_DEBUG(fprintf(stderr, "Populate cache for %s\n", name.c_str()));

	ITransaction* tra = nullptr;
	IResultSet* curs = nullptr;

	try
	{
		ClumpletWriter readOnly(ClumpletWriter::Tpb, MAX_DPB_SIZE, isc_tpb_version1);
		readOnly.insertTag(isc_tpb_read);
		readOnly.insertTag(isc_tpb_wait);
		tra = att->startTransaction(&st, readOnly.getBufferLength(), readOnly.getBuffer());
		check("IAttachment::startTransaction", &st);

		Message mMap;
		Field<Text> usng(mMap, 1);
		Field<Varying> plugin(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> db(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> fromType(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> from(mMap, 255);
		Field<SSHORT> role(mMap);
		Field<Varying> to(mMap, MAX_SQL_IDENTIFIER_SIZE);

		curs = att->openCursor(&st, tra, 0,
			"SELECT RDB$MAP_USING, RDB$MAP_PLUGIN, RDB$MAP_DB, RDB$MAP_FROM_TYPE, "
			"	RDB$MAP_FROM, RDB$MAP_TO_TYPE, RDB$MAP_TO "
			"FROM RDB$AUTH_MAPPING",
			3, nullptr, nullptr, mMap.getMetadata(), nullptr, 0);
		if (st->getState() & IStatus::STATE_ERRORS)
		{
			if (fb_utils::containsErrorCode(st->getErrors(), isc_dsql_relation_err))
			{
				// isc_dsql_relation_err when opening cursor - i.e. table RDB$AUTH_MAPPING
				// is missing due to non-FB3 security DB
				tra->release();
				dataFlag = true;
				return false;
			}
			check("IAttachment::openCursor", &st);
		}

		while (curs->fetchNext(&st, mMap.getBuffer()) == IStatus::RESULT_OK)
		{
			const char* expandedDb = "*";
			PathName target;
			if (!db.null)
			{
				expandedDb = db;
				MAP_DEBUG(fprintf(stderr, "non-expandedDb '%s'\n", expandedDb));
				expandDatabaseName(expandedDb, target, nullptr);
				expandedDb = target.c_str();
				MAP_DEBUG(fprintf(stderr, "expandedDb '%s'\n", expandedDb));
			}

			Map* map = FB_NEW Map(usng, plugin.null ? "*" : plugin, expandedDb,
				fromType, from, role, to.null ? "*" : to);
			MAP_DEBUG(fprintf(stderr, "Add = %s\n", map->makeHashKey().c_str()));
			add(map);
		}
		check("IResultSet::fetchNext", &st);

		curs->close(&st);
		check("IResultSet::close", &st);
		curs = nullptr;

		tra->rollback(&st);
		check("ITransaction::rollback", &st);
		tra = nullptr;

		dataFlag = true;
	}
	catch (const Exception& ex)
	{
		if (curs)
			curs->release();
		if (tra)
			tra->release();

		// If database is shutdown it's not a reason to fail mapping
		StaticStatusVector status;
		ex.stuffException(status);

		const ISC_STATUS* s = status.begin();

		if (fb_utils::containsErrorCode(s, isc_shutdown))
			return true;

		throw;
	}

	return false;
}

void Mapping::Cache::map(bool flagWild, ExtInfo& info, AuthWriter& newBlock)
{
	if (info.type == TYPE_SEEN)
		return;

	Map from(info);

	if (from.from == "*")
		Arg::Gds(isc_map_aster).raise();

	if (!flagWild)
		search(info, from, newBlock, from.from);
	else
		varUsing(info, from, newBlock);
}

void Mapping::Cache::search(ExtInfo& info, const Map& from, AuthWriter& newBlock,
	const NoCaseString& originalUserName)
{
	MAP_DEBUG(fprintf(stderr, "Key = %s\n", from.makeHashKey().c_str()));
	if (!dataFlag)
		return;

	for (Map* to = lookup(from); to; to = to->next(from))
	{
		MAP_DEBUG(fprintf(stderr, "Match!!\n"));
		unsigned flagRolUsr = to->toRole ? FLAG_ROLE : FLAG_USER;
		if (info.found & flagRolUsr)
			continue;

		const NoCaseString& newName(to->to == "*" ? originalUserName : to->to);
		NoCaseString& infoName(to->toRole ? info.currentRole : info.currentUser);
		if (info.current & flagRolUsr)
		{
			if (infoName == newName)
				continue;
			(Arg::Gds(isc_map_multi) << originalUserName).raise();
		}

		info.current |= flagRolUsr;
		infoName = newName;

		AuthReader::Info newInfo;
		newInfo.type = to->toRole ? NM_ROLE : NM_USER;
		newInfo.name = newName;
        newInfo.secDb = this->name;
        newInfo.origPlug = info.origPlug.hasData() ? info.origPlug : info.plugin;
		newBlock.add(newInfo);
	}
}

void Mapping::Cache::varPlugin(ExtInfo& info, Map from, AuthWriter& newBlock)
{
	varDb(info, from, newBlock);
	if (from.plugin != "*")
	{
		from.plugin = "*";
		varDb(info, from, newBlock);
	}
}

void Mapping::Cache::varDb(ExtInfo& info, Map from, AuthWriter& newBlock)
{
	varFrom(info, from, newBlock);
	if (from.db != "*")
	{
		from.db = "*";
		varFrom(info, from, newBlock);
	}
}

void Mapping::Cache::varFrom(ExtInfo& info, Map from, AuthWriter& newBlock)
{
	NoCaseString originalUserName = from.from;
	search(info, from, newBlock, originalUserName);
	from.from = "*";
	search(info, from, newBlock, originalUserName);
}

void Mapping::Cache::varUsing(ExtInfo& info, Map from, AuthWriter& newBlock)
{
	if (from.usng == 'P')
	{
		varPlugin(info, from, newBlock);

		from.usng = '*';
		varPlugin(info, from, newBlock);

		if (!info.secDb.hasData())
		{
			from.usng = 'S';
			from.plugin = "*";
			varDb(info, from, newBlock);
		}
	}
	else if (from.usng == 'M')
	{
		varDb(info, from, newBlock);

		from.usng = '*';
		varDb(info, from, newBlock);
	}
	else
		fb_assert(false);
}

bool Mapping::Cache::map4(bool flagWild, unsigned flagSet, AuthReader& rdr, ExtInfo& info, AuthWriter& newBlock)
{
	if (!flagSet)
	{
		AuthWriter workBlock;

		for (rdr.rewind(); rdr.getInfo(info); rdr.moveNext())
		{
			map(flagWild, info, workBlock);
		}

		info.found |= info.current;
		info.current = 0;
		newBlock.append(workBlock);
	}

	unsigned mapMask = FLAG_USER | FLAG_ROLE;
	return (info.found & mapMask) == mapMask;
}

void Mapping::Cache::eraseEntry(Map* m)
{
	delete m;
}


namespace
{

typedef GenericMap<Pair<Left<NoCaseString, RefPtr<Mapping::Cache> > > > CacheTree;

InitInstance<CacheTree> tree;
GlobalPtr<Mutex> treeMutex;

void setupIpc();

void locate(RefPtr<Mapping::Cache>& cache, const NoCaseString& alias, const NoCaseString& target)
{
	fb_assert(treeMutex->locked());
	fb_assert(!cache);
	tree().get(target, cache);
	if (!cache)
	{
		cache = FB_NEW Mapping::Cache(alias, target);
		*(tree().put(target)) = cache;

		setupIpc();
	}
}

class Found
{
public:
	enum What {FND_NOTHING, FND_PLUG, FND_SEC, FND_DB};

	Found()
		: found(FND_NOTHING)
	{ }

	void set(What find, const AuthReader::Info& val)
	{
		fb_assert(find != FND_NOTHING);

		if (val.plugin.hasData())
			find = FND_PLUG;
		if (find == found && value != val.name)
			Arg::Gds(isc_map_undefined).raise();

		if (find > found)
		{
			found = find;
			value = val.name;
			if (val.plugin.hasData())
				method = val.plugin;
			else
				method = "Mapped from " + val.origPlug;
		}
	}

	NoCaseString value;
	NoCaseString method;
	What found;
};

void resetMap(const char* securityDb)
{
	MutexLockGuard g(treeMutex, FB_FUNCTION);
	tree().remove(securityDb);
}

void resetMap(const char* securityDb, ULONG index);


// ----------------------------------------------------

class MappingHeader : public Firebird::MemoryHeader
{
public:
	SLONG currentProcess;
	ULONG processes;
	char databaseForReset[1024];	// database for which cache to be reset
	ULONG resetIndex;				// what cache to reset

	struct Process
	{
		event_t notifyEvent;
		event_t callbackEvent;
		SLONG id;
		SLONG flags;
	};
	Process process[1];

	static const ULONG FLAG_ACTIVE = 0x1;
	static const ULONG FLAG_DELIVER = 0x2;
};

class MappingIpc final : public Firebird::IpcObject
{
	static const USHORT MAPPING_VERSION = 1;
	static const size_t DEFAULT_SIZE = 1024 * 1024;

public:
	explicit MappingIpc(MemoryPool&)
		: processId(getpid()),
		  cleanupSync(*getDefaultMemoryPool(), clearDelivery, THREAD_high)
	{ }

	~MappingIpc()
	{
		shutdown();
	}

	void shutdown()
	{
		if (!sharedMemory)
			return;
		MutexLockGuard gLocal(initMutex, FB_FUNCTION);
		if (!sharedMemory)
			return;

		{	// scope
			Guard gShared(this);

			MappingHeader* sMem = sharedMemory->getHeader();
			fb_assert(sMem->process[process].id);
			sMem->process[process].flags &= ~MappingHeader::FLAG_ACTIVE;

			(void)  // Ignore errors in cleanup
				sharedMemory->eventPost(&sMem->process[process].notifyEvent);

			cleanupSync.waitForCompletion();

			// Ignore errors in cleanup
			sharedMemory->eventFini(&sMem->process[process].notifyEvent);
			sharedMemory->eventFini(&sMem->process[process].callbackEvent);

			bool found = false;

			for (unsigned n = 0; n < sMem->processes; ++n)
			{
				if (sMem->process[n].flags & MappingHeader::FLAG_ACTIVE)
				{
					found = true;
					break;
				}
			}

			if (!found)
				sharedMemory->removeMapFile();
		}

		sharedMemory = nullptr;
	}

	void clearCache(const char* dbName, USHORT index)
	{
		PathName target;
		expandDatabaseName(dbName, target, nullptr);

		setup();

		Guard gShared(this);

		MappingHeader* sMem = sharedMemory->getHeader();
		target.copyTo(sMem->databaseForReset, sizeof(sMem->databaseForReset));
		sMem->resetIndex = index;

		// Set currentProcess
		sMem->currentProcess = -1;
		for (unsigned n = 0; n < sMem->processes; ++n)
		{
			MappingHeader::Process* p = &sMem->process[n];
			if (!(p->flags & MappingHeader::FLAG_ACTIVE))
				continue;

			if (p->id == processId)
			{
				sMem->currentProcess = n;
				break;
			}
		}

		if (sMem->currentProcess < 0)
		{
			// did not find current process
			// better ignore delivery than fail in it
			gds__log("MappingIpc::clearCache() failed to find current process %d in shared memory", processId);
			return;
		}
		MappingHeader::Process* current = &sMem->process[sMem->currentProcess];

		// Deliver
		for (unsigned n = 0; n < sMem->processes; ++n)
		{
			MappingHeader::Process* p = &sMem->process[n];
			if (!(p->flags & MappingHeader::FLAG_ACTIVE))
				continue;

			if (p->id == processId)
			{
				MAP_DEBUG(fprintf(stderr, "Internal resetMap(%s, %d)\n", sMem->databaseForReset, sMem->resetIndex));
				resetMap(sMem->databaseForReset, sMem->resetIndex);
				continue;
			}

			SLONG value = sharedMemory->eventClear(&current->callbackEvent);
			p->flags |= MappingHeader::FLAG_DELIVER;

			if (sharedMemory->eventPost(&p->notifyEvent) != FB_SUCCESS)
			{
				(Arg::Gds(isc_map_event) << "POST").raise();
			}

			while (sharedMemory->eventWait(&current->callbackEvent, value, 10000) != FB_SUCCESS)
			{
				if (!ISC_check_process_existence(p->id))
				{
					p->flags &= ~MappingHeader::FLAG_ACTIVE;
					sharedMemory->eventFini(&p->notifyEvent);
					sharedMemory->eventFini(&p->callbackEvent);
					break;
				}
			}

			MAP_DEBUG(fprintf(stderr, "Notified pid %d about reset map %s\n", p->id, sMem->databaseForReset));
		}
	}

	void setup()
	{
		if (sharedMemory)
			return;
		MutexLockGuard gLocal(initMutex, FB_FUNCTION);
		if (sharedMemory)
			return;

		AutoSharedMemory tempSharedMemory;
		try
		{
			tempSharedMemory.reset(FB_NEW_POOL(*getDefaultMemoryPool())
				SharedMemory<MappingHeader>(USER_MAP_FILE, DEFAULT_SIZE, this));
		}
		catch (const Exception& ex)
		{
			iscLogException("MappingIpc: Cannot initialize the shared memory region", ex);
			throw;
		}

		MappingHeader* sMem = tempSharedMemory->getHeader();
		checkHeader(sMem);

		Guard gShared(tempSharedMemory);

		for (process = 0; process < sMem->processes; ++process)
		{
			if (!(sMem->process[process].flags & MappingHeader::FLAG_ACTIVE))
				break;

			if (!ISC_check_process_existence(sMem->process[process].id))
			{
				tempSharedMemory->eventFini(&sMem->process[process].notifyEvent);
				tempSharedMemory->eventFini(&sMem->process[process].callbackEvent);

				break;
			}
		}

		if (process >= sMem->processes)
		{
			sMem->processes++;
			if (((U_IPTR) &sMem->process[sMem->processes]) - ((U_IPTR) sMem) > DEFAULT_SIZE)
			{
				sMem->processes--;
				(Arg::Gds(isc_imp_exc) << Arg::Gds(isc_map_overflow)).raise();
			}
		}

		sMem->process[process].id = processId;
		sMem->process[process].flags = MappingHeader::FLAG_ACTIVE;
		sharedMemory.reset(tempSharedMemory.release());

		if (sharedMemory->eventInit(&sMem->process[process].notifyEvent) != FB_SUCCESS)
		{
			(Arg::Gds(isc_map_event) << "INIT").raise();
		}
		if (sharedMemory->eventInit(&sMem->process[process].callbackEvent) != FB_SUCCESS)
		{
			(Arg::Gds(isc_map_event) << "INIT").raise();
		}

		try
		{
			cleanupSync.run(this);
		}
		catch (const Exception&)
		{
			sMem->process[process].flags &= ~MappingHeader::FLAG_ACTIVE;
			throw;
		}
		startupSemaphore.enter();
	}

	void exceptionHandler(const Exception& ex, ThreadFinishSync<MappingIpc*>::ThreadRoutine*)
	{
		iscLogException("Fatal error in clearDeliveryThread", ex);
		fb_utils::logAndDie("Fatal error in clearDeliveryThread");
	}

private:
	void clearDeliveryThread()
	{
		bool startup = true;
		try
		{
			MappingHeader::Process* p = &sharedMemory->getHeader()->process[process];
			while (p->flags & MappingHeader::FLAG_ACTIVE)
			{
				SLONG value = sharedMemory->eventClear(&p->notifyEvent);

				if (p->flags & MappingHeader::FLAG_DELIVER)
				{
					MappingHeader* sMem = sharedMemory->getHeader();
					resetMap(sMem->databaseForReset, sMem->resetIndex);
					p->flags &= ~MappingHeader::FLAG_DELIVER;

					MappingHeader::Process* cur = &sMem->process[sMem->currentProcess];
					if (sharedMemory->eventPost(&cur->callbackEvent) != FB_SUCCESS)
					{
						(Arg::Gds(isc_map_event) << "POST").raise();
					}
				}

				if (startup)
				{
					startup = false;
					startupSemaphore.release();
				}

				if (sharedMemory->eventWait(&p->notifyEvent, value, 0) != FB_SUCCESS)
				{
					(Arg::Gds(isc_map_event) << "WAIT").raise();
				}
			}
			if (startup)
				startupSemaphore.release();
		}
		catch (const Exception& ex)
		{
			exceptionHandler(ex, nullptr);
		}
	}

	// implement pure virtual functions
	bool initialize(SharedMemoryBase* sm, bool initFlag) override
	{
		if (initFlag)
		{
			MappingHeader* header = reinterpret_cast<MappingHeader*>(sm->sh_mem_header);

			// Initialize the shared data header
			initHeader(header);

			header->processes = 0;
			header->currentProcess = -1;
		}

		return true;
	}

	void mutexBug(int osErrorCode, const char* text) override
	{
		iscLogStatus("Error when working with user mapping shared memory",
			(Arg::Gds(isc_sys_request) << text << Arg::OsError(osErrorCode)).value());
	}

	USHORT getType() const override { return SharedMemoryBase::SRAM_MAPPING_RESET; }
	USHORT getVersion() const override { return MAPPING_VERSION; }
	const char* getName() const override { return "MappingIpc"; }

	// copying is prohibited
	MappingIpc(const MappingIpc&);
	MappingIpc& operator =(const MappingIpc&);

	class Guard;
	friend class Guard;
	typedef SharedMemory<MappingHeader> MappingSharedMemory;
	typedef AutoPtr<MappingSharedMemory> AutoSharedMemory;

	class Guard
	{
	public:
		explicit Guard(MappingIpc* ptr)
			: data(ptr->sharedMemory)
		{
			fb_assert(data);
			data->mutexLock();
		}

		explicit Guard(MappingSharedMemory* ptr)
			: data(ptr)
		{
			fb_assert(data);
			data->mutexLock();
		}

		~Guard()
		{
			data->mutexUnlock();
		}

	private:
		Guard(const Guard&);
		Guard& operator=(const Guard&);

		MappingSharedMemory* const data;
	};

	static void clearDelivery(MappingIpc* mapping)
	{
		mapping->clearDeliveryThread();
	}

	AutoSharedMemory sharedMemory;
	Mutex initMutex;
	const SLONG processId;
	unsigned process;
	Semaphore startupSemaphore;
	ThreadFinishSync<MappingIpc*> cleanupSync;
};

GlobalPtr<MappingIpc, InstanceControl::PRIORITY_DELETE_FIRST> mappingIpc;

void setupIpc()
{
	mappingIpc->setup();
}

const char* roleSql =
"with recursive role_tree as ( "
"   select rdb$role_name as nm from rdb$roles "
"       where rdb$role_name = ? "
"   union all "
"   select p.rdb$relation_name as nm from rdb$user_privileges p "
"       join role_tree t on t.nm = p.rdb$user "
"       where p.rdb$privilege = 'M') "
"select r.rdb$system_privileges from role_tree t "
"   join rdb$roles r on t.nm = r.rdb$role_name "
	;

const char* userSql =
"with recursive role_tree as ( "
"   select rdb$relation_name as nm from rdb$user_privileges "
"       where rdb$privilege = 'M' and rdb$field_name = 'D' and rdb$user = ? and rdb$user_type = 8 "
"   union all "
"   select p.rdb$relation_name as nm from rdb$user_privileges p "
"       join role_tree t on t.nm = p.rdb$user "
"       where p.rdb$privilege = 'M' and p.rdb$field_name = 'D') "
"select r.rdb$system_privileges from role_tree t "
"   join rdb$roles r on t.nm = r.rdb$role_name "
	;

class SysPrivCache : public PermanentStorage
{
public:
	SysPrivCache(MemoryPool& p)
		: PermanentStorage(p),
		  databases(getPool())
	{ }

	SyncObject* getSync()
	{
		return &sync;
	}

	bool getPrivileges(const PathName& db, const string& name, const string* sqlRole,
		const string& trusted_role, UserId::Privileges& system_privileges)
	{
		AutoPtr<DbCache>* c = databases.get(db);
		return c && (*c)->getPrivileges(name, sqlRole, trusted_role, system_privileges);
	}

	void populate(const PathName& db, Mapping::DbHandle& iDb, const string& name, const string* sqlRole,
		const string& trusted_role)
	{
		AutoPtr<DbCache>* ptr = databases.get(db);
		DbCache* c = nullptr;
		if (ptr)
		{
			c = ptr->get();
			fb_assert(c);
		}
		else
		{
			c = FB_NEW_POOL(getPool()) DbCache(getPool());
			*(databases.put(db)) = c;
		}
		c->populate(iDb, name, sqlRole, trusted_role);

		setupIpc();
	}

	void invalidate(const PathName& db)
	{
		AutoPtr<DbCache>* c = databases.get(db);
		if (c)
			(*c)->invalidate();
	}

private:
	class DbCache
	{
	public:
		DbCache(MemoryPool& p)
			: logins(p, userSql),
			  roles(p, roleSql),
			  pairs(p)
		{ }

		bool getPrivileges(const string& name, const string* sqlRole, const string& trusted_role,
			UserId::Privileges& system_privileges)
		{
			system_privileges.clearAll();
			if (!logins.getPrivileges(name, system_privileges))
				return false;

			MAP_DEBUG(fprintf(stderr, "name=%s\n", name.c_str()));

			bool granted = false;
			if (!pairs.isRoleGranted(name, sqlRole, granted))
				return false;

			MAP_DEBUG(fprintf(stderr, "granted=%d\n", granted));
			return roles.getPrivileges((granted ? *sqlRole : trusted_role), system_privileges);
		}

		void populate(Mapping::DbHandle& iDb, const string& name, const string* sqlRole,
			const string& trusted_role)
		{
			logins.populate(name, iDb);
			roles.populate(trusted_role, iDb);
			if (sqlRole)
				roles.populate(*sqlRole, iDb);
			pairs.populate(name, sqlRole, iDb);
		}

		void invalidate()
		{
			logins.invalidate();
			roles.invalidate();
			pairs.invalidate();
		}

	private:
		class NameCache : private GenericMap<Pair<Left<string, UserId::Privileges> > >
		{
		public:
			NameCache(MemoryPool& p, const char* s)
				: GenericMap(p),
				  sql(s)
			{ }

			bool getPrivileges(const string& key, UserId::Privileges& system_privileges)
			{
				if (!key.hasData())
					return false;

				UserId::Privileges p;
        		if (!get(key, p))
					return false;

				system_privileges |= p;
				return true;
			}

			void populate(const string& key, Mapping::DbHandle& iDb)
			{
				if (!key.hasData())
					return;

				if (get(key))
					return;

				ThrowLocalStatus st;
				RefPtr<ITransaction> tra(REF_NO_INCR, iDb->startTransaction(&st, 0, nullptr));

				Message par;
				Field<Varying> user(par, MAX_SQL_IDENTIFIER_SIZE);
				user = key.c_str();

				RefPtr<IResultSet> curs(REF_NO_INCR, iDb->openCursor(&st, tra, 0, sql, 3,
					par.getMetadata(), par.getBuffer(), nullptr, nullptr, 0));

				RefPtr<IMessageMetadata> meta(curs->getMetadata(&st));
				AutoPtr<UCHAR, ArrayDelete> buffer(FB_NEW UCHAR[meta->getMessageLength(&st)]);
				UCHAR* bits = buffer + meta->getOffset(&st, 0);
				UserId::Privileges g, l;

				while(curs->fetchNext(&st, buffer) == IStatus::RESULT_OK)
				{
					l.load(bits);
					g |= l;
				}

				ULONG gg = 0; g.store(&gg);
				MAP_DEBUG(fprintf(stderr, "poprole %s 0x%x\n", key.c_str(), gg));
				put(key, g);
			}

			void invalidate()
			{
				clear();
			}

		private:
			const char* sql;
		};

		class RoleCache : private GenericMap<Pair<Full<string, string> > >
		{
			static const char ROLESEP = '\1';

		public:
			RoleCache(MemoryPool& p)
				: GenericMap(p)
			{ }

			bool isRoleGranted(const string& name, const string* role, bool& granted)
			{
				if (!(name.hasData() && role))
				{
					granted = false;
					return true;
				}

				string* r = get(name);
				if (!r)
					return false;

				string zRole;
				zRole += ROLESEP;
				zRole += *role;
				zRole += ROLESEP;

				MAP_DEBUG(fprintf(stderr, "isRoleGranted '%s' '%s'\n", r->c_str(), role->c_str()));

				granted = r->find(zRole) != string::npos;
				return true;
			}

			void populate(const string& name, const string* /*role*/, Mapping::DbHandle& iDb)
			{
				MAP_DEBUG(fprintf(stderr, "populate %s\n", name.c_str()));
				if (!name.hasData())
					return;

				ThrowLocalStatus st;
				RefPtr<ITransaction> tra(REF_NO_INCR, iDb->startTransaction(&st, 0, nullptr));

				Message par;
				Field<Varying> user(par, MAX_SQL_IDENTIFIER_SIZE);
				user = name.c_str();

				Message cols;
				Field<Varying> role(cols, MAX_SQL_IDENTIFIER_SIZE);

				const char* sql = "select RDB$RELATION_NAME from RDB$USER_PRIVILEGES "
					"where RDB$USER = ? and RDB$PRIVILEGE = 'M' and RDB$USER_TYPE = 8 and RDB$OBJECT_TYPE = 13";

				RefPtr<IResultSet> curs(REF_NO_INCR, iDb->openCursor(&st, tra, 0, sql, 3,
					par.getMetadata(), par.getBuffer(), cols.getMetadata(), nullptr, 0));

				void* buffer = cols.getBuffer();
				string z;
				z += ROLESEP;

				while (curs->fetchNext(&st, buffer) == IStatus::RESULT_OK)
				{
					string r = (const char*) role;
					r.trim();
					MAP_DEBUG(fprintf(stderr, "populate 2 %s\n", r.c_str()));
					z += r;
					z += ROLESEP;
				}

				put(name, z);
			}

			void invalidate()
			{
				clear();
			}
		};

		NameCache logins, roles;
		RoleCache pairs;
	};

	SyncObject sync;
	GenericMap<Pair<Left<PathName, AutoPtr<DbCache> > > > databases;
};

InitInstance<SysPrivCache> spCache;

void resetMap(const char* db, ULONG index)
{
	if (index & Mapping::MAPPING_CACHE)
		resetMap(db);

	if (index & Mapping::SYSTEM_PRIVILEGES_CACHE)
		spCache().invalidate(db);
}

} // anonymous namespace

namespace Jrd {

Mapping::Mapping(const ULONG f, Firebird::ICryptKeyCallback* cryptCb)
	: flags(f),
	  internalFlags(0),
	  cryptCallback(cryptCb),
	  authMethod(nullptr),
	  newAuthBlock(nullptr),
	  systemPrivileges(nullptr),
	  authBlock(nullptr),
	  mainAlias(nullptr),
	  mainDb(nullptr),
	  securityAlias(nullptr),
	  errorMessagesContext(nullptr),
	  sqlRole(nullptr)
{ }

bool Mapping::ensureCachePresence(RefPtr<Mapping::Cache>& cache, const char* alias, const char* target,
	Mapping::DbHandle& hdb, ICryptKeyCallback* cryptCb, Mapping::Cache* c2)
{
	fb_assert(!cache);
	fb_assert(authBlock);

	if (!(authBlock && authBlock->hasData()))
		return false;

	MutexEnsureUnlock g(treeMutex, FB_FUNCTION);
	g.enter();

	// Find cache in the tree or create new one
	locate(cache, alias, target);
	fb_assert(cache);

	// If we use self security database no sense performing checks on it twice
	if (cache == c2)
	{
		cache = nullptr;
		return false;
	}

	// Required cache(s) are locked somehow - release treeMutex
	g.leave();

	// Std safe check for data presence in cache
	if (cache->dataFlag)
		return false;
	MutexLockGuard g2(cache->populateMutex, FB_FUNCTION);
	if (cache->dataFlag)
		return false;

	// Create db attachment if missing it and populate cache from it
	bool down = hdb.attach(alias, cryptCb) || cache->populate(hdb);
	if (down)
		cache = nullptr;
	return down;
}


void Mapping::needAuthMethod(Firebird::string& method)
{
	fb_assert(!authMethod);
	authMethod = &method;
}

void Mapping::needAuthBlock(Firebird::AuthReader::AuthBlock& block)
{
	fb_assert(!newAuthBlock);
	newAuthBlock = &block;
}

void Mapping::needSystemPrivileges(UserId::Privileges& privileges)
{
	fb_assert(!systemPrivileges);
	systemPrivileges = &privileges;
}

void Mapping::setAuthBlock(const Firebird::AuthReader::AuthBlock& block)
{
	fb_assert(!authBlock);
	authBlock = &block;
}

void Mapping::setInternalFlags()
{
	internalFlags &= ~(FLAG_DB | FLAG_SEC);

	if (!mainDb)
		internalFlags |= FLAG_DB;
	if (!securityAlias)
		internalFlags |= FLAG_SEC;

	// detect presence of non-plugin databases mapping in authBlock
	// in that case mapUser was already invoked for it
	if (authBlock)
	{
		AuthReader::Info info;
		for (AuthReader rdr(*authBlock); rdr.getInfo(info); rdr.moveNext())
		{
			MAP_DEBUG(fprintf(stderr, "info.plugin=%s info.secDb=%s db=%s securityDb=%s\n",
				info.plugin.c_str(), info.secDb.c_str(), mainDb ? mainDb : "NULL", secExpanded.c_str()));

			if (info.plugin.isEmpty())
			{
				if (mainDb && info.secDb == mainDb)
					internalFlags |= FLAG_DB;
				if (securityAlias && info.secDb == secExpanded.c_str())
					internalFlags |= FLAG_SEC;
			}
		}
	}
}

void Mapping::setSqlRole(const Firebird::string& role)
{
	fb_assert(!sqlRole);
	sqlRole = &role;
}

void Mapping::setDb(const char* a, const char* d, Firebird::IAttachment* attachment)
{
	fb_assert(!mainAlias);
	fb_assert(!mainDb);
	fb_assert(!mainHandle);
	fb_assert(authBlock);

	mainAlias = a;
	mainDb = d;
	mainHandle.setAttachment(attachment);
	setInternalFlags();

	if ((!(internalFlags & FLAG_DB)) &&
		ensureCachePresence(dbCache, mainAlias, mainDb, mainHandle, cryptCallback, secCache))
	{
		internalFlags |= FLAG_DOWN_DB;
	}
}

void Mapping::clearMainHandle()
{
	mainHandle.clear();
}

Mapping::~Mapping()
{
	MAP_DEBUG(if (mainHandle) {mainHandle->addRef(); int r = mainHandle->release();)
	MAP_DEBUG(fprintf(stderr, "~M:Drop MH with refcount %d\n", r);} else fprintf(stderr, "~M:No MH\n");)
}

void Mapping::setSecurityDbAlias(const char* a, const char* mainExpandedName)
{
	fb_assert(!securityAlias);
	fb_assert(authBlock);

	securityAlias = a;
	expandDatabaseName(securityAlias, secExpanded, nullptr);
	setInternalFlags();

	if (mainExpandedName && secExpanded == mainExpandedName)
		return;

	Mapping::DbHandle secHandle;
	if ((!(internalFlags & FLAG_SEC)) &&
		ensureCachePresence(secCache, securityAlias, secExpanded.c_str(), secHandle, cryptCallback, dbCache))
	{
		internalFlags |= FLAG_DOWN_SEC;
	}
}

void Mapping::setErrorMessagesContextName(const char* context)
{
	errorMessagesContext = context;
}


ULONG Mapping::mapUser(string& name, string& trustedRole)
{
	AuthReader::Info info;

	if (flags & MAP_ERROR_HANDLER)
	{
		// We are in the error handler - perform minimum processing
		trustedRole = "";
		name = "<Unknown>";

		if (authBlock)
		{
			for (AuthReader rdr(*authBlock); rdr.getInfo(info); rdr.moveNext())
			{
				if (info.type == NM_USER && info.name.hasData())
				{
					name = info.name.ToString();
					break;
				}
			}
		}

		return 0;
	}

	// Create new writer
	AuthWriter newBlock;

	// Map it only when needed
	if (authBlock && authBlock->hasData() && (dbCache || secCache))
	{
		ExtInfo info;

		// Caches are ready somehow - proceed with analysis
		AuthReader auth(*authBlock);

		// Map in simple mode first main, next security db
		if (!(dbCache && dbCache->map4(false, internalFlags & FLAG_DB, auth, info, newBlock)))
		{
			if (!(secCache && secCache->map4(false, internalFlags & FLAG_SEC, auth, info, newBlock)))
			{
				// Map in wildcard mode first main, next security db
				if (!(dbCache && dbCache->map4(true, internalFlags & FLAG_DB, auth, info, newBlock)))
				{
					if (secCache)
						secCache->map4(true, internalFlags & FLAG_SEC, auth, info, newBlock);
				}
			}
		}
	}

	for (AuthReader rdr(newBlock); rdr.getInfo(info); rdr.moveNext())
	{
		if (mainDb && info.secDb == mainDb)
			internalFlags |= FLAG_DB;
		if (info.secDb == secExpanded.c_str())
			internalFlags |= FLAG_SEC;
	}

	// mark both DBs as 'seen'
	info.plugin = "";
	info.name = "";
	info.type = TYPE_SEEN;

	if (!(internalFlags & FLAG_DB))
	{
		info.secDb = mainDb;
		newBlock.add(info);
	}

	if (!(internalFlags & FLAG_SEC))
	{
		info.secDb = secExpanded.c_str();
		newBlock.add(info);
	}

	newBlock.append(*authBlock);

	Found fName, fRole;
	MAP_DEBUG(fprintf(stderr, "Starting newblock scan\n"));
	for (AuthReader scan(newBlock); scan.getInfo(info); scan.moveNext())
	{
		MAP_DEBUG(fprintf(stderr, "Newblock info: secDb=%s plugin=%s type=%s name=%s origPlug=%s\n",
			info.secDb.c_str(), info.plugin.c_str(), info.type.c_str(), info.name.c_str(), info.origPlug.c_str()));

		Found::What recordWeight =
			(mainDb && info.secDb == mainDb) ? Found::FND_DB :
			(securityAlias && info.secDb == secExpanded.c_str()) ? Found::FND_SEC :
			Found::FND_NOTHING;

		if (recordWeight != Found::FND_NOTHING)
		{
			if (info.type == NM_USER)
				fName.set(recordWeight, info);
			else if (info.type == NM_ROLE)
				fRole.set(recordWeight, info);
		}
	}

	ULONG rc = (internalFlags & (FLAG_DOWN_DB | FLAG_DOWN_SEC)) ? MAP_DOWN : 0;
	if (fName.found == Found::FND_NOTHING)
	{
		if (flags & MAP_THROW_NOT_FOUND)
		{
			NoCaseString msg = "Missing security context required for ";
			if (mainDb)
				msg += mainDb;
			if (mainDb && securityAlias)
				msg += " or ";
			if (securityAlias)
				msg += secExpanded.c_str();

			msg += "\n\tAvailable context(s): ";
			bool fstCtx = true;
			for (AuthReader scan(newBlock); scan.getInfo(info); scan.moveNext())
			{
				if (info.type == NM_USER || info.type == NM_ROLE)
				{
					if (!fstCtx)
						msg += "\n\t\t";
					else
						fstCtx = false;

					msg += info.type;
					msg += ' ';
					msg += info.name;
					if (info.secDb.hasData())
					{
						msg += " in ";
						msg += info.secDb;
					}
					if (info.plugin.hasData())
					{
						msg += " plugin ";
						msg += info.plugin;
					}
				}
			}
			if (fstCtx)
				msg += "<none>";

			gds__log("%s", msg.c_str());

			Arg::Gds v(isc_sec_context);
			v << (errorMessagesContext ? errorMessagesContext : mainAlias ? mainAlias : "<unknown object>");
			if (rc & MAP_DOWN)
				v << Arg::Gds(isc_map_down);
			v.raise();
		}
		rc |= MAP_ERROR_NOT_THROWN;
	}
	else
	{
		name = fName.value.ToString();
		trustedRole = fRole.value.ToString();
		MAP_DEBUG(fprintf(stderr, "login=%s tr=%s\n", name.c_str(), trustedRole.c_str()));
		if (authMethod)
			*authMethod = fName.method.ToString();

		if (newAuthBlock)
		{
			newAuthBlock->shrink(0);
			newAuthBlock->push(newBlock.getBuffer(), newBlock.getBufferLength());
			MAP_DEBUG(fprintf(stderr, "Saved to newAuthBlock %u bytes\n",
				static_cast<unsigned>(newAuthBlock->getCount())));
		}
	}

	if (name.hasData() || trustedRole.hasData())
	{
		if (systemPrivileges && mainDb)
		{
			systemPrivileges->clearAll();

			Sync sync(spCache().getSync(), FB_FUNCTION);
			sync.lock(SYNC_SHARED);

			MAP_DEBUG(fprintf(stderr, "GP: name=%s sql=%s trusted=%s\n",
				name.c_str(), sqlRole ? sqlRole->c_str() : "<nullptr>", trustedRole.c_str()));

			if (!spCache().getPrivileges(mainDb, name, sqlRole, trustedRole, *systemPrivileges))
			{
				sync.unlock();
				sync.lock(SYNC_EXCLUSIVE);

				if (!spCache().getPrivileges(mainDb, name, sqlRole, trustedRole, *systemPrivileges))
				{
					mainHandle.attach(mainAlias, cryptCallback);

					if (mainHandle)
					{
						spCache().populate(mainDb, mainHandle, name, sqlRole, trustedRole);
						spCache().getPrivileges(mainDb, name, sqlRole, trustedRole, *systemPrivileges);
					}
				}
			}
		}
	}

	return rc;
}

void Mapping::clearCache(const char* dbName, USHORT index)
{
	mappingIpc->clearCache(dbName, index);
}


const Format* GlobalMappingScan::getFormat(thread_db* tdbb, jrd_rel* relation) const
{
	jrd_tra* const transaction = tdbb->getTransaction();
	return transaction->getMappingList()->getList(tdbb, relation)->getFormat();
}

bool GlobalMappingScan::retrieveRecord(thread_db* tdbb, jrd_rel* relation,
									FB_UINT64 position, Record* record) const
{
	jrd_tra* const transaction = tdbb->getTransaction();
	return transaction->getMappingList()->getList(tdbb, relation)->fetch(position, record);
}

MappingList::MappingList(jrd_tra* tra)
	: SnapshotData(*tra->tra_pool)
{ }

RecordBuffer* MappingList::makeBuffer(thread_db* tdbb)
{
	MemoryPool* const pool = tdbb->getTransaction()->tra_pool;
	allocBuffer(tdbb, *pool, rel_global_auth_mapping);
	return getData(rel_global_auth_mapping);
}

RecordBuffer* MappingList::getList(thread_db* tdbb, jrd_rel* relation)
{
	fb_assert(relation);
	fb_assert(relation->rel_id == rel_global_auth_mapping);

	RecordBuffer* buffer = getData(relation);
	if (buffer)
	{
		return buffer;
	}

	FbLocalStatus st;
	DispatcherPtr prov;
	IAttachment* att = nullptr;
	ITransaction* tra = nullptr;
	IResultSet* curs = nullptr;

	try
	{
		const char* dbName = tdbb->getDatabase()->dbb_config->getSecurityDatabase();

		ClumpletWriter embeddedSysdba(ClumpletWriter::dpbList, MAX_DPB_SIZE);
		embeddedSysdba.insertString(isc_dpb_user_name, DBA_USER_NAME, fb_strlen(DBA_USER_NAME));
		embeddedSysdba.insertByte(isc_dpb_sec_attach, TRUE);
		embeddedSysdba.insertString(isc_dpb_config, ParsedList::getNonLoopbackProviders(dbName));
		embeddedSysdba.insertByte(isc_dpb_no_db_triggers, TRUE);

		att = prov->attachDatabase(&st, dbName,
			embeddedSysdba.getBufferLength(), embeddedSysdba.getBuffer());
		if (st->getState() & IStatus::STATE_ERRORS)
		{
			if (!fb_utils::containsErrorCode(st->getErrors(), isc_io_error))
				check("IProvider::attachDatabase", &st);

			// In embedded mode we are not raising any errors - silent return
			if (MasterInterfacePtr()->serverMode(-1) < 0)
				return makeBuffer(tdbb);

			(Arg::Gds(isc_map_nodb) << dbName).raise();
		}

		ClumpletWriter readOnly(ClumpletWriter::Tpb, MAX_DPB_SIZE, isc_tpb_version1);
		readOnly.insertTag(isc_tpb_read);
		readOnly.insertTag(isc_tpb_wait);
		tra = att->startTransaction(&st, readOnly.getBufferLength(), readOnly.getBuffer());
		check("IAttachment::startTransaction", &st);

		Message mMap;
		Field<Varying> name(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Text> usng(mMap, 1);
		Field<Varying> plugin(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> db(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> fromType(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<Varying> from(mMap, 255);
		Field<SSHORT> role(mMap);
		Field<Varying> to(mMap, MAX_SQL_IDENTIFIER_SIZE);
		Field<ISC_QUAD> desc(mMap);

		curs = att->openCursor(&st, tra, 0,
			"SELECT RDB$MAP_NAME, RDB$MAP_USING, RDB$MAP_PLUGIN, RDB$MAP_DB, "
			"	RDB$MAP_FROM_TYPE, RDB$MAP_FROM, RDB$MAP_TO_TYPE, RDB$MAP_TO, RDB$DESCRIPTION "
			"FROM RDB$AUTH_MAPPING",
			3, nullptr, nullptr, mMap.getMetadata(), nullptr, 0);
		if (st->getState() & IStatus::STATE_ERRORS)
		{
			if (!fb_utils::containsErrorCode(st->getErrors(), isc_dsql_relation_err))
				check("IAttachment::openCursor", &st);

			// isc_dsql_relation_err when opening cursor - i.e. table RDB$AUTH_MAPPING
			// is missing due to non-FB3 security DB
			tra->release();
			att->detach(&st);

			// In embedded mode we are not raising any errors - silent return
			if (MasterInterfacePtr()->serverMode(-1) < 0)
				return makeBuffer(tdbb);

			(Arg::Gds(isc_map_notable) << dbName).raise();
		}

		buffer = makeBuffer(tdbb);
		Record* record = buffer->getTempRecord();

		while (curs->fetchNext(&st, mMap.getBuffer()) == IStatus::RESULT_OK)
		{
			record->nullify();

			putField(tdbb, record,
					 DumpField(f_sec_map_name, VALUE_STRING, name->len, name->data));

			putField(tdbb, record,
					 DumpField(f_sec_map_using, VALUE_STRING, 1, usng->data));

			if (!plugin.null)
			{
				putField(tdbb, record,
						 DumpField(f_sec_map_plugin, VALUE_STRING, plugin->len, plugin->data));
			}

			if (!db.null)
			{
				putField(tdbb, record,
						 DumpField(f_sec_map_db, VALUE_STRING, db->len, db->data));
			}

			if (!fromType.null)
			{
				putField(tdbb, record,
						 DumpField(f_sec_map_from_type, VALUE_STRING, fromType->len, fromType->data));
			}

			if (!from.null)
			{
				putField(tdbb, record,
						 DumpField(f_sec_map_from, VALUE_STRING, from->len, from->data));
			}

			if (!role.null)
			{
				SINT64 v = role;
				putField(tdbb, record,
						 DumpField(f_sec_map_to_type, VALUE_INTEGER, sizeof(v), &v));
			}

			if (!to.null)
			{
				putField(tdbb, record,
						 DumpField(f_sec_map_to, VALUE_STRING, to->len, to->data));
			}

			if (!desc.null)
			{
				RefPtr<IBlob> blb(REF_NO_INCR, att->openBlob(&st, tra, &desc, 0, nullptr));
				check("IAttachment::openBlob", &st);
				string buf;
				const FB_SIZE_T FLD_LIMIT = MAX_COLUMN_SIZE;
				unsigned length = 0;
				blb->getSegment(&st, FLD_LIMIT, buf.getBuffer(FLD_LIMIT), &length);
				check("IBlob::getSegment", &st);

				putField(tdbb, record,
					DumpField(f_sec_map_comment, VALUE_STRING, length, buf.c_str()));
			}

			buffer->store(record);
		}
		check("IResultSet::fetchNext", &st);

		curs->close(&st);
		check("IResultSet::close", &st);
		curs = nullptr;

		tra->rollback(&st);
		check("ITransaction::rollback", &st);
		tra = nullptr;

		att->detach(&st);
		check("IAttachment::detach", &st);
		att = nullptr;
	}
	catch (const Exception&)
	{
		if (curs)
			curs->release();
		if (tra)
			tra->release();
		if (att)
			att->detach(&st);

		clearSnapshot();
		throw;
	}

	return getData(relation);
}

void Mapping::shutdownIpc()
{
	mappingIpc->shutdown();
}

} // namespace Jrd
