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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef EXTDS_H
#define EXTDS_H

#include "../../common/classes/fb_string.h"
#include "../../common/classes/array.h"
#include "../../common/classes/objects_array.h"
#include "../../common/classes/ClumpletWriter.h"
#include "../../common/classes/locks.h"
#include "../../common/utils_proto.h"


namespace Jrd
{
	class jrd_tra;
	class thread_db;
	class ValueListNode;
}

namespace EDS {

class Manager;
class Provider;
class Connection;
class ConnectionsPool;
class Transaction;
class Statement;
class Blob;

enum TraModes {
	traReadCommited,
	traReadCommitedRecVersions,
	traReadCommitedReadConsistency,
	traConcurrency,
	traConsistency
};

enum TraScope {
	traNotSet = 0,
	traAutonomous,
	traCommon,
	traTwoPhase
};

// Known built-in provider's names
extern const char* FIREBIRD_PROVIDER_NAME;
extern const char* INTERNAL_PROVIDER_NAME;


// Manage providers
class Manager : public Firebird::PermanentStorage
{
public:
	explicit Manager(Firebird::MemoryPool& pool);
	~Manager();

	static void addProvider(Provider* provider);
	static Provider* getProvider(const Firebird::string& prvName);
	static Connection* getConnection(Jrd::thread_db* tdbb,
		const Firebird::string& dataSource, const Firebird::string& user,
		const Firebird::string& pwd, const Firebird::string& role, TraScope tra_scope);

	static ConnectionsPool* getConnPool(bool create);

	// Release bound external connections when some jrd attachment is about to be released
	static void jrdAttachmentEnd(Jrd::thread_db* tdbb, Jrd::Attachment* att, bool forced);

	static int shutdown();

private:
	static Firebird::GlobalPtr<Manager> manager;
	static Firebird::Mutex m_mutex;
	static Provider* m_providers;
	static ConnectionsPool* m_connPool;
};


// manages connections

class Provider : public Firebird::GlobalStorage
{
	friend class Manager;
	friend class EngineCallbackGuard;

public:
	explicit Provider(const char* prvName);

	// create new Connection
	virtual Connection* createConnection(Jrd::thread_db* tdbb,
		const Firebird::PathName& dbName, Firebird::ClumpletReader& dpb,
		TraScope tra_scope);

	// bind connection to the current attachment
	void bindConnection(Jrd::thread_db* tdbb, Connection* conn);

	// get available connection already bound to the current attachment
	Connection* getBoundConnection(Jrd::thread_db* tdbb,
		const Firebird::PathName& dbName, Firebird::ClumpletReader& dpb,
		TraScope tra_scope);

	// Connection gets unused, release it into pool or delete it immediately
	virtual void releaseConnection(Jrd::thread_db* tdbb, Connection& conn, bool inPool = true);

	// release connections bound to the attachment
	virtual void jrdAttachmentEnd(Jrd::thread_db* tdbb, Jrd::Attachment* att, bool forced);

	// cancel execution of every connection
	void cancelConnections();

	const Firebird::string& getName() const { return m_name; }

	virtual void initialize() = 0;

	// Provider properties
	int getFlags() const { return m_flags; }

	// Interprete status and put error description into passed string
	virtual void getRemoteError(const Jrd::FbStatusVector* status, Firebird::string& err) const = 0;

	static const Firebird::string* generate(const Provider* item)
	{
		return &item->m_name;
	}

protected:
	virtual ~Provider();
	void clearConnections(Jrd::thread_db* tdbb);
	virtual Connection* doCreateConnection() = 0;

	void generateDPB(Jrd::thread_db* tdbb, Firebird::ClumpletWriter& dpb,
		const Firebird::string& user, const Firebird::string& pwd,
		const Firebird::string& role) const;

	// Protection against simultaneous attach database calls. Not sure we still
	// need it, but i believe it will not harm
	Firebird::Mutex m_mutex;

