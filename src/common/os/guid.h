/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		guid.h
 *	DESCRIPTION:	Portable GUID definition
 *
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *  Adriano dos Santos Fernandes
 *
 */

#ifndef FB_GUID_H
#define FB_GUID_H

#include <stdlib.h>
#include <stdio.h>
#include <optional>

#include "../common/classes/fb_string.h"

#ifdef WIN_NT
#include <rpc.h>
#else
struct UUID	// Compatible with Win32 UUID struct layout
{
	ULONG Data1;
	USHORT Data2;
	USHORT Data3;
	UCHAR Data4[8];
};
#endif

static_assert(sizeof(UUID) == 16, "Guid size mismatch");

namespace Firebird {

const int GUID_BUFF_SIZE = 39;
const int GUID_BODY_SIZE = 36;

void GenerateRandomBytes(void* buffer, FB_SIZE_T size);

// Generates platform-dependent UUID compatible with RFC 4122

void GenerateGuid(UUID* guid);

// Wrapper class around UUID

class Guid
{
	// Some versions of MSVC cannot recognize hh specifier but MSVC 2015 has it
	static constexpr const char* GUID_FORMAT =
		"{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}";
	static constexpr int GUID_FORMAT_ARGS = 11;

	Guid()
	{
		memset(&m_data, 0, SIZE);
	}

public:
	static constexpr ULONG SIZE = sizeof(UUID);

	static Guid empty()
	{
		return Guid();
	}

	Guid(const Guid& other)
		: m_data(other.m_data)
	{}

	Guid(const UUID& uuid)
		: m_data(uuid)
	{}

	explicit Guid(const UCHAR* data)
	{
		memcpy(&m_data, data, SIZE);
	}

	Guid& operator=(const Guid& other)
	{
		m_data = other.m_data; // copy struct by value
		return *this;
	}

	bool operator==(const Guid& other) const
	{
		return memcmp(&m_data, &other.m_data, SIZE) == 0;
	}

	bool operator!=(const Guid& other) const
	{
		return !(*this == other);
	}

	const UCHAR* getData() const
	{
		return reinterpret_cast<const UCHAR*>(&m_data);
	}

	void assign(const UCHAR* buffer)
	{
		memcpy(&m_data, buffer, SIZE);
	}

	void toString(char* buffer) const
	{
		sprintf(buffer, GUID_FORMAT,
			m_data.Data1, m_data.Data2, m_data.Data3,
			m_data.Data4[0], m_data.Data4[1], m_data.Data4[2], m_data.Data4[3],
			m_data.Data4[4], m_data.Data4[5], m_data.Data4[6], m_data.Data4[7]);
	}

	Firebird::string toString() const
	{
		Firebird::string result;
		toString(result.getBuffer(GUID_BUFF_SIZE - 1));
		return result;
	}

	Firebird::PathName toPathName() const
	{
		Firebird::PathName result;
		toString(result.getBuffer(GUID_BUFF_SIZE - 1));
		return result;
	}

	static std::optional<Guid> fromString(const char* buffer)
	{
		fb_assert(buffer);

		UUID uuid;
		const auto result = sscanf(buffer, GUID_FORMAT,
			&uuid.Data1, &uuid.Data2, &uuid.Data3,
			&uuid.Data4[0], &uuid.Data4[1], &uuid.Data4[2], &uuid.Data4[3],
			&uuid.Data4[4], &uuid.Data4[5], &uuid.Data4[6], &uuid.Data4[7]);

		return (result == GUID_FORMAT_ARGS) ? std::optional<Guid>(uuid) : std::nullopt;
	}

	static std::optional<Guid> fromString(const Firebird::string& str)
	{
		return fromString(str.nullStr());
	}

	void copyTo(UUID& ptr) const
	{
		ptr = m_data; // copy struct by value
	}

	void copyTo(UCHAR* ptr) const
	{
		memcpy(ptr, &m_data, SIZE);
	}

	// Convert platform-dependent GUID into platform-independent form according to RFC 4122

	void convert(UCHAR* data) const
	{
		data[0] = (m_data.Data1 >> 24) & 0xFF;
		data[1] = (m_data.Data1 >> 16) & 0xFF;
		data[2] = (m_data.Data1 >> 8) & 0xFF;
		data[3] = m_data.Data1 & 0xFF;
		data[4] = (m_data.Data2 >> 8) & 0xFF;
		data[5] = m_data.Data2 & 0xFF;
		data[6] = (m_data.Data3 >> 8) & 0xFF;
		data[7] = m_data.Data3 & 0xFF;
		data[8] = m_data.Data4[0];
		data[9] = m_data.Data4[1];
		data[10] = m_data.Data4[2];
		data[11] = m_data.Data4[3];
		data[12] = m_data.Data4[4];
		data[13] = m_data.Data4[5];
		data[14] = m_data.Data4[6];
		data[15] = m_data.Data4[7];
	}

	// Generate new GUID

	static Guid generate()
	{
		UUID uuid;
		GenerateGuid(&uuid);
		return Guid(uuid);
	}

private:
	UUID m_data;
};

}	// namespace

#endif	// FB_GUID_H
