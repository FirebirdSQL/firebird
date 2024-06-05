#include "firebird.h"

//#include "fb_exception.h"

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "gen/iberror.h"
#include "../common/classes/alloc.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/thd.h"

#ifdef WIN_NT
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#endif

#include "../common/classes/fb_tls.h"

using namespace Firebird;

//
// circularAlloc()
//

#ifdef WIN_NT
#include <windows.h>
#endif

namespace {

class ThreadCleanup
{
public:
	static void add(FPTR_VOID_PTR cleanup, void* arg);
	static void remove(FPTR_VOID_PTR cleanup, void* arg);
	static void destructor(void*);

	static void assertNoCleanupChain()
	{
		fb_assert(!chain);
	}

private:
	FPTR_VOID_PTR function;
	void* argument;
	ThreadCleanup* next;

	static ThreadCleanup* chain;
	static GlobalPtr<Mutex> cleanupMutex;

	ThreadCleanup(FPTR_VOID_PTR cleanup, void* arg, ThreadCleanup* chain)
		: function(cleanup), argument(arg), next(chain) { }

	static void initThreadCleanup();
	static void finiThreadCleanup();

	static ThreadCleanup** findCleanup(FPTR_VOID_PTR cleanup, void* arg);
};

ThreadCleanup* ThreadCleanup::chain = NULL;
GlobalPtr<Mutex> ThreadCleanup::cleanupMutex;

#ifdef USE_POSIX_THREADS

pthread_key_t key;
pthread_once_t keyOnce = PTHREAD_ONCE_INIT;
bool keySet = false;

void makeKey()
{
	int err = pthread_key_create(&key, ThreadCleanup::destructor);
	if (err)
	{
		Firebird::system_call_failed::raise("pthread_key_create", err);
	}
	keySet = true;
}

void ThreadCleanup::initThreadCleanup()
{
	int err = pthread_once(&keyOnce, makeKey);
	if (err)
	{
		Firebird::system_call_failed::raise("pthread_once", err);
	}

	err = pthread_setspecific(key, &key);
	if (err)
	{
		Firebird::system_call_failed::raise("pthread_setspecific", err);
	}
}

void ThreadCleanup::finiThreadCleanup()
{
	pthread_setspecific(key, NULL);
}


class FiniThreadCleanup
{
public:
	FiniThreadCleanup(Firebird::MemoryPool&)
	{ }

	~FiniThreadCleanup()
	{
		ThreadCleanup::assertNoCleanupChain();
		if (keySet)
		{
			int err = pthread_key_delete(key);
			if (err)
				gds__log("pthread_key_delete failed with error %d", err);
		}
	}
};

Firebird::GlobalPtr<FiniThreadCleanup> thrCleanup;		// needed to call dtor

#endif // USE_POSIX_THREADS

#ifdef WIN_NT
void ThreadCleanup::initThreadCleanup()
{
}

void ThreadCleanup::finiThreadCleanup()
{
}
#endif // #ifdef WIN_NT

ThreadCleanup** ThreadCleanup::findCleanup(FPTR_VOID_PTR cleanup, void* arg)
{
	for (ThreadCleanup** ptr = &chain; *ptr; ptr = &((*ptr)->next))
	{
		if ((*ptr)->function == cleanup && (*ptr)->argument == arg)
		{
			return ptr;
		}
	}

	return NULL;
}

void ThreadCleanup::destructor(void*)
{
	MutexLockGuard guard(cleanupMutex);

	for (ThreadCleanup* ptr = chain; ptr; ptr = ptr->next)
	{
		ptr->function(ptr->argument);
	}

	finiThreadCleanup();
}

void ThreadCleanup::add(FPTR_VOID_PTR cleanup, void* arg)
{
	Firebird::MutexLockGuard guard(cleanupMutex);

	initThreadCleanup();

	if (findCleanup(cleanup, arg))
	{
		return;
	}

	chain = FB_NEW(*getDefaultMemoryPool()) ThreadCleanup(cleanup, arg, chain);
}

void ThreadCleanup::remove(FPTR_VOID_PTR cleanup, void* arg)
{
	MutexLockGuard guard(cleanupMutex);

	ThreadCleanup** ptr = findCleanup(cleanup, arg);
	if (!ptr)
	{
		return;
	}

	ThreadCleanup* toDelete = *ptr;
	*ptr = toDelete->next;
	delete toDelete;
}

class ThreadBuffer
{
private:
	const static size_t BUFFER_SIZE = 8192;		// make it match with call stack limit == 2048
	char buffer[BUFFER_SIZE];
	char* buffer_ptr;

public:
	ThreadBuffer() : buffer_ptr(buffer) { }

