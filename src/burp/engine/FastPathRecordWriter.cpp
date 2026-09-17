/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * Adriano dos Santos Fernandes <adrianosf@gmail.com>
 */

#include "firebird.h"

// burp.h and ini.h both define struct gfld with different layouts
#define gfld burp_gfld
#include "../burp/FastPathRecordWriter.h"
#include "../burp/RestoreMessageLayout.h"
#include "../burp/burp.h"
#undef gfld

#include "../burp/burp_proto.h"
#include "../common/dsc_proto.h"
#include "../common/msg_encode.h"
#include "../jrd/EngineInterface.h"
#include "../jrd/Attachment.h"
#include "../jrd/jrd.h"
#include "../jrd/Relation.h"
#include "../jrd/met.h"
#include "../jrd/met_proto.h"
#include "../jrd/align.h"
#include "../jrd/blb.h"
#include "../jrd/BulkInsert.h"
#include "../jrd/evl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/obj.h"
#include "../jrd/req.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/tdbb.h"
#include "../jrd/tra.h"
#include "../jrd/vio_proto.h"
#include "../jrd/val.h"

using namespace Firebird;
using namespace Jrd;

namespace Burp {


struct FastPathRecordWriter::Imp
{
	struct Field
	{
		dsc source;
		dsc target;
		RCRD_OFFSET sourceOffset;
		RCRD_OFFSET nullOffset;
		USHORT targetId;
		bool array;
	};

	IAttachment* att = nullptr;
	ITransaction* tra = nullptr;
	const burp_rel* burpRelation = nullptr;
	::JAttachment* jAtt = nullptr;
	Attachment* attachment = nullptr;
	jrd_tra* transaction = nullptr;
	jrd_rel* relation = nullptr;
	const Format* format = nullptr;
	AutoPtr<Record> record;

	HalfStaticArray<Field, 16> fields;
	RCRD_LENGTH messageLength = 0;
	record_param rpb;
	bool initialized = false;

	Imp()
		: fields(*getDefaultMemoryPool()),
		  rpb()
	{}

	void setupRecord(thread_db* tdbb)
	{
		rpb.rpb_relation = relation;
		// VIO_record() allocates rpb_record when null and returns it.
		record = VIO_record(tdbb, &rpb, format, tdbb->getDefaultPool());
		rpb.rpb_format_number = format->fmt_version;
	}

