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
#include "../common/classes/MetaString.h"
#include "../common/os/guid.h"
#include "../jrd/status.h"

#include "Protocol.h"
#include "Manager.h"

namespace Replication
{
	class Replicator :
		public Firebird::StdPlugin<Firebird::IReplicatedSessionImpl<Replicator, Firebird::CheckStatusWrapper> >
	{
		typedef Firebird::Array<Firebird::MetaString> MetaNameCache;
		typedef Firebird::HalfStaticArray<SavNumber, 16> SavepointStack;

		struct BatchBlock
		{
			Block header;
			Firebird::UCharBuffer* buffer;
			MetaNameCache atoms;
			ULONG lastAtom;
			ULONG flushes;

			explicit BatchBlock(MemoryPool& pool)
				: buffer(NULL), atoms(pool),
				  lastAtom(MAX_ULONG), flushes(0)
			{
				memset(&header, 0, sizeof(Block));
			}

			ULONG getSize() const
			{
				return (ULONG) buffer->getCount();
			}

			void putTag(UCHAR tag)
			{
				putByte(tag);
			}

			void putByte(UCHAR value)
			{
				buffer->add(value);
			}

			void putInt16(SLONG value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SSHORT));
			}

			void putInt32(SLONG value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SLONG));
			}

			void putInt64(SINT64 value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SINT64));
			}

			ULONG defineAtom(const Firebird::MetaString& name)
			{
				if (lastAtom < atoms.getCount() && atoms[lastAtom] == name)
					return lastAtom;

				FB_SIZE_T pos;
				if (!atoms.find(name, pos))
				{
					pos = atoms.getCount();
					atoms.add(name);
					putTag(opDefineAtom);
					putMetaName(name);
				}

				lastAtom = (ULONG) pos;
				return lastAtom;
			}

			void putMetaName(const Firebird::MetaString& name)
			{
				const auto length = name.length();
				fb_assert(length <= MAX_UCHAR);
				putByte((UCHAR) length);
				buffer->add((const UCHAR*) name.c_str(), length);
			}

			void putString(const Firebird::string& str)
			{
				const auto length = str.length();
				putInt32(length);
				const auto ptr = (const UCHAR*) str.c_str();
				buffer->add(ptr, length);
			}

			void putBinary(ULONG length, const UCHAR* data)
			{
				buffer->add(data, length);
			}
		};

		class Transaction :
			public Firebird::AutoIface<Firebird::IReplicatedTransactionImpl<Transaction, Firebird::CheckStatusWrapper> >
		{
		public:
			explicit Transaction(Replicator* replicator, Firebird::ITransaction* trans)
				: m_replicator(replicator), m_transaction(trans), m_data(replicator->getPool())
			{}

			BatchBlock& getData()
			{
				return m_data;
			}

			Firebird::ITransaction* getInterface()
			{
				return m_transaction.getPtr();
			}

			// IDisposable methods

			void dispose() override
			{
				m_replicator->releaseTransaction(this);
				delete this;
			}

			// IReplicatedTransaction methods

			void prepare(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->prepareTransaction(status, this);
			}

			void commit(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->commitTransaction(status, this);
			}

			void rollback(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->rollbackTransaction(status, this);
			}

			void startSavepoint(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->startSavepoint(status, this);
			}

			void releaseSavepoint(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->releaseSavepoint(status, this);
			}

			void rollbackSavepoint(Firebird::CheckStatusWrapper* status) override
			{
				m_replicator->rollbackSavepoint(status, this);
			}

			void insertRecord(Firebird::CheckStatusWrapper* status, const char* name,
							  Firebird::IReplicatedRecord* record) override
			{
				insertRecord2(status, nullptr, name, record);
			}

			void updateRecord(Firebird::CheckStatusWrapper* status, const char* name,
							  Firebird::IReplicatedRecord* orgRecord,
							  Firebird::IReplicatedRecord* newRecord) override
			{
				updateRecord2(status, nullptr, name, orgRecord, newRecord);
			}

			void deleteRecord(Firebird::CheckStatusWrapper* status, const char* name,
							  Firebird::IReplicatedRecord* record) override
			{
				deleteRecord2(status, nullptr, name, record);
			}

			void executeSql(Firebird::CheckStatusWrapper* status, const char* sql) override
			{
				m_replicator->executeSql(status, this, sql);
			}

			void executeSqlIntl(Firebird::CheckStatusWrapper* status, unsigned charset, const char* sql) override
			{
				m_replicator->executeSqlIntl(status, this, charset, sql);
			}

			// FIXME: make caller call *2 methods

			void insertRecord2(Firebird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				Firebird::IReplicatedRecord* record) override
			{
				m_replicator->insertRecord(status, this, schemaName, tableName, record);
			}

			void updateRecord2(Firebird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				Firebird::IReplicatedRecord* orgRecord,
							  Firebird::IReplicatedRecord* newRecord) override
			{
				m_replicator->updateRecord(status, this, schemaName, tableName, orgRecord, newRecord);
			}

			void deleteRecord2(Firebird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				Firebird::IReplicatedRecord* record) override
			{
				m_replicator->deleteRecord(status, this, schemaName, tableName, record);
			}

		private:
			Replicator* const m_replicator;
			Firebird::RefPtr<Firebird::ITransaction> m_transaction;
			BatchBlock m_data;
		};

		struct GeneratorValue
		{
			Jrd::MetaName name;
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
		Replicator(Firebird::MemoryPool& pool,
				   Manager* manager,
				   const Firebird::Guid& dbGuid,
				   const Firebird::MetaString& userName);

		// IReplicatedSession methods

		FB_BOOLEAN init(Firebird::CheckStatusWrapper* /*status*/, Firebird::IAttachment* att) override
		{
			m_attachment = att;
			return FB_TRUE;
		}

		Firebird::IReplicatedTransaction* startTransaction(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* trans, SINT64 number) override;
		void cleanupTransaction(Firebird::CheckStatusWrapper* status, SINT64 number) override;
		void setSequence(Firebird::CheckStatusWrapper* status, const char* name, SINT64 value) override;

	private:
		Manager* const m_manager;
		const Config* const m_config;
		Firebird::Guid m_guid;
		const Firebird::MetaString m_user;
		Firebird::Array<Transaction*> m_transactions;
		GeneratorCache m_generators;
		Firebird::RefPtr<Firebird::IAttachment> m_attachment;

		void initialize();
		void flush(BatchBlock& txnData, FlushReason reason, ULONG flags = 0);
		// Blob id is passed by value because BlobWrapper requires reference to non-const ISC_QUAD
		void storeBlob(Transaction* transaction, ISC_QUAD blobId);
		void releaseTransaction(Transaction* transaction);

		void prepareTransaction(Firebird::CheckStatusWrapper* status, Transaction* transaction);
		void commitTransaction(Firebird::CheckStatusWrapper* status, Transaction* transaction);
		void rollbackTransaction(Firebird::CheckStatusWrapper* status, Transaction* transaction);

		void startSavepoint(Firebird::CheckStatusWrapper* status, Transaction* transaction);
		void releaseSavepoint(Firebird::CheckStatusWrapper* status, Transaction* transaction);
		void rollbackSavepoint(Firebird::CheckStatusWrapper* status, Transaction* transaction);

		void insertRecord(Firebird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  Firebird::IReplicatedRecord* record);
		void updateRecord(Firebird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  Firebird::IReplicatedRecord* orgRecord,
						  Firebird::IReplicatedRecord* newRecord);
		void deleteRecord(Firebird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  Firebird::IReplicatedRecord* record);

		void executeSql(Firebird::CheckStatusWrapper* status, Transaction* transaction,
						const char* sql)
		{
			executeSqlIntl(status, transaction, CS_UTF8, sql);
		}

		void executeSqlIntl(Firebird::CheckStatusWrapper* status, Transaction* transaction,
							unsigned charset,
							const char* sql);
};

} // namespace

#endif // JRD_REPLICATION_REPLICATOR_H