	Firebird::string m_name;
	Provider* m_next;

	class AttToConn
	{
	public:
		Jrd::Attachment* m_att;
		Connection* m_conn;

		AttToConn()
			: m_att(NULL),
			  m_conn(NULL)
		{}

		AttToConn(Jrd::Attachment* att, Connection* conn)
			: m_att(att),
			  m_conn(conn)
		{}

		static const AttToConn& generate(const void*, const AttToConn& item)
		{
			return item;
		}

		static bool greaterThan(const AttToConn& i1, const AttToConn& i2)
		{
			return (i1.m_att > i2.m_att) ||
				(i1.m_att == i2.m_att && i1.m_conn > i2.m_conn);
		}
	};

	typedef Firebird::BePlusTree<AttToConn, AttToConn, Firebird::MemoryPool,
								 AttToConn, AttToConn>
		AttToConnMap;

	AttToConnMap m_connections;
	int m_flags;
};

// Provider flags
const int prvTrustedAuth	= 0x0001;	// supports trusted authentication


class ConnectionsPool
{
public:
	ConnectionsPool(Firebird::MemoryPool& pool);
	~ConnectionsPool();

	// find and return cached connection or NULL
	Connection* getConnection(Jrd::thread_db* tdbb, Provider* prv, ULONG hash, const Firebird::PathName& dbName,
		Firebird::ClumpletReader& dpb);

	// put unused connection into pool or destroy it
	void putConnection(Jrd::thread_db* tdbb, Connection* conn);

	// assotiate new active connection with pool
	void addConnection(Jrd::thread_db* tdbb, Connection* conn, ULONG hash);

	// clear connection relation with pool
	void delConnection(Jrd::thread_db* tdbb, Connection* conn, bool destroy);

	ULONG getIdleCount() const { return m_idleArray.getCount(); }
	ULONG getAllCount() const { return m_allCount; } ;

	ULONG getMaxCount() const { return m_maxCount; }
	void setMaxCount(ULONG val);

	ULONG getLifeTime() const	{ return m_lifeTime; }
	void setLifeTime(ULONG val);

	// delete idle connections: all or older than lifetime
	void clearIdle(Jrd::thread_db* tdbb, bool all);

	// delete all idle connections, remove from pool all active connections
	void clear(Jrd::thread_db* tdbb);

	// return time when oldest idle connection should be released, or zero
	time_t getIdleExpireTime();

	// verify bound connection internals
	static bool checkBoundConnection(Jrd::thread_db* tdbb, Connection* conn);

public:
	// this class is embedded into Connection but managed by ConnectionsPool
	class Data
	{
	public:
		// constructor for embedded into Connection instance
		explicit Data(Connection* conn)
		{
			clear();
			m_conn = conn;
		}

		ConnectionsPool* getConnPool() const { return m_connPool; }

		static const Data& generate(const Data* item)
		{
			return *item;
		}

		static bool greaterThan(const Data& i1, const Data& i2)
		{
			if (i1.m_hash == i2.m_hash)
			{
				if (i1.m_lastUsed == i2.m_lastUsed)
					return &i1 > &i2;

				return (i1.m_lastUsed < i2.m_lastUsed);
			}

			return (i1.m_hash > i2.m_hash);
		}

	private:
		friend class ConnectionsPool;

		ConnectionsPool* m_connPool;
		Connection* m_conn;
		ULONG m_hash;
		time_t m_lastUsed;

		// placement in connections list
		Data* m_next;
		Data* m_prev;

		Data(const Data&);
		Data& operator=(const Data&);

		// create instance used to search for recently used connection by hash
		explicit Data(ULONG hash)
		{
			clear();
			m_conn = NULL;
			m_hash = hash;
			m_lastUsed = MAX_SINT64;
		}

		void clear()
		{
			m_connPool = NULL;
			// m_conn = NULL;
			m_hash = 0;
			m_lastUsed = 0;
			m_next = m_prev = NULL;
		}

