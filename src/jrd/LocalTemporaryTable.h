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
 *  Copyright (c) 2025 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_LOCAL_TEMPORARY_TABLE_H
#define JRD_LOCAL_TEMPORARY_TABLE_H

#include "firebird.h"
#include "../jrd/constants.h"
#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/dsc.h"
#include "../common/classes/alloc.h"
#include "../common/classes/objects_array.h"
#include <optional>

namespace Jrd
{
	class jrd_rel;

	class LocalTemporaryTable
	{
	public:
		class Field
		{
		public:
			explicit Field(MemoryPool& pool)
				: name(pool)
			{
				desc.clear();
			}

			Field(MemoryPool& pool, const Field& other)
				: name(pool, other.name),
				  source(pool, other.source),
				  collationId(other.collationId),
				  charSetId(other.charSetId),
				  id(other.id),
				  position(other.position),
				  segLength(other.segLength),
				  charLength(other.charLength),
				  precision(other.precision),
				  notNullFlag(other.notNullFlag),
				  desc(other.desc)
			{
			}

		public:
			MetaName name;
			QualifiedName source;
			std::optional<SSHORT> collationId;
			std::optional<SSHORT> charSetId;
			USHORT id = 0;
			USHORT position = 0;
			USHORT segLength = 0;
			USHORT charLength = 0;
			USHORT precision = 0;
			bool notNullFlag = false;
			dsc desc;
		};

		class Index
		{
		public:
			explicit Index(MemoryPool& pool)
				: name(pool),
				  columns(pool)
			{
			}

			Index(MemoryPool& pool, const QualifiedName& aName)
				: name(pool, aName),
				  columns(pool)
			{
			}

			Index(MemoryPool& pool, const Index& other)
				: name(pool, other.name),
				  columns(pool, other.columns),
				  unique(other.unique),
				  descending(other.descending),
				  inactive(other.inactive),
				  id(other.id)
			{
			}

		public:
			QualifiedName name;
			Firebird::ObjectsArray<MetaName> columns;
			bool unique = false;
			bool descending = false;
			bool inactive = false;
			USHORT id = 0;
		};

	public:
		LocalTemporaryTable(MemoryPool& pool, const QualifiedName& aName)
			: name(pool, aName),
			  fields(pool),
			  pendingFields(pool),
			  indexes(pool)
		{
		}

		LocalTemporaryTable(MemoryPool& pool, const LocalTemporaryTable& other)
			: name(pool, other.name),
			  fields(pool, other.fields),
			  pendingFields(pool, other.pendingFields),
			  indexes(pool, other.indexes),
			  relationType(other.relationType),
			  relation(other.relation),
			  relationId(other.relationId),
			  nextFieldId(other.nextFieldId),
			  nextIndexId(other.nextIndexId),
			  hasPendingChanges(other.hasPendingChanges)
		{
		}

	public:
		QualifiedName name;
		Firebird::ObjectsArray<Field> fields;  // Committed state (stable)
		Firebird::ObjectsArray<Field> pendingFields;  // Uncommitted changes (working copy)
		Firebird::ObjectsArray<Index> indexes;
		rel_t relationType;
		jrd_rel* relation = nullptr;
		USHORT relationId = 0;
		USHORT nextFieldId = 0;
		USHORT nextIndexId = 0;
		bool hasPendingChanges = false;
	};
}	// namespace Jrd

#endif	// JRD_LOCAL_TEMPORARY_TABLE_H