	void writeRecord(thread_db* tdbb, const UCHAR* messageBuffer)
	{
		// Register the bulk insert before field conversion so blobs/arrays
		// use the batched secondary-page path (bulk=true) instead of
		// row-at-a-time DPM_store_blob. The object persists across records and
		// message batches; it is finished at table end (finishBulk) and at
		// commit.
		const auto bulk = transaction->getBulkInsert(tdbb, relation, true);
		if (!bulk)
		{
			fb_assert(false);
			(Arg::Gds(isc_random) <<
				Arg::Str("FAST_PATH could not start bulk insert") <<
				Arg::Str(burpRelation->rel_name.toQuotedString())).raise();
		}

		rpb.rpb_record->nullify();

		for (const auto& field : fields)
		{
			SSHORT nullFlag;
			memcpy(&nullFlag, messageBuffer + field.nullOffset, sizeof(nullFlag));

			if (nullFlag)
				continue;

			rpb.rpb_record->clearNull(field.targetId);

			dsc src = field.source;
			src.dsc_address = const_cast<UCHAR*>(messageBuffer + field.sourceOffset);

			dsc dst = field.target;
			dst.dsc_address = rpb.rpb_record->getData() + (IPTR) field.target.dsc_address;

			if (DTYPE_IS_BLOB(dst.dsc_dtype) || field.array)
				blb::move(tdbb, &src, &dst, relation, rpb.rpb_record, field.targetId, true);
			else
				MOV_move(tdbb, &src, &dst);
		}

		rpb.rpb_address = rpb.rpb_record->getData();
		rpb.rpb_length = format->fmt_length;
		rpb.rpb_record->setTransactionNumber(transaction->tra_number);

		// Buffer the record into the transaction's bulk-insert page images.
		// BulkInsert performs no validation, matching the FAST_PATH
		// design of trusting trusted backups. REPL_store() is
		// intentionally not called: this path only populates freshly created
		// databases.
		bulk->putRecord(tdbb, &rpb, transaction);
	}
};


[[noreturn]] static void raiseInvalidField(const burp_rel* relation, const burp_fld* field,
	const char* reason)
{
	(Arg::Gds(isc_gbak_inv_column) << Arg::Str(field->fld_name) <<
		Arg::Str(relation->rel_name.toQuotedString()) << Arg::Str(reason)).raise();
}


FastPathRecordWriter::FastPathRecordWriter()
	: imp(FB_NEW_POOL(*getDefaultMemoryPool()) Imp())
{
}

FastPathRecordWriter::~FastPathRecordWriter()
{
	clear();
}

void FastPathRecordWriter::init(IAttachment* att, ITransaction* tra, const burp_rel* relation,
	const RestoreMessageLayout& layout)
{
	fb_assert(att && tra && relation && imp);

	imp->initialized = false;
	imp->fields.clear();
	imp->messageLength = layout.getLength();
	imp->att = att;
	imp->tra = tra;
	imp->burpRelation = relation;
	imp->jAtt = static_cast<::JAttachment*>(att);
	imp->attachment = imp->jAtt->getHandle();
	fb_assert(imp->attachment);

	FbLocalStatus status;
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	imp->transaction = imp->jAtt->getEngineTransaction(&status, tra);

	if (status->hasData())
		BURP_abort(&status);

	if (!imp->transaction)
		Arg::Gds(isc_bad_trans_handle).raise();

	tdbb->setTransaction(imp->transaction);

	QualifiedName qualifiedName(relation->rel_name);
	// Resolve old unqualified backup names through the target attachment's
	// default schema; explicitly qualified names are left unchanged.
	imp->attachment->qualifyExistingName(tdbb, qualifiedName, {obj_relation});

	imp->relation = MetadataCache::getVersioned<Cached::Relation>(tdbb, qualifiedName, CacheFlag::AUTOCREATE);

	if (!imp->relation)
	{
		(Arg::Gds(isc_gbak_tbl_missing) <<
			Arg::Str(qualifiedName.toQuotedString())).raise();
	}

	if (relation->rel_type != rel_persistent ||
		(relation->rel_flags & (::REL_view | ::REL_external)) ||
		imp->relation->isSystem() || imp->relation->isView() ||
		imp->relation->isVirtual() || imp->relation->isTemporary() ||
		imp->relation->getExtFile())
	{
		fb_assert(false);
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH selected an ineligible relation") <<
			Arg::Str(relation->rel_name.toQuotedString())).raise();
	}

	imp->format = imp->relation->currentFormat(tdbb);

	if (!imp->format)
	{
		fb_assert(false);
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH relation has no current format") <<
			Arg::Str(relation->rel_name.toQuotedString())).raise();
	}

	imp->setupRecord(tdbb);

	for (const auto& layoutField : layout)
	{
		const burp_fld* field = layoutField.metadata;
		const dsc source = layoutField.descriptor;

		const auto fieldId = MET_lookup_field(tdbb, imp->relation, MetaName(field->fld_name));
		if (fieldId < 0 || fieldId >= imp->format->fmt_count)
			raiseInvalidField(relation, field, "field is not present in the target table");

		for (const auto& mapping : imp->fields)
		{
			if (mapping.targetId == fieldId)
				raiseInvalidField(relation, field, "field is mapped more than once");
		}

		const dsc target = imp->format->fmt_desc[fieldId];
		const auto targetOffset = (IPTR) target.dsc_address;

		// Computed fields are excluded from the message, so a computed/unknown descriptor here
		// means inconsistent backup/target metadata.
		if (target.isUnknown() || (target.dsc_flags & DSC_computed) || targetOffset <= 0 ||
			static_cast<ULONG>(targetOffset) > imp->format->fmt_length ||
			target.dsc_length > imp->format->fmt_length - static_cast<ULONG>(targetOffset))
		{
			raiseInvalidField(relation, field, "invalid target descriptor");
		}

		Imp::Field mapping;
		mapping.source = source;
		mapping.target = target;
		mapping.sourceOffset = layoutField.offset;
		mapping.nullOffset = layoutField.nullOffset;
		mapping.targetId = static_cast<USHORT>(fieldId);
		mapping.array = (field->fld_flags & FLD_array) != 0;
		imp->fields.add(mapping);
	}

	RLCK_reserve_relation(tdbb, imp->transaction, imp->relation->getPermanent(), true);

	// Note: the transaction's bulk insert is registered lazily by the first
	// stored row and finished at table end (see finishBulk), so
	// nothing is set up here.
	imp->initialized = true;
}

