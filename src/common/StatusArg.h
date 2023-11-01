/*
 *	PROGRAM:		Firebird exceptions classes
 *	MODULE:			StatusArg.h
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

#ifndef FB_STATUS_ARG
#define FB_STATUS_ARG

#include "fb_exception.h"
#include "firebird/Interface.h"
#include "../common/SimpleStatusVector.h"
#include "../common/classes/fb_string.h"

namespace Firebird {

class AbstractString;
class MetaString;
class Exception;

namespace Arg {

// forward
class Warning;
class StatusVector;

class Base
{
#ifdef __HP_aCC
// aCC gives error, cannot access protected member class ImplBase
public:
#else
protected:
#endif
	class ImplBase
	{
	private:
		ISC_STATUS kind, code;

	public:
		ISC_STATUS getKind() const noexcept { return kind; }
		ISC_STATUS getCode() const noexcept { return code; }

		virtual const ISC_STATUS* value() const noexcept { return NULL; }
		virtual unsigned int length() const noexcept { return 0; }
		virtual unsigned int firstWarning() const noexcept { return 0; }
		virtual bool hasData() const noexcept { return false; }
		virtual void clear() noexcept { }
		virtual void append(const StatusVector&) noexcept { }
		virtual void prepend(const StatusVector&) noexcept { }
		virtual void assign(const StatusVector& ex) noexcept { }
		virtual void assign(const Exception& ex) noexcept { }
		virtual ISC_STATUS copyTo(ISC_STATUS*) const noexcept { return 0; }
		virtual void copyTo(IStatus*) const noexcept { }
		virtual void appendTo(IStatus*) const noexcept { }

		virtual void shiftLeft(const Base&) noexcept { }
		virtual void shiftLeft(const Warning&) noexcept { }
		virtual void shiftLeft(const char*) noexcept { }
		virtual void shiftLeft(const AbstractString&) noexcept { }
		virtual void shiftLeft(const MetaString&) noexcept { }

		virtual bool compare(const StatusVector& /*v*/) const noexcept { return false; }

		ImplBase(ISC_STATUS k, ISC_STATUS c) noexcept : kind(k), code(c) { }
		virtual ~ImplBase() { }
	};

	Base(ISC_STATUS k, ISC_STATUS c);
	explicit Base(ImplBase* i) noexcept : implementation(i) { }
	~Base() noexcept { delete implementation; }

	ImplBase* const implementation;

public:
	ISC_STATUS getKind() const noexcept { return implementation->getKind(); }
	ISC_STATUS getCode() const noexcept { return implementation->getCode(); }
};

class StatusVector : public Base
{
protected:
	class ImplStatusVector : public ImplBase
	{
	private:
		StaticStatusVector m_status_vector;
		unsigned int m_warning;

		string m_strings;

		void putStrArg(unsigned startWith);
		void setStrPointers(const char* oldBase);

		bool appendErrors(const ImplBase* const v) noexcept;
		bool appendWarnings(const ImplBase* const v) noexcept;
		bool append(const ISC_STATUS* const from, const unsigned int count) noexcept;
		void append(const ISC_STATUS* const from) noexcept;

		ImplStatusVector& operator=(const ImplStatusVector& src);

	public:
		virtual const ISC_STATUS* value() const noexcept { return m_status_vector.begin(); }
		virtual unsigned int length() const noexcept { return m_status_vector.getCount() - 1u; }
		virtual unsigned int firstWarning() const noexcept { return m_warning; }
		virtual bool hasData() const noexcept { return length() > 0u; }
		virtual void clear() noexcept;
		virtual void append(const StatusVector& v) noexcept;
		virtual void prepend(const StatusVector& v) noexcept;
		virtual void assign(const StatusVector& v) noexcept;
		virtual void assign(const Exception& ex) noexcept;
		virtual ISC_STATUS copyTo(ISC_STATUS* dest) const noexcept;
		virtual void copyTo(IStatus* dest) const noexcept;
		virtual void appendTo(IStatus* dest) const noexcept;
		virtual void shiftLeft(const Base& arg) noexcept;
		virtual void shiftLeft(const Warning& arg) noexcept;
		virtual void shiftLeft(const char* text) noexcept;
		virtual void shiftLeft(const AbstractString& text) noexcept;
		virtual void shiftLeft(const MetaString& text) noexcept;
		virtual bool compare(const StatusVector& v) const noexcept;

		ImplStatusVector(ISC_STATUS k, ISC_STATUS c) noexcept
			: ImplBase(k, c),
			  m_status_vector(*getDefaultMemoryPool()),
			  m_strings(*getDefaultMemoryPool())
		{
			clear();
		}

		explicit ImplStatusVector(const ISC_STATUS* s) noexcept;
		explicit ImplStatusVector(const IStatus* s) noexcept;
		explicit ImplStatusVector(const Exception& ex) noexcept;
	};

