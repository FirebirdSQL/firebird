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

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "firebird/impl/blr.h"
#include "../jrd/trig.h"
#include "../jrd/Database.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../common/isc_proto.h"

#include "Publisher.h"
#include "Replicator.h"

using namespace Firebird;
using namespace Jrd;
using namespace Replication;

namespace
{
	// Generator RDB$BACKUP_HISTORY, although defined as system,
	// should be replicated similar to user-defined ones
	const int BACKUP_HISTORY_GENERATOR = 9;

	const char* LOG_ERROR_MSG = "Replication is stopped due to critical error(s)";

 	void handleError(thread_db* tdbb, jrd_tra* transaction = NULL)
	{
		const auto dbb = tdbb->getDatabase();
		fb_assert(dbb);
		const auto attachment = tdbb->getAttachment();
		fb_assert(attachment);

		if (transaction && transaction->tra_replicator)
		{
			transaction->tra_replicator->dispose();
			transaction->tra_replicator = NULL;
		}

		if (attachment->att_replicator)
		{
			const auto status = attachment->att_replicator->getStatus();

			if (status->getState() & IStatus::STATE_ERRORS)
			{
				Firebird::string msg;
				msg.printf("Database: %s\n\t%s", dbb->dbb_filename.c_str(), LOG_ERROR_MSG);
				iscLogStatus(msg.c_str(), status);
			}

			attachment->att_replicator->dispose();
			attachment->att_replicator = NULL;
		}
	}

	Record* upgradeRecord(thread_db* tdbb, jrd_rel* relation, Record* record)
	{
		const auto format = MET_current(tdbb, relation);

		if (record->getFormat()->fmt_version == format->fmt_version)
			return record;

		auto& pool = *tdbb->getDefaultPool();
		const auto newRecord = FB_NEW_POOL(pool) Record(pool, format);

		dsc orgDesc, newDesc;

		for (auto i = 0; i < newRecord->getFormat()->fmt_count; i++)
		{
			newRecord->clearNull(i);

			if (EVL_field(relation, newRecord, i, &newDesc))
			{
				if (EVL_field(relation, record, i, &orgDesc))
					MOV_move(tdbb, &orgDesc, &newDesc);
				else
					newRecord->setNull(i);
			}
		}

		return newRecord;
	}

	IReplicatedTransaction* ensureTransaction(thread_db* tdbb, jrd_tra* transaction)
	{
		// Create a transaction object, unless it already exists

		if (!transaction->tra_replicator)
		{
			const auto attachment = tdbb->getAttachment();
			fb_assert(attachment);

			const auto replicator = attachment->att_replicator;

			if (!replicator)
				return NULL;

			transaction->tra_replicator = replicator->startTransaction(transaction->tra_number);

			if (!transaction->tra_replicator)
				handleError(tdbb);
		}

		return transaction->tra_replicator;
	}

	bool ensureSavepoints(thread_db* tdbb, jrd_tra* transaction)
	{
		const auto dbb = tdbb->getDatabase();
		fb_assert(dbb);

		const auto replicator = transaction->tra_replicator;

		// Replicate the entire stack of active savepoints (excluding priorly replicated),
		// starting with the oldest ones

		HalfStaticArray<Savepoint*, 16> stack;

		for (Savepoint::Iterator iter(transaction->tra_save_point); *iter; ++iter)
		{
			const auto savepoint = *iter;

			if (savepoint->isReplicated())
				break;

			stack.push(savepoint);
		}

		while (stack.hasData())
		{
			const auto savepoint = stack.pop();

			if (!replicator->startSavepoint())
			{
				handleError(tdbb);
				return false;
			}

			savepoint->markAsReplicated();
		}

		return true;
	}

	class ReplicatedRecordImpl :
		public Firebird::AutoIface<Firebird::IReplicatedRecordImpl<ReplicatedRecordImpl, Firebird::CheckStatusWrapper> >
	{
	public:
		ReplicatedRecordImpl(thread_db* tdbb, const Record* record)
			: //m_tdbb(tdbb),
			  m_record(record)
		{
		}

		~ReplicatedRecordImpl()
		{
		}

		unsigned getRawLength()
		{
			return m_record->getLength();
		}

		const unsigned char* getRawData()
		{
			return m_record->getData();
		}

	private:
		//thread_db* const m_tdbb;
		const Record* const m_record;
	};

	class ReplicatedBlobImpl :
		public Firebird::AutoIface<Firebird::IReplicatedBlobImpl<ReplicatedBlobImpl, Firebird::CheckStatusWrapper> >
	{
	public:
		ReplicatedBlobImpl(thread_db* tdbb, jrd_tra* transaction, const bid* blobId) :
			m_tdbb(tdbb), m_blob(blb::open(tdbb, transaction, blobId))
		{
		}

		~ReplicatedBlobImpl()
		{
			m_blob->BLB_close(m_tdbb);
		}

		unsigned getLength()
		{
			return m_blob->blb_length;
		}

		FB_BOOLEAN isEof()
		{
			return (m_blob->blb_flags & BLB_eof);
		}

		unsigned getSegment(unsigned length, unsigned char* buffer)
		{
			auto p = buffer;

			while (length)
			{
				auto n = (USHORT) MIN(length, MAX_SSHORT);

				n = m_blob->BLB_get_segment(m_tdbb, p, n);

				p += n;
				length -= n;

				if (m_blob->blb_flags & BLB_eof)
					break;
			}

			return (unsigned) (p - buffer);
		}

	private:
		thread_db* const m_tdbb;
		blb* const m_blob;
	};
}


