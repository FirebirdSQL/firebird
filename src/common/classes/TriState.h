/*
 *	PROGRAM:		Firebird aux classes.
 *	MODULE:			TriState.h
 *	DESCRIPTION:	Firebird's SQL tri-state emulation class.
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
 *  The Original Code was created by Claudio Valderrama on 28-Aug-2007
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Claudio Valderrama
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef CLASSES_TRISTATE_H
#define CLASSES_TRISTATE_H

namespace Firebird {

class TriState
{
public:
	TriState() noexcept;
	explicit TriState(bool input) noexcept;

	bool operator ==(const TriState& o) const noexcept
	{
		return m_init == o.m_init && m_val == o.m_val;
	}

	bool operator !=(const TriState& o) const noexcept
	{
		return m_init != o.m_init || m_val != o.m_val;
	}

	void operator=(bool input) noexcept;

	bool asBool() const noexcept;
	bool valueOr(bool value) const noexcept;
	void reset() noexcept;
	bool assignOnce(bool input) noexcept;
	bool isUnknown() const noexcept;
	bool isAssigned() const noexcept;
	bool toggle() noexcept;

private:
	bool m_init, m_val;
};

// The var is left uninitialized.
inline TriState::TriState() noexcept
	: m_init(false), m_val(false)
{
}

// The var is initialized to the explicit value.
inline TriState::TriState(bool input) noexcept
	: m_init(true), m_val(input)
{
}

// The var receives a T/F value.
inline void TriState::operator=(bool input) noexcept
{
	m_init = true;
	m_val = input;
}

// The var is coerced to a T/F value as result.
inline bool TriState::asBool() const noexcept
{
	return m_init && m_val;
}

inline bool TriState::valueOr(bool otherValue) const noexcept
{
	return m_init ? m_val : otherValue;
}

// The var is returned to its uninitialized state.
inline void TriState::reset() noexcept
{
	m_init = m_val = false;
}

// The assignment succeeds only if the var is uninitialized.
inline bool TriState::assignOnce(bool input) noexcept
{
	if (m_init)
		return false;

	m_init = true;
	m_val = input;
	return true;
}

// Tests whether the var is uninitialized.
inline bool TriState::isUnknown() const noexcept
{
	return !m_init;
}

// Tests whether the var is initialized.
inline bool TriState::isAssigned() const noexcept
{
	return m_init;
}

// The var is toggled between T and F only if it's already initialized.
inline bool TriState::toggle() noexcept
{
	if (!m_init)
		return false;

	m_val = !m_val;
	return true;
}

} // namespace Firebird

#endif // CLASSES_TRISTATE_H
