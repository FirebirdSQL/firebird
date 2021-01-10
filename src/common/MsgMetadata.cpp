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
 *  Copyright (c) 2011 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *		Alex Peshkoff
 *
 */

#include "firebird.h"
#include "../common/MsgMetadata.h"
#include "../common/utils_proto.h"
#include "../common/classes/MetaString.h"
#include "../common/StatusHolder.h"
#include "../jrd/align.h"

namespace Firebird {

MetadataBuilder::MetadataBuilder(const MsgMetadata* from)
	: msgMetadata(FB_NEW MsgMetadata)
{
	msgMetadata->items = from->items;
}

MetadataBuilder::MetadataBuilder(unsigned fieldCount)
	: msgMetadata(FB_NEW MsgMetadata)
{
	if (fieldCount)
		msgMetadata->items.grow(fieldCount);
}

void MetadataBuilder::setType(CheckStatusWrapper* status, unsigned index, unsigned type)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setType");
		msgMetadata->items[index].type = type;
		if (!msgMetadata->items[index].length)
		{
			unsigned dtype;
			fb_utils::sqlTypeToDsc(0, type, 0, &dtype, NULL, NULL, NULL);
			if (dtype < DTYPE_TYPE_MAX)
				msgMetadata->items[index].length = type_lengths[dtype];
		}
		msgMetadata->items[index].nullable = type & 1;

		// Setting type & length is enough for an item to be ready for use
		if (msgMetadata->items[index].length)
			msgMetadata->items[index].finished = true;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setSubType(CheckStatusWrapper* status, unsigned index, int subType)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setSubType");
		msgMetadata->items[index].subType = subType;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setLength(CheckStatusWrapper* status, unsigned index, unsigned length)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);
		indexError(index, "setLength");
		msgMetadata->items[index].length = length;

		// Setting type & length is enough for an item to be ready for use
		if (msgMetadata->items[index].type)
			msgMetadata->items[index].finished = true;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setCharSet(CheckStatusWrapper* status, unsigned index, unsigned charSet)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setCharSet");
		msgMetadata->items[index].charSet = charSet;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setScale(CheckStatusWrapper* status, unsigned index, int scale)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setScale");
		msgMetadata->items[index].scale = scale;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setField(CheckStatusWrapper* status, unsigned index, const char* field)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setField");
		msgMetadata->items[index].field = field;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setRelation(CheckStatusWrapper* status, unsigned index, const char* relation)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setRelation");
		msgMetadata->items[index].relation = relation;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setOwner(CheckStatusWrapper* status, unsigned index, const char* owner)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setOwner");
		msgMetadata->items[index].owner = owner;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::setAlias(CheckStatusWrapper* status, unsigned index, const char* alias)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "setAlias");
		msgMetadata->items[index].alias = alias;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void MetadataBuilder::truncate(CheckStatusWrapper* status, unsigned count)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		if (count != 0)
			indexError(count - 1, "truncate");

		msgMetadata->items.shrink(count);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::remove(CheckStatusWrapper* status, unsigned index)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "remove");

		msgMetadata->items.remove(index);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void MetadataBuilder::moveNameToIndex(CheckStatusWrapper* status, const char* name, unsigned index)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		indexError(index, "moveNameToIndex");

		for (ObjectsArray<MsgMetadata::Item>::iterator i = msgMetadata->items.begin();
			 i != msgMetadata->items.end();
			 ++i)
		{
			if (i->field == name)
			{
				MsgMetadata::Item copy(getPool(), *i);
				msgMetadata->items.remove(i);
				msgMetadata->items.insert(index, copy);
				return;
			}
		}

		(Arg::Gds(isc_metadata_name) << name).raise();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

unsigned MetadataBuilder::addField(CheckStatusWrapper* status)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		metadataError("addField");

		msgMetadata->items.add();
		return msgMetadata->items.getCount() - 1;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return ~0;
	}
}

