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
 *  The Original Code was created by Vladyslav Khorsun for the
 *  Firebird Open Source RDBMS project and based on execute_statement
 *	module by Alexander Peshkoff.
 *
 *  Copyright (c) 2007 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "fb_types.h"
#include "../../include/fb_blk.h"
#include "fb_exception.h"
#include "iberror.h"

#include "../../common/classes/Hash.h"
#include "../../common/config/config.h"
#include "../../dsql/chars.h"
#include "../../dsql/ExprNodes.h"
#include "../common/dsc.h"
#include "../exe.h"
#include "ExtDS.h"
#include "../jrd.h"
#include "../tra.h"

#include "../blb_proto.h"
#include "../exe_proto.h"
#include "../err_proto.h"
#include "../evl_proto.h"
#include "../intl_proto.h"
#include "../mov_proto.h"


#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef WIN_NT
#include <process.h>
#include <windows.h>
#endif

//#define EDS_DEBUG

#ifdef EDS_DEBUG

#undef FB_ASSERT_FAILURE_STRING
#define FB_ASSERT_FAILURE_STRING	"Procces ID %d: assertion (%s) failure: %s %" LINEFORMAT

#undef fb_assert
#define fb_assert(ex)	{if (!(ex)) {gds__log(FB_ASSERT_FAILURE_STRING, getpid(), #ex, __FILE__, __LINE__);}}

#endif

using namespace Jrd;
using namespace Firebird;


namespace EDS {

// Manager

GlobalPtr<Manager> Manager::manager;
Mutex Manager::m_mutex;
Provider* Manager::m_providers = NULL;
volatile bool Manager::m_initialized = false;
ConnectionsPool* Manager::m_connPool;

const ULONG MIN_CONNPOOL_SIZE		= 0;
const ULONG MAX_CONNPOOL_SIZE		= 1000;

const ULONG MIN_LIFE_TIME		= 1;
const ULONG MAX_LIFE_TIME		= 60 * 60 * 24;	// one day

Manager::Manager(MemoryPool& pool) :
	PermanentStorage(pool)
{
	m_connPool = FB_NEW_POOL(pool) ConnectionsPool(pool);
}

Manager::~Manager()
{
	fb_assert(m_connPool->getAllCount() == 0);

	ThreadContextHolder tdbb;

	while (m_providers)
	{
		Provider* to_delete = m_providers;
		m_providers = m_providers->m_next;
		to_delete->clearConnections(tdbb);
		delete to_delete;
	}

	delete m_connPool;
}

void Manager::addProvider(Provider* provider)
{
	for (const Provider* prv = m_providers; prv; prv = prv->m_next)
	{
		if (prv->m_name == provider->m_name) {
			return;
		}
	}

	provider->m_next = m_providers;
	m_providers = provider;
	provider->initialize();
}

Provider* Manager::getProvider(const string& prvName)
{
	for (Provider* prv = m_providers; prv; prv = prv->m_next)
	{
		if (prv->m_name == prvName) {
			return prv;
		}
	}

	// External Data Source provider ''@1'' not found
	ERR_post(Arg::Gds(isc_eds_provider_not_found) << Arg::Str(prvName));
	return NULL;
}

static void splitDataSourceName(thread_db* tdbb, const string& dataSource,
	string& prvName, PathName& dbName)
{
	// dataSource : registered data source name
	// or connection string : provider::database

	if (dataSource.isEmpty())
	{
		prvName = INTERNAL_PROVIDER_NAME;
		dbName = tdbb->getDatabase()->dbb_database_name;
	}
	else
	{
		FB_SIZE_T pos = dataSource.find("::");
		if (pos != string::npos)
		{
			prvName = dataSource.substr(0, pos);
			dbName = dataSource.substr(pos + 2).ToPathName();
		}
		else
		{
			// search dataSource at registered data sources and get connection
			// string, user and password from this info if found

			// if not found - treat dataSource as Firebird's connection string
			prvName = FIREBIRD_PROVIDER_NAME;
			dbName = dataSource.ToPathName();
		}
	}
}

static bool isCurrentAccount(UserId* currUserID,
	const MetaName& user, const string& pwd, const MetaName& role)
{
	const MetaName& attUser = currUserID->getUserName();
	const MetaName& attRole = currUserID->getSqlRole();

	return ((user.isEmpty() || user == attUser) && pwd.isEmpty() &&
			(role.isEmpty() || role == attRole));
}

Connection* Manager::getConnection(thread_db* tdbb, const string& dataSource,
	const string& user, const string& pwd, const string& role, TraScope tra_scope)
{
	Attachment* att = tdbb->getAttachment();
	if (att->att_ext_call_depth >= MAX_CALLBACKS)
		ERR_post(Arg::Gds(isc_exec_sql_max_call_exceeded));

	string prvName;
	PathName dbName;
	splitDataSourceName(tdbb, dataSource, prvName, dbName);

	Provider* prv = getProvider(prvName);

	const bool isCurrent = (prvName == INTERNAL_PROVIDER_NAME) &&
		isCurrentAccount(att->att_user, user, pwd, role);

	ClumpletWriter dpb(ClumpletReader::Tagged, MAX_DPB_SIZE);
	if (!isCurrent)
		prv->generateDPB(tdbb, dpb, user, pwd, role);

	// look up at connections already bound to current attachment
	Connection* conn = prv->getBoundConnection(tdbb, dbName, dpb, tra_scope);
	if (conn)
		return conn;

	// if could be pooled, ask connections pool

	ULONG hash = 0;

	if (!isCurrent)
	{
		hash = DefaultHash<UCHAR>::hash(dbName.c_str(), dbName.length(), MAX_ULONG) +
			   DefaultHash<UCHAR>::hash(dpb.getBuffer(), dpb.getBufferLength(), MAX_ULONG);

		while (true)
		{
			conn = m_connPool->getConnection(tdbb, prv, hash, dbName, dpb);
			if (!conn)
				break;

			if (conn->validate(tdbb))
			{
				prv->bindConnection(tdbb, conn);
				break;
			}

			// destroy invalid connection
			m_connPool->delConnection(tdbb, conn, true);
		}
	}

	if (!conn)
	{
		// finally, create new connection
		conn = prv->createConnection(tdbb, dbName, dpb, tra_scope);
		if (!isCurrent)
			m_connPool->addConnection(tdbb, conn, hash);
	}

	fb_assert(conn != NULL);
	return conn;
}

void Manager::jrdAttachmentEnd(thread_db* tdbb, Jrd::Attachment* att, bool forced)
{
	for (Provider* prv = m_providers; prv; prv = prv->m_next)
		prv->jrdAttachmentEnd(tdbb, att, forced);
}

int Manager::shutdown()
{
	FbLocalStatus status;
	ThreadContextHolder tdbb(&status);

	m_connPool->clear(tdbb);

	for (Provider* prv = m_providers; prv; prv = prv->m_next) {
		prv->cancelConnections();
	}

	return 0;
}


// Provider

Provider::Provider(const char* prvName) :
	m_name(getPool()),
	m_connections(getPool()),
	m_flags(0)
{
	m_name = prvName;
}

Provider::~Provider()
{
	fb_assert(m_connections.isEmpty());
}

void Provider::generateDPB(thread_db* tdbb, ClumpletWriter& dpb,
	const string& user, const string& pwd, const string& role) const
{
	dpb.reset(isc_dpb_version1);

	const Attachment *attachment = tdbb->getAttachment();

	// bad for connection pooling
	dpb.insertInt(isc_dpb_ext_call_depth, attachment->att_ext_call_depth + 1);

	if ((getFlags() & prvTrustedAuth) &&
		isCurrentAccount(attachment->att_user, user, pwd, role))
	{
		attachment->att_user->populateDpb(dpb);
	}
	else
	{
		if (!user.isEmpty()) {
			dpb.insertString(isc_dpb_user_name, user);
		}

		if (!pwd.isEmpty()) {
			dpb.insertString(isc_dpb_password, pwd);
		}

		if (!role.isEmpty())
		{
			dpb.insertByte(isc_dpb_sql_dialect, 0);
			dpb.insertString(isc_dpb_sql_role_name, role);
		}
	}

	CharSet* const cs = INTL_charset_lookup(tdbb, attachment->att_charset);
	if (cs) {
		dpb.insertString(isc_dpb_lc_ctype, cs->getName());
	}

	// remote network address???
}

Connection* Provider::createConnection(thread_db* tdbb,
	const PathName& dbName, ClumpletReader& dpb, TraScope tra_scope)
{
	Connection* conn = doCreateConnection();
	conn->setup(dbName, dpb);
	try
	{
		conn->attach(tdbb);
	}
	catch (...)
	{
		Connection::deleteConnection(tdbb, conn);
		throw;
	}

	bindConnection(tdbb, conn);
	return conn;
}

void Provider::bindConnection(thread_db* tdbb, Connection* conn)
{
	Attachment* attachment = tdbb->getAttachment();
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	Attachment* oldAtt = conn->getBoundAtt();
	fb_assert(oldAtt == NULL);
	if (m_connections.locate(AttToConn(oldAtt, conn)))
		m_connections.fastRemove();

	conn->setBoundAtt(attachment);
	bool ret = m_connections.add(AttToConn(attachment, conn));
	fb_assert(ret);
}

Connection* Provider::getBoundConnection(Jrd::thread_db* tdbb,
	const Firebird::PathName& dbName, Firebird::ClumpletReader& dpb,
	TraScope tra_scope)
{
	Database* dbb = tdbb->getDatabase();
	Attachment* att = tdbb->getAttachment();

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	AttToConnMap::Accessor acc(&m_connections);
	if (acc.locate(locGreatEqual, AttToConn(att, NULL)))
	{
		do
		{
			Connection* conn = acc.current().m_conn;

			if (conn->getBoundAtt() != att)
				break;

			if (conn->isSameDatabase(dbName, dpb) &&
				conn->isAvailable(tdbb, tra_scope))
			{
				fb_assert(conn->getProvider() == this);

#ifdef EDS_DEBUG
				if (!ConnectionsPool::checkBoundConnection(tdbb, conn))
					continue;
#endif

				return conn;
			}
		} while (acc.getNext());
	}

	return NULL;
}

void Provider::jrdAttachmentEnd(thread_db* tdbb, Jrd::Attachment* att, bool forced)
{
	Database* dbb = tdbb->getDatabase();

	HalfStaticArray<Connection*, 16> toRelease(getPool());

	{	// scope
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		AttToConnMap::Accessor acc(&m_connections);
		if (!acc.locate(locGreatEqual, AttToConn(att, NULL)))
			return;

		do
		{
			Connection* conn = acc.current().m_conn;
			if (conn->getBoundAtt() != att)
				break;

			toRelease.push(conn);
		} while(acc.getNext());
	}

	while (!toRelease.isEmpty())
	{
		Connection* conn = toRelease.pop();
		releaseConnection(tdbb, *conn, !forced);
	}
}

void Provider::releaseConnection(thread_db* tdbb, Connection& conn, bool inPool)
{
	ConnectionsPool* connPool = conn.getConnPool();

	{ // m_mutex scope
		Attachment* att = conn.getBoundAtt();
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		bool found = false;
		AttToConnMap::Accessor acc(&m_connections);
		if (acc.locate(AttToConn(att, &conn)))
		{
			Connection* test = acc.current().m_conn;
			fb_assert(test == &conn);

			found = true;
			acc.fastRemove();
		};

		fb_assert(found);
		conn.setBoundAtt(NULL);

		if (inPool && connPool)
			m_connections.add(AttToConn(NULL, &conn));
	}

	if (!inPool || !connPool || !conn.isConnected() || !conn.resetSession())
	{
		if (connPool)
		{
			{	// scope
				MutexLockGuard guard(m_mutex, FB_FUNCTION);
				AttToConnMap::Accessor acc(&m_connections);
				if (acc.locate(AttToConn(NULL, &conn)))
					acc.fastRemove();
			}

			connPool->delConnection(tdbb, &conn, false);
		}

		Connection::deleteConnection(tdbb, &conn);
	}
	else
		connPool->putConnection(tdbb, &conn);
}

void Provider::clearConnections(thread_db* tdbb)
{
	fb_assert(!tdbb || !tdbb->getDatabase());

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	AttToConnMap::Accessor acc(&m_connections);
	if (acc.getFirst())
	{
		do
		{
			Connection* conn = acc.current().m_conn;
			Connection::deleteConnection(tdbb, conn);
		} while (acc.getNext());
	}

	m_connections.clear();
}

void Provider::cancelConnections()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	AttToConnMap::Accessor acc(&m_connections);
	if (acc.getFirst())
	{
		do
		{
			Connection* conn = acc.current().m_conn;
			conn->cancelExecution(false);
		} while (acc.getNext());
	}
}

