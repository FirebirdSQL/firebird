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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/isc_proto.h"
#include "../common/isc_s_proto.h"
#include "../jrd/jrd.h"

#include "Manager.h"
#include "Protocol.h"
#include "Utils.h"

using namespace Firebird;
using namespace Jrd;
using namespace Replication;

namespace Replication
{
	const size_t MAX_BG_WRITER_LAG = 10 * 1024 * 1024;	// 10 MB

	GlobalPtr<Manager::DbReplMgrMap> Manager::g_rmMap;
	GlobalPtr<Mutex> Manager::g_mapMutex;
}


// Table matcher

TableMatcher::TableMatcher(MemoryPool& pool,
						   const string& includeFilter,
						   const string& excludeFilter)
	: m_tables(pool)
{
	m_cs = FB_NEW_POOL(pool) charset;
	m_tt = FB_NEW_POOL(pool) texttype;

	IntlUtil::initUtf8Charset(m_cs);

	string collAttributes("ICU-VERSION=");
	collAttributes += Jrd::UnicodeUtil::getDefaultIcuVersion();
	IntlUtil::setupIcuAttributes(m_cs, collAttributes, "", collAttributes);

	UCharBuffer collAttributesBuffer;
	collAttributesBuffer.push(reinterpret_cast<const UCHAR*>(collAttributes.c_str()),
							  collAttributes.length());

	if (!IntlUtil::initUnicodeCollation(m_tt, m_cs, "UNICODE", 0, collAttributesBuffer, ""))
		raiseError("Cannot initialize UNICODE collation");

	m_charSet = CharSet::createInstance(pool, 0, m_cs);
	m_textType = FB_NEW_POOL(pool) TextType(0, m_tt, m_charSet);

	if (includeFilter.hasData())
	{
		m_includeMatcher.reset(FB_NEW_POOL(pool) SimilarMatcher(
			pool, m_textType,
			(const UCHAR*) includeFilter.c_str(),
			includeFilter.length(),
			'\\', true));
	}

	if (excludeFilter.hasData())
	{
		m_excludeMatcher.reset(FB_NEW_POOL(pool) SimilarMatcher(
			pool, m_textType,
			(const UCHAR*) excludeFilter.c_str(),
			excludeFilter.length(),
			'\\', true));
	}
}

TableMatcher::~TableMatcher()
{
	if (m_tt && m_tt->texttype_fn_destroy)
		m_tt->texttype_fn_destroy(m_tt);
}

bool TableMatcher::matchTable(const MetaName& tableName)
{
	try
	{
		bool enabled = false;
		if (!m_tables.get(tableName, enabled))
		{
			enabled = true;

			if (m_includeMatcher)
			{
				m_includeMatcher->reset();
				m_includeMatcher->process((const UCHAR*) tableName.c_str(), tableName.length());
				enabled = m_includeMatcher->result();
			}

			if (enabled && m_excludeMatcher)
			{
				m_excludeMatcher->reset();
				m_excludeMatcher->process((const UCHAR*) tableName.c_str(), tableName.length());
				enabled = !m_excludeMatcher->result();
			}

			m_tables.put(tableName, enabled);
		}

		return enabled;
	}
	catch (const Exception&)
	{
		// If we failed matching the table name due to some internal error, then
		// let's allow the table to be replicated. This is not a critical failure.
		return true;
	}
}


// Replication manager

Manager* Manager::create(const string& dbId,
						 const PathName& database,
						 const Guid& guid)
{
	MutexLockGuard guard(g_mapMutex, FB_FUNCTION);

	Manager* mgr = NULL;
	if (!g_rmMap->get(dbId, mgr))
	{
		const auto config = Replication::Config::get(database);

		if (config)
		{
			mgr = FB_NEW Manager(dbId, database, guid, config);

			if (g_rmMap->put(dbId, mgr))
				fb_assert(false);

			guard.release();

			mgr->init();
		}
	}

	if (mgr)
	{
		mgr->addRef();
		return mgr;
	}

	return NULL;
}

void Manager::destroy(Manager* mgr)
{
	if (mgr)
	{
		const string dbId = mgr->m_dbId;

		MutexLockGuard guard(g_mapMutex, FB_FUNCTION);

		if (!mgr->release())
		{
			if (!g_rmMap->remove(dbId))
				fb_assert(false);
		}
	}
}

TableMatcher* Manager::createMatcher(MemoryPool& pool, const string& dbId)
{
	MutexLockGuard guard(g_mapMutex, FB_FUNCTION);

	Manager* mgr = NULL;
	if (g_rmMap->get(dbId, mgr))
	{
		const auto config = mgr->getConfig();
		return FB_NEW_POOL(pool) TableMatcher(pool, config->includeFilter, config->excludeFilter);
	}

	return NULL;
}


Manager::Manager(const string& dbId,
				 const PathName& database,
				 const Guid& guid,
				 const Replication::Config* config)
	: m_dbId(getPool(), dbId),
	  m_database(getPool(), database),
	  m_config(config),
	  m_replicas(getPool()),
	  m_buffers(getPool()),
	  m_queue(getPool()),
	  m_queueSize(0),
	  m_shutdown(false),
	  m_signalled(false)
{
	// Startup the journalling

	const auto tdbb = JRD_get_thread_data();
	const auto dbb = tdbb->getDatabase();

	m_sequence = dbb->dbb_repl_sequence;

	if (config->logDirectory.hasData())
	{
		m_changeLog = FB_NEW_POOL(getPool())
			ChangeLog(getPool(), dbId, database, guid, m_sequence, config);
	}

	Thread::start(writer_thread, this, THREAD_medium, 0);
	m_startupSemaphore.enter();
}

