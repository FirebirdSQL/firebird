/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			fb_exception.h
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
 *  The Original Code was created by Mike Nordell
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2001 Mike Nordell <tamlin at algonet.se>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */


#ifndef FB_EXCEPTION_H
#define FB_EXCEPTION_H

#include <stddef.h>
#include <string.h>

#include <new>

#include "fb_types.h"
#include "firebird/Interface.h"

namespace Firebird
{
class MemoryPool;
class DynamicStatusVector;
template <unsigned S = ISC_STATUS_LENGTH> class SimpleStatusVector;
typedef SimpleStatusVector<> StaticStatusVector;

class Exception
{
protected:
	Exception() throw() { }
	static void processUnexpectedException(ISC_STATUS* vector) throw();

public:
	void stuffException(StaticStatusVector& status_vector) const throw()
	{
		stuffByException(status_vector);
	}

	void stuffException(DynamicStatusVector& status_vector) const throw();
	void stuffException(CheckStatusWrapper* status_vector) const throw();
	virtual ~Exception() throw();

private:
	virtual void stuffByException(StaticStatusVector& status_vector) const throw() = 0;

public:
	virtual const char* what() const throw() = 0;
};

// Used as jmpbuf to unwind when needed
class LongJump : public Exception
{
public:
	virtual void stuffByException(StaticStatusVector& status_vector) const throw();
	virtual const char* what() const throw();
	static void raise();
	LongJump() throw() : Exception() { }
};

// Used in MemoryPool
class BadAlloc : public std::bad_alloc, public Exception
{
public:
	BadAlloc() throw() : std::bad_alloc(), Exception() { }
	virtual void stuffByException(StaticStatusVector& status_vector) const throw();
	virtual const char* what() const throw();
	static void raise();
};

// Main exception class in firebird
class status_exception : public Exception
{
public:
	explicit status_exception(const ISC_STATUS *status_vector) throw();
	status_exception(const status_exception&) throw();

	virtual ~status_exception() throw();

	virtual void stuffByException(StaticStatusVector& status_vector) const throw();
	virtual const char* what() const throw();

	const ISC_STATUS* value() const throw() { return m_status_vector; }

	[[noreturn]] static void raise(const ISC_STATUS* status_vector);
	[[noreturn]] static void raise(const Arg::StatusVector& statusVector);
	[[noreturn]] static void raise(const IStatus* status);

protected:
	// Create exception with undefined status vector, this constructor allows
	// derived classes create empty exception ...
	status_exception() throw();
	// .. and adjust it later using somehow created status vector.
	void set_status(const ISC_STATUS *new_vector) throw();

private:
	ISC_STATUS* m_status_vector;
	ISC_STATUS_ARRAY m_buffer;

	status_exception& operator=(const status_exception&);
};

// Parameter syscall later in both system_error & system_call_failed
// must be literal string! This helps avoid need in StringsBuffer
// when processing this dangerous errors!

// use this class if exception can be handled
class system_error : public status_exception
{
private:
	int errorCode;

protected:
	system_error(const char* syscall, const char* arg, int error_code);

public:
	static void raise(const char* syscall, int error_code);
	static void raise(const char* syscall);

	int getErrorCode() const
	{
		return errorCode;
	}

	static int getSystemError();
};

// use this class if exception can't be handled
// it will call abort() in DEV_BUILD to create core dump
class system_call_failed : public system_error
{
protected:
	system_call_failed(const char* syscall, const char* arg, int error_code);

public:
	static void raise(const char* syscall, int error_code);
	static void raise(const char* syscall);
	static void raise(const char* syscall, const char* arg, int error_code);
	static void raise(const char* syscall, const char* arg);
};

class fatal_exception : public status_exception
{
public:
	explicit fatal_exception(const char* message);
	static void raiseFmt(const char* format, ...);
	const char* what() const throw();
	static void raise(const char* message);
};


}	// namespace Firebird


#endif	// FB_EXCEPTION_H