// Connection

Connection::Connection(Provider& prov) :
	PermanentStorage(prov.getPool()),
	m_provider(prov),
	m_dbName(getPool()),
	m_dpb(getPool()),
	m_boundAtt(NULL),
	m_transactions(getPool()),
	m_statements(getPool()),
	m_freeStatements(NULL),
	m_poolData(this),
	m_used_stmts(0),
	m_free_stmts(0),
	m_deleting(false),
	m_sqlDialect(0),
	m_wrapErrors(true),
	m_broken(false)
{
}

void Connection::setup(const PathName& dbName, const ClumpletReader& dpb)
{
	m_dbName = dbName;

	m_dpb.clear();
	m_dpb.add(dpb.getBuffer(), dpb.getBufferLength());
}

void Connection::deleteConnection(thread_db* tdbb, Connection* conn)
{
	conn->m_deleting = true;

	if (conn->isConnected())
		conn->detach(tdbb);

	delete conn;
}

Connection::~Connection()
{
	fb_assert(m_boundAtt == NULL);
}

bool Connection::isSameDatabase(const PathName& dbName, ClumpletReader& dpb) const
{
	if (m_dbName != dbName)
		return false;

	// it is not exact comparison as clumplets may have same tags
	// but in different order

	const FB_SIZE_T len = m_dpb.getCount();
	return (len == dpb.getBufferLength()) && (memcmp(m_dpb.begin(), dpb.getBuffer(), len) == 0);
}


Transaction* Connection::createTransaction()
{
	Transaction* tran = doCreateTransaction();
	m_transactions.add(tran);
	return tran;
}

void Connection::deleteTransaction(thread_db* tdbb, Transaction* tran)
{
	// Close all active statements in tran context avoiding commit of already
	// deleted transaction
	Statement** stmt_ptr = m_statements.begin();

	while (stmt_ptr < m_statements.end())
	{
		Statement* stmt = *stmt_ptr;
		if (stmt->getTransaction() == tran)
		{
			if (stmt->isActive())
				stmt->close(tdbb, true);
		}

		// close() above could destroy statement and remove it from m_statements
		if (stmt_ptr < m_statements.end() && *stmt_ptr == stmt)
			stmt_ptr++;
	}

	FB_SIZE_T pos;
	if (m_transactions.find(tran, pos))
	{
		m_transactions.remove(pos);
		delete tran;
	}
	else {
		fb_assert(false);
	}

	if (!m_used_stmts && m_transactions.getCount() == 0 && !m_deleting)
		m_provider.releaseConnection(tdbb, *this);
}

Statement* Connection::createStatement(const string& sql)
{
	m_used_stmts++;

	for (Statement** stmt_ptr = &m_freeStatements; *stmt_ptr; stmt_ptr = &(*stmt_ptr)->m_nextFree)
	{
		Statement* stmt = *stmt_ptr;
		if (stmt->getSql() == sql)
		{
			*stmt_ptr = stmt->m_nextFree;
			stmt->m_nextFree = NULL;
			m_free_stmts--;
			return stmt;
		}
	}

	if (m_free_stmts >= MAX_CACHED_STMTS)
	{
		Statement* stmt = m_freeStatements;
		m_freeStatements = stmt->m_nextFree;
		stmt->m_nextFree = NULL;
		m_free_stmts--;
		return stmt;
	}

	Statement* stmt = doCreateStatement();
	m_statements.add(stmt);
	return stmt;
}

