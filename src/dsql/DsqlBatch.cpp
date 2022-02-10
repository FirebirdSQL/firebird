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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2017 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ________________________________
 */

#include "firebird.h"

#include "../dsql/DsqlBatch.h"

#include "../jrd/EngineInterface.h"
#include "../jrd/jrd.h"
#include "../jrd/status.h"
#include "../jrd/exe_proto.h"
#include "../dsql/dsql.h"
#include "../dsql/errd_proto.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/utils_proto.h"
#include "../common/classes/BatchCompletionState.h"

using namespace Firebird;
using namespace Jrd;

namespace {
	const char* const TEMP_NAME = "fb_batch";
	const UCHAR initBlobParameters[] = {isc_bpb_version1, isc_bpb_type, 1, isc_bpb_type_stream};

	class JTransliterate : public Firebird::Transliterate
	{
	public:
		JTransliterate(thread_db* tdbb)
			: m_tdbb(tdbb)
		{ }

		void transliterate(IStatus* status)
		{
			JRD_transliterate(m_tdbb, status);
		}

	private:
		thread_db* m_tdbb;
	};
}

DsqlBatch::DsqlBatch(DsqlDmlRequest* req, const dsql_msg* /*message*/, IMessageMetadata* inMeta, ClumpletReader& pb)
	: m_dsqlRequest(req),
	  m_batch(NULL),
	  m_meta(inMeta),
	  m_messages(m_dsqlRequest->getPool()),
	  m_blobs(m_dsqlRequest->getPool()),
	  m_blobMap(m_dsqlRequest->getPool()),
	  m_blobMeta(m_dsqlRequest->getPool()),
	  m_defaultBpb(m_dsqlRequest->getPool()),
	  m_messageSize(0),
	  m_alignedMessage(0),
	  m_alignment(0),
	  m_flags(0),
	  m_detailed(DETAILED_LIMIT),
	  m_bufferSize(BUFFER_LIMIT),
	  m_lastBlob(MAX_ULONG),
	  m_setBlobSize(false),
	  m_blobPolicy(IBatch::BLOB_NONE)
{
	memset(&m_genId, 0, sizeof(m_genId));

	FbLocalStatus st;
	m_messageSize = m_meta->getMessageLength(&st);
	m_alignedMessage = m_meta->getAlignedLength(&st);
	m_alignment = m_meta->getAlignment(&st);
	check(&st);

	for (pb.rewind(); !pb.isEof(); pb.moveNext())
	{
		UCHAR t = pb.getClumpTag();

		switch (t)
		{
		case IBatch::TAG_MULTIERROR:
		case IBatch::TAG_RECORD_COUNTS:
			setFlag(t, pb.getInt());
			break;

		case IBatch::TAG_BLOB_POLICY:
			m_blobPolicy = pb.getInt();

			switch (m_blobPolicy)
			{
			case IBatch::BLOB_ID_ENGINE:
			case IBatch::BLOB_ID_USER:
			case IBatch::BLOB_STREAM:
				break;
			default:
				m_blobPolicy = IBatch::BLOB_NONE;
				break;
			}

			break;

		case IBatch::TAG_DETAILED_ERRORS:
			m_detailed = pb.getInt();
			if (m_detailed > DETAILED_LIMIT * 4)
				m_detailed = DETAILED_LIMIT * 4;
			break;

		case IBatch::TAG_BUFFER_BYTES_SIZE:
			m_bufferSize = pb.getInt();
			if (m_bufferSize > HARD_BUFFER_LIMIT)
				m_bufferSize = HARD_BUFFER_LIMIT;
			if (!m_bufferSize)
				m_bufferSize = HARD_BUFFER_LIMIT;
			break;
		}
	}

	// parse message to detect blobs
	unsigned fieldsCount = m_meta->getCount(&st);
	check(&st);

	for (unsigned i = 0; i < fieldsCount; ++i)
	{
		unsigned t = m_meta->getType(&st, i);
		check(&st);

		switch (t)
		{
		case SQL_BLOB:
			{
				BlobMeta bm;
				bm.offset = m_meta->getOffset(&st, i);
				check(&st);
				bm.nullOffset = m_meta->getNullOffset(&st, i);
				check(&st);
				m_blobMeta.push(bm);
			}
			break;
		}
	}

	// allocate data buffers
	m_messages.setBuf(m_bufferSize, MAX(m_alignedMessage * 2, RAM_BATCH));
	if (m_blobMeta.hasData())
		m_blobs.setBuf(m_bufferSize, RAM_BATCH);

	// assign initial default BPB
	setDefBpb(FB_NELEM(initBlobParameters), initBlobParameters);
}


