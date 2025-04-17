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
#include "../jrd/ini.h"
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
	const char* NO_PLUGIN_ERROR = "Replication plugin %s is not found";
	const char* STOP_ERROR = "Replication is stopped due to critical error(s)";

	bool checkStatus(thread_db* tdbb, const FbLocalStatus& status,
					 jrd_tra* transaction = nullptr, bool canThrow = true)
	{
		const auto dbb = tdbb->getDatabase();
		const auto attachment = tdbb->getAttachment();

		fb_assert(attachment->att_replicator.hasData());

		const auto config = dbb->replConfig();
		fb_assert(config);

		if (config->logErrors)
			logPrimaryStatus(dbb->dbb_filename, &status);

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			if (config->disableOnError)
			{
				if (transaction)
				{
					transaction->tra_flags &= ~TRA_replicating;

					if (transaction->tra_replicator)
					{
						transaction->tra_replicator->dispose();
						transaction->tra_replicator = nullptr;
					}
				}

				attachment->att_flags &= ~ATT_replicating;
				attachment->att_replicator = nullptr;

				logPrimaryError(dbb->dbb_filename, STOP_ERROR);
			}

			if (config->reportErrors && canThrow)
			{
				(Arg::Gds(isc_repl_error) << Arg::StatusVector(&status)).raise();
			}

			return false;
		}

		return true;
	}

	IReplicatedSession* getReplicator(thread_db* tdbb)
	{
		const auto attachment = tdbb->getAttachment();

		// Disable replication for system attachments

		if (attachment->isSystem())
			return nullptr;

		// Check whether replication is allowed for this session

		if (!(attachment->att_flags & ATT_replicating))
			return nullptr;

		// Check whether replication is configured and enabled for this database

		const auto dbb = tdbb->getDatabase();
		if (!dbb->isReplicating(tdbb))
		{
			attachment->att_flags &= ~ATT_replicating;
			attachment->att_replicator = nullptr;
			return nullptr;
		}

		const auto config = dbb->replConfig();
		fb_assert(config);

		// Create a replicator object, unless it already exists

		if (!attachment->att_replicator)
		{
			if (config->pluginName.empty())
			{
				auto& pool = *attachment->att_pool;
				const auto manager = dbb->replManager(true);
				const auto& guid = dbb->dbb_guid;
				const auto& userName = attachment->getUserName();

				attachment->att_replicator = FB_NEW Replicator(pool, manager, guid, userName);
			}
			else
			{
				GetPlugins<IReplicatedSession> plugins(IPluginManager::TYPE_REPLICATOR,
													   config->pluginName.c_str());
				if (!plugins.hasData())
				{
					string msg;
					msg.printf(NO_PLUGIN_ERROR, config->pluginName.c_str());
					logPrimaryError(dbb->dbb_filename, msg);

					return nullptr;
				}

				attachment->att_replicator = plugins.plugin();
			}

			FbLocalStatus status;
			const bool replicating =
				attachment->att_replicator->init(&status, attachment->getInterface());

			if (!checkStatus(tdbb, status))
				return nullptr;

			if (!replicating)
			{
				attachment->att_flags &= ~ATT_replicating;
				attachment->att_replicator = nullptr;
				return nullptr;
			}
		}

		fb_assert(attachment->att_replicator.hasData());

		return attachment->att_replicator;
	}

	IReplicatedTransaction* getReplicator(thread_db* tdbb,
										  FbLocalStatus& status,
										  jrd_tra* transaction)
	{
		// Disable replication for system and read-only transactions

		if (transaction->tra_flags & (TRA_system | TRA_readonly))
			return nullptr;

		// Check whether replication is allowed for this transaction

		if (!(transaction->tra_flags & TRA_replicating))
			return nullptr;

		// Check parent replicator presense
		// (this includes checking for the database-wise replication state)

		const auto replicator = getReplicator(tdbb);
		if (!replicator)
		{
			transaction->tra_flags &= ~TRA_replicating;

			if (transaction->tra_replicator)
			{
				transaction->tra_replicator->dispose();
				transaction->tra_replicator = nullptr;
			}

			return nullptr;
		}

		// Create a replicator object, unless it already exists

		if (!transaction->tra_replicator)
		{
			transaction->tra_replicator =
				replicator->startTransaction(&status,
											 transaction->getInterface(true),
											 transaction->tra_number);

			if (!checkStatus(tdbb, status, transaction))
				return nullptr;

			if (!transaction->tra_replicator)
			{
				transaction->tra_flags &= ~TRA_replicating;
				return nullptr;
			}
		}

		// Ensure all active savepoints are replicated

		for (Savepoint::Iterator iter(transaction->tra_save_point); *iter; ++iter)
		{
			const auto savepoint = *iter;

			if (savepoint->isReplicated() || savepoint->isRoot())
				break;

			transaction->tra_replicator->startSavepoint(&status);

			if (!checkStatus(tdbb, status, transaction))
				return nullptr;

			savepoint->markAsReplicated();
		}

		return transaction->tra_replicator;
	}

	bool checkTable(thread_db* tdbb, jrd_rel* relation)
	{
		if (relation->isTemporary())
			return false;

		if (!relation->isSystem())
		{
			if (!relation->isReplicating(tdbb))
				return false;

			const auto attachment = tdbb->getAttachment();
			const auto matcher = attachment->att_repl_matcher.get();

			if (matcher && !matcher->matchTable(relation->rel_name))
				return false;
		}
		// Do not replicate RDB$BACKUP_HISTORY as it describes physical-level things
		else if (relation->rel_id == rel_backup_history)
			return false;

		return true;
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

	class ReplicatedRecordImpl :
		public AutoIface<IReplicatedRecordImpl<ReplicatedRecordImpl, CheckStatusWrapper> >,
		public IReplicatedFieldImpl<ReplicatedRecordImpl, CheckStatusWrapper>
	{
	public:
		ReplicatedRecordImpl(thread_db* tdbb, const jrd_rel* relation, const Record* record)
			: m_record(record),
			  m_relation(relation)
		{
		}

		~ReplicatedRecordImpl()
		{
		}

		unsigned getCount() override
		{
			const auto format = m_record->getFormat();
			return format->fmt_count;
		}

		const char* getName() override
		{
			const auto field = MET_get_field(m_relation, m_fieldIndex);
			return field ? field->fld_name.c_str() : nullptr;
		}

		unsigned getType() override
		{
			return m_fieldType;
		}

		int getSubType() override
		{
			return m_desc->getSubType();
		}

		int getScale() override
		{
			return m_desc->dsc_scale;
		}

		unsigned getLength() override
		{
			return m_fieldLength;
		}

		unsigned getCharSet() override
		{
			return m_desc->getCharSet();
		}

		const void* getData() override
		{
			if (m_record->isNull(m_fieldIndex))
				return nullptr;

			return m_record->getData() + (IPTR) m_desc->dsc_address;
		}

		IReplicatedField* getField(unsigned index) override
		{
			const auto format = m_record->getFormat();

			if (index >= format->fmt_count)
				return nullptr;

			const auto desc = &format->fmt_desc[index];

			if (desc->isUnknown() || !desc->dsc_address)
				return nullptr;

			m_desc = desc;
			m_fieldIndex = index;
			SLONG dummySubtype, dummyScale;
			desc->getSqlInfo(&m_fieldLength, &dummySubtype, &dummyScale, &m_fieldType);

			return this;
		}

		unsigned getRawLength() override
		{
			return m_record->getLength();
		}

		const unsigned char* getRawData() override
		{
			return m_record->getData();
		}

	private:
		const Record* const m_record;
		const jrd_rel* const m_relation;
		const dsc* m_desc = nullptr; // optimization
		unsigned m_fieldIndex = 0;
		SLONG m_fieldLength = 0;
		SLONG m_fieldType = 0;
	};
}


