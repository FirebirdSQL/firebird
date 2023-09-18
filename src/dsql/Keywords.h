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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_KEYWORDS_H
#define DSQL_KEYWORDS_H

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/MetaName.h"


namespace Jrd
{
	class Keywords final
	{
	public:
		class Allocator final
		{
		public:
			static Keywords* create();

			static void destroy(Keywords* instance)
			{
				delete instance;
			}
		};

		struct Keyword
		{
			int keyword;
			MetaName* str;
			bool nonReserved;
		};

	public:
		Keywords(MemoryPool& pool);

		~Keywords()
		{
			for (auto& pair : map)
				delete pair.second.str;
		}

		auto get(const MetaName& str) const
		{
			return map.get(str);
		}

		auto begin() const
		{
			return map.begin();
		}

		auto end() const
		{
			return map.end();
		}

	private:
		Firebird::LeftPooledMap<MetaName, Keyword> map;
	};
}	// namespace Jrd

#endif	// DSQL_KEYWORDS_H
