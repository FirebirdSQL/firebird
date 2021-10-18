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
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>
#include <utility>
#include <cassert>
#include <string.h>
#include <unicode/ucal.h>
#include "../../../src/common/TimeZones.h"

using std::exception;
using std::cerr;
using std::cout;
using std::endl;
using std::ofstream;
using std::string;
using std::unordered_set;
using std::vector;
using std::min_element;
using std::pair;


void writeString(ofstream& stream, const char* str, bool includeNullByte)
{
	stream.write(str, strlen(str) + (includeNullByte ? 1 : 0));
}


int run(int argc, const char* argv[])
{
	if (argc != 4)
	{
		cerr << "Syntax: " << argv[0] << " <ids.dat file> <TimeZones.h file> <TimeZoneIds.h file>" << endl;
		return 1;
	}

	const unsigned MAX_ID = 65535;
	unordered_set<string> zonesSet;
	vector<string> zonesVector;

	for (unsigned i = 0; i < sizeof(BUILTIN_TIME_ZONE_LIST) / sizeof(BUILTIN_TIME_ZONE_LIST[0]); ++i)
	{
		zonesSet.insert(BUILTIN_TIME_ZONE_LIST[i]);
		zonesVector.push_back(BUILTIN_TIME_ZONE_LIST[i]);
	}

	UErrorCode icuErrorCode = U_ZERO_ERROR;

	const char* databaseVersion = ucal_getTZDataVersion(&icuErrorCode);
	cout << "Database version: " << databaseVersion << endl;

	UEnumeration* uenum = ucal_openTimeZones(&icuErrorCode);
	assert(uenum);

	int32_t length;
	char buffer[512];

	while (const UChar* str = uenum_unext(uenum, &length, &icuErrorCode))
	{
		for (unsigned i = 0; i <= length; ++i)
			buffer[i] = (char) str[i];

		if (zonesSet.find(buffer) == zonesSet.end())
		{
			cout << "New time zone included: " << buffer << "(" << (MAX_ID - zonesVector.size()) << ")." << endl;
			zonesVector.push_back(buffer);
		}
	}

	uenum_close(uenum);

	ofstream datStream, headerStream, idsHeaderStream;

	datStream.open(argv[1], std::fstream::out | std::fstream::trunc | std::fstream::binary);
	headerStream.open(argv[2], std::fstream::out | std::fstream::trunc);
	idsHeaderStream.open(argv[3], std::fstream::out | std::fstream::trunc);

	uint8_t byte;

	// file signature
	writeString(datStream, "FBTZ", true);

	// file format version
	const unsigned fileVersion = 1;
	byte = fileVersion & 0xFF;
	datStream.write((char*) &byte, 1);
	byte = (fileVersion >> 8) & 0xFF;
	datStream.write((char*) &byte, 1);

	// time zone database version
	writeString(datStream, databaseVersion, true);

	// count
	byte = zonesVector.size() & 0xFF;
	datStream.write((char*) &byte, 1);
	byte = (zonesVector.size() >> 8) & 0xFF;
	datStream.write((char*) &byte, 1);

	unsigned index = MAX_ID;

	for (const auto zone : zonesVector)
	{
		// null terminated name
		datStream.write(zone.c_str(), zone.length() + 1);
		--index;
	}

	datStream.close();

	writeString(headerStream,
		"// The content of this file is generated with help of update-ids utility Do not edit.\n\n",
		false);

	writeString(idsHeaderStream,
		"#ifndef FIREBIRD_TIME_ZONES_H\n"
		"#define FIREBIRD_TIME_ZONES_H\n\n",
		false);

	writeString(idsHeaderStream,
		"// The content of this file is generated with help of update-ids utility Do not edit.\n\n",
		false);

	sprintf(buffer, "static const char* BUILTIN_TIME_ZONE_VERSION = \"%s\";\n\n", databaseVersion);
	writeString(headerStream, buffer, false);

	writeString(headerStream,
		"// Do not change order of items in this array! The index corresponds to a TimeZone ID, which must be fixed!\n",
		false);
	writeString(headerStream, "static const char* BUILTIN_TIME_ZONE_LIST[] = {\n", false);

	index = MAX_ID;

	const std::regex plusNumberRegex("\\+([[:digit:]])");
	const std::regex minusNumberRegex("\\-([[:digit:]])");

	for (const auto zone : zonesVector)
	{
		auto zoneDef = zone;

		zoneDef = std::regex_replace(zoneDef, plusNumberRegex, "_plus_$1");
		zoneDef = std::regex_replace(zoneDef, minusNumberRegex, "_minus_$1");
		std::replace(zoneDef.begin(), zoneDef.end(), '/', '_');
		std::replace(zoneDef.begin(), zoneDef.end(), '-', '_');

		std::transform(zoneDef.begin(), zoneDef.end(), zoneDef.begin(),
			[](unsigned char c) { return std::tolower(c); });

		zoneDef = "fb_tzid_" + zoneDef;

		sprintf(buffer, "\t\"%s\"%s\t// %s - %u\n",
			zone.c_str(), (zone == zonesVector.back() ? "" : ","), zoneDef.c_str(), index);
		writeString(headerStream, buffer, false);

		sprintf(buffer, "#define %-45s%u /* %s */\n", zoneDef.c_str(), index, zone.c_str());
		writeString(idsHeaderStream, buffer, false);

		--index;
	}

	writeString(headerStream, "};\n", false);

	writeString(idsHeaderStream,
		"\n#endif /* FIREBIRD_TIME_ZONES_H */\n",
		false);

	headerStream.close();
	idsHeaderStream.close();

	return 0;
}


int main(int argc, const char* argv[])
{
	try
	{
		return run(argc, argv);
	}
	catch (const exception& e)
	{
		cerr << e.what() << '\n';
	}
}