	StatusVector(ISC_STATUS k, ISC_STATUS v);

public:
	explicit StatusVector(const ISC_STATUS* s);
	explicit StatusVector(const IStatus* s);
	explicit StatusVector(const Exception& ex);
	StatusVector();
	~StatusVector() { }

	// copying is prohibited
	StatusVector(const StatusVector&) = delete;
	void operator=(const StatusVector&) = delete;

	const ISC_STATUS* value() const noexcept { return implementation->value(); }
	unsigned int length() const noexcept { return implementation->length(); }
	bool hasData() const noexcept { return implementation->hasData(); }
	bool isEmpty() const noexcept { return !implementation->hasData(); }

	void clear() noexcept { implementation->clear(); }
	void append(const StatusVector& v) noexcept { implementation->append(v); }
	void prepend(const StatusVector& v) noexcept { implementation->prepend(v); }
	void assign(const StatusVector& v) noexcept { implementation->assign(v); }
	void assign(const Exception& ex) noexcept { implementation->assign(ex); }
	[[noreturn]] void raise() const;
	ISC_STATUS copyTo(ISC_STATUS* dest) const noexcept { return implementation->copyTo(dest); }
	void copyTo(IStatus* dest) const noexcept { implementation->copyTo(dest); }
	void appendTo(IStatus* dest) const noexcept { implementation->appendTo(dest); }

	// generic argument insert
	StatusVector& operator<<(const Base& arg) noexcept
	{
		implementation->shiftLeft(arg);
		return *this;
	}

	// StatusVector case - append multiple args
	StatusVector& operator<<(const StatusVector& arg) noexcept
	{
		implementation->append(arg);
		return *this;
	}

	// warning special case - to setup first warning location
	StatusVector& operator<<(const Warning& arg) noexcept
	{
		implementation->shiftLeft(arg);
		return *this;
	}

	// Str special case - make the code simpler & better readable
	StatusVector& operator<<(const char* text) noexcept
	{
		implementation->shiftLeft(text);
		return *this;
	}

	StatusVector& operator<<(const AbstractString& text) noexcept
	{
		implementation->shiftLeft(text);
		return *this;
	}

	StatusVector& operator<<(const MetaString& text) noexcept
	{
		implementation->shiftLeft(text);
		return *this;
	}

	bool operator==(const StatusVector& arg) const noexcept
	{
		return implementation->compare(arg);
	}

	bool operator!=(const StatusVector& arg) const noexcept
	{
		return !(*this == arg);
	}
};


class Gds : public StatusVector
{
public:
	explicit Gds(ISC_STATUS s) noexcept;
};

// To simplify calls to DYN messages from DSQL, only for private DYN messages
// that do not have presence in system_errors2.sql, when you have to call ENCODE_ISC_MSG.
class PrivateDyn : public Gds
{
public:
	explicit PrivateDyn(ISC_STATUS codeWithoutFacility) noexcept;
};

class Str : public Base
{
public:
	explicit Str(const char* text) noexcept;
	explicit Str(const AbstractString& text) noexcept;
	explicit Str(const MetaString& text) noexcept;
};

class Num : public Base
{
public:
	explicit Num(ISC_STATUS s) noexcept;
};

// On 32-bit architecture ISC_STATUS can't fit 64-bit integer therefore
// convert such a numbers into text and put string into status-vector.
// Make sure that temporary instance of this class is not going out of scope
// before exception is raised !
class Int64 : public Str
{
public:
	explicit Int64(SINT64 val) noexcept;
	explicit Int64(FB_UINT64 val) noexcept;
private:
	char text[24];
};

class Quad : public Str
{
public:
	explicit Quad(const ISC_QUAD* quad) noexcept;
private:
	//		high  :  low  \0
	char text[8 + 1 + 8 + 1];
};

class Interpreted : public StatusVector
{
public:
	explicit Interpreted(const char* text) noexcept;
	explicit Interpreted(const AbstractString& text) noexcept;
};

class Unix : public Base
{
public:
	explicit Unix(ISC_STATUS s) noexcept;
};

class Mach : public Base
{
public:
	explicit Mach(ISC_STATUS s) noexcept;
};

class Windows : public Base
{
public:
	explicit Windows(ISC_STATUS s) noexcept;
};

class Warning : public StatusVector
{
public:
	explicit Warning(ISC_STATUS s) noexcept;
};

class SqlState : public Base
{
public:
	explicit SqlState(const char* text) noexcept;
	explicit SqlState(const AbstractString& text) noexcept;
};

class OsError : public Base
{
public:
	OsError() noexcept;
	explicit OsError(ISC_STATUS s) noexcept;
};

} // namespace Arg

} // namespace Firebird


#endif // FB_STATUS_ARG