	const char* alloc(const char* string, size_t length)
	{
		// if string is already in our buffer - return it
		// it was already saved in our buffer once
		if (string >= buffer && string < &buffer[BUFFER_SIZE])
			return string;

		// if string too long, truncate it
		if (length > BUFFER_SIZE / 4)
			length = BUFFER_SIZE / 4;

		// If there isn't any more room in the buffer, start at the beginning again
		if (buffer_ptr + length + 1 > buffer + BUFFER_SIZE)
			buffer_ptr = buffer;

		char* new_string = buffer_ptr;
		memcpy(new_string, string, length);
		new_string[length] = 0;
		buffer_ptr += length + 1;

		return new_string;
	}
};

TLS_DECLARE(ThreadBuffer*, threadBuffer);

void cleanupAllStrings(void*)
{
	///fprintf(stderr, "Cleanup is called\n");

	delete TLS_GET(threadBuffer);
	TLS_SET(threadBuffer, NULL);

	///fprintf(stderr, "Buffer removed\n");
}

ThreadBuffer* getThreadBuffer()
{
	ThreadBuffer* rc = TLS_GET(threadBuffer);
	if (!rc)
	{
		ThreadCleanup::add(cleanupAllStrings, NULL);
		rc = FB_NEW(*getDefaultMemoryPool()) ThreadBuffer;
		TLS_SET(threadBuffer, rc);
	}

	return rc;
}

// Needed to call dtor
class Strings
{
public:
	Strings(MemoryPool&)
	{ }

	~Strings()
	{
		ThreadCleanup::remove(cleanupAllStrings, NULL);
	}
};
Firebird::GlobalPtr<Strings> cleanStrings;

const char* circularAlloc(const char* s, unsigned len)
{
	return getThreadBuffer()->alloc(s, len);
}

} // namespace

// This is called from ibinitdll.cpp:DllMain() and ThreadStart.cpp:threadStart()
void threadCleanup()
{
#ifdef WIN_NT
	ThreadCleanup::destructor(NULL);
#endif
}