DsqlBatch::~DsqlBatch()
{
	if (m_batch)
		m_batch->resetHandle();
	if (m_dsqlRequest)
		m_dsqlRequest->req_batch = NULL;
}

Attachment* DsqlBatch::getAttachment() const
{
	return m_dsqlRequest->req_dbb->dbb_attachment;
}

void DsqlBatch::setInterfacePtr(JBatch* interfacePtr) throw()
{
	fb_assert(!m_batch);
	m_batch = interfacePtr;
}

DsqlBatch* DsqlBatch::open(thread_db* tdbb, DsqlDmlRequest* req, IMessageMetadata* inMetadata,
	unsigned parLength, const UCHAR* par)
{
	SET_TDBB(tdbb);
	Jrd::ContextPoolHolder context(tdbb, &req->getPool());

	// Validate cursor or batch being not already open

	if (req->req_cursor)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_cursor_open_err));
	}

	if (req->req_batch)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_batch_open));
	}

	// Sanity checks before creating batch

	if (!req->getRequest())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
				  Arg::Gds(isc_unprepared_stmt));
	}

	const auto statement = req->getDsqlStatement();

	if (statement->getFlags() & DsqlStatement::FLAG_ORPHAN)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
		          Arg::Gds(isc_bad_req_handle));
	}

	switch (statement->getType())
	{
		case DsqlStatement::TYPE_INSERT:
		case DsqlStatement::TYPE_DELETE:
		case DsqlStatement::TYPE_UPDATE:
		case DsqlStatement::TYPE_EXEC_PROCEDURE:
		case DsqlStatement::TYPE_EXEC_BLOCK:
			break;

		default:
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
					  Arg::Gds(isc_batch_type));
	}

	const dsql_msg* message = statement->getSendMsg();
	if (! (inMetadata && message && req->parseMetadata(inMetadata, message->msg_parameters)))
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
				  Arg::Gds(isc_batch_param));
	}

	// Open reader for parameters block

	ClumpletReader pb(ClumpletReader::WideTagged, par, parLength);
	if (pb.getBufferLength() && (pb.getBufferTag() != IBatch::VERSION1))
		ERRD_post(Arg::Gds(isc_batch_param_version));

	// Create batch

	DsqlBatch* b = FB_NEW_POOL(req->getPool()) DsqlBatch(req, message, inMetadata, pb);
	req->req_batch = b;
	return b;
}

IMessageMetadata* DsqlBatch::getMetadata(thread_db* tdbb)
{
	m_meta->addRef();
	return m_meta;
}

void DsqlBatch::add(thread_db* tdbb, ULONG count, const void* inBuffer)
{
	if (!count)
		return;
	m_messages.align(m_alignment);
	m_messages.put(inBuffer, (count - 1) * m_alignedMessage + m_messageSize);
	DEB_BATCH(fprintf(stderr, "Put to batch %d messages\n", count));
}

void DsqlBatch::blobCheckMeta()
{
	if (!m_blobMeta.hasData())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_batch_blobs));
	}
}

void DsqlBatch::blobCheckMode(bool stream, const char* fname)
{
	blobCheckMeta();

	switch (m_blobPolicy)
	{
	case IBatch::BLOB_ID_ENGINE:
	case IBatch::BLOB_ID_USER:
		if (!stream)
			return;
		break;
	case IBatch::BLOB_STREAM:
		if (stream)
			return;
		break;
	}

	ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
		Arg::Gds(isc_batch_policy) << fname);
}

