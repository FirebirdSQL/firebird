/*
*	PROGRAM:	Firebird interface.
*	MODULE:		TimerImpl.h
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

#ifndef FB_CLASSES_TIMER_IMPL
#define FB_CLASSES_TIMER_IMPL

#include <functional>

#include "../../common/classes/ImplementHelper.h"
#include "../../common/classes/locks.h"

namespace Firebird {

class TimerImpl;

// Signature of free function to call by timer
typedef void (OnTimerFunc)(TimerImpl*);

// Signature  of member function of class T to call by timer
template <typename T>
using OnTimerMember = void (T::*)(TimerImpl*);

class TimerImpl :
	public RefCntIface<ITimerImpl<TimerImpl, CheckStatusWrapper> >
{
public:
	TimerImpl() :
		m_fireTime(0),
		m_expTime(0),
		m_inHandler(false)
	{ }

	// ITimer implementation
	void handler();

	// Set timer handler function
	void setOnTimer(std::function<OnTimerFunc> onTimer)
	{
		m_onTimer = onTimer;
	}

	// Set member function as timer handler
	template <typename T>
	void setOnTimer(T* obj, OnTimerMember<T> onTimer)
	{
		m_onTimer = std::bind(onTimer, obj, std::placeholders::_1);
	}

	// Set timeout, seconds
	void reset(unsigned int timeout);
	void stop();

	SINT64 getExpireClock() const
	{
		return m_expTime;
	}

private:
	Mutex m_mutex;
	SINT64 m_fireTime;		// when ITimer will fire, could be less than m_expTime
	SINT64 m_expTime;		// when actual idle timeout will expire
	std::function<OnTimerFunc> m_onTimer;
	bool m_inHandler;
};


// Call member function and keep reference on class instance
template <typename T>
class TimerWithRef : public TimerImpl
{
public:
	TimerWithRef(T* obj) :
		TimerImpl(),
		m_ref(obj)
	{}

	void setOnTimer(OnTimerMember<T> onTimer)
	{
		TimerImpl::setOnTimer(m_ref.getPtr(), onTimer);
	}

private:
	RefPtr<T> m_ref;
};

} // namespace Firebird

#endif