		void setConnPool(ConnectionsPool *connPool)
		{
			fb_assert(!connPool || !m_connPool);
			m_connPool = connPool;
		}

		Firebird::string print();
		int verify(ConnectionsPool *connPool, bool active);
	};

private:
	class IdleTimer final :
		public Firebird::RefCntIface<Firebird::ITimerImpl<IdleTimer, Firebird::CheckStatusWrapper> >
	{
	public:
		explicit IdleTimer(ConnectionsPool& connPool)
			: m_connPool(connPool),
			  m_time(0)
		{}

		// ITimer implementation
		void handler();

		void start();
		void stop();

	private:
		ConnectionsPool& m_connPool;
		Firebird::Mutex m_mutex;
		time_t m_time;					// time when timer should fire, or zero
	};

	void addToList(Data** head, Data* item)
	{
		fb_assert(item->m_next == NULL);
		fb_assert(item->m_prev == NULL);
		fb_assert(head == (item->m_lastUsed ? &m_idleList : &m_activeList));

		if (*head)
		{
			item->m_next = (*head);
			item->m_prev = (*head)->m_prev;

			item->m_next->m_prev = item;
			item->m_prev->m_next = item;
		}
		else
		{
			item->m_next = item;
			item->m_prev = item;
		}

		*head = item;
	}

	void removeFromList(Data** head, Data* item)
	{
		if (!item->m_next)
			return;

		fb_assert(head == (item->m_lastUsed ? &m_idleList : &m_activeList));

		if (item->m_next != item)
		{
			item->m_next->m_prev = item->m_prev;
			item->m_prev->m_next = item->m_next;
			if (*head == item)
				*head = item->m_next;
		}
		else
		{
			fb_assert((*head) == item);
			*head = NULL;
		}

		item->m_next = item->m_prev = NULL;
	}

	void removeFromPool(Data* item, FB_SIZE_T pos);
	Data* removeOldest();

	void printPool(Firebird::string& s);
	bool verifyPool();

	// Array of Data*, sorted by [hash, lastUsed desc]
	typedef Firebird::SortedArray<Data*, Firebird::EmptyStorage<Data*>, Data, Data, Data>
		IdleArray;

	Firebird::MemoryPool& m_pool;
	Firebird::Mutex m_mutex;
	IdleArray m_idleArray;
	Data* m_idleList;
	Data* m_activeList;
	ULONG m_allCount;
	ULONG m_maxCount;
	ULONG m_lifeTime;	// How long idle connection should wait before destroying, seconds
	Firebird::RefPtr<IdleTimer> m_timer;
};


class Connection : public Firebird::PermanentStorage
{
protected:
	friend class EngineCallbackGuard;
	friend class Provider;

	// only Provider could create, setup and delete Connections

	explicit Connection(Provider& prov);
	virtual ~Connection();

	static void deleteConnection(Jrd::thread_db* tdbb, Connection* conn);
	void setup(const Firebird::PathName& dbName, const Firebird::ClumpletReader& dpb);

	void setBoundAtt(Jrd::Attachment* att) { m_boundAtt = att; }

public:
	Provider* getProvider() { return &m_provider; }

	Jrd::Attachment* getBoundAtt() const { return m_boundAtt; }

	ConnectionsPool* getConnPool() { return m_poolData.getConnPool(); }
	ConnectionsPool::Data* getPoolData() { return &m_poolData; }

	virtual void attach(Jrd::thread_db* tdbb) = 0;
	virtual void detach(Jrd::thread_db* tdbb);

	virtual bool cancelExecution(bool forced) = 0;

	// Try to reset connection, return true if it can be pooled
	virtual bool resetSession(Jrd::thread_db* tdbb) = 0;

	int getSqlDialect() const { return m_sqlDialect; }

	// Is this connections can be used by current needs ? Not every DBMS
	// allows to use same connection in more than one transaction and\or
	// to have more than on active statement at time. See also provider
	// flags above.
	virtual bool isAvailable(Jrd::thread_db* tdbb, TraScope traScope) const = 0;