void DsqlBatch::blobSetSize()
{
	// Store size of previous blob if it was changed by appendBlobData()
	unsigned blobSize = m_blobs.getSize();
	if (m_setBlobSize)
	{
		blobSize -= (m_lastBlob + SIZEOF_BLOB_HEAD);
		m_blobs.put3(&blobSize, sizeof(blobSize), m_lastBlob + sizeof(ISC_QUAD));
		m_setBlobSize = false;
	}
}

void DsqlBatch::blobPrepare()
{
	blobSetSize();

	// Align blob stream
	m_blobs.align(BLOB_STREAM_ALIGN);
}

void DsqlBatch::setDefaultBpb(thread_db* tdbb, unsigned parLength, const unsigned char* par)
{
	if (m_blobs.getSize())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_batch_defbpb));
	}
	setDefBpb(parLength, par);
}

void DsqlBatch::setDefBpb(unsigned parLength, const unsigned char* par)
{
	m_defaultBpb.clear();
	m_defaultBpb.add(par, parLength);
	setFlag(FLAG_DEFAULT_SEGMENTED, fb_utils::isBpbSegmented(m_defaultBpb.getCount(), m_defaultBpb.begin()));
}

void DsqlBatch::addBlob(thread_db* tdbb, ULONG length, const void* inBuffer, ISC_QUAD* blobId,
	unsigned parLength, const unsigned char* par)
{
	blobCheckMode(false, "addBlob");
	blobPrepare();

	// Get ready to appendBlobData()
	m_lastBlob = m_blobs.getSize();
	fb_assert(m_lastBlob % BLOB_STREAM_ALIGN == 0);

	// Generate auto blob ID if needed
	if (m_blobPolicy == IBatch::BLOB_ID_ENGINE)
		genBlobId(blobId);

	// Determine type of current blob
	setFlag(FLAG_CURRENT_SEGMENTED, parLength ? fb_utils::isBpbSegmented(parLength, par) : m_flags & (1 << FLAG_DEFAULT_SEGMENTED));

	// Store header
	m_blobs.put(blobId, sizeof(ISC_QUAD));
	ULONG fullLength = length + parLength;
	m_blobs.put(&fullLength, sizeof(ULONG));
	m_blobs.put(&parLength, sizeof(ULONG));

	// Store BPB
	if (parLength)
		m_blobs.put(par, parLength);

	// Finally store user data
	putSegment(length, inBuffer);
}

void DsqlBatch::appendBlobData(thread_db* tdbb, ULONG length, const void* inBuffer)
{
	blobCheckMode(false, "appendBlobData");

	if (m_lastBlob == MAX_ULONG)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_batch_blob_append));
	}

	m_setBlobSize = true;
	putSegment(length, inBuffer);
}

void DsqlBatch::putSegment(ULONG length, const void* inBuffer)
{
	if (m_flags & (1 << FLAG_CURRENT_SEGMENTED))
	{
		if (length > MAX_USHORT)
		{
			ERR_post(Arg::Gds(isc_imp_exc) << Arg::Gds(isc_blobtoobig) <<
					 Arg::Gds(isc_big_segment) << Arg::Num(length));
		}
		USHORT l = length;
		m_blobs.align(IBatch::BLOB_SEGHDR_ALIGN);
		m_blobs.put(&l, sizeof(l));
		m_setBlobSize = true;
	}
	m_blobs.put(inBuffer, length);
}

void DsqlBatch::addBlobStream(thread_db* tdbb, unsigned length, const void* inBuffer)
{
	// Sanity checks
	if (length == 0)
		return;
	if (length % BLOB_STREAM_ALIGN)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_batch_stream_align));
	}

	blobCheckMode(true, "addBlobStream");
	blobPrepare();

	// We have no idea where is the last blob located in the stream
	m_lastBlob = MAX_ULONG;

	// store stream for further processing
	DEB_BATCH(fprintf(stderr, "Store stream %d\n", length));
	fb_assert(m_blobs.getSize() % BLOB_STREAM_ALIGN == 0);
	m_blobs.put(inBuffer, length);
}

void DsqlBatch::registerBlob(thread_db*, const ISC_QUAD* existingBlob, ISC_QUAD* blobId)
{
	blobCheckMeta();

	// Generate auto blob ID if needed
	if (m_blobPolicy == IBatch::BLOB_ID_ENGINE)
		genBlobId(blobId);

	registerBlob(existingBlob, blobId);
}

