/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			SimpleStatusVector.h
 *	DESCRIPTION:	Firebird's exception classes
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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Vlad Khorsun <hvlad at users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_SIMPLE_STATUS_VECTOR
#define FB_SIMPLE_STATUS_VECTOR

#include "fb_exception.h"
#include "../common/utils_proto.h"
#include "../common/classes/array.h"

namespace Firebird {

// This trivial container is used when we need to grow vector element by element w/o any conversions
template <unsigned S>	// Default value is in fb_exception.h
class SimpleStatusVector : public HalfStaticArray<ISC_STATUS, S>
{
public:
	SimpleStatusVector()
		: HalfStaticArray<ISC_STATUS, S>()
	{
		fb_assert(S >= 3u);		// Required for no-throw support in makeEmergencyStatus()
	}

	SimpleStatusVector(MemoryPool& p)
		: HalfStaticArray<ISC_STATUS, S>(p)
	{
		fb_assert(S >= 3u);		// Required for no-throw support in makeEmergencyStatus()
	}

	void mergeStatus(const IStatus* from)
	{
		int state = from->getState();
		this->clear();

		if (state & IStatus::STATE_ERRORS)
		{
			const ISC_STATUS* s = from->getErrors();
			unsigned copied = fb_utils::statusLength(s);
			this->append(s, copied);
		}

		if (!this->hasData())
		{
			this->push(isc_arg_gds);
			this->push(0);
		}

		if (state & IStatus::STATE_WARNINGS)
		{
			const ISC_STATUS* s = from->getWarnings();
			unsigned copied = fb_utils::statusLength(s);
			this->append(s, copied);
		}

		this->push(isc_arg_end);
	}

	ISC_STATUS* makeEmergencyStatus() noexcept
	{
		// Should not throw - see assertions in constructors
		return this->getBuffer(3);
	}
};

typedef SimpleStatusVector<> StaticStatusVector;

} // namespace Firebird


#endif // FB_SIMPLE_STATUS_VECTOR
