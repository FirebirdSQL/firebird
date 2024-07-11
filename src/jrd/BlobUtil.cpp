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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/BlobUtil.h"
#include "../jrd/blb.h"
#include "../jrd/tra.h"

using namespace Jrd;
using namespace Firebird;


namespace
{
	blb* getBlobFromHandle(thread_db* tdbb, ISC_INT64 handle)
	{
		const auto transaction = tdbb->getTransaction();
		blb* blob;

		if (transaction->tra_blob_util_map.get(handle, blob))
			return blob;
		else
			status_exception::raise(Arg::Gds(isc_invalid_blob_util_handle));
	}

	BlobIndex* getTempBlobIndexFromId(thread_db* tdbb, const bid& blobId)
	{
		if (blobId.bid_internal.bid_relation_id)
			return nullptr;

		const auto transaction = tdbb->getTransaction();

		if (!transaction->tra_blobs->locate(blobId.bid_temp_id()))
			status_exception::raise(Arg::Gds(isc_bad_segstr_id));

		const auto blobIndex = &transaction->tra_blobs->current();
		fb_assert(blobIndex->bli_blob_object);

		return blobIndex;
	}
}

namespace Jrd {

//--------------------------------------

IExternalResultSet* BlobUtilPackage::cancelBlobProcedure(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const BlobMessage::Type* in, void*)
{
	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();

	const auto blobId = *(bid*) &in->blob;

	if (const auto blobIdx = getTempBlobIndexFromId(tdbb, blobId))
	{
		if (blobIdx->bli_materialized)
			status_exception::raise(Arg::Gds(isc_bad_segstr_id));

		const auto blob = blobIdx->bli_blob_object;
		blob->BLB_cancel(tdbb);

		return nullptr;
	}
	else
		status_exception::raise(Arg::Gds(isc_bad_temp_blob_id));
}

IExternalResultSet* BlobUtilPackage::closeHandleProcedure(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const HandleMessage::Type* in, void*)
{
	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();
	const auto blob = getBlobFromHandle(tdbb, in->handle);

	transaction->tra_blob_util_map.remove(in->handle);
	blob->BLB_close(tdbb);

	return nullptr;
}

void BlobUtilPackage::isWritableFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const BlobMessage::Type* in, BooleanMessage::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();

	const auto blobId = *(bid*) &in->blob;

	out->booleanNull = FB_FALSE;

	if (const auto blobIdx = getTempBlobIndexFromId(tdbb, blobId))
	{
		if (!blobIdx->bli_materialized && (blobIdx->bli_blob_object->blb_flags & BLB_close_on_read))
		{
			out->boolean = FB_TRUE;
			return;
		}
	}

	out->boolean = FB_FALSE;
}

void BlobUtilPackage::newBlobFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const NewBlobInput::Type* in, BlobMessage::Type* out)
{
	thread_db* tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();

	const UCHAR bpb[] = {
		isc_bpb_version1,
		isc_bpb_type, 1, UCHAR(in->segmented ? isc_bpb_type_segmented : isc_bpb_type_stream),
		isc_bpb_storage, 1, UCHAR(in->tempStorage ? isc_bpb_storage_temp : isc_bpb_storage_main)
	};

	bid id;
	blb* blob = blb::create2(tdbb, transaction, &id, sizeof(bpb), bpb);

	blob->blb_flags |= BLB_close_on_read;

	out->blobNull = FB_FALSE;
	out->blob.gds_quad_low = (ULONG) blob->getTempId();
	out->blob.gds_quad_high = ((FB_UINT64) blob->getTempId()) >> 32;
}

void BlobUtilPackage::openBlobFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const BlobMessage::Type* in, HandleMessage::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();

	const auto blobId = *(bid*) &in->blob;
	const auto blob = blb::open(tdbb, transaction, &blobId);

	transaction->tra_blob_util_map.put(++transaction->tra_blob_util_next, blob);

	out->handleNull = FB_FALSE;
	out->handle = transaction->tra_blob_util_next;
}