void DsqlBatch::registerBlob(const ISC_QUAD* engineBlob, const ISC_QUAD* batchBlob)
{
	ISC_QUAD* idPtr = m_blobMap.put(*batchBlob);
	if (!idPtr)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_batch_rpt_blob) << Arg::Quad(batchBlob));
	}

	*idPtr = *engineBlob;
}

Firebird::IBatchCompletionState* DsqlBatch::execute(thread_db* tdbb)
{
	// todo - add new trace event here
	// TraceDSQLExecute trace(req_dbb->dbb_attachment, this);

	jrd_tra* transaction = tdbb->getTransaction();

	// execution timer
	thread_db::TimerGuard timerGuard(tdbb, m_dsqlRequest->setupTimer(tdbb), true);

	// sync internal buffers
	m_messages.done();

	// insert blobs here
	if (m_blobMeta.hasData())
	{
		// This code expects the following to work correctly
		fb_assert(RAM_BATCH % BLOB_STREAM_ALIGN == 0);

		blobSetSize();		// needed after appendBlobData()
		m_blobs.done();		// sync internal buffers

		struct BlobFlow
		{
			ULONG remains;
			UCHAR* data;
			ULONG currentBlobSize;
			ULONG byteCount;

			BlobFlow()
				: remains(0), data(NULL), currentBlobSize(0), byteCount(0)
			{ }

			void newHdr(ULONG blobSize)
			{
				currentBlobSize = blobSize;
				move3(SIZEOF_BLOB_HEAD);
			}

			void move(ULONG step)
			{
				move3(step);
				currentBlobSize -= step;
			}

			bool align(ULONG alignment)
			{
				ULONG a = byteCount % alignment;
				if (a)
				{
					a = alignment - a;
					move3(a);
					if (currentBlobSize)
						currentBlobSize -= a;
				}
				return a;
			}

private:
			void move3(ULONG step)
			{
				data += step;
				byteCount += step;
				remains -= step;
			}
		};
		BlobFlow flow;
		blb* blob = nullptr;
		try
		{
			while ((flow.remains = m_blobs.get(&flow.data)) > 0)
			{
				while (flow.remains)
				{
					// should we get next blob header
					if (!flow.currentBlobSize)
					{
						// align data stream
						if (flow.align(BLOB_STREAM_ALIGN))
							continue;

						// check for partial header in the buffer
						if (flow.remains < SIZEOF_BLOB_HEAD)
							flow.remains = m_blobs.reget(flow.remains, &flow.data, BLOB_STREAM_ALIGN);
						if (flow.remains < SIZEOF_BLOB_HEAD)
						{
							ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
								Arg::Gds(isc_batch_blob_buf) <<
								Arg::Gds(isc_batch_small_data) << "BLOB");
						}

						// parse blob header
						fb_assert(intptr_t(flow.data) % BLOB_STREAM_ALIGN == 0);
						ISC_QUAD batchBlobId = *reinterpret_cast<ISC_QUAD*>(flow.data);
						ULONG* blobSize = reinterpret_cast<ULONG*>(flow.data + sizeof(ISC_QUAD));
						ULONG* bpbSize = reinterpret_cast<ULONG*>(flow.data + sizeof(ISC_QUAD) + sizeof(ULONG));
						flow.newHdr(*blobSize);
						ULONG currentBpbSize = *bpbSize;

						if (batchBlobId.gds_quad_high == 0 && batchBlobId.gds_quad_low == 0)
						{
							// Sanity check
							if (*bpbSize)
							{
								ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
									Arg::Gds(isc_batch_blob_buf) << Arg::Gds(isc_batch_cont_bpb));
							}
						}
						else
						{
							// get BPB
							Bpb localBpb;
							Bpb* bpb;
							bool segmentedMode;
							if (currentBpbSize)
							{
								if (currentBpbSize > flow.remains)
									flow.remains = m_blobs.reget(flow.remains, &flow.data, BLOB_STREAM_ALIGN);
								if (currentBpbSize > flow.remains)
								{
									ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
										Arg::Gds(isc_batch_blob_buf) <<
										Arg::Gds(isc_batch_big_bpb) << Arg::Num(currentBpbSize) << Arg::Num(flow.remains));
								}
								localBpb.add(flow.data, currentBpbSize);
								bpb = &localBpb;
								segmentedMode = fb_utils::isBpbSegmented(currentBpbSize, flow.data);
								flow.move(currentBpbSize);
							}
							else
							{
								bpb = &m_defaultBpb;
								segmentedMode = m_flags & (1 << FLAG_DEFAULT_SEGMENTED);
							}
							setFlag(FLAG_CURRENT_SEGMENTED, segmentedMode);

							// create blob
							if (blob)
							{
								blob->BLB_close(tdbb);
								blob = nullptr;
							}
							bid engineBlobId;
							blob = blb::create2(tdbb, transaction, &engineBlobId, bpb->getCount(),
								bpb->begin(), true);

							//DEB_BATCH(fprintf(stderr, "B-ID: (%x,%x)\n", batchBlobId.gds_quad_high, batchBlobId.gds_quad_low));
							registerBlob(reinterpret_cast<ISC_QUAD*>(&engineBlobId), &batchBlobId);
						}
					}

					// store data
					ULONG dataSize = MIN(flow.currentBlobSize, flow.remains);
					if (dataSize)
					{
						if (m_flags & (1 << FLAG_CURRENT_SEGMENTED))
						{
							if (flow.align(IBatch::BLOB_SEGHDR_ALIGN))
								continue;

							fb_assert(dataSize >= sizeof(USHORT));
							USHORT* segSize = reinterpret_cast<USHORT*>(flow.data);
							flow.move(sizeof(USHORT));

							dataSize = *segSize;
							if (dataSize > flow.currentBlobSize)
							{
								ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
									Arg::Gds(isc_batch_blob_buf) <<
									Arg::Gds(isc_batch_big_segment) << Arg::Num(dataSize) << Arg::Num(flow.currentBlobSize));
							}
							if (dataSize > flow.remains)
							{
								flow.remains = m_blobs.reget(flow.remains, &flow.data, BLOB_STREAM_ALIGN);
								if (dataSize > flow.remains)
								{
									ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
									Arg::Gds(isc_batch_blob_buf) <<
									Arg::Gds(isc_batch_big_seg2) << Arg::Num(dataSize) << Arg::Num(flow.remains));
								}
							}
						}

						blob->BLB_put_segment(tdbb, flow.data, dataSize);
						flow.move(dataSize);
					}
				}

				m_blobs.remained(0);
			}

			if (blob)
			{
				blob->BLB_close(tdbb);
				blob = nullptr;
			}
		}
		catch (const Exception&)
		{
			if (blob)
				blob->BLB_cancel(tdbb);
			cancel(tdbb);

			throw;
		}
	}

	// execute request
	m_dsqlRequest->req_transaction = transaction;
	jrd_req* req = m_dsqlRequest->getRequest();
	fb_assert(req);

	// prepare completion interface
	AutoPtr<BatchCompletionState, SimpleDispose> completionState
		(FB_NEW BatchCompletionState(m_flags & (1 << IBatch::TAG_RECORD_COUNTS), m_detailed));
	AutoSetRestore<bool> batchFlag(&req->req_batch_mode, true);
	const dsql_msg* message = m_dsqlRequest->getDsqlStatement()->getSendMsg();
	bool startRequest = true;

	bool isExecBlock = m_dsqlRequest->getDsqlStatement()->getType() == DsqlStatement::TYPE_EXEC_BLOCK;
	const auto receiveMessage = isExecBlock ? m_dsqlRequest->getDsqlStatement()->getReceiveMsg() : nullptr;
	auto receiveMsgBuffer = isExecBlock ? m_dsqlRequest->req_msg_buffers[receiveMessage->msg_buffer_number] : nullptr;

	// process messages
	ULONG remains;
	UCHAR* data;
	while ((remains = m_messages.get(&data)) > 0)
	{
		if (remains < m_messageSize)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				Arg::Gds(isc_batch_blob_buf) <<
				Arg::Gds(isc_batch_small_data) << "messages");
		}

		while (remains >= m_messageSize)
		{
			// skip alignment data
			UCHAR* alignedData = FB_ALIGN(data, m_alignment);
			if (alignedData != data)
			{
				remains -= (alignedData - data);
				data = alignedData;
				continue;
			}

			if (startRequest)
			{
				EXE_unwind(tdbb, req);
				EXE_start(tdbb, req, transaction);
				startRequest = isExecBlock;
			}

			// translate blob IDs
			fb_assert(intptr_t(data) % m_alignment == 0);
			for (unsigned i = 0; i < m_blobMeta.getCount(); ++i)
			{
				const SSHORT* nullFlag = reinterpret_cast<const SSHORT*>(&data[m_blobMeta[i].nullOffset]);
				if (*nullFlag)
					continue;

				ISC_QUAD* id = reinterpret_cast<ISC_QUAD*>(&data[m_blobMeta[i].offset]);
				if (id->gds_quad_high == 0 && id->gds_quad_low == 0)
					continue;

				ISC_QUAD newId;
				if (!m_blobMap.get(*id, newId))
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						Arg::Gds(isc_batch_blob_id) << Arg::Quad(id));
				}

				m_blobMap.remove(*id);
				*id = newId;
			}

			// map message to internal engine format
			m_dsqlRequest->mapInOut(tdbb, false, message, m_meta, NULL, data);
			data += m_messageSize;
			remains -= m_messageSize;

			UCHAR* msgBuffer = m_dsqlRequest->req_msg_buffers[message->msg_buffer_number];
			try
			{
				// runsend data to request and collect stats
				ULONG before = req->req_records_inserted + req->req_records_updated +
					req->req_records_deleted;
				EXE_send(tdbb, req, message->msg_number, message->msg_length, msgBuffer);
				ULONG after = req->req_records_inserted + req->req_records_updated +
					req->req_records_deleted;
				completionState->regUpdate(after - before);

				if (isExecBlock)
					EXE_receive(tdbb, req, receiveMessage->msg_number, receiveMessage->msg_length, receiveMsgBuffer);
			}
			catch (const Exception& ex)
			{
				FbLocalStatus status;
				ex.stuffException(&status);
				tdbb->tdbb_status_vector->init();

				JTransliterate trLit(tdbb);
				completionState->regError(&status, &trLit);

				if (!(m_flags & (1 << IBatch::TAG_MULTIERROR)))
				{
					cancel(tdbb);
					remains = 0;
					break;
				}

				startRequest = true;
			}
		}

		UCHAR* alignedData = FB_ALIGN(data, m_alignment);
		m_messages.remained(remains, alignedData - data);
	}

	DEB_BATCH(fprintf(stderr, "Sent %d messages\n", completionState->getSize(tdbb->tdbb_status_vector)));

	// make sure all blobs were used in messages
	if (m_blobMap.count())
	{
		DEB_BATCH(fprintf(stderr, "BLOBs %d were not used in messages\n", m_blobMap.count()));
		ERR_post_warning(Arg::Warning(isc_random) << "m_blobMap.count() BLOBs were not used in messages");		// !!!!!!! new warning
	}

	// reset to initial state
	cancel(tdbb);

	return completionState.release();
}

