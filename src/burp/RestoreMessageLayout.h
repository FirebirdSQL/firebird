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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef BURP_RESTORE_MESSAGE_LAYOUT_H
#define BURP_RESTORE_MESSAGE_LAYOUT_H

#include "../common/classes/array.h"
#include "../common/dsc.h"

struct burp_fld;
struct burp_rel;

namespace Burp {

class RestoreMessageLayout
{
public:
	struct Field
	{
		burp_fld* metadata;
		dsc descriptor;
		USHORT alignment;
		RCRD_OFFSET offset;
		RCRD_OFFSET nullOffset;
	};

public:
	explicit RestoreMessageLayout(Firebird::MemoryPool& pool)
		: fields(pool)
	{
	}

	RestoreMessageLayout(const RestoreMessageLayout&) = delete;
	RestoreMessageLayout& operator=(const RestoreMessageLayout&) = delete;

public:
	// Computes message offsets for all non-computed fields and writes them back
	// into the field metadata (fld_offset, fld_missing_*) - the same convention
	// as the BLR message builders.
	void build(burp_rel* relation, bool fixFssData, CSetId fixFssDataId);

	void clear() noexcept;

	RCRD_LENGTH getLength() const noexcept
	{
		return length;
	}

	USHORT getMaxAlignment() const noexcept
	{
		return maxAlignment;
	}

	// In-memory batch stride: message length rounded up to the maximum field
	// alignment so consecutive packed records keep descriptor alignment.
	// The serialized backup format still uses getLength().
	RCRD_LENGTH getStride() const noexcept;

	const Field* begin() const noexcept
	{
		return fields.begin();
	}

	const Field* end() const noexcept
	{
		return fields.end();
	}

private:
	Firebird::HalfStaticArray<Field, 16> fields;
	RCRD_LENGTH length = 0;
	USHORT maxAlignment = 0;
};

} // namespace Burp

#endif // BURP_RESTORE_MESSAGE_LAYOUT_H