void Connection::releaseStatement(Jrd::thread_db* tdbb, Statement* stmt)
{
	fb_assert(stmt && !stmt->isActive());

	if (stmt->isAllocated() && m_free_stmts < MAX_CACHED_STMTS)
	{
		stmt->m_nextFree = m_freeStatements;
		m_freeStatements = stmt;
		m_free_stmts++;
	}
	else
	{
		FB_SIZE_T pos;
		if (m_statements.find(stmt, pos))
		{
			m_statements.remove(pos);
			Statement::deleteStatement(tdbb, stmt);
		}
		else {
			fb_assert(false);
		}
	}

	m_used_stmts--;

	if (!m_used_stmts && m_transactions.getCount() == 0 && !m_deleting)
		m_provider.releaseConnection(tdbb, *this);
}

void Connection::clearTransactions(Jrd::thread_db* tdbb)
{
	while (m_transactions.getCount())
	{
		Transaction* tran = m_transactions[0];
		try
		{
			tran->rollback(tdbb, false);
		}
		catch (const Exception&)
		{
			if (!m_deleting)
				throw;
		}
	}
}

void Connection::clearStatements(thread_db* tdbb)
{
	Statement** stmt_ptr = m_statements.begin();
	while (stmt_ptr < m_statements.end())
	{
		Statement* stmt = *stmt_ptr;
		if (stmt->isActive())
			stmt->close(tdbb);

		// close() above could destroy statement and remove it from m_statements
		if (stmt_ptr < m_statements.end() && *stmt_ptr == stmt)
		{
			Statement::deleteStatement(tdbb, stmt);
			stmt_ptr++;
		}
	}

	m_statements.clear();

	m_freeStatements = NULL;
	m_free_stmts = m_used_stmts = 0;
}

void Connection::detach(thread_db* tdbb)
{
	const bool was_deleting = m_deleting;
	m_deleting = true;

	try
	{
		clearStatements(tdbb);
		clearTransactions(tdbb);
		m_deleting = was_deleting;
	}
	catch (...)
	{
		m_deleting = was_deleting;
		throw;
	}

	fb_assert(m_used_stmts == 0);
	fb_assert(m_transactions.getCount() == 0);

	doDetach(tdbb);
}

Transaction* Connection::findTransaction(thread_db* tdbb, TraScope traScope) const
{
	jrd_tra* tran = tdbb->getTransaction();
	Transaction* ext_tran = NULL;

	switch (traScope)
	{
	case traCommon :
		ext_tran = tran->tra_ext_common;
		while (ext_tran)
		{
			if (ext_tran->getConnection() == this)
				break;
			ext_tran = ext_tran->m_nextTran;
		}
		break;

	case traTwoPhase :
		ERR_post(Arg::Gds(isc_random) << Arg::Str("2PC transactions not implemented"));

		//ext_tran = tran->tra_ext_two_phase;
		// join transaction if not already joined
		break;
	}

	return ext_tran;
}


void Connection::raise(const FbStatusVector* status, thread_db* /*tdbb*/, const char* sWhere)
{
	if (!getWrapErrors(status->getErrors()))
	{
		ERR_post(Arg::StatusVector(status));
	}

	string rem_err;
	m_provider.getRemoteError(status, rem_err);

	// Execute statement error at @1 :\n@2Data source : @3
	ERR_post(Arg::Gds(isc_eds_connection) << Arg::Str(sWhere) <<
											 Arg::Str(rem_err) <<
											 Arg::Str(getDataSourceName()));
}


bool Connection::getWrapErrors(const ISC_STATUS* status)
{
	// Detect if connection is broken
	switch (status[1])
	{
		case isc_network_error:
		case isc_net_read_err:
		case isc_net_write_err:
			m_broken = true;
			break;

		// Always wrap shutdown errors, else user application will disconnect
		case isc_att_shutdown:
		case isc_shutdown:
			m_broken = true;
			return true;
	}

	return m_wrapErrors;
}


/// ConnectionsPool

ConnectionsPool::ConnectionsPool(MemoryPool& pool)
	: m_pool(pool),
	  m_idleArray(pool),
	  m_idleList(NULL),
	  m_activeList(NULL),
	  m_allCount(0),
	  m_maxCount(Config::getExtConnPoolSize()),
	  m_lifeTime(Config::getExtConnPoolLifeTime())
{
	if (m_maxCount > MAX_CONNPOOL_SIZE)
		m_maxCount = MAX_CONNPOOL_SIZE;
	if (m_maxCount < MIN_CONNPOOL_SIZE)
		m_maxCount = MIN_CONNPOOL_SIZE;

	if (m_lifeTime > MAX_LIFE_TIME)
		m_lifeTime = MAX_LIFE_TIME;
	if (m_lifeTime < MIN_LIFE_TIME)
		m_lifeTime = MIN_LIFE_TIME;
}

ConnectionsPool::~ConnectionsPool()
{
	fb_assert(m_idleArray.isEmpty());
	fb_assert(m_idleList == NULL);
	fb_assert(m_activeList == NULL);
}

void ConnectionsPool::removeFromPool(Data* item, FB_SIZE_T pos)
{
	// m_mutex should be locked
	fb_assert(item);

	if (item->m_lastUsed != 0)
	{
		if (pos == -1)
			m_idleArray.find(*item, pos);

		fb_assert(m_idleArray[pos] == item);
		m_idleArray.remove(pos);
		removeFromList(&m_idleList, item);
	}
	else
		removeFromList(&m_activeList, item);

	item->clear();
	m_allCount--;
}


// find least recently used item and remove it from pool
// caller should hold m_mutex and destroy returned item
ConnectionsPool::Data* ConnectionsPool::removeOldest()
{
	if (!m_idleList)
		return NULL;

	Data* lru = m_idleList->m_prev;
	removeFromPool(lru, -1);

	return lru;
}

Connection* ConnectionsPool::getConnection(thread_db* tdbb, Provider* prv, ULONG hash,
	const PathName& dbName, ClumpletReader& dpb)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	Data data(hash);

	FB_SIZE_T pos;
	m_idleArray.find(data, pos);

	for (; pos < m_idleArray.getCount(); pos++)
	{
		Data* item = m_idleArray[pos];
		if (item->m_hash != data.m_hash)
			break;

		Connection* conn = item->m_conn;
		if (conn->getProvider() == prv && conn->isSameDatabase(dbName, dpb))
		{
			m_idleArray.remove(pos);
			removeFromList(&m_idleList, item);

			item->m_lastUsed = 0;		// mark as active
			addToList(&m_activeList, item);
			return conn;
		}
	}

	return NULL;
}

void ConnectionsPool::putConnection(thread_db* tdbb, Connection* conn)
{
	fb_assert(conn->getConnPool() == this);

	Connection* oldConn = NULL;
	bool startIdleTimer = false;
	if (m_maxCount > 0)
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		Data* item = conn->getPoolData();
#ifdef EDS_DEBUG
		if (!verifyPool())
		{
			string str;
			str.printf("Before put Item 0x%08X into pool\n", item);
			printPool(str);
			gds__log("Procces ID %d: connections pool is corrupted\n%s", getpid(), str.c_str());
		}
#endif

		if (item->m_lastUsed)
		{
			// Item was already put into idle list
			fb_assert(item->m_connPool == this);
			return;
		}

		if (m_allCount > m_maxCount)
		{
			Data* oldest = removeOldest();
			if (oldest == item)
			{
#ifdef EDS_DEBUG
				string str;
				str.printf("Item 0x%08X to put into pool is oldest", item);
				gds__log("Procces ID %d: %s", getpid(), str.c_str());
#endif
				m_allCount++;
				oldest = removeOldest();
			}

			if (oldest)
				oldConn = oldest->m_conn;
		}

		if (item->m_lastUsed)
		{
			FB_SIZE_T pos;
			fb_assert(m_idleArray.find(*item, pos));

#ifdef EDS_DEBUG
			const bool ok = verifyPool();
			string str;
			str.printf("Idle item 0x%08X put back into pool. Pool is %s", item, ok ? "OK" : "corrupted\n");

			if (!ok)
				printPool(str);

			gds__log("Procces ID %d: %s", getpid(), str.c_str());
#endif
		}
		else
		{
			removeFromList(&m_activeList, item);

			time(&item->m_lastUsed);
			fb_assert(item->m_lastUsed != 0);
			if (item->m_lastUsed == 0)
				item->m_lastUsed = 1;

			addToList(&m_idleList, item);
			m_idleArray.add(item);

			if (!m_timer)
				m_timer = FB_NEW IdleTimer(*this);

			startIdleTimer = true;
		}

#ifdef EDS_DEBUG
		if (!verifyPool())
		{
			string str;
			str.printf("After put Item 0x%08X into pool\n", item);
			printPool(str);
			gds__log("Procces ID %d: connections pool is corrupted\n%s", getpid(), str.c_str());
		}
#endif
	}
	else
		oldConn = conn;

	if (oldConn)
		oldConn->getProvider()->releaseConnection(tdbb, *oldConn, false);

	if (startIdleTimer)
		m_timer->start();
}

