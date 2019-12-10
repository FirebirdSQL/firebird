/*
 *	PROGRAM:		JRD access method
 *	MODULE:			Mapping.h
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

#ifndef JRD_MAPPING
#define JRD_MAPPING

#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/Hash.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/Monitoring.h"
#include "../jrd/scl.h"

namespace Jrd {

class AuthWriter;

class Mapping
{
public:
	// constructor's flags
	static const ULONG MAP_NO_FLAGS = 0;
	static const ULONG MAP_THROW_NOT_FOUND = 1;
	static const ULONG MAP_ERROR_HANDLER = 2;
	Mapping(const ULONG flags, Firebird::ICryptKeyCallback* cryptCb);

	~Mapping();

	// First provide the main input information - old auth block ...
	void setAuthBlock(const Firebird::AuthReader::AuthBlock& authBlock);

	// ... and finally specify additional information when available.
	void setSqlRole(const Firebird::string& sqlRole);
	void setDb(const char* alias, const char* db, Firebird::IAttachment* att);
	void setSecurityDbAlias(const char* alias, const char* mainExpandedName);
	void setErrorMessagesContextName(const char* context);

	// This should be done before mapUser().
	void needAuthMethod(Firebird::string& authMethod);
	void needAuthBlock(Firebird::AuthReader::AuthBlock& newAuthBlock);
	void needSystemPrivileges(UserId::Privileges& systemPrivileges);

	// bits returned by mapUser
	static const ULONG MAP_ERROR_NOT_THROWN = 1;
	static const ULONG MAP_DOWN = 2;
	// Now mapper is ready to perform main task and provide mapped login and trusted role.
	ULONG mapUser(Firebird::string& name, Firebird::string& trustedRole);

	// Do not keep mainHandle opened longer than needed
	void clearMainHandle();

	// possible clearCache() flags
	static const USHORT MAPPING_CACHE = 0;
	static const USHORT SYSTEM_PRIVILEGES_CACHE = 1;
	// Helper statuc functions to perform cleanup & shutdown.
	static void clearCache(const char* dbName, USHORT id);
	static void shutdownIpc();

private:
	const ULONG flags;
	ULONG internalFlags;
	Firebird::ICryptKeyCallback* cryptCallback;
	Firebird::string* authMethod;
	Firebird::AuthReader::AuthBlock* newAuthBlock;
	UserId::Privileges* systemPrivileges;
	const Firebird::AuthReader::AuthBlock* authBlock;
	const char* mainAlias;
	const char* mainDb;
	const char* securityAlias;
	const char* errorMessagesContext;
	const Firebird::string* sqlRole;

public:
	struct ExtInfo : public Firebird::AuthReader::Info
	{
		Firebird::NoCaseString currentRole, currentUser;
	};

	class DbHandle : public Firebird::RefPtr<Firebird::IAttachment>
	{
	public:
		DbHandle();
		void setAttachment(Firebird::IAttachment* att);
		void clear();
		bool attach(const char* aliasDb, Firebird::ICryptKeyCallback* cryptCb);
	};

	class Map;
	typedef Firebird::HashTable<Map, Firebird::DEFAULT_HASH_SIZE, Map, Firebird::DefaultKeyValue<Map>, Map> MapHash;

	class Map : public MapHash::Entry, public Firebird::GlobalStorage
	{
	public:
		Map(const char* aUsing, const char* aPlugin, const char* aDb,
			const char* aFromType, const char* aFrom,
			SSHORT aRole, const char* aTo);
		explicit Map(Firebird::AuthReader::Info& info);		//type, name, plugin, secDb

		static FB_SIZE_T hash(const Map& value, FB_SIZE_T hashSize);
		Firebird::NoCaseString makeHashKey() const;
		void trimAll();
		virtual bool isEqual(const Map& k) const;
		virtual Map* get();

		Firebird::NoCaseString plugin, db, fromType, from, to;
		bool toRole;
		char usng;
	};

	class Cache : public MapHash, public Firebird::GlobalStorage, public Firebird::RefCounted
	{
	public:
		Cache(const Firebird::NoCaseString& aliasDb, const Firebird::NoCaseString& db);
		~Cache();

		bool populate(Firebird::IAttachment *att);
		void map(bool flagWild, ExtInfo& info, AuthWriter& newBlock);
		void search(ExtInfo& info, const Map& from, AuthWriter& newBlock,
			const Firebird::NoCaseString& originalUserName);
		void varPlugin(ExtInfo& info, Map from, AuthWriter& newBlock);
		void varDb(ExtInfo& info, Map from, AuthWriter& newBlock);
		void varFrom(ExtInfo& info, Map from, AuthWriter& newBlock);
		void varUsing(ExtInfo& info, Map from, AuthWriter& newBlock);
		bool map4(bool flagWild, unsigned flagSet, Firebird::AuthReader& rdr,
			ExtInfo& info, AuthWriter& newBlock);
		static void eraseEntry(Map* m);

	public:
		Firebird::Mutex populateMutex;
		Firebird::NoCaseString alias, name;
		bool dataFlag;
	};

private:
	Firebird::PathName secExpanded;
	Firebird::RefPtr<Cache> dbCache, secCache;
	DbHandle mainHandle;

	void setInternalFlags();
	bool ensureCachePresence(Firebird::RefPtr<Mapping::Cache>& cache, const char* alias,
		const char* target, DbHandle& hdb, Firebird::ICryptKeyCallback* cryptCb, Cache* c2);
};

class GlobalMappingScan: public VirtualTableScan
{
public:
	GlobalMappingScan(CompilerScratch* csb, const Firebird::string& alias,
					  StreamType stream, jrd_rel* relation)
		: VirtualTableScan(csb, alias, stream, relation)
	{}

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position,
		Record* record) const override;
};

class MappingList : public SnapshotData
{
public:
	explicit MappingList(jrd_tra* tra);

	RecordBuffer* getList(thread_db* tdbb, jrd_rel* relation);

private:
	RecordBuffer* makeBuffer(thread_db* tdbb);
};

} // namespace Jrd


#endif // JRD_MAPPING
