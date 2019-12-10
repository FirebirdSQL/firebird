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
 *  Copyright (c) 2013 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_REPLICATION_REPLICATOR_H
#define JRD_REPLICATION_REPLICATOR_H

#include "../common/classes/timestamp.h"
#include "../common/os/guid.h"
#include "../jrd/align.h"
#include "../jrd/status.h"

#include "Protocol.h"
#include "Manager.h"

namespace Replication
{
	class Replicator :
		public Firebird::AutoIface<Firebird::IReplicatedSessionImpl<Replicator, Firebird::CheckStatusWrapper> >,
		private Firebird::PermanentStorage
	{
		typedef Firebird::Array<Firebird::MetaName> MetadataCache;
		typedef Firebird::HalfStaticArray<SavNumber, 16> SavepointStack;

		struct BatchBlock
		{
			Block header;
			Firebird::UCharBuffer* buffer;
			MetadataCache metadata;
			ULONG lastMetaId;
			ULONG flushes;

			explicit BatchBlock(MemoryPool& pool)
				: buffer(NULL), metadata(pool),
				  lastMetaId(MAX_ULONG), flushes(0)
			{
				memset(&header, 0, sizeof(Block));
			}

			ULONG getSize() const
			{
				return (ULONG) buffer->getCount();
			}

			void putTag(UCHAR tag)
			{
				buffer->add(tag);
			}

			void putInt(SLONG value)
			{
				const auto newSize = FB_ALIGN(getSize(), type_alignments[dtype_long]);
				buffer->resize(newSize);
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SLONG));
			}

			void putBigInt(SINT64 value)
			{
				const auto newSize = FB_ALIGN(getSize(), type_alignments[dtype_int64]);
				buffer->resize(newSize);
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SINT64));
			}

			void putMetaName(const Firebird::MetaName& name)
			{
				if (lastMetaId < metadata.getCount() && metadata[lastMetaId] == name)
				{
					putInt(lastMetaId);
					return;
				}

				FB_SIZE_T pos;
				if (!metadata.find(name, pos))
				{
					pos = metadata.getCount();
					metadata.add(name);
				}

				putInt(pos);
				lastMetaId = (ULONG) pos;
			}

			void putString(const Firebird::string& str)
			{
				const auto length = str.length();
				putInt(length);
				buffer->add((const UCHAR*) str.c_str(), length);
			}

			void putBinary(ULONG length, const UCHAR* data)
			{
				putInt(length);
				buffer->add(data, length);
			}
		};

		class Transaction :
			public Firebird::AutoIface<Firebird::IReplicatedTransactionImpl<Transaction, Firebird::CheckStatusWrapper> >
		{
		public:
			explicit Transaction(Replicator* replicator)
				: m_replicator(replicator), m_data(replicator->getPool())
			{}

			BatchBlock& getData()
			{
				return m_data;
			}

			// IDisposable methods

			void dispose()
			{
				delete this;
			}

			// IReplicatedTransaction methods

			FB_BOOLEAN prepare()
			{
				return m_replicator->prepareTransaction(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN commit()
			{
				return m_replicator->commitTransaction(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN rollback()
			{
				return m_replicator->rollbackTransaction(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN startSavepoint()
			{
				return m_replicator->startSavepoint(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN releaseSavepoint()
			{
				return m_replicator->releaseSavepoint(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN rollbackSavepoint()
			{
				return m_replicator->rollbackSavepoint(this) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN insertRecord(const char* name, Firebird::IReplicatedRecord* record)
			{
				return m_replicator->insertRecord(this, name, record) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN updateRecord(const char* name, Firebird::IReplicatedRecord* orgRecord, Firebird::IReplicatedRecord* newRecord)
			{
				return m_replicator->updateRecord(this, name, orgRecord, newRecord) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN deleteRecord(const char* name, Firebird::IReplicatedRecord* record)
			{
				return m_replicator->deleteRecord(this, name, record) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN storeBlob(ISC_QUAD blobId, Firebird::IReplicatedBlob* blob)
			{
				return m_replicator->storeBlob(this, blobId, blob) ? FB_TRUE : FB_FALSE;
			}

			FB_BOOLEAN executeSql(const char* sql)
			{
				return m_replicator->executeSql(this, sql) ? FB_TRUE : FB_FALSE;
			}

		private:
			Replicator* const m_replicator;
			BatchBlock m_data;
		};

		struct GeneratorValue
		{
			Firebird::MetaName name;
			SINT64 value;
		};

		typedef Firebird::Array<GeneratorValue> GeneratorCache;

		enum FlushReason
		{
			FLUSH_OVERFLOW,
			FLUSH_PREPARE,
			FLUSH_SYNC
		};

	public:
		virtual ~Replicator();

		static Replicator* create(Firebird::MemoryPool& pool,
								  const Firebird::string& dbId,
								  const Firebird::PathName& database,
								  const Firebird::Guid& guid,
								  const Firebird::MetaName& user,
								  bool cleanupTransactions);

		// IDisposable methods
		void dispose();

		// IReplicatedSession methods

		Firebird::IStatus* getStatus()
		{
			return &m_status;
		}

		Firebird::IReplicatedTransaction* startTransaction(SINT64 number);
		FB_BOOLEAN cleanupTransaction(SINT64 number);
		FB_BOOLEAN setSequence(const char* name, SINT64 value);

	private:
		Manager* const m_manager;
		const Config* const m_config;
		const Firebird::PathName m_database;
		Firebird::Guid m_guid;
		const Firebird::MetaName m_user;
		Firebird::Array<Transaction*> m_transactions;
		GeneratorCache m_generators;
		Firebird::Mutex m_mutex;
		Firebird::FbLocalStatus m_status;

		Replicator(Firebird::MemoryPool& pool,
				   Manager* manager,
				   const Firebird::PathName& dbName,
				   const Firebird::Guid& dbGuid,
				   const Firebird::MetaName& userName,
				   bool cleanupTransactions);

		void initialize();
		void flush(BatchBlock& txnData, FlushReason reason, ULONG flags = 0);
		void logError(const Firebird::IStatus* status);
		void postError(const Firebird::Exception& ex);

		bool prepareTransaction(Transaction* transaction);
		bool commitTransaction(Transaction* transaction);
		bool rollbackTransaction(Transaction* transaction);

		bool startSavepoint(Transaction* transaction);
		bool releaseSavepoint(Transaction* transaction);
		bool rollbackSavepoint(Transaction* transaction);

		bool insertRecord(Transaction* transaction,
						  const char* name,
						  Firebird::IReplicatedRecord* record);
		bool updateRecord(Transaction* transaction,
						  const char* name,
						  Firebird::IReplicatedRecord* orgRecord,
						  Firebird::IReplicatedRecord* newRecord);
		bool deleteRecord(Transaction* transaction,
						  const char* name,
						  Firebird::IReplicatedRecord* record);

		bool storeBlob(Transaction* transaction,
					   ISC_QUAD blobId,
					   Firebird::IReplicatedBlob* blob);

		bool executeSql(Transaction* transaction,
						const char* sql);
};

} // namespace

#endif // JRD_REPLICATION_REPLICATOR_H