void ConnectionsPool::addConnection(thread_db* tdbb, Connection* conn, ULONG hash)
{
	Data* item = conn->getPoolData();
	item->m_hash = hash;
	item->m_lastUsed = 0;
	item->setConnPool(this);

	Connection* oldConn = NULL;
	{	// scope
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

#ifdef EDS_DEBUG
		if (!verifyPool())
		{
			string str;
			printPool(str);
			str.printf("Before add Item 0x%08X into pool\n", item);
			gds__log("Procces ID %d: connections pool is corrupted\n%s", getpid(), str.c_str());
		}
#endif
		if (m_allCount >= m_maxCount)
		{
			Data* oldest = removeOldest();
			if (oldest)
				oldConn = oldest->m_conn;
		}

		addToList(&m_activeList, item);
		m_allCount++;

#ifdef EDS_DEBUG
		if (!verifyPool())
		{
			string str;
			printPool(str);
			str.printf("After add Item 0x%08X into pool\n", item);
			gds__log("Procces ID %d: connections pool is corrupted\n%s", getpid(), str.c_str());
		}
#endif
	}

	if (oldConn)
		oldConn->getProvider()->releaseConnection(tdbb, *oldConn, false);
}

void ConnectionsPool::delConnection(thread_db* tdbb, Connection* conn, bool destroy)
{
	{	// scope
		MutexLockGuard guard(m_mutex, FB_FUNCTION);
		Data* item = conn->getPoolData();
		if (item->getConnPool() == this)
			removeFromPool(item, -1);
#ifdef EDS_DEBUG
		else
		{
			string str;
			str.printf("Item 0x%08X to delete from pool already not there", item);
			gds__log("Procces ID %d: %s", getpid(), str.c_str());
		}
#endif
	}

	if (destroy)
		conn->getProvider()->releaseConnection(tdbb, *conn, false);
}

void ConnectionsPool::setMaxCount(ULONG val)
{
	if (val < MIN_CONNPOOL_SIZE || val > MAX_CONNPOOL_SIZE)
	{
		string err;
		err.printf("Wrong value for connections pool size (%d). Allowed values are between %d and %d.",
				   val, MIN_CONNPOOL_SIZE, MAX_CONNPOOL_SIZE);

		ERR_post(Arg::Gds(isc_random) << Arg::Str(err));
	}

	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	m_maxCount = val;
}

void ConnectionsPool::setLifeTime(ULONG val)
{
	if (val < MIN_LIFE_TIME || val > MAX_LIFE_TIME)
	{
		string err;
		err.printf("Wrong value for pooled connection lifetime (%d). Allowed values are between %d and %d.",
				   val, MIN_LIFE_TIME, MAX_LIFE_TIME);

		ERR_post(Arg::Gds(isc_random) << Arg::Str(err));
	}

	bool startIdleTimer = false;
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		startIdleTimer = (m_lifeTime > val) && (m_timer != NULL) && (m_idleList != NULL);
		m_lifeTime = val;
	}

	if (startIdleTimer)
		m_timer->start();
}

void ConnectionsPool::clearIdle(thread_db* tdbb, bool all)
{
	Data* free = NULL;
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		if (all)
		{
			while (!m_idleArray.isEmpty())
			{
				FB_SIZE_T pos = m_idleArray.getCount() - 1;
				Data* item = m_idleArray[pos];
				removeFromPool(item, pos);

				item->m_next = free;
				free = item;
			}
			fb_assert(!m_idleList);

			while (m_activeList)
				removeFromPool(m_activeList, -1);

			fb_assert(!m_allCount);
		}
		else
		{
			if (!m_idleList)
				return;

			time_t t;
			time(&t);
			t -= m_lifeTime;

			while (m_idleList)
			{
				Data* item = m_idleList->m_prev;
				if (item->m_lastUsed > t)
					break;

				removeFromPool(item, -1);
				item->m_next = free;
				free = item;
			};
		}
	}

	while (free)
	{
		Connection* conn = free->m_conn;
		free = free->m_next;
		conn->getProvider()->releaseConnection(tdbb, *conn, false);
	}
}

void ConnectionsPool::clear(thread_db* tdbb)
{
	fb_assert(!tdbb || !tdbb->getDatabase());

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	if (m_timer)
	{
		m_timer->stop();
		m_timer = NULL;
	}

#ifdef EDS_DEBUG
	if (!verifyPool())
	{
		string str;
		printPool(str);
		gds__log("Procces ID %d: connections pool is corrupted (clear)\n%s", getpid(), str.c_str());
	}
#endif

	while (m_idleArray.getCount())
	{
		FB_SIZE_T i = m_idleArray.getCount() - 1;
		Data* data = m_idleArray[i];
		Connection* conn = data->m_conn;

		removeFromPool(data, i);
		conn->getProvider()->releaseConnection(tdbb, *conn, false);
	}
	fb_assert(!m_idleList);

	while (m_activeList)
	{
		Data* data = m_activeList;
		removeFromPool(data, -1);
	}

	fb_assert(m_allCount == 0);
}

time_t ConnectionsPool::getIdleExpireTime()
{
	if (!m_idleList)
		return 0;

	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	if (!m_idleList)
		return 0;

	return m_idleList->m_prev->m_lastUsed + m_lifeTime;
}

bool ConnectionsPool::checkBoundConnection(thread_db* tdbb, Connection* conn)
{
	if (conn->isCurrent())
		return true;

	ConnectionsPool::Data* item = conn->getPoolData();
	string s;

	if (!item->getConnPool())
	{
		s.printf("Bound connection 0x%08X is not at the pool.\n", conn);
		s.append(item->print());
		gds__log(s.c_str());
		return false;
	}

	ConnectionsPool* pool = item->m_connPool;
	MutexLockGuard guard(pool->m_mutex, FB_FUNCTION);

	if (!item->m_next || !item->m_prev)
	{
		s.printf("Bound connection 0x%08X is not at the pool list.\n", conn);
		s.append(item->print());
		pool->printPool(s);
		gds__log(s.c_str());
		return false;
	}

	ConnectionsPool::Data* list = NULL;
	if (item->m_lastUsed)
	{
		if (!pool->m_idleArray.exist(*item))
		{
			s.printf("Bound connection 0x%08X is not found in idleArray.\n", conn);
			s.append(item->print());
			pool->printPool(s);
			gds__log(s.c_str());
			return false;
		}
		list = pool->m_idleList;
	}
	else
		list = pool->m_activeList;

	if (!list)
	{
		s.printf("Bound connection 0x%08X belongs to the empty list.\n", conn);
		s.append(item->print());
		pool->printPool(s);
		gds__log(s.c_str());
		return false;
	}

	ConnectionsPool::Data* p = list;
	do
	{
		if (p == item)
			break;

		p = p->m_next;
	} while (p != list);

	if (p == item)
		return true;

	s.printf("Bound connection 0x%08X is not found in pool lists.\n", conn);
	s.append(item->print());
	pool->printPool(s);
	gds__log(s.c_str());
	return false;
}

void ConnectionsPool::printPool(string& str)
{
	string s;
	s.printf("Conn pool 0x%08X, all %d, max %d, lifeTime %d\n",
		this, m_allCount, m_maxCount, m_lifeTime);
	str.append(s);

	s.printf("  active list 0x%08X:\n", m_activeList);
	str.append(s);

	Data* item = m_activeList;
	int cntActive = 0;
	if (item)
	{
		do
		{
			str.append(item->print());
			item = item->m_next;
			cntActive++;
		} while (item != m_activeList);
	}

	s.printf("  idle list 0x%08X:\n", m_idleList);
	str.append(s);

	item = m_idleList;
	int cntIdle = 0;
	if (item)
	{
		do
		{
			str.append(item->print());
			item = item->m_next;
			cntIdle++;
		} while (item != m_idleList);
	}

	s.printf("  active list count: %d\n", cntActive);
	str.append(s);
	s.printf("  idle list count: %d\n", cntIdle);
	str.append(s);
	s.printf("  idle array count: %d\n", m_idleArray.getCount());
	str.append(s);

	for (FB_SIZE_T i = 0; i < m_idleArray.getCount(); i++)
		str.append(m_idleArray[i]->print());
}

