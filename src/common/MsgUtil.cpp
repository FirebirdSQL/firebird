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
 *  Copyright (c) 2021 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "firebird/impl/msg_helper.h"
#include "MsgUtil.h"
#include "msg_encode.h"
#include "../common/classes/alloc.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/init.h"
#include "utils_proto.h"

using namespace Firebird;

namespace
{
	class NameCodeMap
	{
	public:
		NameCodeMap(MemoryPool& pool)
			: m_map(pool)
		{
			#define FB_IMPL_MSG_NO_SYMBOL(facility, number, text)

			#define FB_IMPL_MSG_SYMBOL(facility, number, symbol, text) \
				m_map.put(#symbol, ENCODE_ISC_MSG(number, FB_IMPL_MSG_FACILITY_##facility));

			#define FB_IMPL_MSG(facility, number, symbol, sqlCode, sqlClass, sqlSubClass, text) \
				FB_IMPL_MSG_SYMBOL(facility, number, symbol, text)

			#include "firebird/impl/msg/all.h"

			#undef FB_IMPL_MSG_NO_SYMBOL
			#undef FB_IMPL_MSG_SYMBOL
			#undef FB_IMPL_MSG
		}

		bool find(const char* name, ISC_STATUS& code) const
		{
			return m_map.get(name, code);
		}

	private:
		struct NoCaseCmp
		{
			static bool greaterThan(const char* i1, const char* i2)
			{
				return fb_utils::stricmp(i1, i2) > 0;
			}
		};

		NonPooledMap<const char*, ISC_STATUS, NoCaseCmp> m_map;
	};

	InitInstance<NameCodeMap> nameCodeMap;
}	// namespace


ISC_STATUS MsgUtil::getCodeByName(const char* name)
{
	ISC_STATUS code;

	if (!nameCodeMap().find(name, code))
		code = 0;

	return code;
}
