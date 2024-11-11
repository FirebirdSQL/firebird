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
#include "../common/StatusArg.h"
#include <algorithm>
#include <cctype>

namespace Firebird {

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
		  package(p, src.package)
	{
	}

	BaseQualifiedName(const BaseQualifiedName& src)
		: object(src.object),
		  schema(src.schema),
		  package(src.package)
	{
	}

	template <typename TT>
	BaseQualifiedName(const BaseQualifiedName<TT>& src)
		: object(src.object),
		  schema(src.schema),
		  package(src.package)
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
	static BaseQualifiedName<T> parseSchemaObject(const string& str)
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

			for (size_t i = 1; i < name.length() - 1; ++i)
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

		BaseQualifiedName<T> result;
		string schema, object;

		// Find the last unquoted dot to determine schema and object
		bool inQuotes = false;
		auto lastDotPos = string::npos;

		for (size_t i = 0; i < str.size(); ++i)
		{
			if (str[i] == '"')
				inQuotes = !inQuotes;
			else if (str[i] == '.' && !inQuotes)
				lastDotPos = i;
		}

		if (lastDotPos != string::npos)
		{
			schema = str.substr(0, lastDotPos);
			object = str.substr(lastDotPos + 1);
		}
		else
			object = str;

		schema.trim();
		object.trim();

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

		// Process object
		if (isQuoted(object))
			result.object = unquote(object);
		else
		{
			validateUnquotedIdentifier(object);

			std::transform(object.begin(), object.end(), object.begin(), ::toupper);
			result.object = object;
		}

		return result;
	}

public:
	bool operator<(const BaseQualifiedName& m) const
	{
		return schema < m.schema ||
			(schema == m.schema && object < m.object) ||
			(schema == m.schema && object == m.object && package < m.package);
	}

	bool operator>(const BaseQualifiedName& m) const
	{
		return schema > m.schema ||
			(schema == m.schema && object > m.object) ||
			(schema == m.schema && object == m.object && package > m.package);
	}

	bool operator==(const BaseQualifiedName& m) const
	{
		return schema == m.schema && object == m.object && package == m.package;
	}

	bool operator!=(const BaseQualifiedName& m) const
	{
		return !(*this == m);
	}

public:
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

	Firebird::string toString() const
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
};

using QualifiedMetaString = Firebird::BaseQualifiedName<MetaString>;

} // namespace Firebird

#endif // QUALIFIED_METASTRING_H