void DsqlBatch::cancel(thread_db* tdbb)
{
	m_messages.clear();
	m_blobs.clear();
	m_setBlobSize = false;
	m_lastBlob = MAX_ULONG;
	memset(&m_genId, 0, sizeof(m_genId));
	m_blobMap.clear();
}

void DsqlBatch::genBlobId(ISC_QUAD* blobId)
{
	if (++m_genId.gds_quad_low == 0)
		++m_genId.gds_quad_high;
	memcpy(blobId, &m_genId, sizeof(m_genId));
}

void DsqlBatch::DataCache::setBuf(ULONG size, ULONG cacheCapacity)
{
	m_limit = size;

	fb_assert(m_cacheCapacity == 0);
	fb_assert(cacheCapacity >= RAM_BATCH);
	m_cacheCapacity = cacheCapacity;
}

void DsqlBatch::DataCache::put3(const void* data, ULONG dataSize, ULONG offset)
{
	// This assertion guarantees that data always fits as a whole into m_cache or m_space,
	// never placed half in one storage, half - in another.
	fb_assert((DsqlBatch::RAM_BATCH % dataSize == 0) && (offset % dataSize == 0));

	if (offset >= m_used)
	{
		// data in cache
		UCHAR* to = m_cache.begin();
		to += (offset - m_used);
		fb_assert(to < m_cache.end());
		memcpy(to, data, dataSize);
	}
	else
	{
		const FB_UINT64 writtenBytes = m_space->write(offset, data, dataSize);
		fb_assert(writtenBytes == dataSize);
	}
}

