/*
 *	PROGRAM:	InterBase layered support library
 *	MODULE:		blob.cpp
 *	DESCRIPTION:	Dynamic blob support
 *
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
 * 2001.09.10 Claudio Valderrama: get_name() was preventing the API calls
 *   isc_blob_default_desc, isc_blob_lookup_desc & isc_blob_set_desc
 *   from working properly with dialect 3 names. Therefore, incorrect names
 *   could be returned or a lookup for a blob field could fail. In addition,
 *   a possible buffer overrun due to unchecked bounds was closed. The fc
 *   get_name() as been renamed copy_exact_name().
 *
 */

#include "firebird.h"
#include "firebird/Message.h"
#include "ibase.h"
#include "../jrd/intl.h"
#include "../yvalve/blob_proto.h"
#include "../yvalve/YObjects.h"
#include "../common/StatusArg.h"
#include "../common/utils_proto.h"
#include "../jrd/constants.h"

using namespace Firebird;

static void copy_exact_name (const UCHAR*, UCHAR*, SSHORT);
static ISC_STATUS error(ISC_STATUS* status, const Arg::StatusVector& v);


void API_ROUTINE isc_blob_default_desc(ISC_BLOB_DESC* desc,
									   const UCHAR* relation_name,
									   const UCHAR* field_name)
{
/**************************************
 *
 *	i s c _ b l o b _ d e f a u l t _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 *	This function will set the default
 *	values in the blob_descriptor.
 *
 **************************************/

	desc->blob_desc_subtype = isc_blob_text;
	desc->blob_desc_charset = CS_dynamic;
	desc->blob_desc_segment_size = 80;

    copy_exact_name(field_name, desc->blob_desc_field_name, sizeof(desc->blob_desc_field_name));
    copy_exact_name(relation_name, desc->blob_desc_relation_name, sizeof(desc->blob_desc_relation_name));
}


ISC_STATUS API_ROUTINE isc_blob_gen_bpb(ISC_STATUS* status,
										const ISC_BLOB_DESC* to_desc,
										const ISC_BLOB_DESC* from_desc,
										USHORT bpb_buffer_length,
										UCHAR* bpb_buffer,
										USHORT* bpb_length)
{
/**************************************
 *
 *	i s c _ b l o b _ g e n _ b p b
 *
 **************************************
 *
 * Functional description
 *
 *  	This function will generate a bpb
 *	given a to_desc and a from_desc
 *	which contain the subtype and
 *	character set information.
 *
 **************************************/
	if (bpb_buffer_length < 17)
		return error(status, Arg::Gds(isc_random) << Arg::Str("BPB buffer too small"));

	UCHAR* p = bpb_buffer;
	*p++ = isc_bpb_version1;
	*p++ = isc_bpb_target_type;
	*p++ = 2;
	*p++ = (UCHAR)to_desc->blob_desc_subtype;
	*p++ = (UCHAR)(to_desc->blob_desc_subtype >> 8);
	*p++ = isc_bpb_source_type;
	*p++ = 2;
	*p++ = (UCHAR)from_desc->blob_desc_subtype;
	*p++ = (UCHAR)(from_desc->blob_desc_subtype >> 8);
	*p++ = isc_bpb_target_interp;
	*p++ = 2;
	*p++ = (UCHAR)to_desc->blob_desc_charset;
	*p++ = (UCHAR)(to_desc->blob_desc_charset >> 8);
	*p++ = isc_bpb_source_interp;
	*p++ = 2;
	*p++ = (UCHAR)from_desc->blob_desc_charset;
	*p++ = (UCHAR)(from_desc->blob_desc_charset >> 8);

	*bpb_length = p - bpb_buffer;

	return error(status, Arg::Gds(FB_SUCCESS));
}


