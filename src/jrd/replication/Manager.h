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
#include "../common/isc_s_proto.h"
#include "../../jrd/intl_classes.h"

#include "Config.h"
#include "ChangeLog.h"

namespace Replication
{
	class TableMatcher
	{
		typedef Firebird::GenericMap<Firebird::Pair<Firebird::Left<Jrd::MetaName, bool> > > TablePermissionMap;

	public:
		TableMatcher(MemoryPool& pool,
					 const Firebird::string& includeFilter,
					 const Firebird::string& excludeFilter);

		bool matchTable(const Jrd::MetaName& tableName);

	private:
		Firebird::AutoPtr<Firebird::SimilarToRegex> m_includeMatcher;
		Firebird::AutoPtr<Firebird::SimilarToRegex> m_excludeMatcher;
		TablePermissionMap m_tables;
	};

	class Manager final : public Firebird::GlobalStorage
	{
		struct SyncReplica
		{
			SyncReplica(Firebird::MemoryPool& pool, Firebird::IAttachment* att, Firebird::IReplicator* repl)
				: status(pool), attachment(att), replicator(repl)
			{}

			Firebird::FbLocalStatus status;
			Firebird::IAttachment* attachment;
			Firebird::IReplicator* replicator;
		};

	public:
		Manager(const Firebird::string& dbId,
				const Replication::Config* config);
		~Manager();

		Firebird::UCharBuffer* getBuffer();
		void releaseBuffer(Firebird::UCharBuffer* buffer);

		void flush(Firebird::UCharBuffer* buffer, bool sync);

		void forceLogSwitch()
		{
			if (m_changeLog)
				m_changeLog->forceSwitch();
		}

		const Replication::Config* getConfig() const
		{
			return m_config;
		}

	private:
		void logError(const Firebird::IStatus* status);

		void bgWriter();

		static THREAD_ENTRY_DECLARE writer_thread(THREAD_ENTRY_PARAM arg)
		{
			Manager* const mgr = static_cast<Manager*>(arg);
			mgr->bgWriter();
			return 0;
		}

		Firebird::Semaphore m_startupSemaphore;
		Firebird::Semaphore m_cleanupSemaphore;
		Firebird::Semaphore m_workingSemaphore;

		const Replication::Config* const m_config;
		Firebird::Array<SyncReplica*> m_replicas;
		Firebird::Array<Firebird::UCharBuffer*> m_buffers;
		Firebird::Mutex m_buffersMutex;
		Firebird::Array<Firebird::UCharBuffer*> m_queue;
		Firebird::Mutex m_queueMutex;
		ULONG m_queueSize;
		FB_UINT64 m_sequence;

		volatile bool m_shutdown;
		volatile bool m_signalled;
		Firebird::AtomicCounter m_waiters;

		Firebird::AutoPtr<ChangeLog> m_changeLog;
		Firebird::RWLock m_lock;
	};
}

#endif // JRD_REPLICATION_MANAGER_H