void REPL_attach(thread_db* tdbb, bool cleanupTransactions)
{
	const auto dbb = tdbb->getDatabase();
	const auto attachment = tdbb->getAttachment();

	const auto replConfig = dbb->replConfig();
	if (!replConfig)
		return;

	fb_assert(!attachment->att_repl_matcher);
	auto& pool = *attachment->att_pool;
	attachment->att_repl_matcher = FB_NEW_POOL(pool)
		TableMatcher(pool, replConfig->includeFilter, replConfig->excludeFilter);

	fb_assert(!attachment->att_replicator);
	attachment->att_flags |= ATT_replicating;

	if (cleanupTransactions)
		REPL_trans_cleanup(tdbb, 0);
	// else defer creation of replicator till really needed
}

void REPL_trans_prepare(thread_db* tdbb, jrd_tra* transaction)
{
	// There is no need to call getReplicator() and make it to create a new ReplicatedTransaction
	// just to end it up at once.
	const auto replicator = transaction->tra_replicator;
	if (!replicator)
		return;

	FbLocalStatus status;
	replicator->prepare(&status);

	checkStatus(tdbb, status, transaction);
}

void REPL_trans_commit(thread_db* tdbb, jrd_tra* transaction)
{
	const auto replicator = transaction->tra_replicator;
	if (!replicator)
		return;

	FbLocalStatus status;
	replicator->commit(&status);

	// Commit is a terminal routine, we cannot throw here
	checkStatus(tdbb, status, transaction, false);

	if (transaction->tra_replicator)
	{
		transaction->tra_replicator->dispose();
		transaction->tra_replicator = nullptr;
	}
}

