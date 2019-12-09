/*
 *	PROGRAM:	Operate lists of plugins
 *	MODULE:		ParsedList.h
 *	DESCRIPTION:	Parse, merge, etc. lists of plugins in firebird.conf format
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
 *  Copyright (c) 2010, 2019 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef COMMON_CLASSES_PARSED_LIST_H
#define COMMON_CLASSES_PARSED_LIST_H

#include "../common/classes/objects_array.h"
#include "../common/classes/fb_string.h"
#include "../common/config/config.h"

namespace Firebird {

// tools to operate lists of security-related plugins
class ParsedList : public Firebird::ObjectsArray<Firebird::PathName>
{
public:
	explicit ParsedList(const Firebird::PathName& list);

	ParsedList()
	{ }

	explicit ParsedList(MemoryPool& p)
		: Firebird::ObjectsArray<Firebird::PathName>(p)
	{ }

	ParsedList(const Firebird::PathName& list, const char* delimiters);

	// create plane list from this parsed
	void makeList(Firebird::PathName& list) const;

	// merge lists keeping only commom for both plugins
	static void mergeLists(Firebird::PathName& list, const Firebird::PathName& serverList,
		const Firebird::PathName& clientList);

	// get providers list for particular database amd remove "Loopback" provider from it
	static Firebird::PathName getNonLoopbackProviders(const Firebird::PathName& aliasDb);

private:
	void parse(Firebird::PathName list, const char* delimiters);
};

} // namespace Firebird

#endif // COMMON_CLASSES_PARSED_LIST_H
