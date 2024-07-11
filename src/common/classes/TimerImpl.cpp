/*
*	PROGRAM:	Firebird interface.
*	MODULE:		TimerImpl.cpp
*	DESCRIPTION:	ITimer implementaton
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
*  The Original Code was created by Khorsun Vladyslav
*  for the Firebird Open Source RDBMS project.
*
*  Copyright (c) 2020 Khorsun Vladyslav <hvlad@users.sourceforge.net>
*  and all contributors signed below.
*
*  All Rights Reserved.
*  Contributor(s): ______________________________________.
*
*/

#include "../../common/classes/TimerImpl.h"
#include "../../common/StatusHolder.h"
#include "../../common/utils_proto.h"

namespace Firebird {

void TimerImpl::handler()
{
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);
		fb_assert(!m_handlerTid);

		m_fireTime = 0;
		if (!m_expTime)	// Timer was reset to zero or stopped, do nothing
			return;

		// If timer was reset to fire later, restart ITimer

		const SINT64 curTime = fb_utils::query_performance_counter() / fb_utils::query_performance_frequency();

		if (m_expTime > curTime)
		{
			reset(m_expTime - curTime);
			return;
		}
		m_expTime = 0;

		if (m_onTimer)
			m_handlerTid = Thread::getId();
	}

	if (!m_onTimer)
		return;

	m_onTimer(this);

	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	m_handlerTid = 0;
}

void TimerImpl::reset(unsigned int timeout)
{
	LocalStatus ls;
	CheckStatusWrapper s(&ls);
	ITimerControl* timerCtrl = TimerInterfacePtr();

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	// Start timer if necessary. If timer was already started, don't restart
	// (or stop) it - handler() will take care about it.

	if (!timeout)
	{
		m_expTime = 0;
		return;
	}

	const SINT64 curTime = fb_utils::query_performance_counter() / fb_utils::query_performance_frequency();
	m_expTime = curTime + timeout;

	if (m_fireTime)
	{
		if (m_fireTime <= m_expTime)
			return;

		timerCtrl->stop(&s, this);
		check(&s);
	}

	m_fireTime = m_expTime;

	// new ITimer timeout, ms
	SINT64 tout = (m_expTime - curTime) * (1000 * 1000);

	timerCtrl->start(&s, this, tout);
	check(&s);
}

void TimerImpl::stop()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	// Allow handler() to call stop()
	if (m_handlerTid == Thread::getId())
		return;

	// hvlad: it could be replaced by condition variable when we have good one for Windows
	while (m_handlerTid)
	{
		MutexUnlockGuard unlock(m_mutex, FB_FUNCTION);
		Thread::sleep(10);
	}

	if (!m_fireTime)
		return;

	m_fireTime = m_expTime = 0;

	LocalStatus ls;
	CheckStatusWrapper s(&ls);

	ITimerControl* timerCtrl = TimerInterfacePtr();
	timerCtrl->stop(&s, this);
	check(&s);
}

} // namespace Firebird
