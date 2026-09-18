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
#include "../burp/FastPathRecordReader.h"
#include "../burp/RestoreMessageLayout.h"
#include "../burp/burp.h"
#undef gfld

#include "../burp/burp_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/EngineInterface.h"
#include "../jrd/Attachment.h"
#include "../jrd/Database.h"
#include "../jrd/jrd.h"
#include "../jrd/Record.h"
#include "../jrd/Relation.h"
#include "../jrd/met.h"
#include "../jrd/mov_proto.h"
#include "../jrd/req.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/tdbb.h"
#include "../jrd/tra.h"
#include "../jrd/vio_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/blb.h"
#include "../jrd/evl_proto.h"

using namespace Firebird;
using namespace Jrd;

namespace Burp {


struct FastPathRecordReader::Imp
{
	struct Field
	{
		dsc target;
		RCRD_OFFSET offset;
		RCRD_OFFSET nullOffset;
		USHORT fieldId;
		bool array;
	};

	IAttachment* att = nullptr;
	ITransaction* tra = nullptr;
	const burp_rel* burpRelation = nullptr;
	JAttachment* jAtt = nullptr;
	Attachment* attachment = nullptr;
	jrd_tra* transaction = nullptr;
	jrd_rel* relation = nullptr;
	const RestoreMessageLayout* layout = nullptr;
	RCRD_LENGTH messageLength = 0;

	HalfStaticArray<Field, 16> fields;
	record_param rpb;
	RecordNumber upper;
	bool partition = false;
	bool scanActive = false;
	bool initialized = false;

	Imp()
		: fields(*getDefaultMemoryPool()),
		  rpb()
	{}

	// record_param is a plain struct: VIO_record() allocates rpb_record from
	// the default pool on first use (see VIO_data) and reuses it afterwards,
	// but nothing frees it (sweep code deletes it explicitly too).
	// Always use this instead of assigning a fresh record_param.
	void resetRpb()
	{
		delete rpb.rpb_record;
		rpb = record_param();
	}

