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
#include "../ids.h"
#include "../jrd/jrd.h"
#include "../jrd/blb.h"
#include "../jrd/req.h"
#include "../jrd/ini.h"
#include "ibase.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/vio_proto.h"
#include "../dsql/dsql_proto.h"
#include "firebird/impl/sqlda_pub.h"

#include "Applier.h"
#include "Protocol.h"
#include "Publisher.h"
#include "Utils.h"

// Log conflicts as warnings
#define LOG_CONFLICTS

// Detect and resolve record-level conflicts (in favor of master copy)
#define RESOLVE_CONFLICTS

using namespace Firebird;
using namespace Ods;
using namespace Jrd;
using namespace Replication;

namespace
{
	struct NoKeyTable
	{
		USHORT rel_id;
		USHORT rel_fields[8];
	};

	const auto UNDEF = MAX_USHORT;

	NoKeyTable NO_KEY_TABLES[] = {
		{ rel_segments, { f_seg_name, f_seg_field, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_args, { f_arg_fun_name, f_arg_pos, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_ccon, { f_ccon_cname, f_ccon_tname, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_vrel, { f_vrl_vname, f_vrl_context, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_msgs, { f_msg_trigger, f_msg_number, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_dims, { f_dims_fname, f_dims_dim, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_files, { f_file_name, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } },
		{ rel_priv, { f_prv_user, f_prv_u_type, f_prv_o_type, f_prv_priv, f_prv_grant, f_prv_grantor, f_prv_rname, f_prv_fname } },
		{ rel_db_creators, { f_crt_user, f_crt_u_type, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF, UNDEF } }
	};

	class BlockReader : public AutoStorage
	{
	public:
		BlockReader(ULONG length, const UCHAR* data)
			: m_header((Block*) data),
			  m_data(data + sizeof(Block)),
			  m_end(data + length),
			  m_atoms(getPool())
		{
			fb_assert(m_data + m_header->length == m_end);
		}

		bool isEof() const
		{
			return (m_data >= m_end);
		}

		UCHAR getTag()
		{
			return getByte();
		}

		UCHAR getByte()
		{
			return *m_data++;
		}

		SSHORT getInt16()
		{
			SSHORT value;
			memcpy(&value, m_data, sizeof(SSHORT));
			m_data += sizeof(SSHORT);
			return value;
		}

		SLONG getInt32()
		{
			SLONG value;
			memcpy(&value, m_data, sizeof(SLONG));
			m_data += sizeof(SLONG);
			return value;
		}

		SINT64 getInt64()
		{
			SINT64 value;
			memcpy(&value, m_data, sizeof(SINT64));
			m_data += sizeof(SINT64);
			return value;
		}

		const MetaString& getMetaName()
		{
			const auto pos = getInt32();
			return m_atoms[pos];
		}

		string getString()
		{
			const auto length = getInt32();
			const string str((const char*) m_data, length);
			m_data += length;
			return str;
		}

		const UCHAR* getBinary(ULONG length)
		{
			const auto ptr = m_data;
			m_data += length;
			return ptr;
		}

		TraNumber getTransactionId() const
		{
			return m_header->traNumber;
		}

		ULONG getProtocolVersion() const
		{
			return m_header->protocol;
		}

		void defineAtom()
		{
			const auto length = getByte();
			const auto ptr = getBinary(length);
			const MetaString name((const char*) ptr, length);
			m_atoms.add(name);
		}

	private:
		const Block* const m_header;
		const UCHAR* m_data;
		const UCHAR* const m_end;
		HalfStaticArray<MetaString, 64> m_atoms;
	};

	class LocalThreadContext
	{
	public:
		LocalThreadContext(thread_db* tdbb, jrd_tra* tra, jrd_req* req = NULL)
			: m_tdbb(tdbb)
		{
			tdbb->setTransaction(tra);
			tdbb->setRequest(req);
		}

		~LocalThreadContext()
		{
			m_tdbb->setTransaction(NULL);
			m_tdbb->setRequest(NULL);
		}

	private:
		thread_db* m_tdbb;
	};

} // namespace


Applier* Applier::create(thread_db* tdbb)
{
	const auto dbb = tdbb->getDatabase();

	if (!dbb->isReplica())
		raiseError("Database is not in the replica mode");

	const auto attachment = tdbb->getAttachment();

	if (!attachment->locksmith(tdbb, REPLICATE_INTO_DATABASE))
		status_exception::raise(Arg::Gds(isc_miss_prvlg) << "REPLICATE_INTO_DATABASE");

	const auto req_pool = attachment->createPool();
	Jrd::ContextPoolHolder context(tdbb, req_pool);
	AutoPtr<CompilerScratch> csb(FB_NEW_POOL(*req_pool) CompilerScratch(*req_pool));

	const auto request = JrdStatement::makeRequest(tdbb, csb, true);
	TimeZoneUtil::validateGmtTimeStamp(request->req_gmt_timestamp);
	request->req_attachment = attachment;

	auto& att_pool = *attachment->att_pool;
	const auto applier = FB_NEW_POOL(att_pool) Applier(att_pool, dbb->dbb_filename, request);

	attachment->att_repl_appliers.add(applier);
	return applier;
}

void Applier::shutdown(thread_db* tdbb)
{
	const auto attachment = tdbb->getAttachment();

	cleanupTransactions(tdbb);

	CMP_release(tdbb, m_request);
	m_request = NULL;
	m_record = NULL;

	m_bitmap->clear();

	attachment->att_repl_appliers.findAndRemove(this);

	if (m_interface)
	{
		m_interface->resetHandle();
		m_interface = nullptr;
	}
}

void Applier::process(thread_db* tdbb, ULONG length, const UCHAR* data)
{
	Database* const dbb = tdbb->getDatabase();

	if (dbb->readOnly())
		raiseError("Replication is impossible for read-only database");

	tdbb->tdbb_flags |= TDBB_replicator;

	BlockReader reader(length, data);

	const auto traNum = reader.getTransactionId();
	const auto protocol = reader.getProtocolVersion();

	if (protocol != PROTOCOL_CURRENT_VERSION)
		raiseError("Unsupported replication protocol version %u", protocol);

	while (!reader.isEof())
	{
		const auto op = reader.getTag();

		switch (op)
		{
		case opStartTransaction:
			startTransaction(tdbb, traNum);
			break;

		case opPrepareTransaction:
			prepareTransaction(tdbb, traNum);
			break;

		case opCommitTransaction:
			commitTransaction(tdbb, traNum);
			break;

		case opRollbackTransaction:
			rollbackTransaction(tdbb, traNum, false);
			break;

		case opCleanupTransaction:
			if (traNum)
				rollbackTransaction(tdbb, traNum, true);
			else
				cleanupTransactions(tdbb);
			break;

		case opStartSavepoint:
			startSavepoint(tdbb, traNum);
			break;

		case opReleaseSavepoint:
			cleanupSavepoint(tdbb, traNum, false);
			break;

		case opRollbackSavepoint:
			cleanupSavepoint(tdbb, traNum, true);
			break;

		case opInsertRecord:
			{
				const auto relName = reader.getMetaName();
				const ULONG length = reader.getInt32();
				const auto record = reader.getBinary(length);
				insertRecord(tdbb, traNum, relName, length, record);
			}
			break;

		case opUpdateRecord:
			{
				const auto relName = reader.getMetaName();
				const ULONG orgLength = reader.getInt32();
				const auto orgRecord = reader.getBinary(orgLength);
				const ULONG newLength = reader.getInt32();
				const auto newRecord = reader.getBinary(newLength);
				updateRecord(tdbb, traNum, relName, orgLength, orgRecord, newLength, newRecord);
			}
			break;

		case opDeleteRecord:
			{
				const auto relName = reader.getMetaName();
				const ULONG length = reader.getInt32();
				const auto record = reader.getBinary(length);
				deleteRecord(tdbb, traNum, relName, length, record);
			}
			break;

		case opStoreBlob:
			{
				bid blob_id;
				blob_id.bid_quad.bid_quad_high = reader.getInt32();
				blob_id.bid_quad.bid_quad_low = reader.getInt32();
				do {
					const ULONG length = reader.getInt16();
					if (!length)
					{
						// Close our newly created blob
						storeBlob(tdbb, traNum, &blob_id, 0, nullptr);
						break;
					}
					const auto blob = reader.getBinary(length);
					storeBlob(tdbb, traNum, &blob_id, length, blob);
				} while (!reader.isEof());
			}
			break;

		case opExecuteSql:
		case opExecuteSqlIntl:
			{
				const auto ownerName = reader.getMetaName();
				const unsigned charset =
					(op == opExecuteSql) ? CS_UTF8 : reader.getByte();
				const string sql = reader.getString();
				executeSql(tdbb, traNum, charset, sql, ownerName);
			}
			break;

		case opSetSequence:
			{
				const auto genName = reader.getMetaName();
				const auto value = reader.getInt64();
				setSequence(tdbb, genName, value);
			}
			break;

		case opDefineAtom:
			reader.defineAtom();
			break;

		default:
			fb_assert(false);
		}

		// Check cancellation flags and reset monitoring state if necessary
		tdbb->checkCancelState();
		Monitoring::checkState(tdbb);
	}
}

void Applier::startTransaction(thread_db* tdbb, TraNumber traNum)
{
	const auto attachment = tdbb->getAttachment();

	if (m_txnMap.exist(traNum))
		raiseError("Transaction %" SQUADFORMAT" already exists", traNum);

	const auto transaction = TRA_start(tdbb, TRA_read_committed | TRA_rec_version, 1);

	m_txnMap.put(traNum, transaction);
}

void Applier::prepareTransaction(thread_db* tdbb, TraNumber traNum)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction);

	TRA_prepare(tdbb, transaction, 0, NULL);
}

void Applier::commitTransaction(thread_db* tdbb, TraNumber traNum)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction);

	TRA_commit(tdbb, transaction, false);

	m_txnMap.remove(traNum);
}

void Applier::rollbackTransaction(thread_db* tdbb, TraNumber traNum, bool cleanup)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
	{
		if (cleanup)
			return;

		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);
	}