void REPL_trans_rollback(thread_db* tdbb, jrd_tra* transaction)
{
	const auto replicator = transaction->tra_replicator;
	if (!replicator)
		return;

	FbLocalStatus status;
	replicator->rollback(&status);

	// Rollback is a terminal routine, we cannot throw here
	checkStatus(tdbb, status, transaction, false);

	if (transaction->tra_replicator)
	{
		transaction->tra_replicator->dispose();
		transaction->tra_replicator = nullptr;
	}
}

void REPL_trans_cleanup(Jrd::thread_db* tdbb, TraNumber number)
{
	const auto replicator = getReplicator(tdbb);
	if (!replicator)
		return;

	FbLocalStatus status;
	replicator->cleanupTransaction(&status, number);

	checkStatus(tdbb, status);
}

void REPL_save_cleanup(thread_db* tdbb, jrd_tra* transaction,
				  	   const Savepoint* savepoint, bool undo)
{
	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_in_progress))
		return;

	if (!transaction->tra_save_point->isReplicated())
		return;

	const auto replicator = transaction->tra_replicator;
	if (!replicator)
		return;

	FbLocalStatus status;

	if (undo)
		replicator->rollbackSavepoint(&status);
	else
		replicator->releaseSavepoint(&status);

	checkStatus(tdbb, status, transaction);
}

void REPL_store(thread_db* tdbb, const record_param* rpb, jrd_tra* transaction)
{
	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_in_progress))
		return;

	const auto relation = rpb->rpb_relation;
	fb_assert(relation);

	if (!checkTable(tdbb, relation))
		return;

	FbLocalStatus status;
	const auto replicator = getReplicator(tdbb, status, transaction);
	if (!replicator)
		return;

	const auto record = upgradeRecord(tdbb, relation, rpb->rpb_record);
	fb_assert(record);

	// This temporary auto-pointer is just to delete a temporary record
	AutoPtr<Record> cleanupRecord(record != rpb->rpb_record ? record : nullptr);
	AutoSetRestoreFlag<ULONG> noRecursion(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);
	AutoSetRestoreFlag<ULONG> noBlobCheck(&transaction->tra_flags, TRA_no_blob_check, true);

	ReplicatedRecordImpl replRecord(tdbb, relation, record);

	replicator->insertRecord(&status,
							 relation->rel_name.c_str(),
							 &replRecord);

	checkStatus(tdbb, status, transaction);
}