namespace Firebird {

// Before using thr parameter, make sure that thread is not going to work with
// this functions itself.
// CVC: Do not let "perm" be incremented before "trans", because it may lead to serious memory errors,
// since several places in our code blindly pass the same vector twice.
void makePermanentVector(ISC_STATUS* perm, const ISC_STATUS* trans, FB_THREAD_ID thr) throw()
{
	try
	{
		while (true)
		{
			const ISC_STATUS type = *perm++ = *trans++;

			switch (type)
			{
			case isc_arg_end:
				return;
			case isc_arg_cstring:
				{
					size_t len = *perm++ = *trans++;
					const char* temp = reinterpret_cast<char*>(*trans++);
					*perm++ = (ISC_STATUS)(IPTR) circularAlloc(temp, len);
					perm[-2] = len;
				}
				break;
			case isc_arg_string:
			case isc_arg_interpreted:
			case isc_arg_sql_state:
				{
					const char* temp = reinterpret_cast<char*>(*trans++);
					size_t len = strlen(temp);
					*perm++ = (ISC_STATUS)(IPTR) circularAlloc(temp, len);
				}
				break;
			default:
				*perm++ = *trans++;
				break;
			}
		}
	}
	catch (const system_call_failed& ex)
	{
		memcpy(perm, ex.value(), sizeof(ISC_STATUS_ARRAY));
	}
	catch (const BadAlloc& ex)
	{
		ex.stuff_exception(perm);
	}
	catch (...)
	{
		*perm++ = isc_arg_gds;
		*perm++ = isc_random;
		*perm++ = isc_arg_string;
		*perm++ = (ISC_STATUS)(IPTR) "Unexpected exception in makePermanentVector()";
		*perm++ = isc_arg_end;
	}
}

void makePermanentVector(ISC_STATUS* v, FB_THREAD_ID thr) throw()
{
	makePermanentVector(v, v, thr);
}

// ********************************* Exception *******************************

Exception::~Exception() throw() { }

// ********************************* status_exception *******************************

status_exception::status_exception() throw()
{
	memset(m_status_vector, 0, sizeof(m_status_vector));
}

status_exception::status_exception(const ISC_STATUS *status_vector) throw()
{
	memset(m_status_vector, 0, sizeof(m_status_vector));

	if (status_vector)
	{
		set_status(status_vector);
	}
}

void status_exception::set_status(const ISC_STATUS *new_vector) throw()
{
	fb_assert(new_vector != 0);

	makePermanentVector(m_status_vector, new_vector);
}

status_exception::~status_exception() throw()
{
}

const char* status_exception::what() const throw()
{
	return "Firebird::status_exception";
}

void status_exception::raise(const ISC_STATUS *status_vector)
{
	throw status_exception(status_vector);
}

void status_exception::raise(const Arg::StatusVector& statusVector)
{
	throw status_exception(statusVector.value());
}

ISC_STATUS status_exception::stuff_exception(ISC_STATUS* const status_vector) const throw()
{
	const ISC_STATUS *ptr = value();
	ISC_STATUS *sv = status_vector;

	// Copy status vector
	while (true)
	{
		const ISC_STATUS type = *sv++ = *ptr++;
		if (type == isc_arg_end)
			break;
		if (type == isc_arg_cstring)
			*sv++ = *ptr++;
		*sv++ = *ptr++;
	}

	return status_vector[1];
}

// ********************************* BadAlloc ****************************

void BadAlloc::raise()
{
	throw BadAlloc();
}

ISC_STATUS BadAlloc::stuff_exception(ISC_STATUS* const status_vector) const throw()
{
	ISC_STATUS *sv = status_vector;

	*sv++ = isc_arg_gds;
	*sv++ = isc_virmemexh;
	*sv++ = isc_arg_end;

	return status_vector[1];
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

ISC_STATUS LongJump::stuff_exception(ISC_STATUS* const status_vector) const throw()
{
   /*
	* Do nothing for a while - not all utilities are ready,
	* status_vector is passed in them by other means.
	* Ideally status_exception should be always used for it,
	* and we should activate the following code:

	ISC_STATUS *sv = status_vector;

	*sv++ = isc_arg_gds;
	*sv++ = isc_random;
	*sv++ = isc_arg_string;
	*sv++ = (ISC_STATUS)(IPTR) "Unexpected Firebird::LongJump";
	*sv++ = isc_arg_end;
	*/

	return status_vector[1];
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
#ifndef SUPERCLIENT
	// NS: something unexpected has happened. Log the error to log file
	// In the future we may consider terminating the process even in PROD_BUILD
	gds__log("Operating system call %s failed. Error code %d", syscall, error_code);
#endif
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

// ************************** exception handling routines **************************

// Serialize exception into status_vector
ISC_STATUS stuff_exception(ISC_STATUS *status_vector, const Firebird::Exception& ex) throw()
{
	return ex.stuff_exception(status_vector);
}

}	// namespace Firebird