// Lookup the blob subtype, character set and segment size information from the metadata,
// given a relation/procedure name and column/parameter name.
// It will fill in the information in the BLOB_DESC.
void iscBlobLookupDescImpl(Why::YAttachment* attachment, Why::YTransaction* transaction,
	const UCHAR* relationName, const UCHAR* fieldName, ISC_BLOB_DESC* desc, UCHAR* global)
{
	LocalStatus status;
	CheckStatusWrapper statusWrapper(&status);

	copy_exact_name(fieldName, desc->blob_desc_field_name, sizeof(desc->blob_desc_field_name));
	copy_exact_name(relationName, desc->blob_desc_relation_name, sizeof(desc->blob_desc_relation_name));

	bool flag = false;

	// Shared by both queries.
	FB_MESSAGE(OutputMessage, CheckStatusWrapper,
		(FB_INTEGER, fieldSubType)
		(FB_INTEGER, segmentLength)
		(FB_INTEGER, characterSetId)
	) outputMessage(&statusWrapper, MasterInterfacePtr());

	{	// scope
		// FIXME: schema
		constexpr auto sql = R"""(
			select f.rdb$field_sub_type,
			       f.rdb$segment_length,
			       f.rdb$character_set_id
			    from rdb$relation_fields rf
			    join rdb$fields f
			      on f.rdb$field_name = rf.rdb$field_source
			    where rf.rdb$relation_name = ? and
			          rf.rdb$field_name = ?
		)""";

		FB_MESSAGE(InputMessage, CheckStatusWrapper,
			(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), relationName)
			(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), fieldName)
		) inputMessage(&statusWrapper, MasterInterfacePtr());
		inputMessage.clear();

		inputMessage->relationNameNull = FB_FALSE;
		inputMessage->relationName.set((const char*) relationName);

		inputMessage->fieldNameNull = FB_FALSE;
		inputMessage->fieldName.set((const char*) fieldName);

		auto resultSet = makeNoIncRef(attachment->openCursor(&statusWrapper, transaction, 0, sql,
			SQL_DIALECT_CURRENT, inputMessage.getMetadata(), inputMessage.getData(),
			outputMessage.getMetadata(), nullptr, 0));
		status.check();

		if (resultSet->fetchNext(&statusWrapper, outputMessage.getData()) == IStatus::RESULT_OK)
		{
			flag = true;

			desc->blob_desc_subtype = outputMessage->fieldSubTypeNull ? 0 : outputMessage->fieldSubType;
			desc->blob_desc_charset = outputMessage->characterSetIdNull ? 0 : outputMessage->characterSetId;
			desc->blob_desc_segment_size = outputMessage->segmentLengthNull ? 0 : outputMessage->segmentLength;
		}

		status.check();
	}

	if (!flag)
	{
		// FIXME: schema
		constexpr auto sql = R"""(
			select f.rdb$field_sub_type,
			       f.rdb$segment_length,
			       f.rdb$character_set_id
			    from rdb$procedure_parameters pp
			    join rdb$fields f
			      on f.rdb$field_name = pp.rdb$field_source
			    where pp.rdb$procedure_name = ? and
			          pp.rdb$parameter_name = ? and
			          pp.rdb$package_name is null
		)""";

		FB_MESSAGE(InputMessage, CheckStatusWrapper,
			(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), procedureName)
			(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), fieldName)
		) inputMessage(&statusWrapper, MasterInterfacePtr());
		inputMessage.clear();

		inputMessage->procedureNameNull = FB_FALSE;
		inputMessage->procedureName.set((const char*) relationName);

		inputMessage->fieldNameNull = FB_FALSE;
		inputMessage->fieldName.set((const char*) fieldName);

		auto resultSet = makeNoIncRef(attachment->openCursor(&statusWrapper, transaction, 0, sql,
			SQL_DIALECT_CURRENT, inputMessage.getMetadata(), inputMessage.getData(),
			outputMessage.getMetadata(), nullptr, 0));
		status.check();

		if (resultSet->fetchNext(&statusWrapper, outputMessage.getData()) == IStatus::RESULT_OK)
		{
			flag = true;

			desc->blob_desc_subtype = outputMessage->fieldSubTypeNull ? 0 : outputMessage->fieldSubType;
			desc->blob_desc_charset = outputMessage->characterSetIdNull ? 0 : outputMessage->characterSetId;
			desc->blob_desc_segment_size = outputMessage->segmentLengthNull ? 0 : outputMessage->segmentLength;
		}

		status.check();
	}

	if (!flag)
	{
		(Arg::Gds(isc_fldnotdef) <<
			Arg::Str((const char*)(desc->blob_desc_field_name)) <<
			Arg::Str((const char*)(desc->blob_desc_relation_name))).raise();
	}

	if (global)
		copy_exact_name(fieldName, global, sizeof(desc->blob_desc_field_name));
}


ISC_STATUS API_ROUTINE isc_blob_set_desc(ISC_STATUS* status,
										 const UCHAR* relation_name,
										 const UCHAR* field_name,
										 SSHORT subtype,
										 SSHORT charset,
										 SSHORT segment_size,
										 ISC_BLOB_DESC* desc)
{
/**************************************
 *
 *	i s c _ b l o b _ s e t _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 *	This routine will set the subtype
 *	and character set information in the
 *	BLOB_DESC based on the information
 *	specifically passed in by the user.
 *
 **************************************/

    copy_exact_name(field_name, desc->blob_desc_field_name, sizeof(desc->blob_desc_field_name));
    copy_exact_name(relation_name, desc->blob_desc_relation_name, sizeof(desc->blob_desc_relation_name));

	desc->blob_desc_subtype = subtype;
	desc->blob_desc_charset = charset;
	desc->blob_desc_segment_size = segment_size;

	return error(status, Arg::Gds(FB_SUCCESS));
}




static void copy_exact_name (const UCHAR* from, UCHAR* to, SSHORT bsize)
{
/**************************************
 *
 *  c o p y _ e x a c t _ n a m e
 *
 **************************************
 *
 * Functional description
 *  Copy null terminated name ot stops at bsize - 1.
 *
 **************************************/
	const UCHAR* const from_end = from + bsize - 1;
	UCHAR* to2 = to - 1;
	while (*from && from < from_end)
	{
		if (*from != ' ') {
			to2 = to;
		}
		*to++ = *from++;
	}
	*++to2 = 0;
}


static ISC_STATUS error(ISC_STATUS* status, const Arg::StatusVector& v)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	Stuff a status vector.
 *
 **************************************/
	return v.copyTo(status);
}