void FastPathRecordWriter::writeRecords(const UCHAR* messageBuffer, RCRD_LENGTH msgLength,
	unsigned recordCount)
{
	fb_assert(imp && imp->initialized && messageBuffer);

	if (!recordCount)
		return;

	// msgLength is the in-memory stride and may exceed the serialized message
	// length to preserve descriptor alignment between packed records.
	if (msgLength < imp->messageLength)
	{
		// Internal caller invariant: the stride is derived from the same layout.
		fb_assert(false);
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH message stride is smaller than the layout") <<
			Arg::Str(imp->burpRelation->rel_name.toQuotedString())).raise();
	}

	// One engine attachment sync processes the whole batch. The direct path is
	// fail-fast: BulkInsert has no rollback for blob/array side effects, so
	// continuing after a failed record could leave partial row data.
	FbLocalStatus status;
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	tdbb->setTransaction(imp->transaction);

	for (unsigned record = 0; record < recordCount; ++record)
	{
		const UCHAR* const recordBuffer = messageBuffer + static_cast<FB_SIZE_T>(record) * msgLength;
		imp->writeRecord(tdbb, recordBuffer);
	}
}

void FastPathRecordWriter::releaseRequestlessArrays()
{
	if (!imp || !imp->initialized || !imp->transaction)
		return;

	// Touching the transaction's array list requires the engine sync.
	FbLocalStatus status;
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	tdbb->setTransaction(imp->transaction);

	blb::releaseRequestlessArrays(imp->transaction);
}

void FastPathRecordWriter::finishBulk()
{
	if (!imp || !imp->initialized || !imp->transaction)
		return;

	FbLocalStatus status;
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	tdbb->setTransaction(imp->transaction);

	// Flush buffered pages and unregister from the transaction. Called on
	// table end and at commit (as a safety net). Not called at every
	// message-batch boundary: the bulk object auto-flushes full
	// extents internally and its memory stays bounded (one extent plus blob
	// pages), while forcing a flush per batch would strand a partial-extent
	// tail each time and bloat the restored database. The next stored row
	// transparently registers a fresh bulk object when needed.
	imp->transaction->finiBulkInsert(tdbb, true);
}

void FastPathRecordWriter::clear()
{
	if (!imp)
		return;

	// Touching the transaction's array list requires the engine sync, also in
	// this (possibly destructor) path.
	if (imp->initialized && imp->transaction && imp->jAtt)
	{
		FbLocalStatus status;
		EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
		tdbb->setTransaction(imp->transaction);

		blb::releaseRequestlessArrays(imp->transaction);
	}

	imp->att = nullptr;
	imp->tra = nullptr;
	imp->burpRelation = nullptr;
	imp->jAtt = nullptr;
	imp->attachment = nullptr;
	imp->transaction = nullptr;
	imp->relation = nullptr;
	imp->format = nullptr;
	imp->record = nullptr;
	imp->fields.clear();
	imp->messageLength = 0;
	imp->rpb = record_param();
	imp->initialized = false;
}


} // namespace Burp