	LocalThreadContext context(tdbb, transaction);

	TRA_rollback(tdbb, transaction, false, true);

	m_txnMap.remove(traNum);
}

void Applier::cleanupTransactions(thread_db* tdbb)
{
	TransactionMap::Accessor txnAccessor(&m_txnMap);
	if (txnAccessor.getFirst())
	{
		do {
			const auto transaction = txnAccessor.current()->second;
			TRA_rollback(tdbb, transaction, false, true);
		} while (txnAccessor.getNext());
	}

	m_txnMap.clear();
}

void Applier::startSavepoint(thread_db* tdbb, TraNumber traNum)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction);

	transaction->startSavepoint();
}

void Applier::cleanupSavepoint(thread_db* tdbb, TraNumber traNum, bool undo)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction);

	if (!transaction->tra_save_point)
		raiseError("Transaction %" SQUADFORMAT" has no savepoints to cleanup", traNum);

	if (undo)
		transaction->rollbackSavepoint(tdbb);
	else
		transaction->rollforwardSavepoint(tdbb);
}

void Applier::insertRecord(thread_db* tdbb, TraNumber traNum,
						   const MetaName& relName,
						   ULONG length, const UCHAR* data)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction, m_request);

	TRA_attach_request(transaction, m_request);

	const auto relation = MET_lookup_relation(tdbb, relName);
	if (!relation)
		raiseError("Table %s is not found", relName.c_str());

	if (!(relation->rel_flags & REL_scanned))
		MET_scan_relation(tdbb, relation);

	const auto format = findFormat(tdbb, relation, length);

	record_param rpb;
	rpb.rpb_relation = relation;

	rpb.rpb_record = m_record;
	const auto record = m_record =
		VIO_record(tdbb, &rpb, format, m_request->req_pool);

	rpb.rpb_format_number = format->fmt_version;
	rpb.rpb_address = record->getData();
	rpb.rpb_length = length;
	record->copyDataFrom(data);

	try
	{
		doInsert(tdbb, &rpb, transaction);
		return;
	}
	catch (const status_exception& ex)
	{
		// Uniqueness violation is handled below, other exceptions are re-thrown
		if (ex.value()[1] != isc_unique_key_violation &&
			ex.value()[1] != isc_no_dup)
		{
			throw;
		}

		fb_utils::init_status(tdbb->tdbb_status_vector);
	}

	bool found = false;