void DsqlBatch::DataCache::put(const void* d, ULONG dataSize)
{
	if (m_used + m_cache.getCount() + dataSize > m_limit)
		ERR_post(Arg::Gds(isc_batch_too_big));

	const UCHAR* data = reinterpret_cast<const UCHAR*>(d);

	// Coefficient affecting direct data write to tempspace
	const ULONG K = 4;

	// ensure ram cache presence
	fb_assert(m_cacheCapacity);

	// swap to secondary cache if needed
	if (m_cache.getCount() + dataSize > m_cacheCapacity)
	{
		// store data in the end of ram cache if needed
		// avoid copy in case of huge buffer passed
		ULONG delta = m_cacheCapacity - m_cache.getCount();
		if (dataSize - delta < m_cacheCapacity / K)
		{
			m_cache.append(data, delta);
			data += delta;
			dataSize -= delta;
		}

		// swap ram cache to tempspace
		if (!m_space)
			m_space = FB_NEW_POOL(getPool()) TempSpace(getPool(), TEMP_NAME);

		const FB_UINT64 writtenBytes = m_space->write(m_used, m_cache.begin(), m_cache.getCount());
		fb_assert(writtenBytes == m_cache.getCount());
		m_used += m_cache.getCount();
		m_cache.clear();

		// in a case of huge buffer write directly to tempspace
		if (dataSize > m_cacheCapacity / K)
		{
			const FB_UINT64 writtenBytes = m_space->write(m_used, data, dataSize);
			fb_assert(writtenBytes == dataSize);
			m_used += dataSize;
			return;
		}
	}

	m_cache.append(data, dataSize);
}