	virtual bool isConnected() const = 0;
	virtual bool validate(Jrd::thread_db* tdbb) = 0;

	virtual bool isSameDatabase(const Firebird::PathName& dbName,
		Firebird::ClumpletReader& dpb) const;

	// only Internal provider is able to create "current" connections
	virtual bool isCurrent() const { return false; }

	bool isBroken() const
	{
		return m_broken;
	}

	// Search for existing transaction of given scope, may return NULL.
	Transaction* findTransaction(Jrd::thread_db* tdbb, TraScope traScope) const;

	const Firebird::string getDataSourceName() const
	{
		return m_provider.getName() + "::" + m_dbName.ToString();
	}

	// Get error description from provider and put it with additional context
	// info into locally raised exception
	void raise(const Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, const char* sWhere);

	// will we wrap external errors into our ones (isc_eds_xxx) or pass them as is
	bool getWrapErrors(const ISC_STATUS* status);
	void setWrapErrors(bool val) { m_wrapErrors = val; }

	// Transactions management within connection scope : put newly created
	// transaction into m_transactions array and delete not needed transaction
	// immediately (as we didn't pool transactions)
	Transaction* createTransaction();
	void deleteTransaction(Jrd::thread_db* tdbb, Transaction* tran);

	// Statements management within connection scope : put newly created
	// statement into m_statements array, but don't delete freed statement
	// immediately (as we did pooled statements). Instead keep it in
	// m_freeStatements list for reuse later
	Statement* createStatement(const Firebird::string& sql);
	void releaseStatement(Jrd::thread_db* tdbb, Statement* stmt);

	virtual Blob* createBlob() = 0;

	// Test specified feature flag
	bool testFeature(info_features value) const { return m_features[value]; }
	// Set specified flag
	void setFeature(info_features value) { m_features[value] = true; }
	// Clear specified flag
	void clearFeature(info_features value) { m_features[value] = false; }

protected:
	virtual Transaction* doCreateTransaction() = 0;
	virtual Statement* doCreateStatement() = 0;

	void clearTransactions(Jrd::thread_db* tdbb);
	void clearStatements(Jrd::thread_db* tdbb);

	virtual void doDetach(Jrd::thread_db* tdbb) = 0;

	// Protection against simultaneous ISC API calls for the same connection
	Firebird::Mutex m_mutex;

	Provider& m_provider;
	Firebird::PathName m_dbName;
	Firebird::UCharBuffer m_dpb;
	Jrd::Attachment* m_boundAtt;

	Firebird::Array<Transaction*> m_transactions;
	Firebird::Array<Statement*> m_statements;
	Statement* m_freeStatements;

	ConnectionsPool::Data m_poolData;

	static const int MAX_CACHED_STMTS = 16;
	int	m_used_stmts;
	int	m_free_stmts;
	bool m_deleting;
	int m_sqlDialect;	// must be filled in attach call
	bool m_wrapErrors;
	bool m_broken;
	bool m_features[fb_feature_max];
};

class Transaction : public Firebird::PermanentStorage
{
protected:
	friend class Connection;

	// Create and delete only via parent Connection
	explicit Transaction(Connection& conn);
	virtual ~Transaction();

public:

	Provider* getProvider() { return &m_provider; }

	Connection* getConnection() { return &m_connection; }

	TraScope getScope() const { return m_scope; }

	virtual void start(Jrd::thread_db* tdbb, TraScope traScope, TraModes traMode,
		bool readOnly, bool wait, int lockTimeout);
	virtual void prepare(Jrd::thread_db* tdbb, int info_len, const char* info);
	virtual void commit(Jrd::thread_db* tdbb, bool retain);
	virtual void rollback(Jrd::thread_db* tdbb, bool retain);

	static Transaction* getTransaction(Jrd::thread_db* tdbb,
		Connection* conn, TraScope tra_scope);