	// Balance the rel_scan_count increment made in start(). Called on natural
	// EOF and from clear(), so a finished scan does not keep the relation
	// registered as actively scanned across pointer-page slices or until the
	// reader is destroyed.
	void finishScan()
	{
		if (!scanActive)
			return;

		scanActive = false;

		if (relation && relation->getPermanent()->rel_scan_count)
			--relation->getPermanent()->rel_scan_count;
	}
};


[[noreturn]] static void raiseInvalidField(const burp_rel* relation, const burp_fld* field,
	const char* reason)
{
	(Arg::Gds(isc_gbak_inv_column) << Arg::Str(field->fld_name) <<
		Arg::Str(relation->rel_name.toQuotedString()) << Arg::Str(reason)).raise();
}


FastPathRecordReader::FastPathRecordReader()
	: imp(FB_NEW_POOL(*getDefaultMemoryPool()) Imp())
{
}

FastPathRecordReader::~FastPathRecordReader()
{
	clear();
}

void FastPathRecordReader::init(IAttachment* att, ITransaction* tra, const burp_rel* relation,
	const RestoreMessageLayout& layout, bool partition)
{
	fb_assert(att && tra && relation && imp);

	clear();
	imp->att = att;
	imp->tra = tra;
	imp->burpRelation = relation;
	imp->layout = &layout;
	imp->messageLength = layout.getLength();
	imp->partition = partition;
	imp->jAtt = static_cast<JAttachment*>(att);
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

	imp->relation = MetadataCache::getVersioned<Cached::Relation>(tdbb, relation->rel_id,
		CacheFlag::AUTOCREATE);

	if (!imp->relation)
	{
		(Arg::Gds(isc_gbak_tbl_missing) <<
			Arg::Str(relation->rel_name.toQuotedString())).raise();
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

	const Format* const format = imp->relation->currentFormat(tdbb);
	if (!format)
	{
		fb_assert(false);
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH relation has no current format") <<
			Arg::Str(relation->rel_name.toQuotedString())).raise();
	}

	for (const auto& layoutField : layout)
	{
		const burp_fld* field = layoutField.metadata;
		if (field->fld_id < 0 || field->fld_id >= format->fmt_count)
			raiseInvalidField(relation, field, "field is not present in the source table");

		const dsc source = format->fmt_desc[field->fld_id];
		if (source.isUnknown() || (source.dsc_flags & DSC_computed))
			raiseInvalidField(relation, field, "invalid source descriptor");

		Imp::Field mapping;
		mapping.target = layoutField.descriptor;
		mapping.offset = layoutField.offset;
		mapping.nullOffset = layoutField.nullOffset;
		mapping.fieldId = static_cast<USHORT>(field->fld_id);
		mapping.array = (field->fld_flags & FLD_array) != 0;
		imp->fields.add(mapping);
	}

	RLCK_reserve_relation(tdbb, imp->transaction, imp->relation->getPermanent(), false);
	imp->initialized = true;
}

void FastPathRecordReader::start(ULONG loPP, ULONG hiPP)
{
	fb_assert(imp && imp->initialized);

	FbLocalStatus status;
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	tdbb->setTransaction(imp->transaction);

	imp->resetRpb();
	imp->finishScan();
	imp->rpb.rpb_relation = imp->relation;
	imp->rpb.rpb_stream_flags = 0;
	imp->rpb.getWindow(tdbb).win_flags = WIN_large_scan;
	imp->rpb.rpb_org_scans = imp->relation->getPermanent()->rel_scan_count++;
	imp->rpb.rpb_number.setValue(BOF_NUMBER);

	if (imp->partition)
	{
		const auto dbb = tdbb->getDatabase();

		imp->rpb.rpb_number.compose(dbb->dbb_max_records, dbb->dbb_dp_per_pp,
			0, 0, loPP);
		imp->rpb.rpb_number.decrement();

		imp->upper.compose(dbb->dbb_max_records, dbb->dbb_dp_per_pp,
			0, 0, hiPP);
		imp->upper.decrement();
	}

	imp->scanActive = true;
}

bool FastPathRecordReader::readRecord(UCHAR* messageBuffer, RCRD_LENGTH msgLength)
{
	fb_assert(imp && imp->initialized && messageBuffer);
	return readRecords(messageBuffer, msgLength, 1) != 0;
}

unsigned FastPathRecordReader::readRecords(UCHAR* messageBuffer, RCRD_LENGTH msgLength, unsigned maxRecords)
{
	fb_assert(imp && imp->initialized && messageBuffer);

	// A scan ended (EOF reached) and the caller asks again for records, e.g.
	// when the last batch was partial. Report EOF without touching the
	// finished scan.
	if (!imp->scanActive)
		return 0;

	if (!maxRecords)
		return 0;

	if (msgLength < imp->messageLength)
	{
		// Internal caller invariant: the stride is derived from the same layout.
		fb_assert(false);
		(Arg::Gds(isc_random) <<
			Arg::Str("FAST_PATH message stride is smaller than the layout") <<
			Arg::Str(imp->burpRelation->rel_name.toQuotedString())).raise();
	}

	FbLocalStatus status;
	// The attachment sync stays held for the whole batch; JRD_reschedule()
	// inside the loop keeps shutdown/AST handling responsive.
	EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
	tdbb->setTransaction(imp->transaction);

	const auto scope = imp->partition ? DPM_next_pointer_page : DPM_next_all;
	const RecordNumber* const upper = imp->partition ? &imp->upper : nullptr;
	unsigned records = 0;

	while (records < maxRecords)
	{
		JRD_reschedule(tdbb);

		// With a pool given, VIO_next_record -> VIO_data copies the record out
		// and releases the page window, so no CCH_RELEASE is needed here.
		if (!VIO_next_record(tdbb, &imp->rpb, imp->transaction, tdbb->getDefaultPool(), scope, upper))
		{
			imp->rpb.rpb_number.setValid(false);
			imp->finishScan();
			break;
		}

		UCHAR* const recordBuffer = messageBuffer + static_cast<FB_SIZE_T>(records) * msgLength;
		memset(recordBuffer, 0, msgLength);

		for (const auto& field : imp->fields)
		{
			dsc source;
			const bool present = EVL_field(imp->relation, imp->rpb.rpb_record, field.fieldId, &source);
			const SSHORT nullFlag = present ? 0 : -1;
			memcpy(recordBuffer + field.nullOffset, &nullFlag, sizeof(nullFlag));

			if (!present)
				continue;

			dsc target = field.target;
			target.dsc_address = recordBuffer + field.offset;

			if (field.array || DTYPE_IS_BLOB_OR_QUAD(target.dsc_dtype))
				blb::move(tdbb, &source, &target, nullptr, nullptr, 0);
			else
				MOV_move(tdbb, &source, &target);
		}

		++records;
	}

	return records;
}

void FastPathRecordReader::clear()
{
	if (!imp)
		return;

	// Touching rel_scan_count requires the engine sync, also in this
	// (possibly destructor) path.
	if (imp->scanActive && imp->jAtt && imp->relation)
	{
		FbLocalStatus status;
		EngineContextHolder tdbb(&status, imp->jAtt, FB_FUNCTION);
		imp->finishScan();
	}

	imp->att = nullptr;
	imp->tra = nullptr;
	imp->burpRelation = nullptr;
	imp->jAtt = nullptr;
	imp->attachment = nullptr;
	imp->transaction = nullptr;
	imp->relation = nullptr;
	imp->layout = nullptr;
	imp->messageLength = 0;
	imp->fields.clear();
	imp->resetRpb();
	imp->upper = RecordNumber();
	imp->partition = false;
	imp->scanActive = false;
	imp->initialized = false;
}


} // namespace Burp
