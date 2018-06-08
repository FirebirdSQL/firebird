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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_RECORD_H
#define JRD_RECORD_H

#include "../common/classes/array.h"
#include "../jrd/pag.h"
#include "../jrd/val.h"

namespace Jrd
{
	// Record flags
	const UCHAR REC_fake_nulls	= 1;	// all fields simulate being NULLs
	const UCHAR REC_gc_active	= 2;	// relation garbage collect record block in use
	const UCHAR REC_undo_active	= 4;	// record block in use for undo purposes

	class Record
	{
		template <UCHAR FLAG> friend class AutoTempRecord;

	public:
		Record(MemoryPool& p, const Format* format, UCHAR flags = 0)
			: m_precedence(p), m_data(p)
		{
			m_data.resize(format->fmt_length);
			m_format = format;
			m_flags = flags;
		}

		Record(MemoryPool& p, const Record* other)
			: m_precedence(p), m_data(p, other->m_data),
			  m_format(other->m_format), m_flags(other->m_flags)
		{}

		void reset(const Format* format = NULL, UCHAR flags = 0)
		{
			if (format && format != m_format)
			{
				m_data.resize(format->fmt_length);
				m_format = format;
			}

			m_flags = flags;
		}

		void setNull(USHORT id)
		{
			fb_assert(!(m_flags & REC_fake_nulls));
			getData()[id >> 3] |= (1 << (id & 7));
		}

		void clearNull(USHORT id)
		{
			fb_assert(!(m_flags & REC_fake_nulls));
			getData()[id >> 3] &= ~(1 << (id & 7));
		}

		bool isNull(USHORT id) const
		{
			if (m_flags & REC_fake_nulls)
				return true;

			return ((getData()[id >> 3] & (1 << (id & 7))) != 0);
		}

		void nullify()
		{
			// Zero the record buffer and initialize all fields to NULLs
			const size_t null_bytes = (m_format->fmt_count + 7) >> 3;
			memset(getData(), 0xFF, null_bytes);
			memset(getData() + null_bytes, 0, getLength() - null_bytes);

			// Record has real NULLs now, so clear the "fake-nulls" flag
			m_flags &= ~REC_fake_nulls;
		}

		PageStack& getPrecedence()
		{
			return m_precedence;
		}

		void pushPrecedence(const PageNumber& page)
		{
			m_precedence.push(page);
		}

		void copyFrom(const Record* other)
		{
			m_format = other->m_format;
			m_flags = other->m_flags;

			copyDataFrom(other);
		}

		void copyDataFrom(const Record* other, bool assertLength = false)
		{
			if (assertLength)
				fb_assert(getLength() == other->getLength());

			m_data.assign(other->m_data);
		}

		void copyDataFrom(const UCHAR* data)
		{
			memcpy(getData(), data, getLength());
		}

		void copyDataTo(UCHAR* data) const
		{
			memcpy(data, getData(), getLength());
		}

		const Format* getFormat() const
		{
			return m_format;
		}

		void fakeNulls()
		{
			m_flags |= REC_fake_nulls;
		}

		bool isNull() const
		{
			return (m_flags & REC_fake_nulls);
		}

		ULONG getLength() const
		{
			return m_format->fmt_length;
		}

		UCHAR* getData()
		{
			return m_data.begin();
		}

		const UCHAR* getData() const
		{
			return m_data.begin();
		}

		bool testFlags(UCHAR mask) const
		{
			return ((m_flags & mask) != 0);
		}

	private:
		PageStack m_precedence;			// stack of higher precedence pages/transactions
		Firebird::Array<UCHAR> m_data;	// space for record data
		const Format* m_format;			// what the data looks like
		UCHAR m_flags;					// misc record flags
	};

	// Wrapper for reusable temporary records

	template <UCHAR FLAG>
	class AutoTempRecord
	{
	public:
		explicit AutoTempRecord(Record* record = NULL)
			: m_record(record)
		{
			// validate record and its flag
			fb_assert(!record || (record->m_flags & FLAG));
		}

		~AutoTempRecord()
		{
			release();
		}

		Record* operator=(Record* record)
		{
			// class object can be initialized just once
			fb_assert(!m_record);
			// validate record and its flag
			fb_assert(!record || (record->m_flags & FLAG));

			m_record = record;
			return m_record;
		}

		void release()
		{
			if (m_record)
			{
				m_record->m_flags &= ~FLAG;

				if (FLAG == REC_gc_active && MemoryPool::wipePasses > 0)
					MemoryPool::wipeMemory(m_record->getData(), m_record->getLength());

				m_record = NULL;
			}
		}

		operator Record*()
		{
			return m_record;
		}

		Record* operator->()
		{
			return m_record;
		}

	private:
		Record* m_record;
	};

	typedef AutoTempRecord<REC_gc_active> AutoGCRecord;
	typedef AutoTempRecord<REC_undo_active> AutoUndoRecord;

} // namespace

#endif // JRD_RECORD_H
