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
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_UUID_H
#define CLASSES_UUID_H

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../common/os/guid.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>

namespace Firebird
{

class Uuid
{
private:
	explicit Uuid(unsigned version)
	{
		switch (version)
		{
			case 4:
				generateV4();
				break;

			case 7:
				generateV7();
				break;

			default:
				fb_assert(false);
		}
	}

public:
	static Uuid generate(unsigned version)
	{
		return Uuid(version);
	}

public:
	std::size_t extractBytes(std::uint8_t* buffer, std::size_t bufferSize) const
	{
		fb_assert(bufferSize >= bytes.size());
		std::copy(bytes.begin(), bytes.end(), buffer);
		return bytes.size();
	}

	std::size_t toString(char* buffer, std::size_t bufferSize) const
	{
		fb_assert(bufferSize >= STR_LEN);

		return snprintf(buffer, bufferSize, STR_FORMAT,
			bytes[0], bytes[1], bytes[2], bytes[3],
			bytes[4], bytes[5],
			bytes[6], bytes[7],
			bytes[8], bytes[9],
			bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
	}

private:
	void generateV4()
	{
		GenerateRandomBytes(bytes.data(), bytes.size());

		// version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x40;
		bytes[8] = (bytes[8] & 0x3F) | 0x80;
	}

	void generateV7()
	{
		GenerateRandomBytes(bytes.data() + 6, bytes.size() - 6);

		// current timestamp in ms
		const auto now = std::chrono::system_clock::now();
		const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// timestamp
		bytes[0] = (millis >> 40) & 0xFF;
		bytes[1] = (millis >> 32) & 0xFF;
		bytes[2] = (millis >> 24) & 0xFF;
		bytes[3] = (millis >> 16) & 0xFF;
		bytes[4] = (millis >> 8) & 0xFF;
		bytes[5] = millis & 0xFF;

		// version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x70;
		bytes[8] = (bytes[8] & 0x3F) | 0x80;
	}

public:
	static constexpr std::size_t BYTE_LEN = 16;
	static constexpr std::size_t STR_LEN = 36;
	static constexpr const char* STR_FORMAT =
		"%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX";

private:
	std::array<uint8_t, BYTE_LEN> bytes;
};

}	// namespace Firebird

#endif	// CLASSES_UUID_H