#ifdef RESOLVE_CONFLICTS
	index_desc idx;
	const auto indexed = lookupRecord(tdbb, relation, record, m_bitmap, idx);

	AutoPtr<Record> cleanup;

	if (m_bitmap->getFirst())
	{
		record_param tempRpb = rpb;
		tempRpb.rpb_record = NULL;

		do {
			tempRpb.rpb_number.setValue(m_bitmap->current());

			if (VIO_get(tdbb, &tempRpb, transaction, m_request->req_pool) &&
				(!indexed || compareKey(tdbb, relation, idx, record, tempRpb.rpb_record)))
			{
				if (found)
					raiseError("Record in table %s is ambiguously identified using the primary/unique key", relName.c_str());

				rpb = tempRpb;
				found = true;
			}
		} while (m_bitmap->getNext());

		cleanup = tempRpb.rpb_record;
	}
#endif

	if (found)
	{
		logConflict("Record being inserted into table %s already exists, updating instead", relName.c_str());

		record_param newRpb;
		newRpb.rpb_relation = relation;

		newRpb.rpb_record = NULL;
		AutoPtr<Record> newRecord(VIO_record(tdbb, &newRpb, format, m_request->req_pool));

		newRpb.rpb_format_number = format->fmt_version;
		newRpb.rpb_address = newRecord->getData();
		newRpb.rpb_length = length;
		newRecord->copyDataFrom(data);

		doUpdate(tdbb, &rpb, &newRpb, transaction, NULL);
	}
	else
	{
		doInsert(tdbb, &rpb, transaction); // second (paranoid) attempt
	}
}