void DsqlBatch::DataCache::align(ULONG alignment)
{
	ULONG a = getSize() % alignment;
	if (a)
	{
		fb_assert(alignment <= sizeof(SINT64));
		SINT64 zero = 0;
		put(&zero, alignment - a);
	}
}

void DsqlBatch::DataCache::done()
{
	if (m_cache.getCount() && m_used)
	{
		fb_assert(m_space);

		const FB_UINT64 writtenBytes = m_space->write(m_used, m_cache.begin(), m_cache.getCount());
		fb_assert(writtenBytes == m_cache.getCount());
		m_used += m_cache.getCount();
		m_cache.clear();
	}
}

ULONG DsqlBatch::DataCache::get(UCHAR** buffer)
{
	if (m_used > m_got)
	{
		// get data from tempspace
		ULONG dlen = m_cache.getCount();
		ULONG delta = m_cacheCapacity - dlen;
		if (delta > m_used - m_got)
			delta = m_used - m_got;
		UCHAR* buf = m_cache.getBuffer(dlen + delta);
		buf += dlen;
		const FB_UINT64 readBytes = m_space->read(m_got, buf, delta);
		fb_assert(readBytes == delta);
		m_got += delta;
	}

	if (m_cache.getCount())
	{
		if (m_shift)
			m_cache.removeCount(0, m_shift);

		// return buffer full of data
		*buffer = m_cache.begin();
		fb_assert(intptr_t(*buffer) % FB_ALIGNMENT == 0);
		return m_cache.getCount();
	}

	// no more data
	*buffer = nullptr;
	return 0;
}

ULONG DsqlBatch::DataCache::reget(ULONG remains, UCHAR** buffer, ULONG alignment)
{
	ULONG a = remains % alignment;
	if (a)
	{
		a = alignment - a;
		remains += a;
	}
	fb_assert(remains < m_cache.getCount());

	m_cache.removeCount(0, m_cache.getCount() - remains);
	ULONG size = get(buffer);
	size -= a;
	*buffer += a;
	return size;
}

