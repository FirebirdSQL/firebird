/*
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
 *  The Original Code was created by Vladyslav Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2006 Vladyslav Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_PAGE_NUMBER_H
#define JRD_PAGE_NUMBER_H

//#include "../jrd/ods.h"
#include "../jrd/lls.h"

namespace Jrd
{
	// Possible page space IDs:
	//	- DB_PAGE_SPACE == 1 is a main database pagespace (primary tablespace)
	//	- range [2..MAX_TABLESPACE_ID] identifies user-defined persistent tablespaces
	//	- TRANS_PAGE_SPACE is a pseudo-space to store transaction numbers in precedence stack
	//	- TEMP_PAGE_SPACE and above are temporary pages
	inline constexpr ULONG INVALID_PAGE_SPACE	= 0;
	inline constexpr ULONG DB_PAGE_SPACE		= 1;
	inline constexpr ULONG MAX_TABLESPACE_ID	= 254;
	inline constexpr ULONG TRANS_PAGE_SPACE		= 255;
	inline constexpr ULONG TEMP_PAGE_SPACE		= 256;
	inline constexpr ULONG MAX_PAGE_SPACE_ID	= MAX_ULONG;

	// Page number class. Encapsulates both page space ID and page number.

	class PageNumber
	{
	public:
		// CVC: To be completely in sync, the second param would have to be TraNumber
		inline PageNumber(const ULONG pageSpaceID, const ULONG pageNumber) noexcept
			: m_pageNumber(pageNumber), m_pageSpaceID(pageSpaceID)
		{}

		// Required to be able to keep it in Firebird::Stack
		inline PageNumber() noexcept
			: m_pageNumber(0), m_pageSpaceID(INVALID_PAGE_SPACE)
		{}

		inline PageNumber(const PageNumber& from) = default;

		inline bool isValid() const noexcept
		{
			return (m_pageSpaceID != INVALID_PAGE_SPACE);
		}

		inline ULONG getPageNum() const noexcept
		{
			return m_pageNumber;
		}

		inline ULONG getPageSpaceID() const noexcept
		{
			fb_assert(isValid());

			return m_pageSpaceID;
		}

		inline ULONG setPageSpaceID(const ULONG pageSpaceID) noexcept
		{
			fb_assert(pageSpaceID != INVALID_PAGE_SPACE);

			m_pageSpaceID = pageSpaceID;
			return m_pageSpaceID;
		}

		static inline constexpr USHORT getLockLen() noexcept
		{
			return sizeof(m_pageNumber) + sizeof(m_pageSpaceID);
		}

		inline void getLockStr(UCHAR* str) const
		{
			fb_assert(isValid());

			memcpy(str, &m_pageNumber, sizeof(ULONG));
			str += sizeof(ULONG);
			memcpy(str, &m_pageSpaceID, sizeof(ULONG));
		}

		inline PageNumber& operator=(const PageNumber& from) = default;

		inline ULONG operator=(const ULONG from) noexcept
		{
			m_pageNumber = from;
			return m_pageNumber;
		}

		inline bool operator==(const PageNumber& other) const noexcept
		{
			return (m_pageNumber == other.m_pageNumber) && (m_pageSpaceID == other.m_pageSpaceID);
		}

		inline bool operator!=(const PageNumber& other) const noexcept
		{
			return !(*this == other);
		}

		inline bool operator>(const PageNumber& other) const noexcept
		{
			fb_assert(isValid() && other.isValid());

			return (m_pageSpaceID > other.m_pageSpaceID) ||
				((m_pageSpaceID == other.m_pageSpaceID) && (m_pageNumber > other.m_pageNumber));
		}

		inline bool operator>=(const PageNumber& other) const noexcept
		{
			fb_assert(isValid() && other.isValid());

			return (m_pageSpaceID > other.m_pageSpaceID) ||
				((m_pageSpaceID == other.m_pageSpaceID) && (m_pageNumber >= other.m_pageNumber));
		}

		inline bool operator<(const PageNumber& other) const noexcept
		{
			return !(*this >= other);
		}

		inline bool operator<=(const PageNumber& other) const noexcept
		{
			return !(*this > other);
		}

	private:
		ULONG m_pageNumber;
		ULONG m_pageSpaceID;
	};

	typedef Firebird::Stack<PageNumber> PageStack;

} // namespace Jrd

#endif	// JRD_PAGE_NUMBER_H