void Applier::updateRecord(thread_db* tdbb, TraNumber traNum,
						   const MetaName& relName,
						   ULONG orgLength, const UCHAR* orgData,
						   ULONG newLength, const UCHAR* newData)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction, m_request);

	TRA_attach_request(transaction, m_request);

	const auto relation = MET_lookup_relation(tdbb, relName);
	if (!relation)
		raiseError("Table %s is not found", relName.c_str());

	if (!(relation->rel_flags & REL_scanned))
		MET_scan_relation(tdbb, relation);

	const auto orgFormat = findFormat(tdbb, relation, orgLength);

	record_param orgRpb;
	orgRpb.rpb_relation = relation;

	orgRpb.rpb_record = m_record;
	const auto orgRecord = m_record =
		VIO_record(tdbb, &orgRpb, orgFormat, m_request->req_pool);

	orgRpb.rpb_format_number = orgFormat->fmt_version;
	orgRpb.rpb_address = orgRecord->getData();
	orgRpb.rpb_length = orgLength;
	orgRecord->copyDataFrom(orgData);

	BlobList sourceBlobs(getPool());
	sourceBlobs.resize(orgFormat->fmt_count);
	for (USHORT id = 0; id < orgFormat->fmt_count; id++)
	{
		dsc desc;
		if (DTYPE_IS_BLOB(orgFormat->fmt_desc[id].dsc_dtype) &&
			EVL_field(NULL, orgRecord, id, &desc))
		{
			const auto source = (bid*) desc.dsc_address;

			if (!source->isEmpty())
				sourceBlobs[id] = *source;
		}
	}

	index_desc idx;
	const auto indexed = lookupRecord(tdbb, relation, orgRecord, m_bitmap, idx);

	bool found = false;
	AutoPtr<Record> cleanup;

	if (m_bitmap->getFirst())
	{
		record_param tempRpb = orgRpb;
		tempRpb.rpb_record = NULL;

		do {
			tempRpb.rpb_number.setValue(m_bitmap->current());

			if (VIO_get(tdbb, &tempRpb, transaction, m_request->req_pool) &&
				(!indexed || compareKey(tdbb, relation, idx, orgRecord, tempRpb.rpb_record)))
			{
				if (found)
					raiseError("Record in table %s is ambiguously identified using the primary/unique key", relName.c_str());

				orgRpb = tempRpb;
				found = true;
			}
		} while (m_bitmap->getNext());

		cleanup = tempRpb.rpb_record;
	}

	const auto newFormat = findFormat(tdbb, relation, newLength);

	record_param newRpb;
	newRpb.rpb_relation = relation;

	newRpb.rpb_record = NULL;
	AutoPtr<Record> newRecord(VIO_record(tdbb, &newRpb, newFormat, m_request->req_pool));

	newRpb.rpb_format_number = newFormat->fmt_version;
	newRpb.rpb_address = newRecord->getData();
	newRpb.rpb_length = newLength;
	newRecord->copyDataFrom(newData);

	if (found)
	{
		if (relation->isSystem())
		{
			// For system tables, preserve the fields that was not explicitly changed
			// during the update. This prevents metadata IDs from being overwritten.

			for (USHORT id = 0; id < newFormat->fmt_count; id++)
			{
				dsc from, to;

				const auto orgFlag = EVL_field(NULL, orgRecord, id, &from);
				const auto newFlag = EVL_field(NULL, newRecord, id, &to);

				if (orgFlag == newFlag && (!newFlag || !MOV_compare(tdbb, &from, &to)))
				{
					const auto flag = EVL_field(NULL, orgRpb.rpb_record, id, &from);

					if (flag)
					{
						MOV_move(tdbb, &from, &to);
						newRecord->clearNull(id);

						if (DTYPE_IS_BLOB(from.dsc_dtype))
						{
							const auto source = (bid*) from.dsc_address;
							sourceBlobs[id] = *source;
						}
					}
					else
					{
						newRecord->setNull(id);
						sourceBlobs[id].clear();
					}
				}
			}
		}

		doUpdate(tdbb, &orgRpb, &newRpb, transaction, &sourceBlobs);
	}
	else
	{
#ifdef RESOLVE_CONFLICTS
		logConflict("Record being updated in table %s does not exist, inserting instead", relName.c_str());
		doInsert(tdbb, &newRpb, transaction);
#else
		raiseError("Record in table %s cannot be located via the primary/unique key", relName.c_str());
#endif
	}
}

