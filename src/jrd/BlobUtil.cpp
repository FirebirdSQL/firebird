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
		{
			status_exception::raise(Arg::Gds(isc_invalid_blob_util_handle));
			return nullptr;
		}
	}
}

namespace Jrd {

//--------------------------------------

IExternalResultSet* BlobUtilPackage::appendProcedure(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const AppendInput::Type* in, void*)
{
	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto blob = getBlobFromHandle(tdbb, in->handle);

	if (in->data.length > 0)
		blob->BLB_put_data(tdbb, (const UCHAR*) in->data.str, in->data.length);
	else if (in->data.length == 0 && !(blob->blb_flags & BLB_stream))
		blob->BLB_put_segment(tdbb, (const UCHAR*) in->data.str, 0);

	return nullptr;
}

IExternalResultSet* BlobUtilPackage::cancelProcedure(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const HandleMessage::Type* in, void*)
{
	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto transaction = tdbb->getTransaction();
	const auto blob = getBlobFromHandle(tdbb, in->handle);

	transaction->tra_blob_util_map.remove(in->handle);
	blob->BLB_cancel(tdbb);

	return nullptr;
}

void BlobUtilPackage::newFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const NewInput::Type* in, HandleMessage::Type* out)
{
	thread_db* tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto transaction = tdbb->getTransaction();

	const UCHAR bpb[] = {
		isc_bpb_version1,
		isc_bpb_type, 1, UCHAR(in->segmented ? isc_bpb_type_segmented : isc_bpb_type_stream),
		isc_bpb_storage, 1, UCHAR(in->tempStorage ? isc_bpb_storage_temp : isc_bpb_storage_main)
	};

	bid id;
	blb* newBlob = blb::create2(tdbb, transaction, &id, sizeof(bpb), bpb);

	transaction->tra_blob_util_map.put(++transaction->tra_blob_util_next, newBlob);

	out->handleNull = FB_FALSE;
	out->handle = transaction->tra_blob_util_next;
}

void BlobUtilPackage::openBlobFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const BlobMessage::Type* in, HandleMessage::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto transaction = tdbb->getTransaction();

	bid blobId = *(bid*) &in->blob;
	blb* blob = blb::open(tdbb, transaction, &blobId);

	transaction->tra_blob_util_map.put(++transaction->tra_blob_util_next, blob);

	out->handleNull = FB_FALSE;
	out->handle = transaction->tra_blob_util_next;
}

void BlobUtilPackage::seekFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const SeekInput::Type* in, SeekOutput::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto transaction = tdbb->getTransaction();
	const auto blob = getBlobFromHandle(tdbb, in->handle);

	if (!(in->mode >= 0 && in->mode <= 2))
		status_exception::raise(Arg::Gds(isc_random) << "Seek mode must be 1, 2 or 3");

	out->offsetNull = FB_FALSE;
	out->offset = blob->BLB_lseek(in->mode, in->offset);
}

void BlobUtilPackage::readFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const ReadInput::Type* in, BinaryMessage::Type* out)
{
	if (!in->lengthNull && in->length <= 0)
		status_exception::raise(Arg::Gds(isc_random) << "Length must be NULL or greater than 0");

	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

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

void BlobUtilPackage::makeBlobFunction(ThrowStatusExceptionWrapper* status,
	IExternalContext* context, const HandleMessage::Type* in, BlobMessage::Type* out)
{
	const auto tdbb = JRD_get_thread_data();
	Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

	const auto transaction = tdbb->getTransaction();

	const auto blob = getBlobFromHandle(tdbb, in->handle);

	if (!(blob->blb_flags & BLB_temporary))
		ERR_post(Arg::Gds(isc_cannot_make_blob_opened_handle));

	out->blobNull = FB_FALSE;
	out->blob.gds_quad_low = (ULONG) blob->getTempId();
	out->blob.gds_quad_high = ((FB_UINT64) blob->getTempId()) >> 32;

	transaction->tra_blob_util_map.remove(in->handle);
	blob->BLB_close(tdbb);
}

//--------------------------------------


BlobUtilPackage::BlobUtilPackage(Firebird::MemoryPool& pool)
	: SystemPackage(
		pool,
		"RDB$BLOB_UTIL",
		ODS_13_0,	//// TODO: adjust
		// procedures
		{
			SystemProcedure(
				pool,
				"APPEND",
				SystemProcedureFactory<AppendInput, VoidMessage, appendProcedure>(),
				prc_executable,
				// input parameters
				{
					{"HANDLE", fld_long_number, false},
					{"DATA", fld_varybinary_max, false}
				},
				// output parameters
				{}
			),
			SystemProcedure(
				pool,
				"CANCEL",
				SystemProcedureFactory<HandleMessage, VoidMessage, cancelProcedure>(),
				prc_executable,
				// input parameters
				{
					{"HANDLE", fld_long_number, false},
				},
				// output parameters
				{}
			)
		},
		// functions
		{
			SystemFunction(
				pool,
				"NEW",
				SystemFunctionFactory<NewInput, HandleMessage, newFunction>(),
				// parameters
				{
					{"SEGMENTED", fld_bool, false},
					{"TEMP_STORAGE", fld_bool, false}
				},
				{fld_long_number, false}
			),
			SystemFunction(
				pool,
				"OPEN_BLOB",
				SystemFunctionFactory<BlobMessage, HandleMessage, openBlobFunction>(),
				// parameters
				{
					{"BLOB", fld_blob, false}
				},
				{fld_long_number, false}
			),
			SystemFunction(
				pool,
				"SEEK",
				SystemFunctionFactory<SeekInput, SeekOutput, seekFunction>(),
				// parameters
				{
					{"HANDLE", fld_long_number, false},
					{"MODE", fld_long_number, false},
					{"OFFSET", fld_long_number, false}
				},
				{fld_long_number, false}
			),
			SystemFunction(
				pool,
				"READ",
				SystemFunctionFactory<ReadInput, BinaryMessage, readFunction>(),
				// parameters
				{
					{"HANDLE", fld_long_number, false},
					{"LENGTH", fld_long_number, true}
				},
				{fld_varybinary_max, true}
			),
			SystemFunction(
				pool,
				"MAKE_BLOB",
				SystemFunctionFactory<HandleMessage, BlobMessage, makeBlobFunction>(),
				// parameters
				{
					{"HANDLE", fld_long_number, false}
				},
				{fld_blob, false}
			)
		}
	)
{
}

}	// namespace Jrd