string ConnectionsPool::Data::print()
{
	string s;
	s.printf("    item 0x%08X, conn 0x%08X, hash %8u, used %" UQUADFORMAT ", next 0x%08X, prev 0x%08X, connected %s\n",
		this, m_conn, m_hash, m_lastUsed, m_next, m_prev,
		(m_conn && m_conn->isConnected()) ? "yes" : "NO");
	return s;
}

int ConnectionsPool::Data::verify(ConnectionsPool* connPool, bool active)
{
	int errs = 0;

	if (m_connPool != connPool)
		errs++;
	if (!m_conn)
		errs++;
	if (!m_hash)
		errs++;
	if (!m_lastUsed && !active)
		errs++;
	if (m_lastUsed && active)
		errs++;
	if (!m_next || !m_prev)
		errs++;

	if (m_conn && !m_conn->isConnected())
		errs++;

	return errs;
}

bool ConnectionsPool::verifyPool()
{
	int cntIdle = 0, cntActive = 0;
	int errs = 0;

	Data* item = m_idleList;
	if (item)
	{
		do
		{
			cntIdle++;
			errs += item->verify(this, false);

			FB_SIZE_T pos;
			if (!m_idleArray.find(*item, pos))
				errs++;
			else if (m_idleArray[pos] != item)
				errs++;

			item = item->m_next;
		} while (item != m_idleList);
	}

	item = m_activeList;
	if (item)
	{
		do
		{
			cntActive++;
			errs += item->verify(this, true);
			item = item->m_next;
		} while (item != m_activeList);
	}

	if (cntIdle != m_idleArray.getCount())
		errs++;

	if (cntIdle + cntActive != m_allCount)
		errs++;

	return (errs == 0);
}


void ConnectionsPool::IdleTimer::handler()
{
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);
		m_time = 0;
	}

	FbLocalStatus status;
	ThreadContextHolder tdbb(&status);
	m_connPool.clearIdle(tdbb, false);

	start();
}

int ConnectionsPool::IdleTimer::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}

	return 1;
}

void ConnectionsPool::IdleTimer::start()
{
	FbLocalStatus s;
	ITimerControl* timerCtrl = Firebird::TimerInterfacePtr();

	const time_t expTime = m_connPool.getIdleExpireTime();
	if (expTime == 0)
		return;

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	if (m_time && m_time <= expTime)
		return;

	if (m_time)
		timerCtrl->stop(&s, this);

	time_t t;
	time(&t);
	time_t delta = expTime - t;

	if (delta <= 0)
		delta = 1;

	m_time = expTime;
	timerCtrl->start(&s, this, delta * 1000 * 1000);
}

void ConnectionsPool::IdleTimer::stop()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	if (!m_time)
		return;

	m_time = 0;

	FbLocalStatus s;
	ITimerControl* timerCtrl = Firebird::TimerInterfacePtr();
	timerCtrl->stop(&s, this);
}


// Transaction

Transaction::Transaction(Connection& conn) :
	PermanentStorage(conn.getProvider()->getPool()),
	m_provider(*conn.getProvider()),
	m_connection(conn),
	m_scope(traCommon),
	m_nextTran(0)
{
}

Transaction::~Transaction()
{
}

void Transaction::generateTPB(thread_db* /*tdbb*/, ClumpletWriter& tpb,
		TraModes traMode, bool readOnly, bool wait, int lockTimeout) const
{
	switch (traMode)
	{
	case traReadCommited:
		tpb.insertTag(isc_tpb_read_committed);
		break;

	case traReadCommitedRecVersions:
		tpb.insertTag(isc_tpb_read_committed);
		tpb.insertTag(isc_tpb_rec_version);
		break;

	case traReadCommitedReadConsistency:
		tpb.insertTag(isc_tpb_read_committed);
		tpb.insertTag(isc_tpb_read_consistency);
		break;

	case traConcurrency:
		tpb.insertTag(isc_tpb_concurrency);
		break;

	case traConsistency:
		tpb.insertTag(isc_tpb_consistency);
		break;
	}

	tpb.insertTag(readOnly ? isc_tpb_read : isc_tpb_write);
	tpb.insertTag(wait ? isc_tpb_wait : isc_tpb_nowait);

	if (wait && lockTimeout && lockTimeout != DEFAULT_LOCK_TIMEOUT)
		tpb.insertInt(isc_tpb_lock_timeout, lockTimeout);
}

void Transaction::start(thread_db* tdbb, TraScope traScope, TraModes traMode,
	bool readOnly, bool wait, int lockTimeout)
{
	m_scope = traScope;

	ClumpletWriter tpb(ClumpletReader::Tpb, 64, isc_tpb_version3);
	generateTPB(tdbb, tpb, traMode, readOnly, wait, lockTimeout);

	FbLocalStatus status;
	doStart(&status, tdbb, tpb);

	if (status->getState() & IStatus::STATE_ERRORS) {
		m_connection.raise(&status, tdbb, "transaction start");
	}

	jrd_tra* tran = tdbb->getTransaction();
	switch (m_scope)
	{
	case traCommon:
		this->m_nextTran = tran->tra_ext_common;
		this->m_jrdTran = tran;
		tran->tra_ext_common = this;
		break;

	case traTwoPhase:
		// join transaction
		// this->m_jrdTran = tran;
		// tran->tra_ext_two_phase = ext_tran;
		break;
	}
}

void Transaction::prepare(thread_db* tdbb, int info_len, const char* info)
{
	FbLocalStatus status;
	doPrepare(&status, tdbb, info_len, info);

	if (status->getState() & IStatus::STATE_ERRORS) {
		m_connection.raise(&status, tdbb, "transaction prepare");
	}
}

void Transaction::commit(thread_db* tdbb, bool retain)
{
	FbLocalStatus status;
	doCommit(&status, tdbb, retain);

	if (status->getState() & IStatus::STATE_ERRORS) {
		m_connection.raise(&status, tdbb, "transaction commit");
	}

	if (!retain)
	{
		detachFromJrdTran();
		m_connection.deleteTransaction(tdbb, this);
	}
}

void Transaction::rollback(thread_db* tdbb, bool retain)
{
	FbLocalStatus status;
	doRollback(&status, tdbb, retain);

	Connection& conn = m_connection;
	if (!retain)
	{
		detachFromJrdTran();
		m_connection.deleteTransaction(tdbb, this);
	}

	if (status->getState() & IStatus::STATE_ERRORS) {
		conn.raise(&status, tdbb, "transaction rollback");
	}
}

Transaction* Transaction::getTransaction(thread_db* tdbb, Connection* conn, TraScope tra_scope)
{
	jrd_tra* tran = tdbb->getTransaction();
	Transaction* ext_tran = conn->findTransaction(tdbb, tra_scope);

	if (!ext_tran)
	{
		ext_tran = conn->createTransaction();

		TraModes traMode = traConcurrency;
		if (tran->tra_flags & TRA_read_committed)
		{
			if (tran->tra_flags & TRA_read_consistency)
				traMode = traReadCommitedReadConsistency;
			else if (tran->tra_flags & TRA_rec_version)
				traMode = traReadCommitedRecVersions;
			else
				traMode = traReadCommited;
		}
		else if (tran->tra_flags & TRA_degree3) {
			traMode = traConsistency;
		}

		try {
			ext_tran->start(tdbb,
				tra_scope,
				traMode,
				tran->tra_flags & TRA_readonly,
				tran->getLockWait() != 0,
				-tran->getLockWait()
			);
		}
		catch (const Exception&)
		{
			conn->deleteTransaction(tdbb, ext_tran);
			throw;
		}
	}

	return ext_tran;
}