void Applier::deleteRecord(thread_db* tdbb, TraNumber traNum,
						   const MetaName& relName,
						   ULONG length, const UCHAR* data)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction, m_request);

	TRA_attach_request(transaction, m_request);

	const auto relation = MET_lookup_relation(tdbb, relName);
	if (!relation)
		raiseError("Table %s is not found", relName.c_str());

	if (!(relation->rel_flags & REL_scanned))
		MET_scan_relation(tdbb, relation);

	const auto format = findFormat(tdbb, relation, length);

	record_param rpb;
	rpb.rpb_relation = relation;

	rpb.rpb_record = m_record;
	const auto record = m_record =
		VIO_record(tdbb, &rpb, format, m_request->req_pool);

	rpb.rpb_format_number = format->fmt_version;
	rpb.rpb_address = record->getData();
	rpb.rpb_length = length;
	record->copyDataFrom(data);

	index_desc idx;
	const bool indexed = lookupRecord(tdbb, relation, record, m_bitmap, idx);

	bool found = false;
	AutoPtr<Record> cleanup;

	if (m_bitmap->getFirst())
	{
		record_param tempRpb = rpb;
		tempRpb.rpb_record = NULL;

		do {
			tempRpb.rpb_number.setValue(m_bitmap->current());

			if (VIO_get(tdbb, &tempRpb, transaction, m_request->req_pool) &&
				(!indexed || compareKey(tdbb, relation, idx, record, tempRpb.rpb_record)))
			{
				if (found)
					raiseError("Record in table %s is ambiguously identified using the primary/unique key", relName.c_str());

				rpb = tempRpb;
				found = true;
			}
		} while (m_bitmap->getNext());

		cleanup = tempRpb.rpb_record;
	}

	if (found)
	{
		doDelete(tdbb, &rpb, transaction);
	}
	else
	{
#ifdef RESOLVE_CONFLICTS
		logConflict("Record being deleted from table %s does not exist, ignoring", relName.c_str());
#else
		raiseError("Record in table %s cannot be located via the primary/unique key", relName.c_str());
#endif
	}
}

void Applier::setSequence(thread_db* tdbb, const MetaName& genName, SINT64 value)
{
	const auto attachment = tdbb->getAttachment();

	auto gen_id = attachment->att_generators.lookup(genName);

	if (gen_id < 0)
	{
		gen_id = MET_lookup_generator(tdbb, genName);

		if (gen_id < 0)
			raiseError("Generator %s is not found", genName.c_str());

		attachment->att_generators.store(gen_id, genName);
	}

	if (DPM_gen_id(tdbb, gen_id, false, 0) < value)
		DPM_gen_id(tdbb, gen_id, true, value);
}