	// Notification about end of some jrd transaction. Bound external transaction
	// (with traCommon scope) must be ended the same way as local jrd transaction
	static void jrdTransactionEnd(Jrd::thread_db* tdbb, Jrd::jrd_tra* tran,
		bool commit, bool retain, bool force);

protected:
	virtual void generateTPB(Jrd::thread_db* tdbb, Firebird::ClumpletWriter& tpb,
		TraModes traMode, bool readOnly, bool wait, int lockTimeout) const;
	void detachFromJrdTran();

	virtual void doStart(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, Firebird::ClumpletWriter& tpb) = 0;
	virtual void doPrepare(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, int info_len, const char* info) = 0;
	virtual void doCommit(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain) = 0;
	virtual void doRollback(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain) = 0;

	Provider& m_provider;
	Connection& m_connection;
	TraScope m_scope;
	Transaction* m_nextTran;		// next common transaction
	Firebird::RefPtr<Jrd::JTransaction> m_jrdTran;		// parent JRD transaction
};


typedef Firebird::Array<const Jrd::MetaName*> ParamNames;
typedef Firebird::Array<USHORT> ParamNumbers;

class Statement : public Firebird::PermanentStorage
{
protected:
	friend class Connection;

	// Create and delete only via parent Connection
	explicit Statement(Connection& conn);
	virtual ~Statement();

public:
	static void deleteStatement(Jrd::thread_db* tdbb, Statement* stmt);

	Provider* getProvider() { return &m_provider; }

	Connection* getConnection() { return &m_connection; }

	Transaction* getTransaction() { return m_transaction; }

	void prepare(Jrd::thread_db* tdbb, Transaction* tran, const Firebird::string& sql, bool named);
	void setTimeout(Jrd::thread_db* tdbb, unsigned int timeout);
	void execute(Jrd::thread_db* tdbb, Transaction* tran,
		const Jrd::MetaName* const* in_names, const Jrd::ValueListNode* in_params,
		const ParamNumbers* in_excess, const Jrd::ValueListNode* out_params);
	void open(Jrd::thread_db* tdbb, Transaction* tran,
		const Jrd::MetaName* const* in_names, const Jrd::ValueListNode* in_params,
		const ParamNumbers* in_excess, bool singleton);
	bool fetch(Jrd::thread_db* tdbb, const Jrd::ValueListNode* out_params);
	void close(Jrd::thread_db* tdbb, bool invalidTran = false);
	void deallocate(Jrd::thread_db* tdbb);

	const Firebird::string& getSql() const { return m_sql; }

	void setCallerPrivileges(bool use) { m_callerPrivileges = use; }

	bool isActive() const { return m_active; }

	bool isAllocated() const { return m_allocated; }

	bool isSelectable() const { return m_stmt_selectable; }

	unsigned int getInputs() const { return m_inputs; }

	unsigned int getOutputs() const { return m_outputs; }

	// Get error description from provider and put it with additional contex
	// info into locally raised exception
	void raise(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, const char* sWhere,
		const Firebird::string* sQuery = NULL);

	// Active statement must be bound to parent jrd request
	void bindToRequest(Jrd::Request* request, Statement** impure);
	void unBindFromRequest();

protected:
	virtual void doPrepare(Jrd::thread_db* tdbb, const Firebird::string& sql) = 0;
	virtual void doSetTimeout(Jrd::thread_db* tdbb, unsigned int timeout) = 0;
	virtual void doExecute(Jrd::thread_db* tdbb) = 0;
	virtual void doOpen(Jrd::thread_db* tdbb) = 0;
	virtual bool doFetch(Jrd::thread_db* tdbb) = 0;
	virtual void doClose(Jrd::thread_db* tdbb, bool drop) = 0;

	void setInParams(Jrd::thread_db* tdbb, const Jrd::MetaName* const* names,
		const Jrd::ValueListNode* params, const ParamNumbers* in_excess);
	virtual void getOutParams(Jrd::thread_db* tdbb, const Jrd::ValueListNode* params);

