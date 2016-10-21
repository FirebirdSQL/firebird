/*
 *	PROGRAM:	string class definition
 *	MODULE:		fb_string.h
 *	DESCRIPTION:	Provides almost that same functionality,
 *			that STL::basic_string<char> does,
 *			but behaves MemoryPools friendly.
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef INCLUDE_FB_STRING_H
#define INCLUDE_FB_STRING_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <utility>

#include "firebird.h"
#include "fb_types.h"
#include "fb_exception.h"
#include "../common/classes/alloc.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/Hash.h"

namespace Firebird
{
	class AbstractString : public AutoStorage
	{
	public:
		typedef char char_type;
		typedef FB_SIZE_T size_type;
		typedef FB_SSIZE_T difference_type;
		typedef char* pointer;
		typedef const char* const_pointer;
		typedef char& reference;
		typedef const char& const_reference;
		typedef char value_type;
		typedef pointer iterator;
		typedef const_pointer const_iterator;
		static const size_type npos;
		enum {INLINE_BUFFER_SIZE = 32u, INIT_RESERVE = 16u/*, KEEP_SIZE = 512u*/};

	private:
		const size_type max_length;

	protected:
		char_type inlineBuffer[INLINE_BUFFER_SIZE];
		char_type* stringBuffer;
		size_type stringLength, bufferSize;

	private:
		void checkPos(size_type pos) const
		{
			if (pos >= length()) {
				fatal_exception::raise("Firebird::string - pos out of range");
			}
		}

		void checkLength(size_type len)
		{
			if (len > max_length) {
				fatal_exception::raise("Firebird::string - length exceeds predefined limit");
			}
		}

		// Make sure our buffer is large enough to store at least <length> characters in it
		// (not including null terminator). Resulting buffer is not initialized.
		// Use it in explicit constructors only.
		void initialize(const size_type len)
		{
			if (len < INLINE_BUFFER_SIZE)
			{
				stringBuffer = inlineBuffer;
				bufferSize = INLINE_BUFFER_SIZE;
			}
			else
			{
				stringBuffer = inlineBuffer; // Be safe in case of exception
				checkLength(len);

				// Reserve a few extra bytes in the buffer
				size_type newSize = len + 1u + INIT_RESERVE;

				// Do not grow buffer beyond string length limit
				if (newSize > max_length)
					newSize = max_length;

				// Allocate new buffer
				stringBuffer = FB_NEW_POOL(getPool()) char_type[newSize];
				bufferSize = newSize;
			}
			stringLength = len;
			stringBuffer[stringLength] = 0;
		}

		void shrinkBuffer() throw()
		{
			// Shrink buffer if we decide it is beneficial
		}

	protected:
		// All constructors are protected to disable instantiation of this class

		// Default constructor
		AbstractString(const size_type limit, MemoryPool& p = getAutoMemoryPool()) : AutoStorage(p),
			max_length(limit), stringBuffer(inlineBuffer), stringLength(0), bufferSize(INLINE_BUFFER_SIZE)
		{
			stringBuffer[0] = 0;
		}

		// Initialization constructor
		explicit AbstractString(const size_type limit, const size_type sizeL, const void* datap, MemoryPool& p = getAutoMemoryPool());
		// Initialization constructor with protection from NULL pointer
		explicit AbstractString(const size_type limit, const_pointer data, MemoryPool& p = getAutoMemoryPool());
		// Initialization as a string of chars
		explicit AbstractString(const size_type limit, const size_type n, const char_type c, MemoryPool& p = getAutoMemoryPool())
			: AutoStorage(p), max_length(limit), stringBuffer(inlineBuffer), stringLength(0), bufferSize(INLINE_BUFFER_SIZE)
		{
			assign(n, c);
		}

		// Copy constructor
		// Cannot default allocator to source's one
		explicit AbstractString(const size_type limit, const AbstractString& v, MemoryPool& p = getAutoMemoryPool());
		// Delete default copy constructor and assign operator to prevent compiler from generating implicit bitwise
		// constructor and assignment in derived classes. They could cause problems with using of wrong pool.
		AbstractString(const AbstractString&) = delete;
		AbstractString& operator=(const AbstractString&) = delete;
		// Substring constructor
		AbstractString(const size_type limit, const AbstractString& from, size_type pos, size_type n, MemoryPool& p = getAutoMemoryPool());

