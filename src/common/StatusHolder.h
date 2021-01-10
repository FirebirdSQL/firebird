/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			StatusHolder.h
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

#ifndef FB_STATUS_HOLDER
#define FB_STATUS_HOLDER

#include "firebird/Interface.h"
#include "../common/utils_proto.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/SimpleStatusVector.h"
#include "../common/DynamicStrings.h"

namespace Firebird {

// DynamicVector owns strings, contained in it
template <unsigned S>
class DynamicVector : private SimpleStatusVector<S>
{
public:
	explicit DynamicVector(MemoryPool& p)
		: SimpleStatusVector<S>(p)
	{
		fb_utils::init_status(this->getBuffer(3));
	}

	~DynamicVector()
	{
		delete[] findDynamicStrings(this->getCount(), this->begin());
	}

private:
	char* prepareForNewValue()
	{
		// Do not delete old strings buffer because strings from it
		// may be used in status vector passed to current save() call.
		// I.e. strings should be first saved in new dynamic buffer and only afterwards
		// old buffer may be released.
		char* oldBuffer = findDynamicStrings(this->getCount(), this->begin());
		this->resize(0);
		return oldBuffer;
	}

public:
	void clear()
	{
		delete[] this->prepareForNewValue();
		fb_utils::init_status(this->getBuffer(3));
	}

	void save(unsigned int length, const ISC_STATUS* status, bool warningMode) throw()
	{
		try
		{
			char* oldBuffer = this->prepareForNewValue();
			ISC_STATUS* b = this->getBuffer(length + 1);
			unsigned newLen = makeDynamicStrings(length, b, status);
			delete[] oldBuffer;

			fb_assert(newLen <= length);

			// Sanity check
			if (newLen < 2)
				fb_utils::init_status(this->getBuffer(3));
			else
				this->resize(newLen + 1);
		}
		catch (const BadAlloc&)
		{
			if (!warningMode)
			{
				// do not use stuff here to avoid endless cycle
				this->shrink(3);
				ISC_STATUS* s = this->getBuffer(3);	// Should not throw - see assert() in ctor
				fb_utils::statusBadAlloc(s);
			}
			else
			{
				this->clear();
			}
		}
		catch (const Exception&)
		{
			fb_assert(false);

			this->shrink(3);
			ISC_STATUS* s = this->getBuffer(3);	// Should not throw - see assert() in ctor
			fb_utils::statusUnknown(s);
		}
	}

	ISC_STATUS save(const ISC_STATUS* status, bool warningMode = false) throw()
	{
		save(fb_utils::statusLength(status), status, warningMode);
		return status[1];
	}

	const ISC_STATUS* value() const
	{
		return SimpleStatusVector<S>::begin();
	}

	unsigned length() const
	{
		return SimpleStatusVector<S>::getCount();
	}

	ISC_STATUS operator[](unsigned int index) const
	{
		return SimpleStatusVector<S>::operator[](index);
	}
};


// DynamicStatusVector performs error/success checks and load from IStatus
class DynamicStatusVector : public DynamicVector<ISC_STATUS_LENGTH>
{
public:
	DynamicStatusVector()
		: DynamicVector(*getDefaultMemoryPool())
	{ }

	ISC_STATUS load(const IStatus* status);

	ISC_STATUS getError() const
	{
		return value()[1];
	}

	bool isSuccess() const
	{
		return getError() == 0;
	}
};


// Implements IStatus using 2 DynamicVectors
template <class Final>
class BaseStatus : public IStatusImpl<Final, CheckStatusWrapper>
{
public:
	// IStatus implementation
	void init()
	{
		errors.clear();
		warnings.clear();
	}

	void setErrors(const ISC_STATUS* value)
	{
		errors.save(fb_utils::statusLength(value), value, false);
	}

	void setErrors2(unsigned int length, const ISC_STATUS* value)
	{
		errors.save(length, value, false);
	}

	void setWarnings(const ISC_STATUS* value)
	{
		warnings.save(fb_utils::statusLength(value), value, true);
	}

	void setWarnings2(unsigned int length, const ISC_STATUS* value)
	{
		warnings.save(length, value, true);
	}

	const ISC_STATUS* getErrors() const
	{
		return errors.value();
	}

	const ISC_STATUS* getWarnings() const
	{
		return warnings.value();
	}

	unsigned getState() const
	{
		return (errors.value()[1] ? IStatus::STATE_ERRORS : 0) |
			   (warnings.value()[1] ? IStatus::STATE_WARNINGS  : 0);
	}

	IStatus* clone() const
	{
		IStatus* ret = MasterInterfacePtr()->getStatus();

		ret->setWarnings(getWarnings());
		ret->setErrors(getErrors());

		return ret;
	}

public:
	explicit BaseStatus(MemoryPool& p)
		: errors(p), warnings(p)
	{
		init();
	}

	BaseStatus()
		: errors(*getDefaultMemoryPool()), warnings(*getDefaultMemoryPool())
	{
		init();
	}

	void check()
	{
		const ISC_STATUS* s = errors.value();
		if (s[0] == isc_arg_gds && s[1] != isc_arg_end)
			status_exception::raise(s);
	}

protected:
	void clear()
	{
		errors.clear();
		warnings.clear();
	}

private:
	DynamicVector<11> errors;		// gds-code (2) + 3 args (6) + stack trace (2) + end (1)
	DynamicVector<3> warnings;		// no warnings (empty vector)
};


// Hold status vector and raise error on request
class StatusHolder : public BaseStatus<StatusHolder>
{
public:
	StatusHolder()
		: BaseStatus(*getDefaultMemoryPool()), m_raised(false)
	{ }

	ISC_STATUS save(IStatus* status);
	void clear();
	void raise();

	ISC_STATUS getError()
	{
		return value()->getErrors()[1];
	}

	const IStatus* value()
	{
		if (m_raised)
			clear();

		return this;
	}

	bool isSuccess()
	{
		return getError() == 0;
	}

	const StatusHolder& operator=(const StatusHolder& val)
	{
		setErrors(val.getErrors());
		setWarnings(val.getWarnings());
		m_raised = val.m_raised;
		return *this;
	}

public:
	void dispose() override
	{
	}

private:
	bool m_raised;
};


// Status interface to be used on stack when entering engine
class LocalStatus : public AutoStorage, public BaseStatus<LocalStatus>
{
public:
	LocalStatus()
		: AutoStorage(), BaseStatus<LocalStatus>(getPool())
	{ }

	explicit LocalStatus(MemoryPool& p)
		: AutoStorage(p), BaseStatus<LocalStatus>(getPool())
	{ }

public:
	void dispose() override
	{
	}

public:
	static void checkException(LocalStatus* status)
	{
	}

	static void catchException(LocalStatus* status)
	{
		BaseStatusWrapper<LocalStatus>::catchException(status);
	}

	static void setVersionError(IStatus* status, const char* interfaceName,
		unsigned currentVersion, unsigned expectedVersion)
	{
		BaseStatusWrapper<LocalStatus>::setVersionError(
			status, interfaceName,currentVersion, expectedVersion);
	}
};

} // namespace Firebird


#endif // FB_STATUS_HOLDER