	virtual void doSetInParams(Jrd::thread_db* tdbb, unsigned int count,
		const Firebird::MetaString* const* names, const NestConst<Jrd::ValueExprNode>* params);

	virtual void putExtBlob(Jrd::thread_db* tdbb, dsc& src, dsc& dst);
	virtual void getExtBlob(Jrd::thread_db* tdbb, const dsc& src, dsc& dst);

	// Preprocess user sql string : replace parameter names by placeholders (?)
	// and remember correspondence between logical parameter names and unnamed
	// placeholders numbers. This is needed only if provider didn't support
	// named parameters natively.
	void preprocess(const Firebird::string& sql, Firebird::string& ret);
	void clearNames();


	Provider	&m_provider;
	Connection	&m_connection;
	Transaction	*m_transaction;

	Statement* m_nextFree;		// next free statement

	Jrd::Request* m_boundReq;
	Statement** m_ReqImpure;
	Statement* m_nextInReq;
	Statement* m_prevInReq;

	Firebird::string m_sql;

	// passed in open()
	bool	m_singleton;

	// set in open()
	bool	m_active;

	// set in fetch()
	bool	m_fetched;

	// if statement executed in autonomous transaction, it must be rolled back,
	// so track the error condition of a statement
	bool	m_error;

	// set in prepare()
	bool	m_allocated;
	bool	m_stmt_selectable;
	unsigned int m_inputs;
	unsigned int m_outputs;

	bool	m_callerPrivileges;
	Jrd::Request* m_preparedByReq;

	// set in preprocess
	Firebird::SortedObjectsArray<const Firebird::MetaString> m_sqlParamNames;
	Firebird::Array<const Firebird::MetaString*> m_sqlParamsMap;

	// set in prepare()
	Firebird::UCharBuffer m_in_buffer;
	Firebird::UCharBuffer m_out_buffer;
	Firebird::Array<dsc> m_inDescs;
	Firebird::Array<dsc> m_outDescs;
};


class Blob : public Firebird::PermanentStorage
{
	friend class Connection;
protected:
	explicit Blob(Connection& conn) :
		 PermanentStorage(conn.getProvider()->getPool())
	{}

public:
	virtual ~Blob() {}

	virtual void open(Jrd::thread_db* tdbb, Transaction& tran, const dsc& desc,
		const Firebird::UCharBuffer* bpb) = 0;
	virtual void create(Jrd::thread_db* tdbb, Transaction& tran, dsc& desc,
		const Firebird::UCharBuffer* bpb) = 0;
	virtual USHORT read(Jrd::thread_db* tdbb, UCHAR* buff, USHORT len) = 0;
	virtual void write(Jrd::thread_db* tdbb, const UCHAR* buff, USHORT len) = 0;
	virtual void close(Jrd::thread_db* tdbb) = 0;
	virtual void cancel(Jrd::thread_db* tdbb) = 0;
};


class EngineCallbackGuard
{
public:
	EngineCallbackGuard(Jrd::thread_db* tdbb, Connection& conn, const char* from)
	{
		init(tdbb, conn, from);
	}

	EngineCallbackGuard(Jrd::thread_db* tdbb, Transaction& tran, const char* from)
	{
		init(tdbb, *tran.getConnection(), from);
	}

	EngineCallbackGuard(Jrd::thread_db* tdbb, Statement& stmt, const char* from)
	{
		init(tdbb, *stmt.getConnection(), from);
	}

	~EngineCallbackGuard();

private:
	void init(Jrd::thread_db* tdbb, Connection& conn, const char* from);

	Jrd::thread_db* m_tdbb;
	Firebird::RefPtr<Jrd::StableAttachmentPart> m_stable;
	Firebird::Mutex* m_mutex;
	Connection* m_saveConnection;
};

} // namespace EDS

#endif // EXTDS_H
