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
#include "../common/classes/objects_array.h"
#include "../common/StatusArg.h"
#include "../jrd/constants.h"
#include <algorithm>
#include <cctype>

#ifdef SFIO
#include <stdio.h>
#endif

namespace Firebird {

template <typename T>
string toQuotedString(const T& name)
{
	string s;

	if (name.hasData())
	{
		s.reserve(name.length() + 2);

		s.append("\"");

		for (const auto c : name)
		{
			if (c == '"')
				s.append("\"");

			s.append(&c, 1);
		}

		s.append("\"");
	}

	return s;
}

class MetaString
{
private:
	char data[MAX_SQL_IDENTIFIER_SIZE];
	unsigned int count;

	void init() noexcept
	{
		memset(data, 0, MAX_SQL_IDENTIFIER_SIZE);
	}
	MetaString& set(const MetaString& m) noexcept
	{
		memcpy(data, m.data, MAX_SQL_IDENTIFIER_SIZE);
		count = m.count;
		return *this;
	}

public:
	MetaString() noexcept { init(); count = 0; }
	MetaString(const char* s) noexcept { assign(s); }
	MetaString(const char* s, FB_SIZE_T l) noexcept { assign(s, l); }
	MetaString(const MetaString& m) noexcept { set(m); }
	MetaString(const AbstractString& s) noexcept { assign(s.c_str(), s.length()); }
	explicit MetaString(MemoryPool&) noexcept { init(); count = 0; }
	MetaString(MemoryPool&, const char* s) noexcept { assign(s); }
	MetaString(MemoryPool&, const char* s, FB_SIZE_T l) noexcept { assign(s, l); }
	MetaString(MemoryPool&, const MetaString& m) noexcept { set(m); }
	MetaString(MemoryPool&, const AbstractString& s) noexcept { assign(s.c_str(), s.length()); }

public:
	static void parseList(const string& str, ObjectsArray<MetaString>& list)
	{
		auto pos = str.begin();

		const auto skipSpaces = [&pos, &str]() noexcept
		{
			while (pos != str.end() && (*pos == ' ' || *pos == '\t' || *pos == '\f' || *pos == '\r' || *pos == '\n'))
				++pos;

			return pos != str.end();
		};

		const auto isQuoted = [](const string& name) -> bool
		{
			return name.length() >= 2 && name[0] == '"' && name[name.length() - 1] == '"';
		};

		const auto unquote = [&](const string& name) -> string
		{
			if (!isQuoted(name))
				return name;

			string result;

			for (FB_SIZE_T i = 1; i < name.length() - 1; ++i)
			{
				if (name[i] == '"')
				{
					if (i + 1 < name.length() - 1 && name[i + 1] == '"')
						++i;
					else
						(Arg::Gds(isc_invalid_unqualified_name_list) << str).raise();
				}

				result += name[i];
			}

			return result;
		};

		const auto validateUnquotedIdentifier = [&](const string& name)
		{
			if (name.length() > MAX_SQL_IDENTIFIER_LEN)
				(Arg::Gds(isc_invalid_name) << str).raise();

			bool first = true;

			for (const auto c : name)
			{
				if (!((c >= 'A' && c <= 'Z') ||
					  (c >= 'a' && c <= 'z') ||
					  c == '{' ||
					  c == '}' ||
					  (!first && c >= '0' && c <= '9') ||
					  (!first && c == '$') ||
					  (!first && c == '_')))
				{
					(Arg::Gds(isc_invalid_unqualified_name_list) << str).raise();
				}

				first = false;
			}

			return true;
		};

		list.clear();

		if (!skipSpaces())
			return;

		do
		{
			const auto nameStart = pos;
			auto nameEnd = pos;
			bool inQuotes = false;

			while (pos != str.end())
			{
				if (*pos == '"')
					inQuotes = !inQuotes;
				else if (*pos == ',' && !inQuotes)
					break;

				nameEnd = ++pos;
				skipSpaces();
			}

			string name(nameStart, nameEnd);

			if (isQuoted(name))
				name = unquote(name);
			else
			{
				validateUnquotedIdentifier(name);
				std::transform(name.begin(), name.end(), name.begin(), ::toupper);
			}

			if (name.isEmpty())
				(Arg::Gds(isc_invalid_unqualified_name_list) << str).raise();

			list.add(name);

			if (pos == str.end())
				break;

			if (*pos == ',')
			{
				++pos;
				skipSpaces();
			}
		} while(true);
	}

public:
	MetaString& assign(const char* s, FB_SIZE_T l) noexcept;
	MetaString& assign(const char* s) noexcept { return assign(s, s ? fb_strlen(s) : 0); }
	MetaString& clear() noexcept { return assign(nullptr, 0); }
	MetaString& operator=(const char* s) noexcept { return assign(s); }
	MetaString& operator=(const AbstractString& s) noexcept { return assign(s.c_str(), s.length()); }
	MetaString& operator=(const MetaString& m) noexcept { return set(m); }
	char* getBuffer(const FB_SIZE_T l) noexcept;

	FB_SIZE_T length() const noexcept { return count; }
	const char* c_str() const noexcept { return data; }
	const char* nullStr() const noexcept { return (count == 0 ? NULL : data); }
	bool isEmpty() const noexcept { return count == 0; }
	bool hasData() const noexcept { return count != 0; }

	char& operator[](unsigned n) noexcept { return data[n]; }
	char operator[](unsigned n) const noexcept { return data[n]; }

	const char* begin() const noexcept { return data; }
	const char* end() const noexcept { return data + count; }

	int compare(const char* s, FB_SIZE_T l) const noexcept;
	int compare(const char* s) const noexcept { return compare(s, s ? fb_strlen(s) : 0); }
	int compare(const AbstractString& s) const noexcept { return compare(s.c_str(), s.length()); }
	int compare(const MetaString& m) const noexcept { return memcmp(data, m.data, MAX_SQL_IDENTIFIER_SIZE); }

	string toQuotedString() const
	{
		return Firebird::toQuotedString(*this);
	}

	bool operator==(const char* s) const noexcept { return compare(s) == 0; }
	bool operator!=(const char* s) const noexcept { return compare(s) != 0; }
	bool operator==(const AbstractString& s) const noexcept { return compare(s) == 0; }
	bool operator!=(const AbstractString& s) const noexcept { return compare(s) != 0; }
	bool operator==(const MetaString& m) const noexcept { return compare(m) == 0; }
	bool operator!=(const MetaString& m) const noexcept { return compare(m) != 0; }
	bool operator<=(const MetaString& m) const noexcept { return compare(m) <= 0; }
	bool operator>=(const MetaString& m) const noexcept { return compare(m) >= 0; }
	bool operator< (const MetaString& m) const noexcept { return compare(m) <  0; }
	bool operator> (const MetaString& m) const noexcept { return compare(m) >  0; }

	void printf(const char*, ...);
	FB_SIZE_T copyTo(char* to, FB_SIZE_T toSize) const;

protected:
	static void adjustLength(const char* const s, FB_SIZE_T& l) noexcept;
};

} // namespace Firebird

#endif // METASTRING_H