void REPL_modify(thread_db* tdbb, const record_param* orgRpb,
				 const record_param* newRpb, jrd_tra* transaction)
{
	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_in_progress))
		return;

	const auto relation = newRpb->rpb_relation;
	fb_assert(relation);

	if (!checkTable(tdbb, relation))
		return;

	FbLocalStatus status;
	const auto replicator = getReplicator(tdbb, status, transaction);
	if (!replicator)
		return;

	const auto newRecord = upgradeRecord(tdbb, relation, newRpb->rpb_record);
	fb_assert(newRecord);

	const auto orgRecord = upgradeRecord(tdbb, relation, orgRpb->rpb_record);
	fb_assert(orgRecord);

	// These temporary auto-pointers are just to delete temporary records
	AutoPtr<Record> cleanupOrgRecord(orgRecord != orgRpb->rpb_record ? orgRecord : nullptr);
	AutoPtr<Record> cleanupNewRecord(newRecord != newRpb->rpb_record ? newRecord : nullptr);

	const auto orgLength = orgRecord->getLength();
	const auto newLength = newRecord->getLength();

	// Ignore dummy updates
	if (orgLength == newLength &&
		!memcmp(orgRecord->getData(), newRecord->getData(), orgLength))
	{
		return;
	}

	AutoSetRestoreFlag<ULONG> noRecursion(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);
	AutoSetRestoreFlag<ULONG> noBlobCheck(&transaction->tra_flags, TRA_no_blob_check, true);

	ReplicatedRecordImpl replOrgRecord(tdbb, relation, orgRecord);
	ReplicatedRecordImpl replNewRecord(tdbb, relation, newRecord);

	replicator->updateRecord(&status,
							 relation->rel_name.c_str(),
							 &replOrgRecord, &replNewRecord);

	checkStatus(tdbb, status, transaction);
}


void REPL_erase(thread_db* tdbb, const record_param* rpb, jrd_tra* transaction)
{
	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_in_progress))
		return;

	const auto relation = rpb->rpb_relation;
	fb_assert(relation);

	if (!checkTable(tdbb, relation))
		return;

	FbLocalStatus status;
	const auto replicator = getReplicator(tdbb, status, transaction);
	if (!replicator)
		return;

	const auto record = upgradeRecord(tdbb, relation, rpb->rpb_record);
	fb_assert(record);

	// This temporary auto-pointer is just to delete a temporary record
	AutoPtr<Record> cleanupRecord(record != rpb->rpb_record ? record : nullptr);
	AutoSetRestoreFlag<ULONG> noRecursion(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);
	AutoSetRestoreFlag<ULONG> noBlobCheck(&transaction->tra_flags, TRA_no_blob_check, true);

	ReplicatedRecordImpl replRecord(tdbb, relation, record);

	replicator->deleteRecord(&status,
							 relation->rel_name.c_str(),
							 &replRecord);

	checkStatus(tdbb, status, transaction);
}

void REPL_gen_id(thread_db* tdbb, SLONG genId, SINT64 value)
{
	if (tdbb->tdbb_flags & (TDBB_dont_post_dfw | TDBB_repl_in_progress))
		return;

	if (genId == 0) // special case: ignore RDB$GENERATORS
		return;

	// Ignore other system generators
	for (auto generator = generators; generator->gen_name; generator++)
	{
		if (generator->gen_id == genId)
			return;
	}

	const auto replicator = getReplicator(tdbb);
	if (!replicator)
		return;

	const auto attachment = tdbb->getAttachment();

	MetaName genName;
	if (!attachment->att_generators.lookup(genId, genName))
	{
		MET_lookup_generator_id(tdbb, genId, genName, nullptr);
		attachment->att_generators.store(genId, genName);
	}

	fb_assert(genName.hasData());

	AutoSetRestoreFlag<ULONG> noRecursion(&tdbb->tdbb_flags, TDBB_repl_in_progress, true);

	FbLocalStatus status;
	replicator->setSequence(&status, genName.c_str(), value);

	checkStatus(tdbb, status);
}

void REPL_exec_sql(thread_db* tdbb, jrd_tra* transaction, const string& sql)
{
	fb_assert(tdbb->tdbb_flags & TDBB_repl_in_progress);

	if (tdbb->tdbb_flags & TDBB_dont_post_dfw)
		return;

	FbLocalStatus status;
	const auto replicator = getReplicator(tdbb, status, transaction);
	if (!replicator)
		return;

	const auto attachment = tdbb->getAttachment();
	const auto charset = attachment->att_charset;

	// This place is already protected from recursion in calling code

	replicator->executeSqlIntl(&status, charset, sql.c_str());

	checkStatus(tdbb, status, transaction);
}

void REPL_journal_switch(thread_db* tdbb)
{
	const auto dbb = tdbb->getDatabase();

	const auto replMgr = dbb->replManager();
	if (!replMgr)
		return;

	replMgr->forceJournalSwitch();
}
