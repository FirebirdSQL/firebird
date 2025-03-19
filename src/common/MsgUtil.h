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
 *  Contributor(s): Alexey Mochalov.
 */

#ifndef FB_COMMON_MSG_UTIL_H
#define FB_COMMON_MSG_UTIL_H

#include "firebird.h"
#include "../common/StatusHolder.h"


namespace Firebird {
	namespace MsgUtil {
		struct SubfieldData
		{
			// const char* field;
			// const char* relation;
			// const char* owner;
			const char* alias;
			int subType, scale;
			unsigned type, length, charSet;
			bool nullable, nullFlag;
			// short* nullInd;
			const char* compositeDescriptor = nullptr;
			unsigned subfieldsNum = 0;

			union TypeMix
			{
				ISC_TIMESTAMP* asDateTime;
				ISC_TIMESTAMP_TZ* asDateTimeTz;
				ISC_TIMESTAMP_TZ_EX* asDateTimeTzEx;
				ISC_TIME* asTime;
				ISC_TIME_TZ* asTimeTz;
				ISC_TIME_TZ_EX* asTimeTzEx;
				ISC_DATE* asDate;
				SSHORT* asSmallint;
				SLONG* asInteger;
				SINT64* asBigint;
				float* asFloat;
				double* asDouble;
				FB_BOOLEAN* asBoolean;
				ISC_QUAD* blobid;
				vary* asVary;
				char* asChar;
				FB_DEC16* asDec16;
				FB_DEC34* asDec34;
				FB_I128* asInt128;
				void* setPtr;
			};
			TypeMix value;
		};

		ISC_STATUS getCodeByName(const char* name);
	}
} // namespace Firebird


#endif // FB_COMMON_MSG_UTIL_H