void Transaction::detachFromJrdTran()
{
	if (m_scope != traCommon)
		return;

	fb_assert(m_jrdTran || m_connection.isBroken());
	if (!m_jrdTran)
		return;

	Transaction** tran_ptr = &m_jrdTran->tra_ext_common;
	m_jrdTran = NULL;
	for (; *tran_ptr; tran_ptr = &(*tran_ptr)->m_nextTran)
	{
		if (*tran_ptr == this)
		{
			*tran_ptr = this->m_nextTran;
			this->m_nextTran = NULL;
			return;
		}
	}

	fb_assert(false);
}

void Transaction::jrdTransactionEnd(thread_db* tdbb, jrd_tra* transaction,
		bool commit, bool retain, bool force)
{
	Transaction* tran = transaction->tra_ext_common;
	while (tran)
	{
		Transaction* next = tran->m_nextTran;
		try
		{
			if (commit)
				tran->commit(tdbb, retain);
			else
				tran->rollback(tdbb, retain);
		}
		catch (const Exception&)
		{
			if (!force || commit)
				throw;

			// ignore rollback error
			fb_utils::init_status(tdbb->tdbb_status_vector);
			tran->detachFromJrdTran();
		}
		tran = next;
	}
}

// Statement

Statement::Statement(Connection& conn) :
	PermanentStorage(conn.getProvider()->getPool()),
	m_provider(*conn.getProvider()),
	m_connection(conn),
	m_transaction(NULL),
	m_nextFree(NULL),
	m_boundReq(NULL),
	m_ReqImpure(NULL),
	m_nextInReq(NULL),
	m_prevInReq(NULL),
	m_sql(getPool()),
	m_singleton(false),
	m_active(false),
	m_fetched(false),
	m_error(false),
	m_allocated(false),
	m_stmt_selectable(false),
	m_inputs(0),
	m_outputs(0),
	m_callerPrivileges(false),
	m_preparedByReq(NULL),
	m_sqlParamNames(getPool()),
	m_sqlParamsMap(getPool()),
	m_in_buffer(getPool()),
	m_out_buffer(getPool()),
	m_inDescs(getPool()),
	m_outDescs(getPool())
{
}

Statement::~Statement()
{
	clearNames();
}

void Statement::deleteStatement(Jrd::thread_db* tdbb, Statement* stmt)
{
	if (stmt->m_boundReq)
		stmt->unBindFromRequest();
	stmt->deallocate(tdbb);
	delete stmt;
}

void Statement::prepare(thread_db* tdbb, Transaction* tran, const string& sql, bool named)
{
	fb_assert(!m_active);

	// already prepared the same non-empty statement
	if (isAllocated() && (m_sql == sql) && (m_sql != "") &&
		m_preparedByReq == (m_callerPrivileges ? tdbb->getRequest() : NULL))
	{
		return;
	}

	m_error = false;
	m_transaction = tran;
	m_sql = "";
	m_preparedByReq = NULL;

	m_in_buffer.clear();
	m_out_buffer.clear();
	m_inDescs.clear();
	m_outDescs.clear();
	clearNames();

	string sql2(getPool());
	const string* readySql = &sql;

	if (named && !(m_provider.getFlags() & prvNamedParams))
	{
		preprocess(sql, sql2);
		readySql = &sql2;
	}

	doPrepare(tdbb, *readySql);

	m_sql = sql;
	m_sql.trim();
	m_preparedByReq = m_callerPrivileges ? tdbb->getRequest() : NULL;
}

void Statement::setTimeout(thread_db* tdbb, unsigned int timeout)
{
	doSetTimeout(tdbb, timeout);
}

void Statement::execute(thread_db* tdbb, Transaction* tran,
	const MetaName* const* in_names, const ValueListNode* in_params,
	const ValueListNode* out_params)
{
	fb_assert(isAllocated() && !m_stmt_selectable);
	fb_assert(!m_error);
	fb_assert(!m_active);

	m_transaction = tran;

	setInParams(tdbb, in_names, in_params);
	doExecute(tdbb);
	getOutParams(tdbb, out_params);
}

void Statement::open(thread_db* tdbb, Transaction* tran,
	const MetaName* const* in_names, const ValueListNode* in_params, bool singleton)
{
	fb_assert(isAllocated() && m_stmt_selectable);
	fb_assert(!m_error);
	fb_assert(!m_active);

	m_singleton = singleton;
	m_transaction = tran;

	setInParams(tdbb, in_names, in_params);
	doOpen(tdbb);

	m_active = true;
	m_fetched = false;
}

bool Statement::fetch(thread_db* tdbb, const ValueListNode* out_params)
{
	fb_assert(isAllocated() && m_stmt_selectable);
	fb_assert(!m_error);
	fb_assert(m_active);

	if (!doFetch(tdbb))
		return false;

	m_fetched = true;

	getOutParams(tdbb, out_params);

	if (m_singleton)
	{
		if (doFetch(tdbb))
		{
			FbLocalStatus status;
			Arg::Gds(isc_sing_select_err).copyTo(&status);
			raise(&status, tdbb, "isc_dsql_fetch");
		}
		return false;
	}

	return true;
}

void Statement::close(thread_db* tdbb, bool invalidTran)
{
	// we must stuff exception if and only if this is the first time it occurs
	// once we stuff exception we must punt

	const bool wasError = m_error;
	bool doPunt = false;

	if (isAllocated() && m_active)
	{
		fb_assert(isAllocated() && m_stmt_selectable);
		try {
			doClose(tdbb, false);
		}
		catch (const Exception& ex)
		{
			if (!doPunt && !wasError)
			{
				doPunt = true;
				ex.stuffException(tdbb->tdbb_status_vector);
			}
		}
		m_active = false;
	}

	if (m_boundReq) {
		unBindFromRequest();
	}

	if (invalidTran)
		m_transaction = NULL;

	if (m_transaction && m_transaction->getScope() == traAutonomous)
	{
		bool commitFailed = false;
		if (!m_error)
		{
			try {
				m_transaction->commit(tdbb, false);
			}
			catch (const Exception& ex)
			{
				commitFailed = true;
				if (!doPunt && !wasError)
				{
					doPunt = true;
					ex.stuffException(tdbb->tdbb_status_vector);
				}
			}
		}

		if (m_error || commitFailed)
		{
			try {
				m_transaction->rollback(tdbb, false);
			}
			catch (const Exception& ex)
			{
				if (!doPunt && !wasError)
				{
					doPunt = true;
					ex.stuffException(tdbb->tdbb_status_vector);
				}
			}
		}
	}

	m_error = false;
	m_transaction = NULL;
	m_connection.releaseStatement(tdbb, this);

	if (doPunt) {
		ERR_punt();
	}
}

void Statement::deallocate(thread_db* tdbb)
{
	if (isAllocated())
	{
		try {
			doClose(tdbb, true);
		}
		catch (const Exception&)
		{
			// ignore
			fb_utils::init_status(tdbb->tdbb_status_vector);
		}
	}
	fb_assert(!isAllocated());
}


enum TokenType {ttNone, ttWhite, ttComment, ttBrokenComment, ttString, ttParamMark, ttIdent, ttOther};

static TokenType getToken(const char** begin, const char* end)
{
	TokenType ret = ttNone;
	const char* p = *begin;

	char c = *p++;
	switch (c)
	{
	case ':':
	case '?':
		ret = ttParamMark;
	break;

	case '\'':
	case '"':
		while (p < end)
		{
			if (*p++ == c)
			{
				ret = ttString;
				break;
			}
		}
		break;

	case '/':
		if (p < end && *p == '*')
		{
			ret = ttBrokenComment;
			p++;
			while (p < end)
			{
				if (*p++ == '*' && p < end && *p == '/')
				{
					p++;
					ret = ttComment;
					break;
				}
			}
		}
		else {
			ret = ttOther;
		}
		break;

	case '-':
		if (p < end && *p == '-')
		{
			while (++p < end)
			{
				if (*p == '\r')
				{
					p++;
					if (p < end && *p == '\n')
						p++;
					break;
				}
				else if (*p == '\n')
					break;
			}

			ret = ttComment;
		}
		else
			ret = ttOther;
		break;

	default:
		if (classes(c) & CHR_DIGIT)
		{
			while (p < end && (classes(*p) & CHR_DIGIT))
				p++;
			ret = ttOther;
		}
		else if (classes(c) & CHR_IDENT)
		{
			while (p < end && (classes(*p) & CHR_IDENT))
				p++;
			ret = ttIdent;
		}
		else if (classes(c) & CHR_WHITE)
		{
			while (p < end && (classes(*p) & CHR_WHITE))
				p++;
			ret = ttWhite;
		}
		else
		{
			while (p < end && !( classes(*p) & (CHR_DIGIT | CHR_IDENT | CHR_WHITE) ) &&
				   (*p != '/') && (*p != '-') && (*p != ':') && (*p != '?') &&
				   (*p != '\'') && (*p != '"') )
			{
				p++;
			}
			ret = ttOther;
		}
	}

	*begin = p;
	return ret;
}