#ifdef NOT_USED
		AbstractString(const size_type limit, const_pointer p1, const size_type n1,
			const_pointer p2, const size_type n2);

#endif // NOT_USED

		// Trim the range making sure that it fits inside specified length
		static void adjustRange(const size_type length, size_type& pos, size_type& n) throw();

		// Replaces the contents with a copy of s
		void baseAssign(const void* s, size_type n)
		{
			memcpy(getBuffer(n, false), s, n);
		}

		// Prepare n bytes of space at the end of existing string and return pointer to it
		pointer baseAppend(const size_type n);

		// Prepare n bytes of space at position p0 and return pointer to it
		pointer baseInsert(const size_type p0, const size_type n);

		// Delete from string n bytes starting from position p0
		void baseErase(size_type p0, size_type n) throw();

		enum TrimType {TrimLeft, TrimRight, TrimBoth};

		// Cut this and that from here and there
		void baseTrim(const TrimType whereTrim, const_pointer toTrim);

	public:
		const_pointer c_str() const
		{
			return stringBuffer;
		}
		size_type length() const
		{
			return stringLength;
		}
		// Almost same as c_str(), but return 0, not "",
		// when string has no data. Useful when interacting
		// with old code, which does check for NULL.
		const_pointer nullStr() const
		{
			return stringLength ? stringBuffer : 0;
		}
		// Call it only when you have worked with buffer directly, using at(), data() or operator[]
		// in case a null ASCII was inserted in the middle of the string.
		size_type recalculate_length()
		{
		    stringLength = static_cast<size_type>(strlen(stringBuffer));
		    return stringLength;
		}

		// Reserve buffer to allow storing at least newLen characters there
		// (not including null terminator). Existing contents of our string are optionally preserved.
		pointer getBuffer(const size_type newLen, bool preserve = true);

		void reserve(size_type n = 0);
		void resize(const size_type n, char_type c = ' ');
#ifdef NOT_USED
		void grow(const size_type n)
		{
			resize(n);
		}