IMessageMetadata* MetadataBuilder::getMetadata(CheckStatusWrapper* status)
{
	try
	{
		MutexLockGuard g(mtx, FB_FUNCTION);

		metadataError("getMetadata");

		unsigned i = msgMetadata->makeOffsets();
		if (i != ~0u)
		{
			(Arg::Gds(isc_item_finish) << Arg::Num(i)).raise();
		}

		MsgMetadata* rc = FB_NEW MsgMetadata(msgMetadata);
		rc->addRef();
		return rc;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}

void MetadataBuilder::metadataError(const char* functionName)
{
	// ASF: Current implementation should never set msgMetadata to NULL, but I'm leaving this
	// function and check for now.
	if (!msgMetadata)
	{
		(Arg::Gds(isc_random) << (string("IMetadataBuilder interface is already inactive: "
			"IMetadataBuilder::") + functionName)).raise();
	}
}

void MetadataBuilder::indexError(unsigned index, const char* functionName)
{
	metadataError(functionName);

	if (index >= msgMetadata->items.getCount())
	{
		(Arg::Gds(isc_invalid_index_val) << Arg::Num(index) << (string("IMetadataBuilder::") +
			functionName)).raise();
	}
}


// Add an item based on a descriptor.
void MsgMetadata::addItem(const MetaString& name, bool nullable, const dsc& desc)
{
	Item& item = items.add();
	item.field.assign(name.c_str(), name.length());
	item.nullable = nullable;

	SLONG sqlLen, sqlSubType, sqlScale, sqlType;
	desc.getSqlInfo(&sqlLen, &sqlSubType, &sqlScale, &sqlType);

	item.type = sqlType;
	item.subType = sqlSubType;
	item.length = sqlLen;
	item.scale = sqlScale;
	item.charSet = desc.getCharSet();

	item.finished = true;
}


// returns ~0 on success or index of not finished item
unsigned MsgMetadata::makeOffsets()
{
	length = alignedLength = 0;
	alignment = type_alignments[dtype_short];	// NULL indicator

	for (unsigned n = 0; n < items.getCount(); ++n)
	{
		Item* param = &items[n];
		if (!param->finished)
		{
			length = alignment = 0;
			return n;
		}

		unsigned dtype;
		length = fb_utils::sqlTypeToDsc(length, param->type, param->length,
			&dtype, NULL /*length*/, &param->offset, &param->nullInd);

		if (dtype >= DTYPE_TYPE_MAX)
		{
			length = alignment = 0;
			return n;
		}

		alignment = MAX(alignment, type_alignments[dtype]);
	}

	alignedLength = FB_ALIGN(length, alignment);

	return ~0u;
}


IMetadataBuilder* MsgMetadata::getBuilder(CheckStatusWrapper* status)
{
	try
	{
		IMetadataBuilder* rc = FB_NEW MetadataBuilder(this);
		rc->addRef();
		return rc;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void MsgMetadata::assign(IMessageMetadata* from)
{
	LocalStatus ls;
	CheckStatusWrapper status(&ls);

	unsigned count = from->getCount(&status);
	check(&status);
	items.resize(count);

	for (unsigned index = 0; index < count; ++index)
	{
		items[index].field = from->getField(&status, index);
		check(&status);

		items[index].relation = from->getRelation(&status, index);
		check(&status);

		items[index].owner = from->getOwner(&status, index);
		check(&status);

		items[index].alias = from->getAlias(&status, index);
		check(&status);

		items[index].type = from->getType(&status, index);
		check(&status);

		items[index].nullable = from->isNullable(&status, index);
		check(&status);

		items[index].subType = from->getSubType(&status, index);
		check(&status);

		items[index].length = from->getLength(&status, index);
		check(&status);

		items[index].scale = from->getScale(&status, index);
		check(&status);

		items[index].charSet = from->getCharSet(&status, index);
		check(&status);

		items[index].finished = true;
		check(&status);
	}

	makeOffsets();
}

}	// namespace Firebird
