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
#include "../../common/classes/BlobWrapper.h"

#include "Config.h"
#include "Replicator.h"
#include "Utils.h"

using namespace Firebird;
using namespace Jrd;
using namespace Replication;


Replicator::Replicator(MemoryPool& pool,
					   Manager* manager,
					   const Guid& guid,
					   const MetaString& user)
	: m_manager(manager),
	  m_config(manager->getConfig()),
	  m_guid(guid),
	  m_user(user),
	  m_transactions(pool),
	  m_generators(pool)
{
}

void Replicator::flush(BatchBlock& block, FlushReason reason, ULONG flags)
{
	const auto traNumber = block.header.traNumber;

	const auto length = (ULONG) (block.buffer->getCount() - sizeof(Block));
	fb_assert(length);

	block.header.protocol = PROTOCOL_CURRENT_VERSION;
	block.header.flags |= flags;
	block.header.length = length;

	// Re-write the updated header

	memcpy(block.buffer->begin(), &block.header, sizeof(Block));

	// Pass the buffer to the replication manager and setup the new one

	const auto sync = (reason == FLUSH_SYNC);
	const auto prepare = (reason == FLUSH_PREPARE);
	m_manager->flush(block.buffer, sync, prepare);

	memset(&block.header, 0, sizeof(Block));
	block.header.traNumber = traNumber;

	block.atoms.clear();
	block.lastAtom = MAX_ULONG;
	block.buffer = m_manager->getBuffer();
	block.flushes++;
}

void Replicator::storeBlob(Transaction* transaction, ISC_QUAD blobId)
{
	FbLocalStatus localStatus;

	// We need the blob exactly as stored, without possible transliteration to the connection charset
	const UCHAR bpb[] = {isc_bpb_version1, isc_bpb_target_interp, 1, CS_NONE};

	BlobWrapper blob(&localStatus);
	if (!blob.open(m_attachment, transaction->getInterface(), blobId, sizeof(bpb), bpb))
		localStatus.raise();

	UCharBuffer buffer;
	const auto bufferLength = MAX_USHORT;
	auto data = buffer.getBuffer(bufferLength);

	auto& txnData = transaction->getData();
	bool newOp = true;

	FB_SIZE_T segmentLength;
	while (blob.getSegment(bufferLength, data, segmentLength))
	{
		if (!segmentLength)
			continue; // Zero-length segments are unusual but OK

		if (newOp)
		{
			txnData.putTag(opStoreBlob);
			txnData.putInt32(blobId.gds_quad_high);
			txnData.putInt32(blobId.gds_quad_low);
			newOp = false;
		}

		fb_assert(segmentLength <= MAX_USHORT);
		txnData.putInt16(segmentLength);
		txnData.putBinary(segmentLength, data);

		if (txnData.getSize() > m_config->bufferSize)
		{
			flush(txnData, FLUSH_OVERFLOW);
			newOp = true;
		}
	}

	localStatus.check();

	blob.close();

	if (newOp)
	{
		txnData.putTag(opStoreBlob);
		txnData.putInt32(blobId.gds_quad_high);
		txnData.putInt32(blobId.gds_quad_low);
	}

	txnData.putInt16(0); // end-of-blob marker

	if (txnData.getSize() > m_config->bufferSize)
		flush(txnData, FLUSH_OVERFLOW);
}

void Replicator::releaseTransaction(Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();
		m_manager->releaseBuffer(txnData.buffer);

		FB_SIZE_T pos;
		if (m_transactions.find(transaction, pos))
			m_transactions.remove(pos);
	}
	catch (const Exception&)
	{} // no-op
}

// IReplicatedSession implementation