#endif // NOT_USED

		size_type max_size() const
		{
			return max_length;
		}

		iterator begin()
		{
			return stringBuffer;
		}
		const_iterator begin() const
		{
			return c_str();
		}
		iterator end()
		{
			return stringBuffer + length();
		}
		const_iterator end() const
		{
			return c_str() + length();
		}
		const_reference at(const size_type pos) const
		{
			checkPos(pos);
			return c_str()[pos];
		}
		reference at(const size_type pos)
		{
			checkPos(pos);
			return begin()[pos];
		}
		const_reference operator[](size_type pos) const
		{
			return at(pos);
		}
		reference operator[](size_type pos)
		{
			return at(pos);
		}
		const_pointer data() const
		{
			return c_str();
		}
		size_type size() const
		{
			return length();
		}
		size_type capacity() const
		{
			return bufferSize - 1u;
		}
		bool empty() const
		{
			return length() == 0;
		}
		bool hasData() const
		{
			return !empty();
		}
		// to satisfy both ways to check for empty string
		bool isEmpty() const
		{
			return empty();
		}

		void ltrim(const_pointer ToTrim = " ")
		{
			baseTrim(TrimLeft, ToTrim);
		}
		void rtrim(const_pointer ToTrim = " ")
		{
			baseTrim(TrimRight, ToTrim);
		}
		void trim(const_pointer ToTrim = " ")
		{
			baseTrim(TrimBoth, ToTrim);
		}
		void alltrim(const_pointer ToTrim = " ")
		{
			baseTrim(TrimBoth, ToTrim);
		}

		bool LoadFromFile(FILE* file);
		void vprintf(const char* Format, va_list params);
		void printf(const char* Format, ...);

		size_type copyTo(pointer to, size_type toSize) const
		{
			fb_assert(to);
			fb_assert(toSize);
			if (--toSize > length())
			{
				toSize = length();
			}
			memcpy(to, c_str(), toSize);
			to[toSize] = 0;
			return toSize;
		}

		virtual AbstractString& assign(const void* s, size_type n)
		{
			baseAssign(s, n);
			return *this;
		}

		AbstractString& append(const AbstractString& str)
		{
			fb_assert(&str != this);
			return append(str.c_str(), str.length());
		}
		AbstractString& append(const AbstractString& str, size_type pos, size_type n = npos)
		{
			fb_assert(&str != this);
			adjustRange(str.length(), pos, n);
			return append(str.c_str() + pos, n);
		}
		AbstractString& append(const_pointer s, const size_type n)
		{
			memcpy(baseAppend(n), s, n);
			return *this;
		}
		AbstractString& append(const_pointer s)
		{
			return append(s, static_cast<size_type>(strlen(s)));
		}
		AbstractString& append(size_type n, char_type c)
		{
			memset(baseAppend(n), c, n);
			return *this;
		}
		AbstractString& append(const_iterator first, const_iterator last)
		{
			return append(first, last - first);
		}

	protected:
		// Following methods are protected to allow to override them in derived classes
		// without being virtual

		AbstractString& assign(const AbstractString& v)
		{
			if (this != &v)
			{
				baseAssign(v.c_str(), v.length());
			}
			return *this;
		}
		AbstractString& assign(const_pointer s)
		{
			assign(s, static_cast<size_type>(strlen(s)));
			return *this;
		}
		AbstractString& assign(const size_type n, const char_type c)
		{
			checkLength(n);
			memset(getBuffer(n, false), c, n);
			return *this;
		}
		// Pick up substring from other string
		AbstractString& assign(const AbstractString& v, size_type pos, size_type n = npos);

		// Move buffer from source if possible
		void move(AbstractString& v)
		{
			if (this != &v)
			{
				if (&getPool() == &v.getPool() && v.stringBuffer != v.inlineBuffer) // We can and have what to steal
				{
					if (stringBuffer != inlineBuffer) // we also have allocated buffer, exchange
					{
						std::swap(stringBuffer, v.stringBuffer);
						std::swap(stringLength, v.stringLength);
					}
					else // Just take it
					{
						stringBuffer = v.stringBuffer;
						stringLength = v.stringLength;
						v.stringBuffer = v.inlineBuffer;
						v.stringLength = 0;
					}
				}
				else // fallback to copy
				{
					assign(v);
				}
			}
		}

		AbstractString& insert(size_type p0, const AbstractString& str)
		{
			fb_assert(&str != this);
			return insert(p0, str.c_str(), str.length());
		}
		AbstractString& insert(size_type p0, const AbstractString& str, size_type pos,
			size_type n)
		{
			fb_assert(&str != this);
			adjustRange(str.length(), pos, n);
			return insert(p0, &str.c_str()[pos], n);
		}
		AbstractString& insert(size_type p0, const_pointer s, const size_type n)
		{
			if (p0 >= length())
				append(s, n);
			else
				memcpy(baseInsert(p0, n), s, n);
			return *this;
		}
		AbstractString& insert(size_type p0, const_pointer s)
		{
			return insert(p0, s, static_cast<size_type>(strlen(s)));
		}
		AbstractString& insert(size_type p0, const size_type n, const char_type c)
		{
			if (p0 >= length()) {
				append(n, c);
			}
			memset(baseInsert(p0, n), c, n);
			return *this;
		}
		//Following methods have conflicting signature with methods above because of implicit conversion int->*void
		//void insert(iterator it, size_type n, char_type c)
		//{
		//	insert(it - c_str(), n, c);
		//}
		//void insert(iterator it, const_iterator first, const_iterator last)
		//{
		//	insert(it - c_str(), first, last - first);
		//}

		// Find family methods are strictly case-sensitive
		size_type find(const AbstractString& str, size_type pos = 0) const
		{
			return find(str.c_str(), pos);
		}
		size_type find(const_pointer s, size_type pos = 0) const
		{
			const_pointer p = strstr(c_str() + pos, s);
			return p ? p - c_str() : npos;
		}
		size_type find(char_type c, size_type pos = 0) const
		{
			const_pointer p = strchr(c_str() + pos, c);
			return p ? p - c_str() : npos;
		}
		size_type rfind(const AbstractString& str, size_type pos = npos) const
		{
			return rfind(str.c_str(), pos);
		}
		size_type rfind(const_pointer s, const size_type pos = npos) const;
		size_type rfind(char_type c, const size_type pos = npos) const;
		size_type find_first_of(const AbstractString& str, size_type pos = 0) const
		{
			return find_first_of(str.c_str(), pos, str.length());
		}
		size_type find_first_of(const_pointer s, size_type pos, size_type n) const;
		size_type find_first_of(const_pointer s, size_type pos = 0) const
		{
			return find_first_of(s, pos, static_cast<size_type>(strlen(s)));
		}
		size_type find_first_of(char_type c, size_type pos = 0) const
		{
			return find(c, pos);
		}
		size_type find_last_of(const AbstractString& str, size_type pos = npos) const
		{
			return find_last_of(str.c_str(), pos, str.length());
		}
		size_type find_last_of(const_pointer s, const size_type pos, size_type n = npos) const;
		size_type find_last_of(const_pointer s, size_type pos = npos) const
		{
			return find_last_of(s, pos, static_cast<size_type>(strlen(s)));
		}
		size_type find_last_of(char_type c, size_type pos = npos) const
		{
			return rfind(c, pos);
		}
		size_type find_first_not_of(const AbstractString& str, size_type pos = 0) const
		{
			return find_first_not_of(str.c_str(), pos, str.length());
		}
		size_type find_first_not_of(const_pointer s, size_type pos, size_type n) const;
		size_type find_first_not_of(const_pointer s, size_type pos = 0) const
		{
			return find_first_not_of(s, pos, static_cast<size_type>(strlen(s)));
		}
		size_type find_first_not_of(char_type c, size_type pos = 0) const
		{
			const char_type s[2] = { c, 0 };
			return find_first_not_of(s, pos, 1);
		}
		size_type find_last_not_of(const AbstractString& str, size_type pos = npos) const
		{
			return find_last_not_of(str.c_str(), pos, str.length());
		}
		size_type find_last_not_of(const_pointer s, const size_type pos, size_type n = npos) const;
		size_type find_last_not_of(const_pointer s, size_type pos = npos) const
		{
			return find_last_not_of(s, pos, static_cast<size_type>(strlen(s)));
		}
		size_type find_last_not_of(char_type c, size_type pos = npos) const
		{
			const char_type s[2] = { c, 0 };
			return find_last_not_of(s, pos, 1);
		}

		// Replaces piece of string from pos with value of s
		AbstractString& replace(size_type pos, size_type len, const_pointer s, size_type n);
		AbstractString& replace(size_type p0, size_type n0, const_pointer s)
		{
			return replace(p0, n0, s, static_cast<size_type>(strlen(s)));
		}
		AbstractString& replace(size_type p0, size_type n0, const AbstractString& str)
		{
			fb_assert(&str != this);
			return replace(p0, n0, str.c_str(), str.length());
		}