void Applier::storeBlob(thread_db* tdbb, TraNumber traNum, bid* blobId,
						ULONG length, const UCHAR* data)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	LocalThreadContext context(tdbb, transaction);

	const auto orgBlobId = blobId->get_permanent_number().getValue();

	blb* blob = NULL;

	ReplBlobMap::Accessor accessor(&transaction->tra_repl_blobs);
	if (accessor.locate(orgBlobId))
	{
		const auto tempBlobId = accessor.current()->second;

		if (transaction->tra_blobs->locate(tempBlobId))
		{
			const auto current = &transaction->tra_blobs->current();
			fb_assert(!current->bli_materialized);
			blob = current->bli_blob_object;
		}
	}
	else
	{
		bid newBlobId;
		blob = blb::create(tdbb, transaction, &newBlobId);
		transaction->tra_repl_blobs.put(orgBlobId, newBlobId.bid_temp_id());
	}

	fb_assert(blob);

	if (length)
		blob->BLB_put_segment(tdbb, data, length);
	else
		blob->BLB_close(tdbb);
}

void Applier::executeSql(thread_db* tdbb,
						 TraNumber traNum,
						 unsigned charset,
						 const string& sql,
						 const MetaName& owner)
{
	jrd_tra* transaction = NULL;
	if (!m_txnMap.get(traNum, transaction))
		raiseError("Transaction %" SQUADFORMAT" is not found", traNum);

	const auto dbb = tdbb->getDatabase();
	const auto attachment = transaction->tra_attachment;

	LocalThreadContext context(tdbb, transaction);

	const auto dialect =
		(dbb->dbb_flags & DBB_DB_SQL_dialect_3) ? SQL_DIALECT_V6 : SQL_DIALECT_V5;

	UserId user(*attachment->att_user);
	user.setUserName(owner);

	AutoSetRestore<SSHORT> autoCharset(&attachment->att_charset, charset);
	AutoSetRestore<UserId*> autoOwner(&attachment->att_user, &user);

	DSQL_execute_immediate(tdbb, attachment, &transaction,
						   0, sql.c_str(), dialect,
						   NULL, NULL, NULL, NULL, false);
}

bool Applier::lookupKey(thread_db* tdbb, jrd_rel* relation, index_desc& key)
{
	RelationPages* const relPages = relation->getPages(tdbb);
	auto page = relPages->rel_index_root;
	if (!page)
	{
		DPM_scan_pages(tdbb);
		page = relPages->rel_index_root;
	}

	const PageNumber root_page(relPages->rel_pg_space_id, page);
	win window(root_page);
	const auto root = (index_root_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_root);

	index_desc idx;
	idx.idx_id = key.idx_id = idx_invalid;

	for (USHORT i = 0; i < root->irt_count; i++)
	{
		if (BTR_description(tdbb, relation, root, &idx, i))
		{
			if (idx.idx_flags & idx_primary)
			{
				key = idx;
				break;
			}

			if (idx.idx_flags & idx_unique)
			{
				if (key.idx_id == idx_invalid)
					key = idx;
				else if (relation->isSystem())
				{
					// For unique system indices, prefer ones using metanames rather than IDs
					USHORT metakeys1 = 0, metakeys2 = 0;

					for (USHORT id = 0; id < idx.idx_count; id++)
					{
						if (idx.idx_rpt[id].idx_itype == idx_metadata)
							metakeys1++;
					}

					for (USHORT id = 0; id < key.idx_count; id++)
					{
						if (key.idx_rpt[id].idx_itype == idx_metadata)
							metakeys2++;
					}

					if (metakeys1 > metakeys2)
						key = idx;
				}
				else if (idx.idx_count < key.idx_count)
					key = idx;
			}
		}
	}

	CCH_RELEASE(tdbb, &window);

	return (key.idx_id != idx_invalid);
}

bool Applier::compareKey(thread_db* tdbb, jrd_rel* relation, const index_desc& idx,
						 Record* record1, Record* record2)
{
	bool equal = true;

	for (USHORT i = 0; i < idx.idx_count; i++)
	{
		const auto field_id = idx.idx_rpt[i].idx_field;

		dsc desc1, desc2;

		const bool null1 = !EVL_field(relation, record1, field_id, &desc1);
		const bool null2 = !EVL_field(relation, record2, field_id, &desc2);

		if (null1 != null2 || (!null1 && MOV_compare(tdbb, &desc1, &desc2)))
		{
			equal = false;
			break;
		}
	}

	return equal;
}