void Statement::preprocess(const string& sql, string& ret)
{
	bool passAsIs = true, execBlock = false;
	const char* p = sql.begin(), *end = sql.end();
	const char* start = p;
	TokenType tok = getToken(&p, end);

	const char* i = start;
	while (p < end && (tok == ttComment || tok == ttWhite))
	{
		i = p;
		tok = getToken(&p, end);
	}

	if (p >= end || tok != ttIdent)
	{
		// Execute statement preprocess SQL error
		// Statement expected
		ERR_post(Arg::Gds(isc_eds_preprocess) <<
				 Arg::Gds(isc_eds_stmt_expected));
	}

	start = i; // skip leading comments ??
	string ident(i, p - i);
	ident.upper();

	if (ident == "EXECUTE")
	{
		const char* i2 = p;
		tok = getToken(&p, end);
		while (p < end && (tok == ttComment || tok == ttWhite))
		{
			i2 = p;
			tok = getToken(&p, end);
		}
		if (p >= end || tok != ttIdent)
		{
			// Execute statement preprocess SQL error
			// Statement expected
			ERR_post(Arg::Gds(isc_eds_preprocess) <<
					 Arg::Gds(isc_eds_stmt_expected));
		}
		string ident2(i2, p - i2);
		ident2.upper();

		execBlock = (ident2 == "BLOCK");
		passAsIs = false;
	}
	else
	{
		passAsIs = !(ident == "INSERT" || ident == "UPDATE" ||  ident == "DELETE" ||
			ident == "MERGE" || ident == "SELECT" || ident == "WITH");
	}

	if (passAsIs)
	{
		ret = sql;
		return;
	}

	ret.append(start, p - start);

	while (p < end)
	{
		start = p;
		tok = getToken(&p, end);
		switch (tok)
		{
		case ttParamMark:
			tok = getToken(&p, end);
			if (tok == ttIdent /*|| tok == ttString*/)
			{
				// hvlad: TODO check quoted param names
				ident.assign(start + 1, p - start - 1);
				if (tok == ttIdent)
					ident.upper();

				FB_SIZE_T n = 0;
				for (; n < m_sqlParamNames.getCount(); n++)
				{
					if ((*m_sqlParamNames[n]) == ident)
						break;
				}

				if (n >= m_sqlParamNames.getCount())
				{
					n = m_sqlParamNames.getCount();
					m_sqlParamNames.add(FB_NEW_POOL(getPool()) MetaName(getPool(), ident));
				}
				m_sqlParamsMap.add(m_sqlParamNames[n]);
			}
			else
			{
				// Execute statement preprocess SQL error
				// Parameter name expected
				ERR_post(Arg::Gds(isc_eds_preprocess) <<
						 Arg::Gds(isc_eds_prm_name_expected));
			}
			ret += '?';
			break;

		case ttIdent:
			if (execBlock)
			{
				ident.assign(start, p - start);
				ident.upper();
				if (ident == "AS")
				{
					ret.append(start, end - start);
					return;
				}
			}
			// fall thru

		case ttWhite:
		case ttComment:
		case ttString:
		case ttOther:
			ret.append(start, p - start);
			break;

		case ttBrokenComment:
			{
				// Execute statement preprocess SQL error
				// Unclosed comment found near ''@1''
				string s(start, MIN(16, end - start));
				ERR_post(Arg::Gds(isc_eds_preprocess) <<
						 Arg::Gds(isc_eds_unclosed_comment) << Arg::Str(s));
			}
			break;


		case ttNone:
			// Execute statement preprocess SQL error
			ERR_post(Arg::Gds(isc_eds_preprocess));
			break;
		}
	}
	return;
}

void Statement::setInParams(thread_db* tdbb, const MetaName* const* names,
	const ValueListNode* params)
{
	const FB_SIZE_T count = params ? params->items.getCount() : 0;

	m_error = (names && (m_sqlParamNames.getCount() != count || count == 0)) ||
		(!names && m_sqlParamNames.getCount());

	if (m_error)
	{
		// Input parameters mismatch
		status_exception::raise(Arg::Gds(isc_eds_input_prm_mismatch));
	}

	if (m_sqlParamNames.getCount())
	{
		const unsigned int sqlCount = m_sqlParamsMap.getCount();
		// Here NestConst plays against its objective. It temporary unconstifies the values.
		Array<NestConst<ValueExprNode> > sqlParamsArray(getPool(), 16);
		NestConst<ValueExprNode>* sqlParams = sqlParamsArray.getBuffer(sqlCount);

		for (unsigned int sqlNum = 0; sqlNum < sqlCount; sqlNum++)
		{
			const MetaName* sqlName = m_sqlParamsMap[sqlNum];

			unsigned int num = 0;
			for (; num < count; num++)
			{
				if (*names[num] == *sqlName)
					break;
			}

			if (num == count)
			{
				// Input parameter ''@1'' have no value set
				status_exception::raise(Arg::Gds(isc_eds_input_prm_not_set) << Arg::Str(*sqlName));
			}

			sqlParams[sqlNum] = params->items[num];
		}

		doSetInParams(tdbb, sqlCount, m_sqlParamsMap.begin(), sqlParams);
	}
	else
		doSetInParams(tdbb, count, names, (params ? params->items.begin() : NULL));
}

void Statement::doSetInParams(thread_db* tdbb, unsigned int count, const MetaName* const* /*names*/,
	const NestConst<ValueExprNode>* params)
{
	if (count != getInputs())
	{
		m_error = true;
		// Input parameters mismatch
		status_exception::raise(Arg::Gds(isc_eds_input_prm_mismatch));
	}

	if (!count)
		return;

	const NestConst<ValueExprNode>* jrdVar = params;
	GenericMap<Pair<NonPooled<const ValueExprNode*, dsc*> > > paramDescs(getPool());

	jrd_req* request = tdbb->getRequest();

	for (FB_SIZE_T i = 0; i < count; ++i, ++jrdVar)
	{
		dsc* src = NULL;
		dsc& dst = m_inDescs[i * 2];
		dsc& null = m_inDescs[i * 2 + 1];

		if (!paramDescs.get(*jrdVar, src))
		{
			src = EVL_expr(tdbb, request, *jrdVar);
			paramDescs.put(*jrdVar, src);

			if (src)
			{
				if (request->req_flags & req_null)
					src->setNull();
				else
					src->clearNull();
			}
		}

		const bool srcNull = !src || src->isNull();
		*((SSHORT*) null.dsc_address) = (srcNull ? -1 : 0);

		if (srcNull) {
			memset(dst.dsc_address, 0, dst.dsc_length);
		}
		else if (!dst.isNull())
		{
			if (dst.isBlob())
			{
				dsc srcBlob;
				srcBlob.clear();
				ISC_QUAD srcBlobID;

				if (src->isBlob())
				{
					srcBlob.makeBlob(src->getBlobSubType(), src->getTextType(), &srcBlobID);
					memmove(srcBlob.dsc_address, src->dsc_address, src->dsc_length);
				}
				else
				{
					srcBlob.makeBlob(dst.getBlobSubType(), dst.getTextType(), &srcBlobID);
					MOV_move(tdbb, src, &srcBlob);
				}

				putExtBlob(tdbb, srcBlob, dst);
			}
			else
				MOV_move(tdbb, src, &dst);
		}
	}
}