#ifdef TO_BE_OPTIMIZED_IF_EVER_USED
		AbstractString& replace(const size_type p0, const size_type n0,
			const AbstractString& str, size_type pos, size_type n)
		{
			fb_assert(&str != this);
			adjustRange(str.length(), pos, n);
			return replace(p0, n0, &str.c_str()[pos], n);
		}
		AbstractString& replace(const size_type p0, const size_type n0, const_pointer s,
			size_type n)
		{
			erase(p0, n0);
			return insert(p0, s, n);
		}
		AbstractString& replace(const size_type p0, const size_type n0, size_type n,
			char_type c)
		{
			erase(p0, n0);
			return insert(p0, n, c);
		}
		AbstractString& replace(iterator first0, iterator last0, const AbstractString& str)
		{
			fb_assert(&str != this);
			return replace(first0 - c_str(), last0 - first0, str);
		}
		AbstractString& replace(iterator first0, iterator last0, const_pointer s,
			size_type n)
		{
			return replace(first0 - c_str(), last0 - first0, s, n);
		}
		AbstractString& replace(iterator first0, iterator last0, const_pointer s)
		{
			return replace(first0 - c_str(), last0 - first0, s);
		}
		AbstractString& replace(iterator first0, iterator last0, size_type n, char_type c)
		{
			return replace(first0 - c_str(), last0 - first0, n, c);
		}
		AbstractString& replace(iterator first0, iterator last0, const_iterator first,
			const_iterator last)
		{
			return replace(first0 - c_str(), last0 - first0, first, last - first);
		}

