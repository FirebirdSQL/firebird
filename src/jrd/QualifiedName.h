/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		QualifiedName.h
 *	DESCRIPTION:	Qualified metadata name holder.
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_QUALIFIEDNAME_H
#define JRD_QUALIFIEDNAME_H

#include "MetaName.h"
#include "../common/classes/array.h"

namespace Jrd {

class QualifiedName
{
public:
	QualifiedName(MemoryPool& p, const MetaName& aIdentifier, const MetaName& aPackage)
		: identifier(p, aIdentifier),
		  package(p, aPackage)
	{
	}

	QualifiedName(const MetaName& aIdentifier, const MetaName& aPackage)
		: identifier(aIdentifier),
		  package(aPackage)
	{
	}

	QualifiedName(MemoryPool& p, const MetaName& aIdentifier)
		: identifier(p, aIdentifier),
		  package(p)
	{
	}

	explicit QualifiedName(const MetaName& aIdentifier)
		: identifier(aIdentifier)
	{
	}

	explicit QualifiedName(MemoryPool& p)
		: identifier(p),
		  package(p)
	{
	}

	QualifiedName()
	{
	}

	QualifiedName(MemoryPool& p, const QualifiedName& src)
		: identifier(p, src.identifier),
		  package(p, src.package)
	{
	}

public:
	bool operator >(const QualifiedName& m) const
	{
		return package > m.package || (package == m.package && identifier > m.identifier);
	}

	bool operator ==(const QualifiedName& m) const
	{
		return identifier == m.identifier && package == m.package;
	}

	bool operator !=(const QualifiedName& m) const
	{
		return !(identifier == m.identifier && package == m.package);
	}

public:
	Firebird::string toString() const
	{
		Firebird::string s;
		if (package.hasData())
		{
			s = package.c_str();
			s.append(".");
		}
		s.append(identifier.c_str());
		return s;
	}

public:
	MetaName identifier;
	MetaName package;
};

} // namespace Jrd

#endif // JRD_QUALIFIEDNAME_H