void REPL_attach(thread_db* tdbb, bool cleanupTransactions)
{
	const auto attachment = tdbb->getAttachment();

	if (attachment->isSystem())
		return;

	fb_assert(!attachment->att_replicator);

	const auto dbb = tdbb->getDatabase();
	fb_assert(dbb);

	dbb->ensureGuid(tdbb);

	const string dbId = dbb->getUniqueFileId();
	const PathName& dbName = dbb->dbb_filename;
	const Guid& dbGuid = dbb->dbb_guid;
	const MetaName& currentUser = attachment->att_user->getUserName();

	MemoryPool& pool = *attachment->att_pool;

	attachment->att_replicator = (IReplicatedSession*)
		Replicator::create(pool, dbId, dbName, dbGuid, currentUser,
						   cleanupTransactions);

	if (attachment->att_replicator)
		attachment->att_repl_matcher = Manager::createMatcher(pool, dbId);
}

void REPL_trans_prepare(thread_db* tdbb, jrd_tra* transaction)
{
	const auto replicator = transaction->tra_replicator;

	if (!replicator)
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	if (!replicator->prepare())
		handleError(tdbb, transaction);
}

void REPL_trans_commit(thread_db* tdbb, jrd_tra* transaction)
{
	const auto replicator = transaction->tra_replicator;

	if (!replicator)
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	if (!replicator->commit())
		handleError(tdbb, transaction);

	transaction->tra_replicator = NULL;
}

void REPL_trans_rollback(thread_db* tdbb, jrd_tra* transaction)
{
	const auto replicator = transaction->tra_replicator;

	if (!replicator)
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	if (!replicator->rollback())
		handleError(tdbb, transaction);

	transaction->tra_replicator = NULL;
}

void REPL_trans_cleanup(Jrd::thread_db* tdbb, TraNumber number)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	const auto replicator = attachment->att_replicator;

	if (!replicator)
		return;

	if (!replicator->cleanupTransaction(number))
		handleError(tdbb);
}

void REPL_save_cleanup(thread_db* tdbb, jrd_tra* transaction,
				  	   const Savepoint* savepoint, bool undo)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	const auto replicator = transaction->tra_replicator;

	if (!replicator)
		return;

	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_sql))
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	if (!transaction->tra_save_point->isReplicated())
		return;

	if (undo)
	{
		if (!replicator->rollbackSavepoint())
			handleError(tdbb, transaction);
	}
	else
	{
		if (!replicator->releaseSavepoint())
			handleError(tdbb, transaction);
	}
}

void REPL_store(thread_db* tdbb, const record_param* rpb, jrd_tra* transaction)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	if (!attachment->att_replicator)
		return;

	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_sql))
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	const auto relation = rpb->rpb_relation;
	fb_assert(relation);

	if (relation->isTemporary())
		return;

	const auto matcher = attachment->att_repl_matcher.get();

	if (!relation->isSystem() && matcher && !matcher->matchTable(relation->rel_name))
		return;

	const auto record = upgradeRecord(tdbb, relation, rpb->rpb_record);
	fb_assert(record);

	// This temporary auto-pointer is just to delete a temporary record
	AutoPtr<Record> cleanupRecord(record != rpb->rpb_record ? record : NULL);

	const auto replicator = ensureTransaction(tdbb, transaction);
	if (!replicator)
		return;

	const auto format = record->getFormat();

	UCharBuffer buffer;
	for (auto id = 0; id < format->fmt_count; id++)
	{
		dsc desc;
		if (DTYPE_IS_BLOB(format->fmt_desc[id].dsc_dtype) &&
			EVL_field(NULL, record, id, &desc))
		{
			const auto destination = (bid*) desc.dsc_address;

			if (!destination->isEmpty())
			{
				const auto blobId = *(ISC_QUAD*) desc.dsc_address;

				ReplicatedBlobImpl replBlob(tdbb, transaction, destination);

				if (!replicator->storeBlob(blobId, &replBlob))
				{
					handleError(tdbb);
					return;
				}
			}
		}
	}

	if (!ensureSavepoints(tdbb, transaction))
		return;

	ReplicatedRecordImpl replRecord(tdbb, record);

	if (!replicator->insertRecord(relation->rel_name.c_str(), &replRecord))
		handleError(tdbb, transaction);
}