#endif // TO_BE_OPTIMIZED_IF_EVER_USED

	public:
		AbstractString& erase(size_type p0 = 0, size_type n = npos) throw()
		{
			baseErase(p0, n);
			return *this;
		}
		iterator erase(iterator it) throw()
		{
			erase(it - c_str(), 1);
			return it;
		}
		iterator erase(iterator first, iterator last) throw()
		{
			erase(first - c_str(), last - first);
			return first;
		}

		bool getWord(AbstractString& from, const char* sep)
		{
			from.alltrim(sep);
			size_type p = from.find_first_of(sep);
			if (p == npos)
			{
				if (from.isEmpty())
				{
					this->erase();
					return false;
				}
				this->assign(from);
				from.erase();
				return true;
			}

			this->assign(from, 0, p);
			from.erase(0, p);
			from.ltrim(sep);
			return true;
		}

		virtual ~AbstractString()
		{
			if (stringBuffer != inlineBuffer)
				delete[] stringBuffer;
		}
	};

	// All-purpose string of char.
	// Encoding unspecified, assume ascii.
	// Case-sensitive.
	class string : public AbstractString
	{
		static const size_type MAX_SIZE = 0x7FFFFFFEu;

		int compare(const_pointer str) const
		{
			size_t str_len = strlen(str);
			if (str_len > length())
				str_len = length();
			return memcmp(c_str(), str, str_len + 1);
		}
	public:
		// Explicit constructors could make code writes to feel overhead of temporary objects creating.
		// Unfortunatelly, in current codebase implicit conversions are used too often to use them.

		string(MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p) {}
		// Copy constructor
		string(const string& v, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, v, p) {}
		// Move constructor
		string(string&& v, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p) { move(v); }
		explicit string(MemoryPool& p, const string& v) : AbstractString(MAX_SIZE, v, p) {}
		string(const_pointer s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p) {}
		explicit string(const AbstractString& s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p) {}
		explicit string(const_pointer s, const size_type n, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, n, s, p) {}
		explicit string(const size_type n, const char_type c, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, n, c, p) {}
		explicit string(const AbstractString& v, const size_type pos, const size_type n = npos, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, v, pos, n, p) {}

		using AbstractString::assign;
		using AbstractString::insert;
		using AbstractString::replace;
		using AbstractString::find;
		using AbstractString::rfind;
		using AbstractString::find_first_of;
		using AbstractString::find_last_of;
		using AbstractString::find_first_not_of;
		using AbstractString::find_last_not_of;

		// For cases when string has to be compared with a literal without
		// case-sensitivity and cannot be replaced with NoCaseString
		bool equalsNoCase(const_pointer string) const;

		void upper();
		void lower();

		// Include terminating zero into comparison operation for simplification
		bool operator< (const string& str) const { return memcmp(c_str(), str.c_str(), MIN(str.length(), length()) + 1) < 0; }
		bool operator<=(const string& str) const { return memcmp(c_str(), str.c_str(), MIN(str.length(), length()) + 1) <= 0; }
		bool operator==(const string& str) const { return (length() == str.length()) && (memcmp(c_str(), str.c_str(), length() + 1) == 0); }
		bool operator>=(const string& str) const { return memcmp(c_str(), str.c_str(), MIN(str.length(), length()) + 1) >= 0; }
		bool operator> (const string& str) const { return memcmp(c_str(), str.c_str(), MIN(str.length(), length()) + 1) > 0; }
		bool operator!=(const string& str) const { return (length() != str.length()) || (memcmp(c_str(), str.c_str(), length() + 1) != 0); }
		bool operator< (const_pointer str) const { return compare(str) < 0; }
		bool operator<=(const_pointer str) const { return compare(str) <= 0; }
		bool operator==(const_pointer str) const { return (length() == strlen(str)) && (memcmp(c_str(), str, length() + 1) == 0); }
		bool operator>=(const_pointer str) const { return compare(str) >= 0; }
		bool operator> (const_pointer str) const { return compare(str) > 0; }
		bool operator!=(const_pointer str) const { return (length() != strlen(str)) || (memcmp(c_str(), str, length() + 1) != 0); }


		string& operator=(const string& v)
		{
			if (&v != this)
			{
				baseAssign(v.c_str(), v.length());
			}
			return *this;
		}
		string& operator=(string&& v)
		{
			move(v);
			return *this;
		}
		string& operator=(const AbstractString& v)
		{
			if (&v != this)
			{
				baseAssign(v.c_str(), v.length());
			}
			return *this;
		}
		string& operator=(const_pointer s)
		{
			baseAssign(s, static_cast<size_type>(strlen(s)));
			return *this;
		}
		string& operator=(const char_type c)
		{
			baseAssign(&c, 1);
			return *this;
		}

		string& operator+=(const AbstractString& v)
		{
			append(v);
			return *this;
		}
		string& operator+=(const_pointer s)
		{
			append(s);
			return *this;
		}
		string& operator+=(const char_type c)
		{
			append(1, c);
			return *this;
		}

		// Ugly and ineffective
		string operator+(const_pointer str) const
		{
			return string(*this) += str;
		}
		string operator+(const AbstractString& str) const
		{
			return string(*this) += str;
		}

		unsigned int hash(size_type tableSize) const
		{
			return InternalHash::hash(length(), reinterpret_cast<const UCHAR*>(c_str()), tableSize);
		}

		string substr(const size_type pos, const size_type n = npos) const
		{
			return string(*this, pos, n);
		}
	};

