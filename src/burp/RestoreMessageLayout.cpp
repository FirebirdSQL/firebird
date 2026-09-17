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
#include "../burp/RestoreMessageLayout.h"
#include "../burp/burp.h"
#undef gfld

#include "../common/classes/SafeArg.h"
#include "../common/dsc_proto.h"
#include "../jrd/align.h"
#include "../jrd/val.h"

using namespace Firebird;

namespace Burp {


static bool makeMessageDescriptor(dsc& desc, const burp_fld* field)
{
	USHORT blrType = field->fld_type;
	FLD_LENGTH length = field->fld_length;

	if (field->fld_flags & FLD_array)
		blrType = blr_quad;
	else
	{
		// Normalize extended text opcodes: charset/collation are passed
		// separately to DSC_make_descriptor().
		switch (blrType)
		{
			case blr_text2:
				blrType = blr_text;
				break;
			case blr_varying2:
				blrType = blr_varying;
				break;
			case blr_cstring2:
				blrType = blr_cstring;
				break;
			default:
				break;
		}
	}

	if (blrType != blr_blob && blrType > DTYPE_BLR_MAX)
		return false;

	SSHORT subType = field->fld_sub_type;
	CSetId charSet = field->fld_character_set_id;

	if (field->fld_flags & FLD_array)
	{
		subType = 0;
		charSet = CS_NONE;
	}

	return DSC_make_descriptor(&desc, blrType, field->fld_scale, length,
		subType, charSet, field->fld_collation_id);
}

[[noreturn]] static void raiseInvalidField(const burp_rel* relation, const burp_fld* field,
	const char* reason)
{
	(Arg::Gds(isc_gbak_inv_column) << Arg::Str(field->fld_name) <<
		Arg::Str(relation->rel_name.toQuotedString()) << Arg::Str(reason)).raise();
}

[[noreturn]] static void raiseInvalidLength(const burp_rel* relation)
{
	(Arg::Gds(isc_gbak_inv_record_length) <<
		Arg::Str(relation->rel_name.toQuotedString())).raise();
}

static RCRD_OFFSET alignField(const burp_rel* relation, RCRD_OFFSET offset,
	USHORT alignment, USHORT length)
{
	if (offset > MAX_RECORD_SIZE)
		raiseInvalidLength(relation);

	if (alignment)
		offset = FB_ALIGN(offset, alignment);

	if (offset > MAX_RECORD_SIZE || length > MAX_RECORD_SIZE - offset)
		raiseInvalidLength(relation);

	return offset;
}


void RestoreMessageLayout::build(burp_rel* relation, bool fixFssData, CSetId fixFssDataId)
{
	fb_assert(relation);

	clear();

	if (fixFssData)
	{
		for (burp_fld* field = relation->rel_fields; field; field = field->fld_next)
		{
			if (!(field->fld_flags & FLD_computed) &&
				field->fld_character_set_id == CS_UNICODE_FSS &&
				(field->fld_type == blr_blob || field->fld_type == blr_text ||
					field->fld_type == blr_varying))
			{
				field->fld_character_set_id = fixFssDataId;
			}
		}
	}

	RCRD_OFFSET offset = 0;
	USHORT peakAlignment = sizeof(SSHORT);

	for (burp_fld* field = relation->rel_fields; field; field = field->fld_next)
	{
		if (field->fld_flags & FLD_computed)
			continue;

		dsc descriptor;
		if (!makeMessageDescriptor(descriptor, field))
			(Arg::Gds(isc_gbak_unk_type) << Arg::Num(field->fld_type)).raise();

		const auto alignment = type_alignments[descriptor.dsc_dtype];

		if (alignment > peakAlignment)
			peakAlignment = alignment;

		const auto fieldOffset = alignField(relation, offset, alignment, descriptor.dsc_length);
		field->fld_offset = fieldOffset;
		offset = fieldOffset + descriptor.dsc_length;

		fields.add({field, descriptor, alignment, fieldOffset, 0});
	}

	SSHORT count = 0;
	for (auto& field : fields)
	{
		const auto nullOffset = alignField(relation, offset, sizeof(SSHORT), sizeof(SSHORT));
		field.nullOffset = nullOffset;
		field.metadata->fld_missing_parameter = count++;
		field.metadata->fld_missing_offset = nullOffset;
		offset = nullOffset + sizeof(SSHORT);
	}

	length = offset;
	maxAlignment = fields.hasData() ? peakAlignment : 0;
}

RCRD_LENGTH RestoreMessageLayout::getStride() const noexcept
{
	if (!maxAlignment)
		return length;
	return FB_ALIGN(length, maxAlignment);
}

void RestoreMessageLayout::clear() noexcept
{
	fields.clear();
	length = 0;
	maxAlignment = 0;
}


} // namespace Burp
