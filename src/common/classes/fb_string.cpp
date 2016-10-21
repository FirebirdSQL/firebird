/*
 *	PROGRAM:	string class definition
 *	MODULE:		fb_string.cpp
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

#include "firebird.h"
#include "../common/classes/fb_string.h"

#include <ctype.h>
#include <stdarg.h>

#include "../common/os/path_utils.h"
#include "../common/os/os_utils.h"

#if !defined(HAVE_STRCASECMP) && !defined(HAVE_STRICMP)
namespace
{
	int StringIgnoreCaseCompare(const char* s1, const char* s2, unsigned int l)
	{
		while (l--)
		{
			const int delta = toupper(*s1++) - toupper(*s2++);
			if (delta) {
				return delta;
			}
		}
		return 0;
	}
} // namespace
#endif

namespace {
	class strBitMask
	{
	private:
		char m[32];
	public:
		strBitMask(Firebird::AbstractString::const_pointer s, Firebird::AbstractString::size_type l)
		{
			memset(m, 0, sizeof(m));
			if (l == Firebird::AbstractString::npos) {
				l = static_cast<Firebird::AbstractString::size_type>(strlen(s));
			}
			Firebird::AbstractString::const_pointer end = s + l;
			while (s < end)
			{
				const unsigned char uc = static_cast<unsigned char>(*s++);
				m[uc >> 3] |= (1 << (uc & 7));
			}
		}
		inline bool Contains(const char c) const
		{
			const unsigned char uc = static_cast<unsigned char>(c);
			return m[uc >> 3] & (1 << (uc & 7));
		}
	};

} // namespace

namespace Firebird
{
	const AbstractString::size_type AbstractString::npos = (AbstractString::size_type)(~0);

	// May be here can be optimization for C++11 move semantic and the same pool
	AbstractString::AbstractString(const size_type limit, const AbstractString& v, MemoryPool& p)
		: AutoStorage(p), max_length(limit)
	{
		initialize(v.length());
		memcpy(stringBuffer, v.c_str(), v.length());
	}

	AbstractString::AbstractString(const size_type limit, const size_type sizeL, const void* dataL, MemoryPool& p)
		: AutoStorage(p), max_length(limit)
	{
		initialize(sizeL);
		memcpy(stringBuffer, dataL, sizeL);
	}

	AbstractString::AbstractString(const size_type limit, const_pointer data, MemoryPool& p)
		: AutoStorage(p), max_length(limit)
	{
		if (data)
		{
			size_type sizeL = static_cast<size_type>(strlen(data));
			initialize(sizeL);
			memcpy(stringBuffer, data, sizeL);
		}
		else
		{
			initialize(0);
		}
	}

	AbstractString::AbstractString(const size_type limit, const AbstractString& from, size_type pos, size_type n, MemoryPool& p)
		: AutoStorage(p), max_length(limit)
	{
		adjustRange(from.length(), pos, n);
		initialize(n);
		memcpy(stringBuffer, from.c_str() + pos, n);
	}

#ifdef NOT_USED
	AbstractString::AbstractString(const size_type limit, const_pointer p1, const size_type n1,
		const_pointer p2, const size_type n2)
		: max_length(limit)
	{
		// CVC: npos must be maximum size_type value for all platforms.
		// fb_assert(n2 < npos - n1 && n1 + n2 <= max_length());
		if (n2 > npos - n1)
		{
			Firebird::fatal_exception::raise("String length overflow");
		}
		// checkLength(n1 + n2); redundant: initialize() will check.
		initialize(n1 + n2);
		memcpy(stringBuffer, p1, n1);
		memcpy(stringBuffer + n1, p2, n2);
	}

#endif // NOT_USED

	AbstractString::pointer AbstractString::getBuffer(const size_type newLen, bool preserve)
	{
		// Make sure we do not exceed string length limit
		checkLength(newLen);

		if (newLen >= bufferSize)
		{
			size_type newSize = newLen + 1u + INIT_RESERVE; // Be pessimistic

			// Grow buffer exponentially to prevent memory fragmentation
			if (newSize / 2u < bufferSize)
				newSize = size_type(bufferSize) * 2u;

			// Do not grow buffer beyond string length limit
			if (newSize > max_length)
				newSize = max_length;

			// Order of assignments below is important in case of low memory conditions
			if (preserve)
			{
				// Allocate new buffer and take care that it will be freed if something go wrong below
				Firebird::AutoPtr<char_type, ArrayDelete<char_type>> newBuffer(FB_NEW_POOL(getPool()) char_type[newSize]);

				// Carefully copy string data including null terminator
				memcpy(newBuffer, stringBuffer, sizeof(char_type) * (stringLength + 1u));

				// Deallocate old buffer if needed
				if (stringBuffer != inlineBuffer)
					delete[] stringBuffer;

				stringBuffer = newBuffer.release();
			}
			else
			{
				// Deallocate old buffer if needed
				if (stringBuffer != inlineBuffer)
					delete[] stringBuffer;

				stringBuffer = FB_NEW_POOL(getPool()) char_type[newSize];
			}

			bufferSize = newSize;
			// Let caller not to care about termination of the buffer
		}
		stringBuffer[newLen] = 0;
		stringLength = newLen;
		return stringBuffer;
	}

	void AbstractString::adjustRange(const size_type length, size_type& pos, size_type& n) throw()
	{
		if (pos == npos) {
			pos = length > n ? length - n : 0;
		}
		if (pos >= length)
		{
			pos = length;
			n = 0;
		}
		else if (n > length || pos + n > length || n == npos) {
			n = length - pos;
		}
	}

	AbstractString::pointer AbstractString::baseAppend(const size_type n)
	{
		getBuffer(stringLength + n);
		return stringBuffer + stringLength - n;
	}

	AbstractString::pointer AbstractString::baseInsert(const size_type p0, const size_type n)
	{
		if (p0 >= length()) {
			return baseAppend(n);
		}
		getBuffer(stringLength + n);
		// Do not forget to move null terminator, too
		memmove(stringBuffer + p0 + n, stringBuffer + p0, stringLength - p0 - n + 1);
		return stringBuffer + p0;
	}

	void AbstractString::baseErase(size_type p0, size_type n) throw()
	{
		adjustRange(length(), p0, n);
		if (n == 0)
			return;
		memmove(stringBuffer + p0, stringBuffer + p0 + n, stringLength - (p0 + n) + 1);
		stringLength -= n;
		shrinkBuffer();
	}

	void AbstractString::reserve(size_type n)
	{
		// Do not allow to reserve huge buffers
		if (n > max_length)
			n = max_length;

		getBuffer(n);
	}

	void AbstractString::resize(const size_type n, char_type c)
	{
		if (n == length()) {
			return;
		}
		if (n > stringLength)
		{
			const size_type oldLen = stringLength;
			getBuffer(n);
			memset(stringBuffer + oldLen, c, n - oldLen);
		}
		else
		{
			stringLength = n;
			stringBuffer[n] = 0;
			shrinkBuffer();
		}
	}

	AbstractString& AbstractString::assign(const AbstractString& v, size_type pos, size_type n)
	{
		adjustRange(v.length(), pos, n);
		if (&v == this)
		{
			erase(0, pos);
			resize(n);
		}
		else
		{
			baseAssign(v.c_str() + pos, n);
		}
		return *this;
	}

	AbstractString::size_type AbstractString::rfind(const_pointer s, const size_type pos) const
	{
		const size_type l = static_cast<size_type>(strlen(s));
		int lastpos = length() - l;
		if (lastpos < 0) {
			return npos;
		}
		if (pos < static_cast<size_type>(lastpos)) {
			lastpos = pos;
		}
		const_pointer start = c_str();
		for (const_pointer endL = &start[lastpos]; endL >= start; --endL)
		{
			if (memcmp(endL, s, l) == 0) {
				return endL - start;
			}
		}
		return npos;
	}

	AbstractString::size_type AbstractString::rfind(char_type c, const size_type pos) const
	{
		int lastpos = length() - 1;
		if (lastpos < 0) {
			return npos;
		}
		if (pos < static_cast<size_type>(lastpos)) {
			lastpos = pos;
		}
		const_pointer start = c_str();
		for (const_pointer endL = &start[lastpos]; endL >= start; --endL)
		{
			if (*endL == c) {
				return endL - start;
			}
		}
		return npos;
	}

	AbstractString::size_type AbstractString::find_first_of(const_pointer s, size_type pos, size_type n) const
	{
		const strBitMask sm(s, n);
		const_pointer p = &c_str()[pos];
		while (pos < length())
		{
			if (sm.Contains(*p++)) {
				return pos;
			}
			++pos;
		}
		return npos;
	}

	AbstractString::size_type AbstractString::find_last_of(const_pointer s, const size_type pos, size_type n) const
	{
		const strBitMask sm(s, n);
		int lpos = length() - 1;
		if (static_cast<int>(pos) < lpos && pos != npos) {
			lpos = pos;
		}
		const_pointer p = &c_str()[lpos];
		while (lpos >= 0)
		{
			if (sm.Contains(*p--)) {
				return lpos;
			}
			--lpos;
		}
		return npos;
	}

	AbstractString::size_type AbstractString::find_first_not_of(const_pointer s, size_type pos, size_type n) const
	{
		const strBitMask sm(s, n);
		const_pointer p = &c_str()[pos];
		while (pos < length())
		{
			if (! sm.Contains(*p++)) {
				return pos;
			}
			++pos;
		}
		return npos;
	}

	AbstractString::size_type AbstractString::find_last_not_of(const_pointer s, const size_type pos, size_type n) const
	{
		const strBitMask sm(s, n);
		int lpos = length() - 1;
		if (static_cast<int>(pos) < lpos && pos != npos) {
			lpos = pos;
		}
		const_pointer p = &c_str()[lpos];
		while (lpos >= 0)
		{
			if (! sm.Contains(*p--)) {
				return lpos;
			}
			--lpos;
		}
		return npos;
	}

	AbstractString& AbstractString::replace(size_type pos, size_type len, const_pointer s, size_type n)
	{
		adjustRange(length(), pos, len);
		if (len < n)
		{
			// expand string
			baseInsert(pos, n - len);
		}
		else if (len > n)
		{
			// Shrink string
			baseErase(pos, len - n);
		}
		memcpy(stringBuffer + pos, s, n);
		return *this;
	}

	bool AbstractString::LoadFromFile(FILE* file)
	{
		baseErase(0, length());
		if (! file)
			return false;

		bool rc = false;
		int c;
		while ((c = getc(file)) != EOF)
		{
			rc = true;
			if (c == '\n') {
				break;
			}
			*baseAppend(1) = c;
		}
		return rc;
	}

	void AbstractString::baseTrim(const TrimType whereTrim, const_pointer toTrim)
	{
		const strBitMask sm(toTrim, static_cast<size_type>(strlen(toTrim)));
		const_pointer b = c_str();
		const_pointer e = c_str() + length() - 1;
		if (whereTrim != TrimRight)
		{
			while (b <= e)
			{
				if (! sm.Contains(*b)) {
					break;
				}
				++b;
			}
		}
		if (whereTrim != TrimLeft)
		{
			while (b <= e)
			{
				if (! sm.Contains(*e)) {
					break;
				}
				--e;
			}
		}
		const size_type NewLength = e - b + 1;

		if (NewLength == length())
			return;

		if (b != c_str())
		{
			memmove(stringBuffer, b, NewLength);
		}
		stringLength = NewLength;
		stringBuffer[NewLength] = 0;
		shrinkBuffer();
	}

	void AbstractString::printf(const char* format,...)
	{
		va_list params;
		va_start(params, format);
		vprintf(format, params);
		va_end(params);
	}

// Need macros here - va_copy()/va_end() should be called in SAME function
#ifdef HAVE_VA_COPY
#define FB_VA_COPY(to, from) va_copy(to, from)
#define FB_CLOSE_VACOPY(to) va_end(to)
#else
#define FB_VA_COPY(to, from) to = from
#define FB_CLOSE_VACOPY(to)
#endif

	void AbstractString::vprintf(const char* format, va_list params)
	{
#ifndef HAVE_VSNPRINTF
#error NS: I am lazy to implement version of this routine based on plain vsprintf.
#error Please find an implementation of vsnprintf function for your platform.
#error For example, consider importing library from http://www.ijs.si/software/snprintf/
#error to Firebird extern repository
#endif
		enum { tempsize = 256 };
		char temp[tempsize];
		va_list paramsCopy;
		FB_VA_COPY(paramsCopy, params);
		int l = VSNPRINTF(temp, tempsize, format, paramsCopy);
		FB_CLOSE_VACOPY(paramsCopy);
		if (l < 0)
		{
			size_type n = sizeof(temp);
			while (true)
			{
				n *= 2;
				if (n > max_length)
					n = max_length;
				FB_VA_COPY(paramsCopy, params);
				l = VSNPRINTF(getBuffer(n, false), n + 1, format, paramsCopy);
				FB_CLOSE_VACOPY(paramsCopy);
				if (l >= 0)
					break;
				if (n >= max_length)
				{
					stringBuffer[max_length] = 0;
					return;
				}
			}
			resize(l);
			return;
		}
		temp[tempsize - 1] = 0;
		if (l < tempsize) {
			memcpy(getBuffer(l, false), temp, l);
		}
		else
		{
			resize(l);
			FB_VA_COPY(paramsCopy, params);
			VSNPRINTF(begin(), l + 1, format, paramsCopy);
			FB_CLOSE_VACOPY(paramsCopy);
		}
	}


	void string::upper()
	{
		for (pointer p = begin(); *p; p++) {
			*p = toupper(*p);
		}
	}

	void string::lower()
	{
		for (pointer p = begin(); *p; p++) {
			*p = tolower(*p);
		}
	}

	bool string::equalsNoCase(AbstractString::const_pointer string) const
	{
		size_t l = strlen(string);

		return (l == length()) && (STRNCASECMP(c_str(), string, ++l) == 0);
	}


	PathName::PathName(PathName& prefix, PathName& suffix, MemoryPool& p)
		: AbstractString(MAX_SIZE, p), normalized(true)
	{
		// Force normalization of prefix beforehand in case it will be reused later.
		// It is typical usage if PathName is constructed in loop with the same prefix but different suffixes.
		if (!prefix.normalized)
			prefix.normalize();

		// Preserve buffer large enough to contain concatenation of both strings
		getBuffer(prefix.length() + suffix.length() + 2, false);

		AbstractString::assign(prefix);
		appendPath(suffix);
	}

	PathName::PathName(const PathName& dir, const char* fileName, size_type n, MemoryPool& p)
		: AbstractString(MAX_SIZE, p), normalized(true)
	{
		if (fileName == NULL)
		{
			// It must zero literal from substring constructor
			// implicitly converted to NULL pointer.
			// In this case n to be set to some sane value.
			// Otherwise it is really a bug in code because using
			// of substring constructor for whole string is meaningless.
			fb_assert(n > 0 && n != npos);

			assign(dir, 0, n);
			return;
		}

		if (n == npos)
			n = static_cast<size_type>(strlen(fileName));

		if (!dir.normalized)
			dir.normalize();

		// Preserve buffer large enough to contain concatenation of both strings
		getBuffer(dir.length() + n + 2, false);

		AbstractString::assign(dir);
		ensureSeparator();
		appendString(fileName);
	}

	int PathName::compare(const PathName& str) const
	{
		// On Windows file names are unicode, we cannot use locale-dependent comparison routines.
		// Make sure that file names are normalized before comparison.
		if (!normalized)
			normalize();
		if (!str.normalized)
			str.normalize();
		// Normalization can change length of the string
		const size_type len_this = length();
		const size_type len_str = str.length();
		return memcmp(c_str(), str.c_str(), MIN(len_this, len_str) + 1);
	}

	bool PathName::equals(const PathName& str) const
	{
		if (!normalized)
			normalize();
		if (!str.normalized)
			str.normalize();
		return (length() == str.length()) && (memcmp(c_str(), str.c_str(), length()) == 0);
	}

	bool PathName::different(const PathName& str) const
	{
		if (!normalized)
			normalize();
		if (!str.normalized)
			str.normalize();
		return (length() != str.length()) || (memcmp(c_str(), str.c_str(), length()) != 0);
	}

	bool PathName::operator==(const char* str) const
	{
		size_t str_len = strlen(str);
		return (length() == str_len) &&
			((CASE_SENSITIVITY ? memcmp(c_str(), str, str_len) : STRNCASECMP(c_str(), str, str_len)) == 0);
	}

	bool PathName::operator!=(const char* str) const
	{
		size_t str_len = strlen(str);
		return (length() != str_len) ||
			((CASE_SENSITIVITY ? memcmp(c_str(), str, str_len) : STRNCASECMP(c_str(), str, str_len)) != 0);
	}

	PathName& PathName::assign(const PathName& v, size_type pos, size_type n)
	{
		adjustRange(v.length(), pos, n);
		if (&v == this)
		{
			erase(0, pos);
			resize(n);
		}
		else
		{
			baseAssign(v.c_str() + pos, n);
			normalized = v.normalized;
		}
		return *this;
	}

	static bool isNormalChar(AbstractString::char_type c)
	{
		static const char chars[] = { PathUtils::dir_sep, '.', '_', '$', '-', '(', ')', '!' };
		// List of chars that are allowed in file names and don't break normalization
		static const strBitMask normalChars(chars, sizeof(chars));
		return normalChars.Contains(c);
	}

	PathName::size_type PathName::find(char_type c, size_type pos) const
	{
		if (c == PathUtils::dir_sep)
			normalize();
		return AbstractString::find(c, pos);
	}

	PathName::size_type PathName::rfind(char_type c, const size_type pos) const
	{
		if (c == PathUtils::dir_sep)
			normalize();
		return AbstractString::rfind(c, pos);
	}

	PathName::size_type PathName::find_first_of(const_pointer s, size_type pos) const
	{
		if (!normalized && strchr(s, PathUtils::dir_sep) != NULL)
			normalize();
		return AbstractString::find_first_of(s, pos);
	}

	PathName& PathName::operator=(char_type c)
	{
		baseAssign(&c, 1);
		normalized = isNormalChar(c);
		return *this;
	}

	void PathName::ensureSeparator()
	{
		if (!normalized)
			normalize();
		// Do not add separator to an empty string because it will transform a relative
		// path into absolute, which is not a good idea in common case.
		if (stringLength > 0 && stringBuffer[stringLength - 1] != PathUtils::dir_sep)
			AbstractString::append(1, PathUtils::dir_sep);
	}

	bool PathName::isRelative() const
	{
		if (!normalized)
			normalize();
#ifdef WIN_NT
		// Path starting from single \ is not absolute because it is relative to a current/given drive
		return (stringLength < 2) || // Short path cannot be absolute
				!((stringBuffer[1] == ':' && stringBuffer[0] >= 'A' && stringBuffer[0] <= 'Z') // Path starts from drive letter is absolute
				  || (stringBuffer[0] == '\\' && stringBuffer[1] == '\\'));					// UNC path is absolute
#else
		return stringLength == 0 || stringBuffer[0] != PathUtils::dir_sep;
#endif
	}


	PathName& PathName::appendPath(const PathName& v)
	{
		// If appending path is absolute
		if (!v.isRelative())
			erase();

		// or is appended to an empty path - trust the user, do not check for links inside
		if (length() == 0)
		{
			assign(v);
			return *this;
		}

		// Append path by pieces to handle directory links
		ensureSeparator();

		size_type cur_pos = 0;

#ifdef WIN_NT
		if (v.find(PathUtils::dir_sep) == 0) // Handle path relative to drive
		{
			if (isRelative()) // This string is also relative, put drive assignment off
			{
				*this = PathUtils::dir_sep;
			}
			else
			{
				size_type first_sep = find(PathUtils::dir_sep);
				erase(first_sep + 1);
			}
			cur_pos++;
		}
#endif
		for (size_type pos = 0; cur_pos < v.length(); cur_pos = pos + 1)
		{
			pos = v.find(PathUtils::dir_sep, cur_pos);
			if (pos == npos) // simple name, simple handling
			{
				pos = v.length();
			}
			if (pos == cur_pos) // Empty piece, ignore
			{
				continue;
			}
			if (pos == cur_pos + 1 && memcmp(v.c_str() + cur_pos, PathUtils::curr_dir_link, 1) == 0) // Current dir, ignore
			{
				continue;
			}
			if (pos == cur_pos + 2 && memcmp(v.c_str() + cur_pos, PathUtils::up_dir_link, 2) == 0) // One dir up
			{
				if (length() < 2) // We have nothing to cut off, ignore this piece (may be throw an error?..)
					continue;

				const size_type up_dir = rfind(PathUtils::dir_sep, length() - 2);
				if (up_dir == npos)
					continue;

				erase(up_dir + 1);
				continue;
			}
			AbstractString::append(v, cur_pos, pos - cur_pos + 1); // append the piece including separator
		}

		return *this;
	}

	PathName& PathName::appendString(const char_type c)
	{
		AbstractString::append(1, c);
		normalized = normalized && isNormalChar(c);
		return *this;
	}

	unsigned int PathName::hash(const size_type tableSize) const
	{
		if (!normalized)
			normalize();
		return InternalHash::hash(length(), reinterpret_cast<const UCHAR*>(c_str()), tableSize);
	}

	void PathName::normalize() const
	{
		if (normalized) // nothing to do
			return;

		if (stringLength > 0)
		{
			// Bring any path separators to a right one
			for (char* itr = stringBuffer; *itr; ++itr)
			{
				if ((*itr == '\\') || (*itr == '/'))
					*itr = PathUtils::dir_sep;
			}

#ifdef WIN_NT
			// For case-insensitive file systems convert it to uppercase
			os_utils::WideCharBuffer temp(*this);
			if (!temp.toUpper())
			{
				DWORD err = GetLastError();
				Firebird::status_exception::raise(
					Firebird::Arg::Gds(isc_transliteration_failed) <<
					Firebird::Arg::Windows(err));
			}
			// Assure backslash after drive letter in absolute path
			if (temp.getLength() >= 2 && temp[1] == L':')
			{
				if (temp.getLength() == 2 || temp[2] != L'\\')
					temp.insert(2, L'\\');
			}
			temp.toString(CP_UTF8, *const_cast<PathName*>(this));
#endif
		}

		normalized = true;
	}

	bool PathName::isSubdirOf(PathName dir)
	{
		dir.ensureSeparator(); // it will call normalize() as well

		size_type dir_len = dir.length();
		
		if (length() <= dir_len)
			return false;

		if (!normalized)
			normalize();

		return memcmp(c_str(), dir.c_str(), dir_len) == 0;
	}

	unsigned int NoCaseString::hash(const size_type tableSize)
	{
		unsigned int value = 0;
		unsigned char c;

		for (iterator itr = begin(); itr < end(); ++itr)
		{
			c = toupper(*itr);
			value = value * 11 + c;
		}

		return value % tableSize;
	}

}	// namespace Firebird
