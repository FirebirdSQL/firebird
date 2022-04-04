/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			StatusHolder.cpp
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

#include "firebird.h"
#include "StatusHolder.h"
#include "iberror.h"
#include "classes/alloc.h"

namespace Firebird {

ISC_STATUS DynamicStatusVector::load(const IStatus* status)
{
	StaticStatusVector tmp;
	tmp.mergeStatus(status);
	return save(tmp.begin(), false);
}

ISC_STATUS StatusHolder::save(IStatus* status)
{
	fb_assert(isSuccess() || m_raised);
	if (m_raised)
	{
		clear();
	}

	setErrors(status->getErrors());
	setWarnings(status->getWarnings());
	return getErrors()[1];
}

void StatusHolder::clear()
{
	BaseStatus<StatusHolder>::clear();
	m_raised = false;
}

void StatusHolder::raise()
{
	if (getError())
	{
		Arg::StatusVector tmp(getErrors());
		tmp << Arg::StatusVector(getWarnings());
		m_raised = true;
		tmp.raise();
	}
}

} // namespace Firebird