#ifdef HAVE_STRCASECMP
#define STRNCASECMP strncasecmp
#else
#ifdef HAVE_STRICMP
#define STRNCASECMP strnicmp
#else
#define STRNCASECMP StringIgnoreCaseCompare
#endif // HAVE_STRICMP
#endif // HAVE_STRCASECMP

	class NoCaseString : public AbstractString
	{
		static const size_type MAX_SIZE = 0x7FFFFFFEu;

		int compare(const_pointer str) const
		{
			size_t str_len = strlen(str);
			if (str_len > length())
				str_len = length();
			return STRNCASECMP(c_str(), str, str_len + 1);
		}
	public:
		NoCaseString(MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p) {}
		NoCaseString(const NoCaseString& s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p) {}
		NoCaseString(NoCaseString&& v, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p) { move(v); }
		NoCaseString(const_pointer s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p) {}
		NoCaseString(MemoryPool& p, const NoCaseString& s) : AbstractString(MAX_SIZE, s, p) {}
		explicit NoCaseString(const AbstractString& s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p) {}
		explicit NoCaseString(const_pointer s, const size_type n, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, n, s, p) {}
		// Substring constructor
		explicit NoCaseString(const AbstractString& from, const size_type pos, const size_type n = npos, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, from, pos, n) {}

		bool operator< (const NoCaseString& str) const { return STRNCASECMP(c_str(), str.c_str(), MIN(str.length(), length()) + 1) < 0; }
		bool operator<=(const NoCaseString& str) const { return STRNCASECMP(c_str(), str.c_str(), MIN(str.length(), length()) + 1) <= 0; }
		bool operator==(const NoCaseString& str) const { return (length() == str.length()) && (STRNCASECMP(c_str(), str.c_str(), length() + 1) == 0); }
		bool operator>=(const NoCaseString& str) const { return STRNCASECMP(c_str(), str.c_str(), MIN(str.length(), length()) + 1) >= 0; }
		bool operator> (const NoCaseString& str) const { return STRNCASECMP(c_str(), str.c_str(), MIN(str.length(), length()) + 1) > 0; }
		bool operator!=(const NoCaseString& str) const { return (length() != str.length()) || (STRNCASECMP(c_str(), str.c_str(), length() + 1) != 0); }
		bool operator< (const_pointer str) const { return compare(str) < 0; }
		bool operator<=(const_pointer str) const { return compare(str) <= 0; }
		bool operator==(const_pointer str) const { return (length() == strlen(str)) && (STRNCASECMP(c_str(), str, length() + 1) == 0); }
		bool operator>=(const_pointer str) const { return compare(str) >= 0; }
		bool operator> (const_pointer str) const { return compare(str) > 0; }
		bool operator!=(const_pointer str) const { return (length() != strlen(str)) || (STRNCASECMP(c_str(), str, length() + 1) != 0); }

		NoCaseString& operator=(const NoCaseString& v)
		{
			if (&v != this)
			{
				baseAssign(v.c_str(), v.length());
			}
			return *this;
		}
		NoCaseString& operator=(NoCaseString&& v)
		{
			move(v);
			return *this;
		}
		NoCaseString& operator=(const AbstractString& v)
		{
			if (&v != this)
			{
				baseAssign(v.c_str(), v.length());
			}
			return *this;
		}
		NoCaseString& operator=(const_pointer s)
		{
			baseAssign(s, static_cast<size_type>(strlen(s)));
			return *this;
		}
		NoCaseString& operator=(const char_type c)
		{
			baseAssign(&c, 1);
			return *this;
		}

		NoCaseString& operator+=(const AbstractString& v)
		{
			append(v);
			return *this;
		}
		NoCaseString& operator+=(const_pointer s)
		{
			append(s);
			return *this;
		}
		NoCaseString& operator+=(const char_type c)
		{
			append(1, c);
			return *this;
		}

		using AbstractString::assign;
		using AbstractString::insert;
		using AbstractString::replace;
		using AbstractString::find;
		using AbstractString::rfind;
		using AbstractString::find_first_of;
		using AbstractString::find_last_of;
		using AbstractString::find_first_not_of;
		using AbstractString::find_last_not_of;

		unsigned int hash(const size_type tableSize);

		NoCaseString operator+(const_pointer str) const
		{
			return NoCaseString(*this) += str;
		}
		NoCaseString operator+(const AbstractString& str) const
		{
			return NoCaseString(*this) += str;
		}

		NoCaseString substr(const size_type pos, const size_type n = npos)
		{
			return NoCaseString(*this, pos, n);
		}
	};

	// String that keeps file name or path.
	// Encoding UTF-8.
	// Case sensitivity is platform-dependent.
	class PathName : public AbstractString
	{
		static const size_type MAX_SIZE = 0x1FFFEu; // max path on Windows is 32k-1 of wide chars which in utf-8 is multiplied to 4
		int compare(const PathName& str) const;
		bool equals(const PathName& str) const;
		bool different(const PathName& str) const;
	public:
		mutable bool normalized;

		PathName(MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p), normalized(false) {}
		PathName(const PathName& from, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, from, p), normalized(from.normalized) {}
		PathName(PathName&& from, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, p) { move(from); normalized = from.normalized; }
		explicit PathName(const AbstractString& from, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, from, p), normalized(false) {}
		PathName(MemoryPool& p, const PathName& from) : AbstractString(MAX_SIZE, from, p), normalized(from.normalized) {}
		PathName(const_pointer s, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, s, p), normalized(false) {}
		explicit PathName(const_pointer s, const size_type n, MemoryPool& p = getAutoMemoryPool()) : AbstractString(MAX_SIZE, n, s, p), normalized(false) {}
		// Substring constructor
		explicit PathName(const PathName& from, const size_type pos, const size_type n) : AbstractString(MAX_SIZE, from, pos, n), normalized(from.normalized) {}
		// Optimized constructor for concatenation of two paths
		PathName(PathName& prefix, PathName& suffix, MemoryPool& p = getAutoMemoryPool());
		// Optimized constructor for concatenation of path and file name
		explicit PathName(const PathName& dir, const char* fileName, size_type n = npos, MemoryPool& p = getAutoMemoryPool());
		explicit PathName(const PathName& dir, const AbstractString& fileName, MemoryPool& p = getAutoMemoryPool()) : PathName(dir, fileName.c_str(), fileName.length(), p) {}

		PathName& assign(const PathName& v)
		{
			if (this != &v)
			{
				baseAssign(v.c_str(), v.length());
				normalized = v.normalized;
			}
			return *this;
		}
		PathName& assign(const AbstractString& s, const size_type pos, const size_type n = npos)
		{
			AbstractString::assign(s, pos, n);
			normalized = false;
			return *this;
		}
		PathName& assign(const void* s, size_type n) override
		{
			baseAssign(s, n);
			normalized = false;
			return *this;
		}
		PathName& assign(const PathName& v, size_type pos, size_type n);

		// Append path to the end ensuring proper separation and handling of '.' and '..'
		PathName& appendPath(const PathName& v);

		// String is appended as-is, no dir separator before
		PathName& appendString(const AbstractString& s)
		{
			AbstractString::append(s);
			normalized = false;
			return *this;
		}
		PathName& appendString(const_pointer s)
		{
			AbstractString::append(s);
			normalized = false;
			return *this;
		}
		PathName& appendString(const char_type c);

		size_type find(char_type c, size_type pos = 0) const;
		size_type find(const_pointer s, size_type pos = 0) const
		{
			return AbstractString::find(s, pos);
		}
		size_type rfind(char_type c, const size_type pos = npos) const;
		size_type find_first_of(const_pointer s, size_type pos = 0) const;
		size_type find_first_of(char_type c, size_type pos = 0) const
		{
			return find(c, pos);
		}

		// Comparsion of this class can change both sides
		bool operator< (const PathName& str) const { return compare(str) < 0; }
		bool operator<=(const PathName& str) const { return compare(str) <= 0; }
		bool operator==(const PathName& str) const { return equals(str); }
		bool operator>=(const PathName& str) const { return compare(str) >= 0; }
		bool operator> (const PathName& str) const { return compare(str) > 0; }
		bool operator!=(const PathName& str) const { return different(str); }

		// For performance reasons string literal to compare must not contain directory separators or non-ascii letters
		bool operator==(const char* str) const;
		bool operator!=(const char* str) const;

		PathName& operator=(const PathName& v)
		{
			if (&v != this)
			{
				baseAssign(v.c_str(), v.length());
				normalized = v.normalized;
			}
			return *this;
		}
		PathName& operator=(PathName&& v)
		{
			move(v);
			normalized = v.normalized;
			return *this;
		}
		PathName& operator=(const AbstractString& v)
		{
			baseAssign(v.c_str(), v.length());
			normalized = false;
			return *this;
		}
		PathName& operator=(const_pointer s)
		{
			baseAssign(s, static_cast<size_type>(strlen(s)));
			normalized = false;
			return *this;
		}
		PathName& operator=(const char_type c);

		unsigned int hash(const size_type tableSize) const;
		// Force path normalization
		void normalize() const;
		// Make sure that there is proper dir separator at the end
		void ensureSeparator();
		// Return true if path is relative
		bool isRelative() const;
		// Return true if this string is a subdir of given one
		bool isSubdirOf(PathName dir);

		PathName& insert(const size_type pos, const PathName& str)
		{
			AbstractString::insert(pos, str);
			normalized = normalized && str.normalized;
			return *this;
		}
		PathName& insert(const size_type pos, const char* str)
		{
			AbstractString::insert(pos, str);
			normalized = false;
			return *this;
		}

		PathName substr(const size_type pos, const size_type n = npos)
		{
			return PathName(*this, pos, n);
		}
	};

	static inline string operator+(string::const_pointer s, const string& str)
	{
		string rc(s);
		rc += str;
		return rc;
	}
	static inline NoCaseString operator+(NoCaseString::const_pointer s, const NoCaseString& str)
	{
		NoCaseString rc(s);
		rc += str;
		return rc;
	}
	// Type for plugin names
	typedef NoCaseString PluginName;
	// reference-counted strings
	typedef AnyRef<string> RefString;
	typedef RefPtr<RefString> RefStrPtr;
}


#endif	// INCLUDE_FB_STRING_H
