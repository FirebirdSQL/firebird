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
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef QUALIFIED_METASTRING_H
#define QUALIFIED_METASTRING_H

#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include "../common/StatusArg.h"
#include <algorithm>
#include <cctype>

namespace Firebird {

template <typename T>
concept IsQualifiedName = requires(T t)
{
	{ t.schema };
	{ t.object };
	{ t.package };
};

template <typename T>
class BaseQualifiedName
{
public:
	explicit BaseQualifiedName(MemoryPool& p, const T& aObject,
			const T& aSchema = {}, const T& aPackage = {})
		: object(p, aObject),
		  schema(p, aSchema),
		  package(p, aPackage)
	{
	}

	explicit BaseQualifiedName(const T& aObject, const T& aSchema = {}, const T& aPackage = {})
		: object(aObject),
		  schema(aSchema),
		  package(aPackage)
	{
	}

	BaseQualifiedName(MemoryPool& p, const BaseQualifiedName& src)
		: object(p, src.object),
		  schema(p, src.schema),
		  package(p, src.package),
		  unambiguous(src.isUnambiguous())
	{
	}

	BaseQualifiedName(const BaseQualifiedName& src) = default;

	template <typename TT>
	BaseQualifiedName(const BaseQualifiedName<TT>& src)
		: object(src.object),
		  schema(src.schema),
		  package(src.package),
		  unambiguous(src.isUnambiguous())
	{
	}

	explicit BaseQualifiedName(MemoryPool& p)
		: object(p),
		  schema(p),
		  package(p)
	{
	}

	BaseQualifiedName()
	{
	}

public:
	static void parseSchemaObjectListNoSep(const string& str, ObjectsArray<BaseQualifiedName<T>>& list)
	{
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
						(Arg::Gds(isc_invalid_name) << str).raise();
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
					(Arg::Gds(isc_invalid_name) << str).raise();
				}

				first = false;
			}

			return true;
		};

		FB_SIZE_T i = 0;

		const auto skipSpaces = [&]()
		{
			while (i < str.size() &&
				(str[i] == ' ' || str[i] == '\t' || str[i] == '\f' || str[i] == '\r' || str[i] == '\n'))
			{
				++i;
			}
		};

		skipSpaces();

		do
		{
			BaseQualifiedName<T> result;
			string schema, object;

			const auto start = i;
			bool nameFound = false;
			bool inQuotes = false;
			auto dotPos = string::npos;

			for (; !nameFound && i < str.size(); ++i)
			{
				const auto ch = str[i];

				if (inQuotes)
				{
					if (ch == '"')
					{
						if (i + 1 < str.size() && str[i + 1] == '"')
							++i;
						else
							inQuotes = false;
					}
				}
				else
				{
					switch (ch)
					{
						case '"':
							inQuotes = true;
							break;

						case '.':
						{
							dotPos = i++;
							skipSpaces();
							--i;
							break;
						}

						case ' ':
						case '\t':
						case '\f':
						case '\r':
						case '\n':
						{
							skipSpaces();

							if (i < str.size() && str[i] == '.')
							{
								dotPos = i++;
								skipSpaces();
							}
							else
								nameFound = true;

							--i;
							break;
						}
					}
				}
			}

			if (dotPos != string::npos)
			{
				schema = str.substr(start, dotPos - start);
				object = str.substr(dotPos + 1, i - dotPos - 1);
			}
			else
				object = str.substr(start, i - start);

			schema.trim(" \t\f\r\n");
			object.trim(" \t\f\r\n");

			// Process schema if it exists
			if (schema.hasData())
			{
				if (isQuoted(schema))
					result.schema = unquote(schema);
				else
				{
					validateUnquotedIdentifier(schema);

					std::transform(schema.begin(), schema.end(), schema.begin(), ::toupper);
					result.schema = schema;
				}
			}

			if (dotPos != string::npos && result.schema.isEmpty())
				(Arg::Gds(isc_invalid_name) << str).raise();

			// Process object
			if (isQuoted(object))
				result.object = unquote(object);
			else
			{
				validateUnquotedIdentifier(object);

				std::transform(object.begin(), object.end(), object.begin(), ::toupper);
				result.object = object;
			}

			if (result.object.isEmpty())
				(Arg::Gds(isc_invalid_name) << str).raise();

			list.add(result);
		} while (i < str.size());
	}

	static BaseQualifiedName<T> parseSchemaObject(const string& str)
	{
		ObjectsArray<BaseQualifiedName<T>> list;
		parseSchemaObjectListNoSep(str, list);

		if (list.getCount() != 1)
			(Arg::Gds(isc_invalid_name) << str).raise();

		return list[0];
	}

public:
	template <IsQualifiedName U>
	bool operator<(const U& m) const
	{
		return schema < m.schema ||
			(schema == m.schema && object < m.object) ||
			(schema == m.schema && object == m.object && package < m.package);
	}

	template <IsQualifiedName U>
	bool operator>(const U& m) const
	{
		return schema > m.schema ||
			(schema == m.schema && object > m.object) ||
			(schema == m.schema && object == m.object && package > m.package);
	}

	template <IsQualifiedName U>
	bool operator==(const U& m) const
	{
		return schema == m.schema && object == m.object && package == m.package;
	}

	template <IsQualifiedName U>
	bool operator!=(const U& m) const
	{
		return !(*this == m);
	}

public:
	bool isUnambiguous() const
	{
		return unambiguous;
	}

	void setUnambiguous(bool value)
	{
		unambiguous = value;
	}

	BaseQualifiedName getSchemaAndPackage() const
	{
		return BaseQualifiedName(package, schema);
	}

	void clear()
	{
		object = {};
		schema = {};
		package = {};
	}

	bool hasData() const
	{
		return object.hasData();
	}

	bool isEmpty() const
	{
		return object.isEmpty();
	}

	Firebird::string toQuotedString() const
	{
		Firebird::string s;

		const auto appendName = [&s](const T& name) {
			if (name.hasData())
			{
				s += name.toQuotedString();
				return true;
			}

			return false;
		};

		if (appendName(schema))
			s.append(".");

		if (appendName(package))
			s.append(".");

		appendName(object);

		return s;
	}

public:
	T object;
	T schema;
	T package;

private:
	bool unambiguous = false;
};

using QualifiedMetaString = Firebird::BaseQualifiedName<MetaString>;

} // namespace Firebird

#endif // QUALIFIED_METASTRING_H
