#include "firebird.h"
#include "firebird/Interface.h"
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "iberror.h"
#include "../common/classes/alloc.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/ThreadStart.h"
#include "../common/utils_proto.h"
#include "../common/SimpleStatusVector.h"
#include "../common/StatusHolder.h"

namespace Firebird {

// ********************************* Exception *******************************

Exception::~Exception() throw() { }

void Exception::stuffException(DynamicStatusVector& status_vector) const throw()
{
	StaticStatusVector status;
	stuffException(status);
	try
	{
		status_vector.save(status.begin());
	}
	catch (const BadAlloc&)
	{
		ISC_STATUS tmp[3];
		processUnexpectedException(tmp);
		status_vector.save(tmp);
	}
}

void Exception::stuffException(CheckStatusWrapper* status_vector) const throw()
{
	StaticStatusVector status;
	stuffException(status);
	fb_utils::setIStatus(status_vector, status.begin());
}

void Exception::processUnexpectedException(ISC_STATUS* vector) throw()
{
	// do not use stuffException() here to avoid endless loop
	try
	{
		throw;
	}
	catch (const BadAlloc&)
	{
		fb_utils::statusBadAlloc(vector);
	}
	catch (const Exception&)
	{
		fb_assert(false);

		fb_utils::statusUnknown(vector);
	}
}

// ********************************* status_exception *******************************

status_exception::status_exception() throw()
	: m_status_vector(m_buffer)
{
	fb_utils::init_status(m_status_vector);
}

status_exception::status_exception(const ISC_STATUS *status_vector) throw()
	: m_status_vector(m_buffer)
{
	fb_utils::init_status(m_status_vector);

	if (status_vector)
	{
		set_status(status_vector);
	}
}

status_exception::status_exception(const status_exception& from) throw()
	: m_status_vector(m_buffer)
{
	fb_utils::init_status(m_status_vector);

	set_status(from.m_status_vector);
}

void status_exception::set_status(const ISC_STATUS *new_vector) throw()
{
	fb_assert(new_vector != 0);
	unsigned len = fb_utils::statusLength(new_vector);

	try
	{
		if (len >= FB_NELEM(m_buffer))
		{
			m_status_vector = FB_NEW_POOL(*getDefaultMemoryPool()) ISC_STATUS[len + 1];
		}
		len = makeDynamicStrings(len, m_status_vector, new_vector);
		m_status_vector[len] = isc_arg_end;
	}
	catch (const Exception&)
	{
		if (m_status_vector != m_buffer)
		{
			delete[] m_status_vector;
			m_status_vector = m_buffer;
		}
		processUnexpectedException(m_buffer);
	}
}

status_exception::~status_exception() throw()
{
	delete[] findDynamicStrings(fb_utils::statusLength(m_status_vector), m_status_vector);
	if (m_status_vector != m_buffer)
	{
		delete[] m_status_vector;
	}
}

const char* status_exception::what() const throw()
{
	return "Firebird::status_exception";
}

void status_exception::raise(const ISC_STATUS *status_vector)
{
	throw status_exception(status_vector);
}

void status_exception::raise(const IStatus* status)
{
	StaticStatusVector status_vector;
	status_vector.mergeStatus(status);
	throw status_exception(status_vector.begin());
}

void status_exception::raise(const Arg::StatusVector& statusVector)
{
	throw status_exception(statusVector.value());
}

void status_exception::stuffByException(StaticStatusVector& status) const throw()
{
	try
	{
		status.assign(m_status_vector, fb_utils::statusLength(m_status_vector) + 1);
	}
	catch (const BadAlloc&)
	{
		processUnexpectedException(status.makeEmergencyStatus());
	}
}

// ********************************* BadAlloc ****************************

void BadAlloc::raise()
{
	throw BadAlloc();
}

void BadAlloc::stuffByException(StaticStatusVector& status) const throw()
{
	fb_utils::statusBadAlloc(status.makeEmergencyStatus());
}

const char* BadAlloc::what() const throw()
{
	return "Firebird::BadAlloc";
}

// ********************************* LongJump ***************************

void LongJump::raise()
{
	throw LongJump();
}

void LongJump::stuffByException(StaticStatusVector& status) const throw()
{
	ISC_STATUS sv[] = {isc_arg_gds, isc_random, isc_arg_string,
		(ISC_STATUS)(IPTR) "Unexpected call to Firebird::LongJump::stuffException()", isc_arg_end};

	try
	{
		status.assign(sv, FB_NELEM(sv));
	}
	catch (const BadAlloc&)
	{
		processUnexpectedException(status.makeEmergencyStatus());
	}
}

const char* LongJump::what() const throw()
{
	return "Firebird::LongJump";
}


// ********************************* system_error ***************************

system_error::system_error(const char* syscall, int error_code) :
	status_exception(), errorCode(error_code)
{
	Arg::Gds temp(isc_sys_request);
	temp << Arg::Str(syscall);
	temp << SYS_ERR(errorCode);
	set_status(temp.value());
}

void system_error::raise(const char* syscall, int error_code)
{
	throw system_error(syscall, error_code);
}

void system_error::raise(const char* syscall)
{
	throw system_error(syscall, getSystemError());
}

int system_error::getSystemError()
{
#ifdef WIN_NT
	return GetLastError();
#else
	return errno;
#endif
}

// ********************************* system_call_failed ***************************

system_call_failed::system_call_failed(const char* syscall, int error_code) :
	system_error(syscall, error_code)
{
	// NS: something unexpected has happened. Log the error to log file
	// In the future we may consider terminating the process even in PROD_BUILD
	gds__log("Operating system call %s failed. Error code %d", syscall, error_code);
#ifdef DEV_BUILD
	// raised failed system call exception in DEV_BUILD in 99.99% means
	// problems with the code - let's create memory dump now
	abort();
#endif
}

void system_call_failed::raise(const char* syscall, int error_code)
{
	throw system_call_failed(syscall, error_code);
}

void system_call_failed::raise(const char* syscall)
{
	throw system_call_failed(syscall, getSystemError());
}

// ********************************* fatal_exception *******************************

fatal_exception::fatal_exception(const char* message) :
	status_exception()
{
	const ISC_STATUS temp[] =
	{
		isc_arg_gds,
		isc_random,
		isc_arg_string,
		(ISC_STATUS)(IPTR) message,
		isc_arg_end
	};
	set_status(temp);
}

// Keep in sync with the constructor above, please; "message" becomes 4th element
// after initialization of status vector in constructor.
const char* fatal_exception::what() const throw()
{
	return reinterpret_cast<const char*>(value()[3]);
}

void fatal_exception::raise(const char* message)
{
	throw fatal_exception(message);
}

void fatal_exception::raiseFmt(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[1024];
	VSNPRINTF(buffer, sizeof(buffer), format, args);
	buffer[sizeof(buffer) - 1] = 0;
	va_end(args);
	throw fatal_exception(buffer);
}

}	// namespace Firebird