bool Applier::lookupRecord(thread_db* tdbb,
						   jrd_rel* relation, Record* record,
						   RecordBitmap* bitmap,
						   index_desc& idx)
{
	RecordBitmap::reset(bitmap);

	// Special case: RDB$DATABASE has no keys but it's guaranteed to have only one record
	if (relation->rel_id == rel_database)
	{
		bitmap->set(0);
		return false;
	}

	if (lookupKey(tdbb, relation, idx))
	{
		temporary_key key;
		const auto result = BTR_key(tdbb, relation, record, &idx, &key, false);
		if (result != idx_e_ok)
		{
			IndexErrorContext context(relation, &idx);
			context.raise(tdbb, result, record);
		}

		IndexRetrieval retrieval(relation, &idx, idx.idx_count, &key);
		retrieval.irb_generic = irb_equality | (idx.idx_flags & idx_descending ? irb_descending : 0);

		BTR_evaluate(tdbb, &retrieval, &bitmap, NULL);
		return true;
	}

	NoKeyTable* table = NULL;

	for (size_t i = 0; i < FB_NELEM(NO_KEY_TABLES); i++)
	{
		const auto tab = &NO_KEY_TABLES[i];

		if (tab->rel_id == relation->rel_id)
		{
			table = tab;
			break;
		}
	}

	if (!table)
		raiseError("Table %s has no unique key", relation->rel_name.c_str());

	const auto transaction = tdbb->getTransaction();

	RLCK_reserve_relation(tdbb, transaction, relation, false);

	record_param rpb;
	rpb.rpb_relation = relation;
	rpb.rpb_number.setValue(BOF_NUMBER);

	while (VIO_next_record(tdbb, &rpb, transaction, m_request->req_pool, false))
	{
		const auto seq_record = rpb.rpb_record;
		fb_assert(seq_record);

		bool matched = true;

		for (size_t i = 0; i < FB_NELEM(table->rel_fields); i++)
		{
			const USHORT field_id = table->rel_fields[i];

			if (field_id == MAX_USHORT)
				break;

			dsc desc1, desc2;

			const bool null1 = !EVL_field(relation, record, field_id, &desc1);
			const bool null2 = !EVL_field(relation, seq_record, field_id, &desc2);

			if (null1 != null2 || !null1 && MOV_compare(tdbb, &desc1, &desc2))
			{
				matched = false;
				break;
			}
		}

		if (matched)
			bitmap->set(rpb.rpb_number.getValue());
	}

	delete rpb.rpb_record;
	return false;
}

const Format* Applier::findFormat(thread_db* tdbb, jrd_rel* relation, ULONG length)
{
	auto format = MET_current(tdbb, relation);

	while (format->fmt_length != length && format->fmt_version)
		format = MET_format(tdbb, relation, format->fmt_version - 1);

	if (format->fmt_length != length)
	{
		raiseError("Record format with length %u is not found for table %s",
				   length, relation->rel_name.c_str());
	}

	return format;
}

void Applier::doInsert(thread_db* tdbb, record_param* rpb, jrd_tra* transaction)
{
	fb_assert(!(transaction->tra_flags & TRA_system));

	const auto record = rpb->rpb_record;
	const auto format = record->getFormat();
	const auto relation = rpb->rpb_relation;

	RLCK_reserve_relation(tdbb, transaction, relation, true);

	for (USHORT id = 0; id < format->fmt_count; id++)
	{
		dsc desc;
		if (DTYPE_IS_BLOB(format->fmt_desc[id].dsc_dtype) &&
			EVL_field(NULL, record, id, &desc))
		{
			const auto blobId = (bid*) desc.dsc_address;

			if (!blobId->isEmpty())
			{
				bool found = false;

				const auto numericId = blobId->get_permanent_number().getValue();

				ReplBlobMap::Accessor accessor(&transaction->tra_repl_blobs);
				if (accessor.locate(numericId) &&
					transaction->tra_blobs->locate(accessor.current()->second))
				{
					const auto current = &transaction->tra_blobs->current();

					if (!current->bli_materialized)
					{
						const auto blob = current->bli_blob_object;
						fb_assert(blob);
						blob->blb_relation = relation;
						blob->blb_sub_type = desc.getBlobSubType();
						blob->blb_charset = desc.getCharSet();
						blobId->set_permanent(relation->rel_id, DPM_store_blob(tdbb, blob, record));
						current->bli_materialized = true;
						current->bli_blob_id = *blobId;
						transaction->tra_blobs->fastRemove();
						accessor.fastRemove();
						found = true;
					}
				}

				if (!found)
				{
					const ULONG num1 = blobId->bid_quad.bid_quad_high;
					const ULONG num2 = blobId->bid_quad.bid_quad_low;
					raiseError("Blob %u.%u is not found for table %s",
							   num1, num2, relation->rel_name.c_str());
				}
			}
		}
	}

	Savepoint::ChangeMarker marker(transaction->tra_save_point);

	VIO_store(tdbb, rpb, transaction);
	IDX_store(tdbb, rpb, transaction);
	REPL_store(tdbb, rpb, transaction);
}

