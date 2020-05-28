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
		ISC_STATUS getKind() const throw() { return kind; }
		ISC_STATUS getCode() const throw() { return code; }

		virtual const ISC_STATUS* value() const throw() { return NULL; }
		virtual unsigned int length() const throw() { return 0; }
		virtual unsigned int firstWarning() const throw() { return 0; }
		virtual bool hasData() const throw() { return false; }
		virtual void clear() throw() { }
		virtual void append(const StatusVector&) throw() { }
		virtual void prepend(const StatusVector&) throw() { }
		virtual void assign(const StatusVector& ex) throw() { }
		virtual void assign(const Exception& ex) throw() { }
		virtual ISC_STATUS copyTo(ISC_STATUS*) const throw() { return 0; }
		virtual void copyTo(IStatus*) const throw() { }
		virtual void appendTo(IStatus*) const throw() { }

		virtual void shiftLeft(const Base&) throw() { }
		virtual void shiftLeft(const Warning&) throw() { }
		virtual void shiftLeft(const char*) throw() { }
		virtual void shiftLeft(const AbstractString&) throw() { }
		virtual void shiftLeft(const MetaString&) throw() { }

		virtual bool compare(const StatusVector& /*v*/) const throw() { return false; }

		ImplBase(ISC_STATUS k, ISC_STATUS c) throw() : kind(k), code(c) { }
		virtual ~ImplBase() { }
	};

	Base(ISC_STATUS k, ISC_STATUS c);
	explicit Base(ImplBase* i) throw() : implementation(i) { }
	~Base() throw() { delete implementation; }

	ImplBase* const implementation;

public:
	ISC_STATUS getKind() const throw() { return implementation->getKind(); }
	ISC_STATUS getCode() const throw() { return implementation->getCode(); }
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

		bool appendErrors(const ImplBase* const v) throw();
		bool appendWarnings(const ImplBase* const v) throw();
		bool append(const ISC_STATUS* const from, const unsigned int count) throw();
		void append(const ISC_STATUS* const from) throw();

		ImplStatusVector& operator=(const ImplStatusVector& src);

	public:
		virtual const ISC_STATUS* value() const throw() { return m_status_vector.begin(); }
		virtual unsigned int length() const throw() { return m_status_vector.getCount() - 1u; }
		virtual unsigned int firstWarning() const throw() { return m_warning; }
		virtual bool hasData() const throw() { return length() > 0u; }
		virtual void clear() throw();
		virtual void append(const StatusVector& v) throw();
		virtual void prepend(const StatusVector& v) throw();
		virtual void assign(const StatusVector& v) throw();
		virtual void assign(const Exception& ex) throw();
		virtual ISC_STATUS copyTo(ISC_STATUS* dest) const throw();
		virtual void copyTo(IStatus* dest) const throw();
		virtual void appendTo(IStatus* dest) const throw();
		virtual void shiftLeft(const Base& arg) throw();
		virtual void shiftLeft(const Warning& arg) throw();
		virtual void shiftLeft(const char* text) throw();
		virtual void shiftLeft(const AbstractString& text) throw();
		virtual void shiftLeft(const MetaString& text) throw();
		virtual bool compare(const StatusVector& v) const throw();

		ImplStatusVector(ISC_STATUS k, ISC_STATUS c) throw()
			: ImplBase(k, c),
			  m_status_vector(*getDefaultMemoryPool()),
			  m_strings(*getDefaultMemoryPool())
		{
			clear();
		}

		explicit ImplStatusVector(const ISC_STATUS* s) throw();
		explicit ImplStatusVector(const IStatus* s) throw();
		explicit ImplStatusVector(const Exception& ex) throw();
	};

	StatusVector(ISC_STATUS k, ISC_STATUS v);