void DsqlBatch::DataCache::remained(ULONG size, ULONG alignment)
{
	if (size > alignment)
	{
		size -= alignment;
		alignment = 0;
	}
	else
	{
		alignment -= size;
		size = 0;
	}

	if (!size)
		m_cache.clear();
	else
		m_cache.removeCount(0, m_cache.getCount() - size);

	m_shift = alignment;
}

ULONG DsqlBatch::DataCache::getSize() const
{
	if (!m_cacheCapacity)
		return 0;

	fb_assert((MAX_ULONG - 1) - m_used > m_cache.getCount());
	return m_used + m_cache.getCount();
}

ULONG DsqlBatch::DataCache::getCapacity() const
{
	if (!m_cacheCapacity)
		return 0;

	return m_limit;
}

void DsqlBatch::DataCache::clear()
{
	m_cache.clear();
	if (m_space && m_used)
		m_space->releaseSpace(0, m_used);
	m_used = m_got = m_shift = 0;
}

void DsqlBatch::info(thread_db* tdbb, unsigned int itemsLength, const unsigned char* items,
	unsigned int bufferLength, unsigned char* buffer)
{
	// Sanity check
	if (bufferLength < 3)	// bigger values will be processed by later code OK
	{
		if (bufferLength-- > 0)
		{
			*buffer++ = isc_info_truncated;
			if (bufferLength-- > 0)
				*buffer++ = isc_info_end;
		}
		return;
	}

	ClumpletReader it(ClumpletReader::InfoItems, items, itemsLength);
	ClumpletWriter out(ClumpletReader::InfoResponse, bufferLength - 1);		// place for isc_info_truncated / isc_info_end
	enum BufCloseState {BUF_OPEN, BUF_INTERNAL, BUF_END};
	BufCloseState closeOut = BUF_OPEN;

	try
	{
		bool flInfoLength = false;

		for (it.rewind(); !it.isEof(); it.moveNext())
		{
			UCHAR item = it.getClumpTag();
			if (item == isc_info_end)
				break;

			switch(item)
			{
			case IBatch::INF_BUFFER_BYTES_SIZE:
				out.insertInt(item, m_messages.getCapacity());
				break;
			case IBatch::INF_DATA_BYTES_SIZE:
				out.insertInt(item, FB_ALIGN(m_messages.getSize(), m_alignment));
				break;
			case IBatch::INF_BLOBS_BYTES_SIZE:
				if (m_blobs.getSize())
					out.insertInt(item, m_blobs.getSize());
				break;
			case IBatch::INF_BLOB_ALIGNMENT:
				out.insertInt(item, BLOB_STREAM_ALIGN);
				break;
			case IBatch::INF_BLOB_HEADER:
				out.insertInt(item, SIZEOF_BLOB_HEAD);
				break;
			case isc_info_length:
				flInfoLength = true;
				break;
			default:
				out.insertInt(isc_info_error, isc_infunk);
				break;
			}
		}

		// finalize writer
		closeOut = BUF_INTERNAL;	// finished adding internal info
		out.insertTag(isc_info_end);
		closeOut = BUF_END;			// alreayd marked with isc_info_end but misses isc_info_length
		if (flInfoLength)
		{
			out.rewind();
			out.insertInt(isc_info_length, out.getBufferLength());
		}
	}
	catch(const fatal_exception&)
	{
		// here it's sooner of all caused by writer overflow but carefully check that
		if (out.hasOverflow())
		{
			memcpy(buffer, out.getBuffer(), out.getBufferLength());
			buffer += out.getBufferLength();
			switch (closeOut)
			{
			case BUF_OPEN:
				*buffer++ = isc_info_truncated;
				if (out.getBufferLength() <= bufferLength - 2)
					*buffer++ = isc_info_end;
				break;
			case BUF_INTERNAL:
				// overflow adding isc_info_end, but we actually have 1 reserved byte
				*buffer++ = isc_info_end;
				break;
			case BUF_END:
				// ignore isc_info_length
				break;
			}
			return;
		}
		else
			throw;
	}

	memcpy(buffer, out.getBuffer(), out.getBufferLength());
}