void Applier::doUpdate(thread_db* tdbb, record_param* orgRpb, record_param* newRpb,
					   jrd_tra* transaction, BlobList* blobs)
{
	fb_assert(!(transaction->tra_flags & TRA_system));

	const auto orgRecord = orgRpb->rpb_record;
	const auto newRecord = newRpb->rpb_record;
	const auto format = newRecord->getFormat();
	const auto relation = newRpb->rpb_relation;

	RLCK_reserve_relation(tdbb, transaction, relation, true);

	for (USHORT id = 0; id < format->fmt_count; id++)
	{
		dsc desc;
		if (DTYPE_IS_BLOB(format->fmt_desc[id].dsc_dtype) &&
			EVL_field(NULL, newRecord, id, &desc))
		{
			const auto dstBlobId = (bid*) desc.dsc_address;
			const auto srcBlobId = (blobs && id < blobs->getCount()) ? (bid*) &(*blobs)[id] : NULL;

			if (!dstBlobId->isEmpty())
			{
				const bool same_blobs = (srcBlobId && *srcBlobId == *dstBlobId);

				if (same_blobs)
				{
					if (EVL_field(NULL, orgRecord, id, &desc))
						*dstBlobId = *(bid*) desc.dsc_address;
					else
						dstBlobId->clear();
				}
				else
				{
					bool found = false;

					const auto numericId = dstBlobId->get_permanent_number().getValue();

					ReplBlobMap::Accessor accessor(&transaction->tra_repl_blobs);
					if (accessor.locate(numericId) &&
						transaction->tra_blobs->locate(accessor.current()->second))
					{
						const auto current = &transaction->tra_blobs->current();

						if (!current->bli_materialized)
						{
							const auto blob = current->bli_blob_object;
							fb_assert(blob);
							blob->blb_relation = relation;
							blob->blb_sub_type = desc.getBlobSubType();
							blob->blb_charset = desc.getCharSet();
							dstBlobId->set_permanent(relation->rel_id, DPM_store_blob(tdbb, blob, newRecord));
							current->bli_materialized = true;
							current->bli_blob_id = *dstBlobId;
							transaction->tra_blobs->fastRemove();
							accessor.fastRemove();
							found = true;
						}
					}

					if (!found)
					{
						const ULONG num1 = dstBlobId->bid_quad.bid_quad_high;
						const ULONG num2 = dstBlobId->bid_quad.bid_quad_low;
						raiseError("Blob %u.%u is not found for table %s",
								   num1, num2, relation->rel_name.c_str());
					}
				}
			}
		}
	}

	Savepoint::ChangeMarker marker(transaction->tra_save_point);

	VIO_modify(tdbb, orgRpb, newRpb, transaction);
	IDX_modify(tdbb, orgRpb, newRpb, transaction);
	REPL_modify(tdbb, orgRpb, newRpb, transaction);
}

void Applier::doDelete(thread_db* tdbb, record_param* rpb, jrd_tra* transaction)
{
	fb_assert(!(transaction->tra_flags & TRA_system));

	RLCK_reserve_relation(tdbb, transaction, rpb->rpb_relation, true);

	Savepoint::ChangeMarker marker(transaction->tra_save_point);

	VIO_erase(tdbb, rpb, transaction);
	REPL_erase(tdbb, rpb, transaction);
}

void Applier::logConflict(const char* msg, ...)
{
#ifdef LOG_CONFLICTS
	char buffer[BUFFER_LARGE];

	va_list ptr;
	va_start(ptr, msg);
	vsprintf(buffer, msg, ptr);
	va_end(ptr);

	logReplicaWarning(m_database, buffer);
#endif
}