public:
	explicit StatusVector(const ISC_STATUS* s);
	explicit StatusVector(const IStatus* s);
	explicit StatusVector(const Exception& ex);
	StatusVector();
	~StatusVector() { }

	const ISC_STATUS* value() const throw() { return implementation->value(); }
	unsigned int length() const throw() { return implementation->length(); }
	bool hasData() const throw() { return implementation->hasData(); }
	bool isEmpty() const throw() { return !implementation->hasData(); }

	void clear() throw() { implementation->clear(); }
	void append(const StatusVector& v) throw() { implementation->append(v); }
	void prepend(const StatusVector& v) throw() { implementation->prepend(v); }
	void assign(const StatusVector& v) throw() { implementation->assign(v); }
	void assign(const Exception& ex) throw() { implementation->assign(ex); }
	void raise() const;
	ISC_STATUS copyTo(ISC_STATUS* dest) const throw() { return implementation->copyTo(dest); }
	void copyTo(IStatus* dest) const throw() { implementation->copyTo(dest); }
	void appendTo(IStatus* dest) const throw() { implementation->appendTo(dest); }

	// generic argument insert
	StatusVector& operator<<(const Base& arg) throw()
	{
		implementation->shiftLeft(arg);
		return *this;
	}

	// StatusVector case - append multiple args
	StatusVector& operator<<(const StatusVector& arg) throw()
	{
		implementation->append(arg);
		return *this;
	}

	// warning special case - to setup first warning location
	StatusVector& operator<<(const Warning& arg) throw()
	{
		implementation->shiftLeft(arg);
		return *this;
	}

	// Str special case - make the code simpler & better readable
	StatusVector& operator<<(const char* text) throw()
	{
		implementation->shiftLeft(text);
		return *this;
	}

	StatusVector& operator<<(const AbstractString& text) throw()
	{
		implementation->shiftLeft(text);
		return *this;
	}

	StatusVector& operator<<(const MetaString& text) throw()
	{
		implementation->shiftLeft(text);
		return *this;
	}

	bool operator==(const StatusVector& arg) const throw()
	{
		return implementation->compare(arg);
	}

	bool operator!=(const StatusVector& arg) const throw()
	{
		return !(*this == arg);
	}
};


class Gds : public StatusVector
{
public:
	explicit Gds(ISC_STATUS s) throw();
};

// To simplify calls to DYN messages from DSQL, only for private DYN messages
// that do not have presence in system_errors2.sql, when you have to call ENCODE_ISC_MSG.
class PrivateDyn : public Gds
{
public:
	explicit PrivateDyn(ISC_STATUS codeWithoutFacility) throw();
};

class Str : public Base
{
public:
	explicit Str(const char* text) throw();
	explicit Str(const AbstractString& text) throw();
	explicit Str(const MetaString& text) throw();
};

class Num : public Base
{
public:
	explicit Num(ISC_STATUS s) throw();
};

// On 32-bit architecture ISC_STATUS can't fit 64-bit integer therefore
// convert such a numbers into text and put string into status-vector.
// Make sure that temporary instance of this class is not going out of scope
// before exception is raised !
class Int64 : public Str
{
public:
	explicit Int64(SINT64 val) throw();
	explicit Int64(FB_UINT64 val) throw();
private:
	char text[24];
};

class Quad : public Str
{
public:
	explicit Quad(const ISC_QUAD* quad) throw();
private:
	//		high  :  low  \0
	char text[8 + 1 + 8 + 1];
};

class Interpreted : public StatusVector
{
public:
	explicit Interpreted(const char* text) throw();
	explicit Interpreted(const AbstractString& text) throw();
};

class Unix : public Base
{
public:
	explicit Unix(ISC_STATUS s) throw();
};

class Mach : public Base
{
public:
	explicit Mach(ISC_STATUS s) throw();
};

class Windows : public Base
{
public:
	explicit Windows(ISC_STATUS s) throw();
};

class Warning : public StatusVector
{
public:
	explicit Warning(ISC_STATUS s) throw();
};

class SqlState : public Base
{
public:
	explicit SqlState(const char* text) throw();
	explicit SqlState(const AbstractString& text) throw();
};

class OsError : public Base
{
public:
	OsError() throw();
	explicit OsError(ISC_STATUS s) throw();
};

} // namespace Arg

} // namespace Firebird


#endif // FB_STATUS_ARG