void REPL_modify(thread_db* tdbb, const record_param* orgRpb,
				 const record_param* newRpb, jrd_tra* transaction)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	if (!attachment->att_replicator)
		return;

	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_sql))
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	const auto relation = newRpb->rpb_relation;
	fb_assert(relation);

	if (relation->isTemporary())
		return;

	const auto matcher = attachment->att_repl_matcher.get();

	if (!relation->isSystem() && matcher && !matcher->matchTable(relation->rel_name))
		return;

	const auto newRecord = upgradeRecord(tdbb, relation, newRpb->rpb_record);
	fb_assert(newRecord);

	const auto orgRecord = upgradeRecord(tdbb, relation, orgRpb->rpb_record);
	fb_assert(orgRecord);

	// These temporary auto-pointers are just to delete temporary records
	AutoPtr<Record> cleanupOrgRecord(orgRecord != orgRpb->rpb_record ? orgRecord : NULL);
	AutoPtr<Record> cleanupNewRecord(newRecord != newRpb->rpb_record ? newRecord : NULL);

	const auto orgLength = orgRecord->getLength();
	const auto newLength = newRecord->getLength();

	// Ignore dummy updates
	if (orgLength == newLength &&
		!memcmp(orgRecord->getData(), newRecord->getData(), orgLength))
	{
		return;
	}

	const auto replicator = ensureTransaction(tdbb, transaction);
	if (!replicator)
		return;

	const auto format = newRecord->getFormat();

	UCharBuffer buffer;
	for (auto id = 0; id < format->fmt_count; id++)
	{
		dsc desc;
		if (DTYPE_IS_BLOB(format->fmt_desc[id].dsc_dtype) &&
			EVL_field(NULL, newRecord, id, &desc))
		{
			const auto destination = (bid*) desc.dsc_address;

			if (!destination->isEmpty())
			{
				const auto blobId = *(ISC_QUAD*) desc.dsc_address;

				ReplicatedBlobImpl replBlob(tdbb, transaction, destination);

				if (!replicator->storeBlob(blobId, &replBlob))
				{
					handleError(tdbb);
					return;
				}
			}
		}
	}

	if (!ensureSavepoints(tdbb, transaction))
		return;

	ReplicatedRecordImpl replOrgRecord(tdbb, orgRecord);
	ReplicatedRecordImpl replNewRecord(tdbb, newRecord);

	if (!replicator->updateRecord(relation->rel_name.c_str(), &replOrgRecord, &replNewRecord))
		handleError(tdbb, transaction);
}


void REPL_erase(thread_db* tdbb, const record_param* rpb, jrd_tra* transaction)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	if (!attachment->att_replicator)
		return;

	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_sql))
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	const auto relation = rpb->rpb_relation;
	fb_assert(relation);

	if (relation->isTemporary())
		return;

	const auto matcher = attachment->att_repl_matcher.get();

	if (!relation->isSystem() && matcher && !matcher->matchTable(relation->rel_name))
		return;

	const auto record = upgradeRecord(tdbb, relation, rpb->rpb_record);
	fb_assert(record);

	// This temporary auto-pointer is just to delete a temporary record
	AutoPtr<Record> cleanupRecord(record != rpb->rpb_record ? record : NULL);

	const auto replicator = ensureTransaction(tdbb, transaction);
	if (!replicator)
		return;

	if (!ensureSavepoints(tdbb, transaction))
		return;

	ReplicatedRecordImpl replRecord(tdbb, record);

	if (!replicator->deleteRecord(relation->rel_name.c_str(), &replRecord))
		handleError(tdbb, transaction);
}

void REPL_gen_id(thread_db* tdbb, SLONG genId, SINT64 value)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	const auto replicator = attachment->att_replicator;

	if (!replicator)
		return;

	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_sql))
		return;

	if (genId == 0) // special case: ignore RDB$GENERATORS
		return;

	// Ignore other system generators, except RDB$BACKUP_HISTORY
	if (genId != BACKUP_HISTORY_GENERATOR)
	{
		for (auto generator = generators; generator->gen_name; generator++)
		{
			if (generator->gen_id == genId)
				return;
		}
	}

	MetaName genName;

	if (!attachment->att_generators.lookup(genId, genName))
	{
		MET_lookup_generator_id(tdbb, genId, genName, NULL);
		attachment->att_generators.store(genId, genName);
	}

	if (!replicator->setSequence(genName.c_str(), value))
		handleError(tdbb);
}

void REPL_exec_sql(thread_db* tdbb, jrd_tra* transaction, const string& sql)
{
	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	if (!attachment->att_replicator)
		return;

	fb_assert(tdbb->tdbb_flags & TDBB_repl_sql);

	if (tdbb->tdbb_flags & TDBB_dont_post_dfw)
		return;

	if (transaction->tra_flags & (TRA_system | TRA_readonly))
		return;

	const auto replicator = ensureTransaction(tdbb, transaction);
	if (!replicator)
		return;

	if (!ensureSavepoints(tdbb, transaction))
		return;

	if (!replicator->executeSql(sql.c_str()))
		handleError(tdbb, transaction);
}
