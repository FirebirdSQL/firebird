/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		MetaString.h
 *	DESCRIPTION:	metadata name holder
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2005 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef METASTRING_H
#define METASTRING_H

#include "../common/classes/fb_string.h"
#include "../common/classes/fb_pair.h"
#include "../jrd/constants.h"

#ifdef SFIO
#include <stdio.h>
#endif

namespace Firebird {

class MetaString
{
private:
	char data[MAX_SQL_IDENTIFIER_SIZE];
	unsigned int count;

	void init()
	{
		memset(data, 0, MAX_SQL_IDENTIFIER_SIZE);
	}
	MetaString& set(const MetaString& m)
	{
		memcpy(data, m.data, MAX_SQL_IDENTIFIER_SIZE);
		count = m.count;
		return *this;
	}

public:
	MetaString() { init(); count = 0; }
	MetaString(const char* s) { assign(s); }
	MetaString(const char* s, FB_SIZE_T l) { assign(s, l); }
	MetaString(const MetaString& m) { set(m); }
	MetaString(const AbstractString& s) { assign(s.c_str(), s.length()); }
	explicit MetaString(MemoryPool&) { init(); count = 0; }
	MetaString(MemoryPool&, const char* s) { assign(s); }
	MetaString(MemoryPool&, const char* s, FB_SIZE_T l) { assign(s, l); }
	MetaString(MemoryPool&, const MetaString& m) { set(m); }
	MetaString(MemoryPool&, const AbstractString& s) { assign(s.c_str(), s.length()); }

	MetaString& assign(const char* s, FB_SIZE_T l);
	MetaString& assign(const char* s) { return assign(s, s ? fb_strlen(s) : 0); }
	MetaString& operator=(const char* s) { return assign(s); }
	MetaString& operator=(const AbstractString& s) { return assign(s.c_str(), s.length()); }
	MetaString& operator=(const MetaString& m) { return set(m); }
	char* getBuffer(const FB_SIZE_T l);

	FB_SIZE_T length() const { return count; }
	const char* c_str() const { return data; }
	const char* nullStr() const { return (count == 0 ? NULL : data); }
	bool isEmpty() const { return count == 0; }
	bool hasData() const { return count != 0; }

	char& operator[](unsigned n) { return data[n]; }
	char operator[](unsigned n) const { return data[n]; }

	const char* begin() const { return data; }
	const char* end() const { return data + count; }

	int compare(const char* s, FB_SIZE_T l) const;
	int compare(const char* s) const { return compare(s, s ? fb_strlen(s) : 0); }
	int compare(const AbstractString& s) const { return compare(s.c_str(), s.length()); }
	int compare(const MetaString& m) const { return memcmp(data, m.data, MAX_SQL_IDENTIFIER_SIZE); }

	bool operator==(const char* s) const { return compare(s) == 0; }
	bool operator!=(const char* s) const { return compare(s) != 0; }
	bool operator==(const AbstractString& s) const { return compare(s) == 0; }
	bool operator!=(const AbstractString& s) const { return compare(s) != 0; }
	bool operator==(const MetaString& m) const { return compare(m) == 0; }
	bool operator!=(const MetaString& m) const { return compare(m) != 0; }
	bool operator<=(const MetaString& m) const { return compare(m) <= 0; }
	bool operator>=(const MetaString& m) const { return compare(m) >= 0; }
	bool operator< (const MetaString& m) const { return compare(m) <  0; }
	bool operator> (const MetaString& m) const { return compare(m) >  0; }

	void printf(const char*, ...);
	FB_SIZE_T copyTo(char* to, FB_SIZE_T toSize) const;

protected:
	static void adjustLength(const char* const s, FB_SIZE_T& l);
};

} // namespace Firebird

#endif // METASTRING_H