Manager::~Manager()
{
	m_shutdown = true;

	m_workingSemaphore.release();
	m_cleanupSemaphore.enter();

	MutexLockGuard guard(m_queueMutex, FB_FUNCTION);

	// Detach from synchronous replicas

	FbLocalStatus localStatus;

	for (auto& iter : m_replicas)
	{
		iter->replicator->close(&localStatus);
		iter->attachment->detach(&localStatus);
	}

	while (m_buffers.hasData())
		delete m_buffers.pop();
}

void Manager::init()
{
	MutexLockGuard guard(m_queueMutex, FB_FUNCTION);

	// Check whether everything is already initialized

	if (m_config->syncReplicas.isEmpty() || m_replicas.hasData())
		return;

	// Attach to synchronous replicas (if any)

	FbLocalStatus localStatus;
	DispatcherPtr provider;

	for (const auto iter : m_config->syncReplicas)
	{
		string database = iter;
		string login, password;

		auto pos = database.find('@');
		if (pos != string::npos)
		{
			const string temp = database.substr(0, pos);
			database = database.substr(pos + 1);

			pos = temp.find(':');
			if (pos != string::npos)
			{
				login = temp.substr(0, pos);
				password = temp.substr(pos + 1);
			}
			else
			{
				login = temp;
			}
		}

		ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);

		if (login.hasData())
		{
			dpb.insertString(isc_dpb_user_name, login);

			if (password.hasData())
				dpb.insertString(isc_dpb_password, password);
		}

		const auto attachment = provider->attachDatabase(&localStatus, database.c_str(),
												   	     dpb.getBufferLength(), dpb.getBuffer());
		if (!localStatus.isSuccess())
		{
			logError(&localStatus);
			continue;
		}

		const auto replicator = attachment->createReplicator(&localStatus);
		if (!localStatus.isSuccess())
		{
			logError(&localStatus);
			attachment->detach(&localStatus);
			continue;
		}

		m_replicas.add(FB_NEW_POOL(getPool()) SyncReplica(getPool(), attachment, replicator));
	}
}

UCharBuffer* Manager::getBuffer()
{
	MutexLockGuard guard(m_buffersMutex, FB_FUNCTION);

	const auto buffer = m_buffers.hasData() ?
		m_buffers.pop() : FB_NEW_POOL(getPool()) UCharBuffer(getPool());

	fb_assert(buffer->isEmpty());
	buffer->resize(sizeof(Block));
	return buffer;
}

void Manager::releaseBuffer(UCharBuffer* buffer)
{
	fb_assert(buffer);
	buffer->clear();

	MutexLockGuard guard(m_buffersMutex, FB_FUNCTION);

	fb_assert(!m_buffers.exist(buffer));
	m_buffers.add(buffer);
}

void Manager::logError(const IStatus* status)
{
	string message;

	auto statusPtr = status->getErrors();

	char temp[BUFFER_LARGE];
	while (fb_interpret(temp, sizeof(temp), &statusPtr))
	{
		if (!message.isEmpty())
			message += "\n\t";

		message += temp;
	}

	logOriginMessage(m_database, message, ERROR_MSG);
}

void Manager::flush(UCharBuffer* buffer, bool sync)
{
	fb_assert(buffer && buffer->hasData());

	MutexLockGuard guard(m_queueMutex, FB_FUNCTION);

	// Add the current chunk to the queue
	m_queue.add(buffer);
	m_queueSize += buffer->getCount();

	// If the background thread is lagging too far behind,
	// replicate packets synchronously rather than relying
	// on the background thread to catch up any time soon
	if (!sync && m_queueSize > MAX_BG_WRITER_LAG)
		sync = true;

	if (sync)
	{
		const auto tdbb = JRD_get_thread_data();
		const auto dbb = tdbb->getDatabase();

		for (auto& buffer : m_queue)
		{
			if (buffer)
			{
				const auto length = (ULONG) buffer->getCount();

				if (m_changeLog)
				{
					const auto sequence = m_changeLog->write(length, buffer->begin(), true);

					if (sequence != m_sequence)
					{
						dbb->setReplSequence(tdbb, sequence);
						m_sequence = sequence;
					}
				}

				for (auto& iter : m_replicas)
				{
					iter->status.check();
					iter->replicator->process(&iter->status, length, buffer->begin());
					iter->status.check();
				}

				m_queueSize -= length;
				releaseBuffer(buffer);
				buffer = NULL;
			}
		}

		m_queue.clear();
		m_queueSize = 0;
	}
	else if (!m_signalled)
	{
		m_signalled = true;
		m_workingSemaphore.release();
	}
}

void Manager::bgWriter()
{
	try
	{
		// Signal about our startup

		m_startupSemaphore.release();

		// Loop to replicate queued changes

		while (!m_shutdown)
		{
			MutexLockGuard guard(m_queueMutex, FB_FUNCTION);

			for (auto& buffer : m_queue)
			{
				if (buffer)
				{
					const auto length = (ULONG) buffer->getCount();
					fb_assert(length);

					if (m_changeLog)
					{
						m_changeLog->write(length, buffer->begin(), false);
					}

					for (auto& iter : m_replicas)
					{
						if (iter->status.isSuccess())
						{
							iter->replicator->process(&iter->status, length, buffer->begin());
						}
					}

					m_queueSize -= length;
					releaseBuffer(buffer);
					buffer = NULL;
				}
			}

			guard.release();

			if (m_shutdown)
				break;

			m_signalled = false;
			m_workingSemaphore.tryEnter(1);
		}
	}
	catch (const Exception& ex)
	{
		iscLogException("Error in replicator thread", ex);
	}

	// Signal about our exit

	try
	{
		m_cleanupSemaphore.release();
	}
	catch (const Firebird::Exception& ex)
	{
		iscLogException("Error while exiting replicator thread", ex);
	}
}
