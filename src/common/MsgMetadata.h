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
 *		Alex Peshkov
 *
 */

#ifndef COMMON_MSG_METADATA_H
#define COMMON_MSG_METADATA_H

#include "firebird/Interface.h"
#include "iberror.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/dsc.h"

namespace Firebird {

class MetadataBuilder;
class MetaString;
class StatementMetadata;
class MetadataFromBlr;

class MsgMetadata : public RefCntIface<IMessageMetadataImpl<MsgMetadata, CheckStatusWrapper> >
{
	friend class MetadataBuilder;
	friend class StatementMetadata;
	friend class MetadataFromBlr;

public:
	struct Item
	{
		explicit Item(MemoryPool& pool)
			: field(pool),
			  relation(pool),
			  owner(pool),
			  alias(pool),
			  type(0),
			  subType(0),
			  length(0),
			  scale(0),
			  charSet(0),
			  offset(0),
			  nullInd(0),
			  nullable(false),
			  finished(false)
		{
		}

		Item(MemoryPool& pool, const Item& v)
			: field(pool, v.field),
			  relation(pool, v.relation),
			  owner(pool, v.owner),
			  alias(pool, v.alias),
			  type(v.type),
			  subType(v.subType),
			  length(v.length),
			  scale(v.scale),
			  charSet(v.charSet),
			  offset(v.offset),
			  nullInd(v.nullInd),
			  nullable(v.nullable),
			  finished(v.finished)
		{
		}

		string field;
		string relation;
		string owner;
		string alias;
		unsigned type;
		int subType;
		unsigned length;
		int scale;
		unsigned charSet;
		unsigned offset;
		unsigned nullInd;
		bool nullable;
		bool finished;
	};

public:
	explicit MsgMetadata(MsgMetadata* from)
		: items(getPool(), from->items),
		  length(from->length),
		  alignment(from->alignment),
		  alignedLength(from->alignedLength)
	{
	}

	explicit MsgMetadata(IMessageMetadata* from)
		: items(getPool()),
		  length(0),
		  alignment(0),
		  alignedLength(0)
	{
		assign(from);
	}

	MsgMetadata()
		: items(getPool()),
		  length(0),
		  alignment(0),
		  alignedLength(0)
	{
	}

	void setItemsCount(unsigned n)
	{
		items.resize(n);
		length = 0;
	}

	Item& getItem(unsigned n)
	{
		fb_assert(n < items.getCount());
		return items[n];
	}

	unsigned getMessageLength()
	{
		return length;
	}

	unsigned getCount()
	{
		return items.getCount();
	}

	void reset()
	{
		setItemsCount(0);
	}

	// IMessageMetadata implementation
	unsigned getCount(CheckStatusWrapper* /*status*/)
	{
		return (unsigned) items.getCount();
	}

	const char* getField(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].field.c_str();

		raiseIndexError(status, index, "getField");
		return NULL;
	}

	const char* getRelation(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].relation.c_str();

		raiseIndexError(status, index, "getRelation");
		return NULL;
	}

	const char* getOwner(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].owner.c_str();

		raiseIndexError(status, index, "getOwner");
		return NULL;
	}

	const char* getAlias(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].alias.c_str();

		raiseIndexError(status, index, "getAlias");
		return NULL;
	}

	unsigned getType(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].type;

		raiseIndexError(status, index, "getType");
		return 0;
	}

	FB_BOOLEAN isNullable(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].nullable;

		raiseIndexError(status, index, "isNullable");
		return false;
	}

	int getSubType(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].subType;

		raiseIndexError(status, index, "getSubType");
		return 0;
	}

	unsigned getLength(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].length;

		raiseIndexError(status, index, "getLength");
		return 0;
	}

	int getScale(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].scale;

		raiseIndexError(status, index, "getScale");
		return 0;
	}

	unsigned getCharSet(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].charSet;

		raiseIndexError(status, index, "getCharSet");
		return 0;
	}

	unsigned getOffset(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].offset;

		raiseIndexError(status, index, "getOffset");
		return 0;
	}

	unsigned getNullOffset(CheckStatusWrapper* status, unsigned index)
	{
		if (index < items.getCount())
			return items[index].nullInd;

		raiseIndexError(status, index, "getOffset");
		return 0;
	}

	IMetadataBuilder* getBuilder(CheckStatusWrapper* status);

	unsigned getMessageLength(CheckStatusWrapper* /*status*/)
	{
		return length;
	}

	unsigned getAlignment(CheckStatusWrapper* /*status*/)
	{
		return alignment;
	}

	unsigned getAlignedLength(CheckStatusWrapper* /*status*/)
	{
		return alignedLength;
	}

public:
	void addItem(const MetaString& name, bool nullable, const dsc& desc);
	unsigned makeOffsets();

private:
	void raiseIndexError(CheckStatusWrapper* status, unsigned index, const char* method) const
	{
		(Arg::Gds(isc_invalid_index_val) <<
		 Arg::Num(index) << (string("IMessageMetadata::") + method)).copyTo(status);
	}

	void assign(IMessageMetadata* from);

private:
	ObjectsArray<Item> items;
	unsigned length, alignment, alignedLength;
};

//class AttMetadata : public IMessageMetadataBaseImpl<AttMetadata, CheckStatusWrapper, MsgMetadata>
class AttMetadata : public MsgMetadata
{
public:
	explicit AttMetadata(RefCounted* att)
		: attachment(att)
	{ }

	RefPtr<RefCounted> attachment;
};

class MetadataBuilder FB_FINAL :
	public RefCntIface<IMetadataBuilderImpl<MetadataBuilder, CheckStatusWrapper> >
{
public:
	explicit MetadataBuilder(const MsgMetadata* from);
	MetadataBuilder(unsigned fieldCount);

	// IMetadataBuilder implementation
	void setType(CheckStatusWrapper* status, unsigned index, unsigned type);
	void setSubType(CheckStatusWrapper* status, unsigned index, int subType);
	void setLength(CheckStatusWrapper* status, unsigned index, unsigned length);
	void setCharSet(CheckStatusWrapper* status, unsigned index, unsigned charSet);
	void setScale(CheckStatusWrapper* status, unsigned index, int scale);
	void truncate(CheckStatusWrapper* status, unsigned count);
	void remove(CheckStatusWrapper* status, unsigned index);
	void moveNameToIndex(CheckStatusWrapper* status, const char* name, unsigned index);
	unsigned addField(CheckStatusWrapper* status);
	IMessageMetadata* getMetadata(CheckStatusWrapper* status);
	void setField(CheckStatusWrapper* status, unsigned index, const char* field);
	void setRelation(CheckStatusWrapper* status, unsigned index, const char* relation);
	void setOwner(CheckStatusWrapper* status, unsigned index, const char* owner);
	void setAlias(CheckStatusWrapper* status, unsigned index, const char* alias);

private:
	RefPtr<MsgMetadata> msgMetadata;
	Mutex mtx;

	void metadataError(const char* functionName);
	void indexError(unsigned index, const char* functionName);
};

}	// namespace Firebird

#endif	// COMMON_MSG_METADATA_H
