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


#ifndef JRD_REPLICATION_MANAGER_H
#define JRD_REPLICATION_MANAGER_H

#include "../common/classes/array.h"
#include "../common/classes/semaphore.h"
#include "../common/SimilarToRegex.h"
#include "../common/os/guid.h"
#include "../../jrd/QualifiedName.h"
#include "../common/isc_s_proto.h"
#include "../../jrd/intl_classes.h"

#include "Config.h"
#include "ChangeLog.h"

namespace Firebird::Jrd::Replication
{
	class TableMatcher
	{
		typedef GenericMap<Pair<Left<Jrd::QualifiedName, bool> > > TablePermissionMap;

	public:
		TableMatcher(MemoryPool& pool,
					 const string& includeSchemaFilter,
					 const string& excludeSchemaFilter,
					 const string& includeFilter,
					 const string& excludeFilter);

		bool matchTable(const Jrd::QualifiedName& tableName);

	private:
		AutoPtr<SimilarToRegex> m_includeSchemaMatcher;
		AutoPtr<SimilarToRegex> m_excludeSchemaMatcher;
		AutoPtr<SimilarToRegex> m_includeMatcher;
		AutoPtr<SimilarToRegex> m_excludeMatcher;
		TablePermissionMap m_tables;
	};

	class Manager final : public GlobalStorage
	{
		struct SyncReplica
		{
			SyncReplica(MemoryPool& pool, IAttachment* att, IReplicator* repl)
				: status(pool), attachment(att), replicator(repl)
			{}

			FbLocalStatus status;
			IAttachment* attachment;
			IReplicator* replicator;
		};

	public:
		Manager(const string& dbId, const Replication::Config* config);
		~Manager();

		void shutdown();

		UCharBuffer* getBuffer();
		void releaseBuffer(UCharBuffer* buffer);

		void flush(UCharBuffer* buffer, bool sync, bool prepare);

		void forceJournalSwitch()
		{
			if (m_changeLog)
				m_changeLog->forceSwitch();
		}

		void journalCleanup()
		{
			if (m_changeLog)
				m_changeLog->cleanup();
		}

		const Replication::Config* getConfig() const noexcept
		{
			return m_config;
		}

	private:
		void bgWriter();

		static THREAD_ENTRY_DECLARE writer_thread(THREAD_ENTRY_PARAM arg)
		{
			Manager* const mgr = static_cast<Manager*>(arg);
			mgr->bgWriter();
			return 0;
		}

		Semaphore m_startupSemaphore;
		Semaphore m_cleanupSemaphore;
		Semaphore m_workingSemaphore;

		const Replication::Config* const m_config;
		Array<SyncReplica*> m_replicas;
		Array<UCharBuffer*> m_buffers;
		Mutex m_buffersMutex;
		Array<UCharBuffer*> m_queue;
		Mutex m_queueMutex;
		ULONG m_queueSize;
		FB_UINT64 m_sequence;

		volatile bool m_shutdown;
		volatile bool m_signalled;

		AutoPtr<ChangeLog> m_changeLog;
		RWLock m_lock;
	};
}

#endif // JRD_REPLICATION_MANAGER_H
