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


#ifndef JRD_REPLICATION_APPLIER_H
#define JRD_REPLICATION_APPLIER_H

#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/QualifiedName.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"

#include "Utils.h"

namespace Jrd
{
	class Applier : private Firebird::PermanentStorage
	{
		typedef Firebird::GenericMap<Firebird::Pair<Firebird::NonPooled<TraNumber, jrd_tra*> > > TransactionMap;
		typedef Firebird::HalfStaticArray<bid, 16> BlobList;
/*
		class ReplicatedTransaction : public Firebird::IReplicatedTransaction
		{
		public:
			// IDispose methods

			void dispose();

			// IReplicatedTransaction methods

			bool prepare()
			{
				return m_applier->prepareTransaction(this);
			}

			bool commit()
			{
				return m_applier->commitTransaction(this);
			}

			bool rollback()
			{
				return m_applier->rollbackTransaction(this);
			}

			bool startSavepoint()
			{
				return m_applier->startSavepoint(this);
			}

			bool releaseSavepoint()
			{
				return m_applier->releaseSavepoint(this);
			}

			bool rollbackSavepoint()
			{
				return m_applier->rollbackSavepoint(this);
			}

			bool insertRecord(const char* name,
							  Firebird::IReplicatedRecord* record)
			{
				return m_applier->insertRecord(this, name, record);
			}

			bool updateRecord(const char* name,
							  Firebird::IReplicatedRecord* orgRecord,
							  Firebird::IReplicatedRecord* newRecord)
			{
				return m_applier->updateRecord(this, name, orgRecord, newRecord);
			}

			bool deleteRecord(const char* name,
							  Firebird::IReplicatedRecord* record)
			{
				return m_applier->insertRecord(this, name, record);
			}

			bool storeBlob(ISC_QUAD blobId, Firebird::IReplicatedBlob* blob)
			{
				return m_applier->storeBlob(this, blobId, blob);
			}

			bool executeSql(const char* sql)
			{
				return m_applier->executeSql(this, sql);
			}

			bool executeSqlIntl(unsigned charset, const char* sql)
			{
				return m_applier->executeSqlIntl(this, charset, sql);
			}

			// Construstor

			ReplicatedTransaction(Applier* applier, jrd_tra* transaction)
				: m_applier(applier), m_transaction(transaction)
			{}

		private:
			Applier* const m_applier;
			jrd_tra* const m_transaction;
		};
*/
	public:
		Applier(Firebird::MemoryPool& pool,
				const Firebird::PathName& database,
				Request* request, bool cascade)
			: PermanentStorage(pool),
			  m_txnMap(pool), m_database(pool, database),
			  m_request(request), m_enableCascade(cascade)
		{}

		static Applier* create(thread_db* tdbb);

		void process(thread_db* tdbb, ULONG length, const UCHAR* data);
		void cleanupTransactions(thread_db* tdbb);
		void shutdown(thread_db* tdbb);

		Attachment* getAttachment() const
		{
			return m_request ? m_request->req_attachment : nullptr;
		}

		void setInterfacePtr(JReplicator* interfacePtr)
		{
			m_interface = interfacePtr;
		}

	private:
		TransactionMap m_txnMap;
		const Firebird::PathName m_database;
		Request* m_request;
		RecordBitmap* m_bitmap = nullptr;
		Record* m_record = nullptr;
		JReplicator* m_interface;
		const bool m_enableCascade;

		void startTransaction(thread_db* tdbb, TraNumber traNum);
		void prepareTransaction(thread_db* tdbb, TraNumber traNum);
		void commitTransaction(thread_db* tdbb, TraNumber traNum);
		void rollbackTransaction(thread_db* tdbb, TraNumber traNum, bool cleanup);

		void startSavepoint(thread_db* tdbb, TraNumber traNum);
		void cleanupSavepoint(thread_db* tdbb, TraNumber traNum, bool undo);

		void insertRecord(thread_db* tdbb, TraNumber traNum,
						  const QualifiedName& relName,
						  ULONG length, const UCHAR* data);
		void updateRecord(thread_db* tdbb, TraNumber traNum,
						  const QualifiedName& relName,
						  ULONG orgLength, const UCHAR* orgData,
						  ULONG newLength, const UCHAR* newData);
		void deleteRecord(thread_db* tdbb, TraNumber traNum,
						  const QualifiedName& relName,
						  ULONG length, const UCHAR* data);

		void setSequence(thread_db* tdbb, const QualifiedName& genName, SINT64 value);

		void storeBlob(thread_db* tdbb, TraNumber traNum, bid* blob_id,
					   ULONG length, const UCHAR* data);

		void executeSql(thread_db* tdbb, TraNumber traNum,
						unsigned charset,
						const Firebird::string& sql,
						const MetaName& owner);

		bool lookupKey(thread_db* tdbb, jrd_rel* relation, index_desc& idx);
		bool compareKey(thread_db* tdbb, jrd_rel* relation,
						const index_desc& idx,
						Record* record1, Record* record2);
		bool lookupRecord(thread_db* tdbb, jrd_rel* relation,
						  Record* record, index_desc& idx, const QualifiedName& idxName = {});

		const Format* findFormat(thread_db* tdbb, jrd_rel* relation, ULONG length);

		void doInsert(thread_db* tdbb, record_param* rpb,
						jrd_tra* transaction);
		void doUpdate(thread_db* tdbb, record_param* org_rpb, record_param* new_rpb,
						jrd_tra* transaction, BlobList* blobs);
		void doDelete(thread_db* tdbb, record_param* rpb,
						jrd_tra* transaction);

		void logConflict(const char* msg, ...);
	};
}

#endif // JRD_REPLICATION_APPLIER_H