void BlobUtilPackage::seekFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const SeekInput::Type* in, SeekOutput::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();
	const auto blob = getBlobFromHandle(tdbb, in->handle);

	if (!(in->mode >= 0 && in->mode <= 2))
		status_exception::raise(Arg::Gds(isc_random) << "Seek mode must be 0 (START), 1 (CURRENT) or 2 (END)");

	if (in->mode == 2 && in->offset > 0)	// 2 == from END
	{
		status_exception::raise(
			Arg::Gds(isc_random) <<
			"Argument OFFSET for RDB$BLOB_UTIL must be zero or negative when argument MODE is 2");
	}

	out->offsetNull = FB_FALSE;
	out->offset = blob->BLB_lseek(in->mode, in->offset);
}

void BlobUtilPackage::readDataFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const ReadDataInput::Type* in, BinaryMessage::Type* out)
{
	if (!in->lengthNull && in->length <= 0)
		status_exception::raise(Arg::Gds(isc_random) << "Length must be NULL or greater than 0");

	const auto tdbb = JRD_get_thread_data();
	const auto transaction = tdbb->getTransaction();
	const auto blob = getBlobFromHandle(tdbb, in->handle);

	if (in->lengthNull)
		out->data.length = blob->BLB_get_segment(tdbb, (UCHAR*) out->data.str, sizeof(out->data.str));
	else
	{
		out->data.length = blob->BLB_get_data(tdbb, (UCHAR*) out->data.str,
			MIN(in->length, sizeof(out->data.str)), false);
	}

	out->dataNull = out->data.length == 0 && (blob->blb_flags & BLB_eof) ? FB_TRUE : FB_FALSE;
}

//--------------------------------------


BlobUtilPackage::BlobUtilPackage(Firebird::MemoryPool& pool)
	: SystemPackage(
		pool,
		"RDB$BLOB_UTIL",
		ODS_13_1,
		// procedures
		{
			SystemProcedure(
				pool,
				"CANCEL_BLOB",
				SystemProcedureFactory<BlobMessage, VoidMessage, cancelBlobProcedure>(),
				prc_executable,
				// input parameters
				{
					{"BLOB", fld_blob, false}
				},
				// output parameters
				{}
			),
			SystemProcedure(
				pool,
				"CLOSE_HANDLE",
				SystemProcedureFactory<HandleMessage, VoidMessage, closeHandleProcedure>(),
				prc_executable,
				// input parameters
				{
					{"HANDLE", fld_butil_handle, false},
				},
				// output parameters
				{}
			)
		},
		// functions
		{
			SystemFunction(
				pool,
				"IS_WRITABLE",
				SystemFunctionFactory<BlobMessage, BooleanMessage, isWritableFunction>(),
				// parameters
				{
					{"BLOB", fld_blob, false}
				},
				{fld_bool, false}
			),
			SystemFunction(
				pool,
				"NEW_BLOB",
				SystemFunctionFactory<NewBlobInput, BlobMessage, newBlobFunction>(),
				// parameters
				{
					{"SEGMENTED", fld_bool, false},
					{"TEMP_STORAGE", fld_bool, false}
				},
				{fld_blob, false}
			),
			SystemFunction(
				pool,
				"OPEN_BLOB",
				SystemFunctionFactory<BlobMessage, HandleMessage, openBlobFunction>(),
				// parameters
				{
					{"BLOB", fld_blob, false}
				},
				{fld_butil_handle, false}
			),
			SystemFunction(
				pool,
				"SEEK",
				SystemFunctionFactory<SeekInput, SeekOutput, seekFunction>(),
				// parameters
				{
					{"HANDLE", fld_butil_handle, false},
					{"MODE", fld_integer, false},
					{"OFFSET", fld_integer, false}
				},
				{fld_integer, false}
			),
			SystemFunction(
				pool,
				"READ_DATA",
				SystemFunctionFactory<ReadDataInput, BinaryMessage, readDataFunction>(),
				// parameters
				{
					{"HANDLE", fld_butil_handle, false},
					{"LENGTH", fld_integer, true}
				},
				{fld_varybinary_max, true}
			)
		}
	)
{
}

}	// namespace Jrd