IReplicatedTransaction* Replicator::startTransaction(CheckStatusWrapper* status, ITransaction* trans, SINT64 number)
{
	AutoPtr<Transaction> transaction;

	try
	{
		MemoryPool& pool = getPool();
		transaction = FB_NEW_POOL(pool) Transaction(this, trans);
		m_transactions.add(transaction);

		auto& txnData = transaction->getData();

		fb_assert(!txnData.header.traNumber);
		txnData.header.traNumber = number;
		txnData.header.flags = BLOCK_BEGIN_TRANS;

		txnData.buffer = m_manager->getBuffer();

		txnData.putTag(opStartTransaction);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return transaction.release();
}

void Replicator::prepareTransaction(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		txnData.putTag(opPrepareTransaction);

		flush(txnData, FLUSH_PREPARE);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::commitTransaction(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		// Assert this transaction being de-facto dirty
		const auto dataLength = txnData.buffer->getCount() - sizeof(Block);
		fb_assert(txnData.flushes || dataLength > sizeof(UCHAR));

		for (const auto& generator : m_generators)
		{
			fb_assert(generator.name.hasData());

			const auto atom = txnData.defineAtom(generator.name);

			txnData.putTag(opSetSequence);
			txnData.putInt32(atom);
			txnData.putInt64(generator.value);
		}

		m_generators.clear();

		txnData.putTag(opCommitTransaction);
		flush(txnData, FLUSH_SYNC, BLOCK_END_TRANS);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::rollbackTransaction(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		if (txnData.flushes)
		{
			txnData.putTag(opRollbackTransaction);
			flush(txnData, FLUSH_SYNC, BLOCK_END_TRANS);
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::startSavepoint(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		txnData.putTag(opStartSavepoint);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::releaseSavepoint(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		txnData.putTag(opReleaseSavepoint);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::rollbackSavepoint(CheckStatusWrapper* status, Transaction* transaction)
{
	try
	{
		auto& txnData = transaction->getData();

		txnData.putTag(opRollbackSavepoint);

		flush(txnData, FLUSH_SYNC);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::insertRecord(CheckStatusWrapper* status,
							  Transaction* transaction,
							  const char* relName,
							  IReplicatedRecord* record)
{
	try
	{
		for (unsigned id = 0; id < record->getCount(); id++)
		{
			IReplicatedField* field = record->getField(id);
			if (field != nullptr)
			{
				auto type = field->getType();
				if (type == SQL_ARRAY || type == SQL_BLOB)
				{
					const auto blobId = (ISC_QUAD*) field->getData();

					if (blobId)
						storeBlob(transaction, *blobId);
				}
			}
		}

		const auto length = record->getRawLength();
		const auto data = record->getRawData();

		auto& txnData = transaction->getData();

		const auto atom = txnData.defineAtom(relName);

		txnData.putTag(opInsertRecord);
		txnData.putInt32(atom);
		txnData.putInt32(length);
		txnData.putBinary(length, data);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::updateRecord(CheckStatusWrapper* status,
							  Transaction* transaction,
							  const char* relName,
							  IReplicatedRecord* orgRecord,
							  IReplicatedRecord* newRecord)
{
	try
	{
		for (unsigned id = 0; id < newRecord->getCount(); id++)
		{
			IReplicatedField* field = newRecord->getField(id);
			if (field != nullptr)
			{
				auto type = field->getType();
				if (type == SQL_ARRAY || type == SQL_BLOB)
				{
					const auto blobId = (ISC_QUAD*) field->getData();

					if (blobId)
						storeBlob(transaction, *blobId);
				}
			}
		}

		const auto orgLength = orgRecord->getRawLength();
		const auto orgData = orgRecord->getRawData();

		const auto newLength = newRecord->getRawLength();
		const auto newData = newRecord->getRawData();

		auto& txnData = transaction->getData();

		const auto atom = txnData.defineAtom(relName);

		txnData.putTag(opUpdateRecord);
		txnData.putInt32(atom);
		txnData.putInt32(orgLength);
		txnData.putBinary(orgLength, orgData);
		txnData.putInt32(newLength);
		txnData.putBinary(newLength, newData);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::deleteRecord(CheckStatusWrapper* status,
							  Transaction* transaction,
							  const char* relName,
							  IReplicatedRecord* record)
{
	try
	{
		const auto length = record->getRawLength();
		const auto data = record->getRawData();

		auto& txnData = transaction->getData();

		const auto atom = txnData.defineAtom(relName);

		txnData.putTag(opDeleteRecord);
		txnData.putInt32(atom);
		txnData.putInt32(length);
		txnData.putBinary(length, data);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::executeSqlIntl(CheckStatusWrapper* status,
								Transaction* transaction,
								unsigned charset,
								const char* sql)
{
	try
	{
		auto& txnData = transaction->getData();

		const auto atom = txnData.defineAtom(m_user);

		txnData.putTag(opExecuteSqlIntl);
		txnData.putInt32(atom);
		txnData.putByte(charset);
		txnData.putString(sql);

		if (txnData.getSize() > m_config->bufferSize)
			flush(txnData, FLUSH_OVERFLOW);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::cleanupTransaction(CheckStatusWrapper* status,
									SINT64 number)
{
	try
	{
		BatchBlock block(getPool());
		block.header.traNumber = number;
		block.buffer = m_manager->getBuffer();
		block.putTag(opCleanupTransaction);

		flush(block, FLUSH_SYNC, BLOCK_END_TRANS);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void Replicator::setSequence(CheckStatusWrapper* status,
							 const char* genName,
							 SINT64 value)
{
	try
	{
		for (auto& generator : m_generators)
		{
			if (generator.name == genName)
			{
				generator.value = value;
				return;
			}
		}

		GeneratorValue generator;
		generator.name = genName;
		generator.value = value;

		m_generators.add(generator);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}