// m_outDescs -> ValueExprNode
void Statement::getOutParams(thread_db* tdbb, const ValueListNode* params)
{
	const size_t count = params ? params->items.getCount() : 0;

	if (count != getOutputs())
	{
		m_error = true;
		// Output parameters mismatch
		status_exception::raise(Arg::Gds(isc_eds_output_prm_mismatch));
	}

	if (!count)
		return;

	const NestConst<ValueExprNode>* jrdVar = params->items.begin();

	for (FB_SIZE_T i = 0; i < count; ++i, ++jrdVar)
	{
		/*
		dsc* d = EVL_assign_to(tdbb, *jrdVar);
		if (d->dsc_dtype >= FB_NELEM(sqlType) || sqlType[d->dsc_dtype] < 0)
		{
			m_error = true;
			status_exception::raise(
				Arg::Gds(isc_exec_sql_invalid_var) << Arg::Num(i + 1) << Arg::Str(m_sql.substr(0, 31)));
		}
		*/

		// build the src descriptor
		dsc& src = m_outDescs[i * 2];
		const dsc& null = m_outDescs[i * 2 + 1];
		dsc* local = &src;
		dsc localDsc;
		bid localBlobID;

		const bool srcNull = (*(SSHORT*) null.dsc_address) == -1;
		if (src.isBlob() && !srcNull)
		{
			localDsc = src;
			localDsc.dsc_address = (UCHAR*) &localBlobID;
			getExtBlob(tdbb, src, localDsc);
			local = &localDsc;
		}

		// and assign to the target
		EXE_assignment(tdbb, *jrdVar, local, srcNull, NULL, NULL);
	}
}

// read external blob (src), store it as temporary local blob and put local blob_id into dst
void Statement::getExtBlob(thread_db* tdbb, const dsc& src, dsc& dst)
{
	blb* destBlob = NULL;
	AutoPtr<Blob> extBlob(m_connection.createBlob());
	try
	{
		extBlob->open(tdbb, *m_transaction, src, NULL);

		jrd_req* request = tdbb->getRequest();
		const UCHAR bpb[] = {isc_bpb_version1, isc_bpb_storage, 1, isc_bpb_storage_temp};
		bid* localBlobID = (bid*) dst.dsc_address;
		destBlob = blb::create2(tdbb, request->req_transaction, localBlobID, sizeof(bpb), bpb);

		// hvlad ?
		destBlob->blb_sub_type = src.getBlobSubType();
		destBlob->blb_charset = src.getCharSet();

		Array<UCHAR> buffer;
		const int bufSize = 32 * 1024 - 2/*input->getMaxSegment()*/;
		UCHAR* buff = buffer.getBuffer(bufSize);

		while (true)
		{
			const USHORT length = extBlob->read(tdbb, buff, bufSize);
			if (!length)
				break;

			destBlob->BLB_put_segment(tdbb, buff, length);
		}

		extBlob->close(tdbb);
		destBlob->BLB_close(tdbb);
	}
	catch (const Exception&)
	{
		extBlob->close(tdbb);
		if (destBlob) {
			destBlob->BLB_cancel(tdbb);
		}
		throw;
	}
}

// read local blob, store it as external blob and put external blob_id in dst
void Statement::putExtBlob(thread_db* tdbb, dsc& src, dsc& dst)
{
	blb* srcBlob = NULL;
	AutoPtr<Blob> extBlob(m_connection.createBlob());
	try
	{
		extBlob->create(tdbb, *m_transaction, dst, NULL);

		jrd_req* request = tdbb->getRequest();
		bid* srcBid = (bid*) src.dsc_address;

		UCharBuffer bpb;
		BLB_gen_bpb_from_descs(&src, &dst, bpb);
		srcBlob = blb::open2(tdbb, request->req_transaction, srcBid, bpb.getCount(), bpb.begin());

		HalfStaticArray<UCHAR, 2048> buffer;
		const int bufSize = srcBlob->getMaxSegment();
		UCHAR* buff = buffer.getBuffer(bufSize);

		while (true)
		{
			USHORT length = srcBlob->BLB_get_segment(tdbb, buff, srcBlob->getMaxSegment());
			if (srcBlob->blb_flags & BLB_eof) {
				break;
			}

			extBlob->write(tdbb, buff, length);
		}

		srcBlob->BLB_close(tdbb);
		extBlob->close(tdbb);
	}
	catch (const Exception&)
	{
		extBlob->cancel(tdbb);
		if (srcBlob) {
			srcBlob->BLB_close(tdbb);
		}
		throw;
	}
}

void Statement::clearNames()
{
	MetaName** s = m_sqlParamNames.begin(), **end = m_sqlParamNames.end();
	for (; s < end; s++)
	{
		delete *s;
		*s = NULL;
	}

	m_sqlParamNames.clear();
	m_sqlParamsMap.clear();
}


void Statement::raise(FbStatusVector* status, thread_db* tdbb, const char* sWhere,
		const string* sQuery)
{
	m_error = true;

	if (!m_connection.getWrapErrors(status->getErrors()))
	{
		ERR_post(Arg::StatusVector(status));
	}

	string rem_err;
	if (status)
	{
		m_provider.getRemoteError(status, rem_err);

		if (status == tdbb->tdbb_status_vector)
		{
			status->init();
		}
	}

	// Execute statement error at @1 :\n@2Statement : @3\nData source : @4
	ERR_post(Arg::Gds(isc_eds_statement) << Arg::Str(sWhere) <<
											Arg::Str(rem_err) <<
											Arg::Str(sQuery ? sQuery->substr(0, 255) : m_sql.substr(0, 255)) <<
											Arg::Str(m_connection.getDataSourceName()));
}

void Statement::bindToRequest(jrd_req* request, Statement** impure)
{
	fb_assert(!m_boundReq);
	fb_assert(!m_prevInReq);
	fb_assert(!m_nextInReq);

	if (request->req_ext_stmt)
	{
		this->m_nextInReq = request->req_ext_stmt;
		request->req_ext_stmt->m_prevInReq = this;
	}

	request->req_ext_stmt = this;
	m_boundReq = request;
	m_ReqImpure = impure;
	*m_ReqImpure = this;
}

void Statement::unBindFromRequest()
{
	fb_assert(m_boundReq);
	fb_assert(*m_ReqImpure == this);

	if (m_boundReq->req_ext_stmt == this)
		m_boundReq->req_ext_stmt = m_nextInReq;

	if (m_nextInReq)
		m_nextInReq->m_prevInReq = this->m_prevInReq;

	if (m_prevInReq)
		m_prevInReq->m_nextInReq = this->m_nextInReq;

	*m_ReqImpure = NULL;
	m_ReqImpure = NULL;
	m_boundReq = NULL;
	m_prevInReq = m_nextInReq = NULL;
}


//  EngineCallbackGuard

void EngineCallbackGuard::init(thread_db* tdbb, Connection& conn, const char* from)
{
	m_tdbb = tdbb;
	m_mutex = &conn.m_mutex;
	m_saveConnection = NULL;

	if (m_tdbb && m_tdbb->getDatabase())
	{
		jrd_tra* transaction = m_tdbb->getTransaction();
		if (transaction)
		{
			if (transaction->tra_callback_count >= MAX_CALLBACKS)
				ERR_post(Arg::Gds(isc_exec_sql_max_call_exceeded));

			transaction->tra_callback_count++;
		}

		Jrd::Attachment* attachment = m_tdbb->getAttachment();
		if (attachment)
		{
			m_saveConnection = attachment->att_ext_connection;
			m_stable = attachment->getStable();
			m_stable->getMutex()->leave();

			MutexLockGuard guardAsync(*m_stable->getMutex(true, true), FB_FUNCTION);
			MutexLockGuard guardMain(*m_stable->getMutex(), FB_FUNCTION);
			if (m_stable->getHandle() == attachment)
				attachment->att_ext_connection = &conn;
		}
	}

	if (m_mutex) {
		m_mutex->enter(from);
	}
}

EngineCallbackGuard::~EngineCallbackGuard()
{
	if (m_mutex) {
		m_mutex->leave();
	}

	if (m_tdbb && m_tdbb->getDatabase())
	{
		Jrd::Attachment* attachment = m_tdbb->getAttachment();
		if (attachment && m_stable.hasData())
		{
			MutexLockGuard guardAsync(*m_stable->getMutex(true, true), FB_FUNCTION);
			m_stable->getMutex()->enter(FB_FUNCTION);

			if (m_stable->getHandle() == attachment)
				attachment->att_ext_connection = m_saveConnection;
			else
				m_stable->getMutex()->leave();
		}

		jrd_tra* transaction = m_tdbb->getTransaction();

		if (transaction)
			transaction->tra_callback_count--;
	}
}

} // namespace EDS
