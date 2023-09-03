/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Coercion.h
 *      DESCRIPTION:    Automatically coercing user datatypes
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2019 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 */

#ifndef JRD_COERCION_H
#define JRD_COERCION_H

#include "firebird.h"

#include "../common/classes/array.h"
#include "../common/dsc.h"

namespace Jrd
{
class thread_db;
class TypeClause;

class CoercionRule
{
public:
	CoercionRule()
		: fromMask(0), toMask(0)
	{
		fromDsc.clear();
		toDsc.clear();
	}

	void setRule(const TypeClause* from, const TypeClause *to);
	dsc* makeLegacy(USHORT mask = 0);
	bool coerce(thread_db* tdbb, dsc* d) const;
	bool match(const dsc* d) const;
	bool operator==(const CoercionRule& rule) const;

private:
	void raiseError();
	dsc fromDsc, toDsc;
	USHORT fromMask, toMask;
};

class CoercionArray : public Firebird::HalfStaticArray<CoercionRule, 4>
{
public:
	CoercionArray(MemoryPool& p)
		: Firebird::HalfStaticArray<CoercionRule, 4>(p)
	{
	}

	bool coerce(thread_db* tdbb, dsc* d, unsigned startItem = 0) const;
	void setRule(const TypeClause* from, const TypeClause *to);
};

} // namespace Jrd

#endif // JRD_COERCION_H
