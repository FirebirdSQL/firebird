/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			StatusArg.cpp
 *	DESCRIPTION:	Build status vector with variable number of elements
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
 *  Copyright (c) 2008 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "../common/StatusArg.h"
#include "../common/utils_proto.h"

#include "../common/classes/MetaString.h"
#include "../common/classes/alloc.h"
#include "fb_exception.h"
#include "iberror.h"
#include "firebird/Interface.h"
#include "../common/msg_encode.h"

#ifdef WIN_NT
#include <windows.h>
#else
#include <errno.h>
#endif

namespace {
	// Didn't want to bring dyn.h and friends here.
	const int DYN_MSG_FAC		= 8;
}

namespace Firebird {

namespace Arg {

Base::Base(ISC_STATUS k, ISC_STATUS c) :
	implementation(FB_NEW_POOL(*getDefaultMemoryPool()) ImplBase(k, c))
{
}

StatusVector::ImplStatusVector::ImplStatusVector(const ISC_STATUS* s) throw()
	: Base::ImplBase(0, 0),
	  m_status_vector(*getDefaultMemoryPool()),
	  m_strings(*getDefaultMemoryPool())
{
	fb_assert(s);

	clear();

	// special case - empty initialized status vector, no warnings
	if (s[0] != isc_arg_gds || s[1] != 0 || s[2] != 0)
		append(s);
}

StatusVector::ImplStatusVector::ImplStatusVector(const IStatus* s) throw()
	: Base::ImplBase(0, 0),
	  m_status_vector(*getDefaultMemoryPool()),
	  m_strings(*getDefaultMemoryPool())
{
	fb_assert(s);

	clear();

	if (s->getState() & IStatus::STATE_ERRORS)
		append(s->getErrors());
	if (s->getState() & IStatus::STATE_WARNINGS)
		append(s->getWarnings());
}

StatusVector::ImplStatusVector::ImplStatusVector(const Exception& ex) throw()
	: Base::ImplBase(0, 0),
	  m_status_vector(*getDefaultMemoryPool()),
	  m_strings(*getDefaultMemoryPool())
{
	clear();

	assign(ex);
}

StatusVector::StatusVector(ISC_STATUS k, ISC_STATUS c) :
	Base(FB_NEW_POOL(*getDefaultMemoryPool()) ImplStatusVector(k, c))
{
	operator<<(*(static_cast<Base*>(this)));
}

StatusVector::StatusVector(const ISC_STATUS* s) :
	Base(FB_NEW_POOL(*getDefaultMemoryPool()) ImplStatusVector(s))
{
}

StatusVector::StatusVector(const IStatus* s) :
	Base(FB_NEW_POOL(*getDefaultMemoryPool()) ImplStatusVector(s))
{
}

StatusVector::StatusVector(const Exception& ex) :
	Base(FB_NEW_POOL(*getDefaultMemoryPool()) ImplStatusVector(ex))
{
}

StatusVector::StatusVector() :
	Base(FB_NEW_POOL(*getDefaultMemoryPool()) ImplStatusVector(0, 0))
{
}

void StatusVector::ImplStatusVector::clear() throw()
{
	m_warning = 0;
	m_status_vector.clear();
	m_status_vector.push(isc_arg_end);
	m_strings.erase();
}

bool StatusVector::ImplStatusVector::compare(const StatusVector& v) const throw()
{
	return length() == v.length() && fb_utils::cmpStatus(length(), value(), v.value());
}

void StatusVector::ImplStatusVector::assign(const StatusVector& v) throw()
{
	clear();
	append(v);
}

void StatusVector::ImplStatusVector::assign(const Exception& ex) throw()
{
	clear();
	ex.stuffException(m_status_vector);
	putStrArg(0);
}

void StatusVector::ImplStatusVector::putStrArg(unsigned startWith)
{
	for (ISC_STATUS* arg = m_status_vector.begin() + startWith; *arg != isc_arg_end; arg += fb_utils::nextArg(*arg))
	{
		if (!fb_utils::isStr(*arg))
			continue;

		const char** ptr = reinterpret_cast<const char**>(&arg[fb_utils::nextArg(*arg) - 1]);
		unsigned pos = m_strings.length();
		const char* oldBase = m_strings.c_str();

		if (*arg == isc_arg_cstring)
		{
			m_strings.reserve(m_strings.length() + arg[1] + 1);
			m_strings.append(*ptr, arg[1]);
			m_strings.append(1, '\0');
		}
		else
			 m_strings.append(*ptr, strlen(*ptr) + 1);

		*ptr = &m_strings[pos];
		setStrPointers(oldBase);
	}
}

void StatusVector::ImplStatusVector::setStrPointers(const char* oldBase)
{
	const char* const newBase = m_strings.c_str();
	if (newBase == oldBase)
		return;

	const char* const newEnd = m_strings.end();

	for (ISC_STATUS* arg = m_status_vector.begin(); *arg != isc_arg_end; arg += fb_utils::nextArg(*arg))
	{
		if (!fb_utils::isStr(*arg))
			continue;

		const char** ptr = reinterpret_cast<const char**>(&arg[fb_utils::nextArg(*arg) - 1]);
		if (*ptr >= newBase && *ptr < newEnd)
			break;

		*ptr = &newBase[*ptr - oldBase];
	}
}

void StatusVector::ImplStatusVector::append(const StatusVector& v) throw()
{
	ImplStatusVector newVector(getKind(), getCode());

	if (newVector.appendErrors(this))
	{
		if (newVector.appendErrors(v.implementation))
		{
			if (newVector.appendWarnings(this))
				newVector.appendWarnings(v.implementation);
		}
	}

	*this = newVector;
}

void StatusVector::ImplStatusVector::prepend(const StatusVector& v) throw()
{
	ImplStatusVector newVector(getKind(), getCode());

	if (newVector.appendErrors(v.implementation))
	{
		if (newVector.appendErrors(this))
		{
			if (newVector.appendWarnings(v.implementation))
				newVector.appendWarnings(this);
		}
	}

	*this = newVector;
}

StatusVector::ImplStatusVector& StatusVector::ImplStatusVector::operator=(const StatusVector::ImplStatusVector& src)
{
	m_status_vector = src.m_status_vector;
	m_warning = src.m_warning;
	m_strings = src.m_strings;
	setStrPointers(src.m_strings.c_str());

	return *this;
}

bool StatusVector::ImplStatusVector::appendErrors(const ImplBase* const v) throw()
{
	return append(v->value(), v->firstWarning() ? v->firstWarning() : v->length());
}

bool StatusVector::ImplStatusVector::appendWarnings(const ImplBase* const v) throw()
{
	if (! v->firstWarning())
		return true;
	return append(v->value() + v->firstWarning(), v->length() - v->firstWarning());
}

bool StatusVector::ImplStatusVector::append(const ISC_STATUS* const from, const unsigned int count) throw()
{
	// CVC: I didn't expect count to be zero but it's, in some calls
	fb_assert(count >= 0);
	if (!count)
		return true; // not sure it's the best option here

	unsigned lenBefore = length();
	ISC_STATUS* s = m_status_vector.getBuffer(lenBefore + count + 1);
	unsigned int copied =
		fb_utils::copyStatus(&s[lenBefore], count + 1, from, count);
	if (copied < count)
		m_status_vector.shrink(lenBefore + copied + 1);
	putStrArg(lenBefore);

	if (!m_warning)
	{
		for (unsigned n = 0; n < length(); )
		{
			if (m_status_vector[n] == isc_arg_warning)
			{
				m_warning = n;
				break;
			}
			n += (m_status_vector[n] == isc_arg_cstring) ? 3 : 2;
		}
	}

	return copied == count;
}

void StatusVector::ImplStatusVector::append(const ISC_STATUS* const from) throw()
{
	unsigned l = fb_utils::statusLength(from);
	append(from, l + 1);
}

void StatusVector::ImplStatusVector::shiftLeft(const Base& arg) throw()
{
	m_status_vector[length()] = arg.getKind();
	m_status_vector.push(arg.getCode());
	m_status_vector.push(isc_arg_end);

	putStrArg(length() - 2);
}

void StatusVector::ImplStatusVector::shiftLeft(const Warning& arg) throw()
{
	const int cur = m_warning ? 0 : length();
	shiftLeft(*static_cast<const Base*>(&arg));
	if (cur && m_status_vector[cur] == isc_arg_warning)
		m_warning = cur;
}

void StatusVector::ImplStatusVector::shiftLeft(const char* text) throw()
{
	shiftLeft(Str(text));
}

void StatusVector::ImplStatusVector::shiftLeft(const AbstractString& text) throw()
{
	shiftLeft(Str(text));
}

void StatusVector::ImplStatusVector::shiftLeft(const MetaString& text) throw()
{
	shiftLeft(Str(text));
}

void StatusVector::raise() const
{
	if (hasData())
	{
		status_exception::raise(*this);
	}
	status_exception::raise(Gds(isc_random) << Str("Attempt to raise empty exception"));
}

ISC_STATUS StatusVector::ImplStatusVector::copyTo(ISC_STATUS* dest) const throw()
{
	if (hasData())
	{
		fb_utils::copyStatus(dest, ISC_STATUS_LENGTH, value(), length() + 1u);
	}
	else
	{
		dest[0] = isc_arg_gds;
		dest[1] = FB_SUCCESS;
		dest[2] = isc_arg_end;
	}
	return dest[1];
}

void StatusVector::ImplStatusVector::copyTo(IStatus* dest) const throw()
{
	dest->init();
	if (hasData())
	{
		const ISC_STATUS* v = m_status_vector.begin();
		unsigned int len = length();
		unsigned int warning = m_warning;

		if (v[warning] == isc_arg_warning)
		{
			 dest->setWarnings2(len - warning, &v[warning]);
			 if (warning)
				dest->setErrors2(warning, v);
		}
		else
			dest->setErrors2(len, v);
	}
}

void StatusVector::ImplStatusVector::appendTo(IStatus* dest) const throw()
{
	if (hasData())
	{
		ImplStatusVector tmpVector(dest);
		ImplStatusVector newVector(getKind(), getCode());

		if (newVector.appendErrors(&tmpVector))
		{
			if (newVector.appendErrors(this))
			{
				if (newVector.appendWarnings(&tmpVector))
					newVector.appendWarnings(this);
			}
		}

		// take special care about strings safety
		// that's why tmpStatus is needed here
		AutoPtr<IStatus, SimpleDispose> tmpStatus(dest->clone());
		newVector.copyTo(tmpStatus);

		dest->setErrors(tmpStatus->getErrors());
		dest->setWarnings(tmpStatus->getWarnings());
	}
}

Gds::Gds(ISC_STATUS s) throw() :
	StatusVector(isc_arg_gds, s) { }

PrivateDyn::PrivateDyn(ISC_STATUS codeWithoutFacility) throw() :
	Gds(ENCODE_ISC_MSG(codeWithoutFacility, DYN_MSG_FAC)) { }

Num::Num(ISC_STATUS s) throw() :
	Base(isc_arg_number, s) { }

Int64::Int64(SINT64 val) throw() :
	Str(text)
{
	sprintf(text, "%" SQUADFORMAT, val);
}

Int64::Int64(FB_UINT64 val) throw() :
	Str(text)
{
	sprintf(text, "%" UQUADFORMAT, val);
}

Quad::Quad(const ISC_QUAD* quad) throw() :
	Str(text)
{
	sprintf(text, "%x:%x", quad->gds_quad_high, quad->gds_quad_low);
}

Interpreted::Interpreted(const char* text) throw() :
	StatusVector(isc_arg_interpreted, (ISC_STATUS)(IPTR) text) { }

Interpreted::Interpreted(const AbstractString& text) throw() :
	StatusVector(isc_arg_interpreted, (ISC_STATUS)(IPTR) text.c_str()) { }

Unix::Unix(ISC_STATUS s) throw() :
	Base(isc_arg_unix, s) { }

Mach::Mach(ISC_STATUS s) throw() :
	Base(isc_arg_next_mach, s) { }

Windows::Windows(ISC_STATUS s) throw() :
	Base(isc_arg_win32, s) { }

Warning::Warning(ISC_STATUS s) throw() :
	StatusVector(isc_arg_warning, s) { }

// Str overloading.
Str::Str(const char* text) throw() :
	Base(isc_arg_string, (ISC_STATUS)(IPTR) text) { }

Str::Str(const AbstractString& text) throw() :
	Base(isc_arg_string, (ISC_STATUS)(IPTR) text.c_str()) { }

Str::Str(const MetaString& text) throw() :
	Base(isc_arg_string, (ISC_STATUS)(IPTR) text.c_str()) { }

SqlState::SqlState(const char* text) throw() :
	Base(isc_arg_sql_state, (ISC_STATUS)(IPTR) text) { }

SqlState::SqlState(const AbstractString& text) throw() :
	Base(isc_arg_sql_state, (ISC_STATUS)(IPTR) text.c_str()) { }

OsError::OsError() throw() :
#ifdef WIN_NT
	Base(isc_arg_win32, GetLastError()) { }
#else
	Base(isc_arg_unix, errno) { }
#endif

OsError::OsError(ISC_STATUS s) throw() :
#ifdef WIN_NT
	Base(isc_arg_win32, s) { }
#else
	Base(isc_arg_unix, s) { }
#endif
} // namespace Arg

} // namespace Firebird
