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
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "../jrd/Routine.h"
#include "../jrd/Statement.h"
#include "../jrd/Function.h"
#include "../jrd/jrd.h"
#include "../jrd/exe.h"
#include "../common/StatusHolder.h"
#include "../jrd/lck.h"
#include "../jrd/par_proto.h"
#include "../jrd/met.h"

using namespace Firebird;


namespace Jrd {

RoutinePermanent::RoutinePermanent(thread_db* tdbb, MemoryPool& p, MetaId metaId, NoData)
	: PermanentStorage(p),
	  id(metaId),
	  name(p),
	  securityName(p),
	  subRoutine(false)
{ }


// Create a MsgMetadata from a parameters array.
MsgMetadata* Routine::createMetadata(const Array<NestConst<Parameter> >& parameters, bool isExtern)
{
	RefPtr<MsgMetadata> metadata(FB_NEW MsgMetadata);

	for (Array<NestConst<Parameter> >::const_iterator i = parameters.begin();
		 i != parameters.end();
		 ++i)
	{
		const dsc d((*i)->prm_desc);
		metadata->addItem((*i)->prm_name, (*i)->prm_nullable, d);
	}

	metadata->makeOffsets();
	metadata->addRef();

	return metadata;
}

// Create a Format based on an IMessageMetadata.
Format* Routine::createFormat(MemoryPool& pool, IMessageMetadata* params, bool addEof)
{
	FbLocalStatus status;

	const unsigned count = params->getCount(&status);
	status.check();

	Format* format = Format::newFormat(pool, count * 2 + (addEof ? 1 : 0));
	unsigned runOffset = 0;

	dsc* desc = format->fmt_desc.begin();

	for (unsigned i = 0; i < count; ++i)
	{
		unsigned descOffset, nullOffset, descDtype, descLength;

		const unsigned type = params->getType(&status, i);
		status.check();
		const unsigned len = params->getLength(&status, i);
		status.check();
		runOffset = fb_utils::sqlTypeToDsc(runOffset, type, len, &descDtype, &descLength,
			&descOffset, &nullOffset);

		desc->clear();
		desc->dsc_dtype = descDtype;
		desc->dsc_length = descLength;
		desc->dsc_scale = params->getScale(&status, i);
		status.check();
		desc->dsc_sub_type = params->getSubType(&status, i);
		status.check();
		desc->setTextType(TTypeId(params->getCharSet(&status, i)));
		status.check();
		desc->dsc_address = (UCHAR*)(IPTR) descOffset;
		desc->dsc_flags = (params->isNullable(&status, i) ? DSC_nullable : 0);
		status.check();

		++desc;
		desc->makeShort(0, (SSHORT*)(IPTR) nullOffset);

		++desc;
	}

	if (addEof)
	{
		// Next item is aligned on USHORT, so as the previous one.
		desc->makeShort(0, (SSHORT*)(IPTR) runOffset);
		runOffset += sizeof(USHORT);
	}

	format->fmt_length = runOffset;

	return format;
}

void Routine::setStatement(Statement* value)
{
	statement = value;

	if (statement)
	{
		switch (getObjectType())
		{
		case obj_procedure:
			statement->procedure = static_cast<jrd_prc*>(this);
			break;

		case obj_udf:
			statement->function = static_cast<Function*>(this);
			break;

		case obj_package_constant:
			break;

		default:
			fb_assert(false);
			break;
		}
	}
}

// Parse routine BLR and debug info.
void Routine::parseBlr(thread_db* tdbb, CompilerScratch* csb, const bid* blob_id, bid* blobDbg)
{
	Jrd::Attachment* attachment = tdbb->getAttachment();

	if (blobDbg)
		DBG_parse_debug_info(tdbb, blobDbg, *csb->csb_dbg_info);

	UCharBuffer tmp;

	if (blob_id)
	{
		blb* blob = blb::open(tdbb, attachment->getSysTransaction(), blob_id);
		ULONG length = blob->blb_length + 10;
		UCHAR* temp = tmp.getBuffer(length);
		length = blob->BLB_get_data(tdbb, temp, length);
		tmp.resize(length);
	}

	parseMessages(tdbb, csb, BlrReader(tmp.begin(), (unsigned) tmp.getCount()));

	Statement* statement = getStatement();
	flReload = false;
	PAR_blr(tdbb, &getName().schema, nullptr, tmp.begin(), (ULONG) tmp.getCount(), NULL, &csb, &statement, false, 0);
	setStatement(statement);

	if (csb->csb_g_flags & csb_reload)
		flReload = true;

	setImplemented(blob_id);
}

// Parse the messages of a blr request. For specified message, allocate a format (Format) block.
void Routine::parseMessages(thread_db* tdbb, CompilerScratch* csb, BlrReader blrReader)
{
	if (blrReader.getLength() < 2)
		status_exception::raise(Arg::Gds(isc_metadata_corrupt));

	csb->csb_schema = getName().schema;
	csb->csb_blr_reader = blrReader;

	PAR_getBlrVersionAndFlags(csb);

	if (csb->csb_blr_reader.getByte() != blr_begin)
		status_exception::raise(Arg::Gds(isc_metadata_corrupt));

	while (csb->csb_blr_reader.getByte() == blr_message)
	{
		const USHORT msgNumber = csb->csb_blr_reader.getByte();
		const USHORT count = csb->csb_blr_reader.getWord();
		Format* format = Format::newFormat(*tdbb->getDefaultPool(), count);

		USHORT padField;
		const bool shouldPad = csb->csb_message_pad.get(msgNumber, padField);

		USHORT maxAlignment = 0;
		ULONG offset = 0;
		USHORT i = 0;

		for (Format::fmt_desc_iterator desc = format->fmt_desc.begin(); i < count; ++i, ++desc)
		{
			const USHORT align = PAR_desc(tdbb, csb, &*desc);
			if (align)
				offset = FB_ALIGN(offset, align);

			desc->dsc_address = (UCHAR*)(IPTR) offset;
			offset += desc->dsc_length;

			maxAlignment = MAX(maxAlignment, align);

			if (maxAlignment && shouldPad && i + 1 == padField)
				offset = FB_ALIGN(offset, maxAlignment);
		}

		format->fmt_length = offset;

		switch (msgNumber)
		{
			case 0:
				setInputFormat(format);
				break;

			case 1:
				setOutputFormat(format);
				break;

			default:
				delete format;
		}
	}
}

bool Routine::hash(thread_db* tdbb, Firebird::sha512& digest)
{
	if (inputFields.hasData())
	{
		if (!inputFormat)
			return false;
		inputFormat->hash(digest);
	}

	if (outputFields.hasData())
	{
		if (!outputFormat)
			return false;
		outputFormat->hash(digest);
	}

	return true;
}

void Routine::releaseStatement(thread_db* tdbb)
{
	if (getStatement())
	{
		getStatement()->release(tdbb);
		setStatement(NULL);
	}

	setInputFormat(NULL);
	setOutputFormat(NULL);
}

bool RoutinePermanent::destroy(thread_db* tdbb, RoutinePermanent* routine)
{
	return false;
}

void Routine::destroy(thread_db* tdbb, Routine* routine)
{
	if (routine->statement)
		routine->statement->release(tdbb);
	delete routine;
}

}	// namespace Jrd
