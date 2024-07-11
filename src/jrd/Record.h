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
	class Record
	{
		friend class AutoTempRecord;

	public:
		Record(MemoryPool& p, const Format* format, const bool temp_active = false)
			: m_precedence(p), m_data(p), m_fake_nulls(false), m_temp_active(temp_active)
		{
			m_data.resize(format->fmt_length);
			m_format = format;
		}

		Record(MemoryPool& p, const Record* other)
			: m_precedence(p), m_data(p, other->m_data),
			  m_format(other->m_format), m_fake_nulls(other->m_fake_nulls), m_temp_active(false)
		{}

		void reset(const Format* format = NULL)
		{
			if (format && format != m_format)
			{
				m_data.resize(format->fmt_length);
				m_format = format;
			}

			m_fake_nulls = false;
		}

		void setNull(USHORT id)
		{
			fb_assert(!m_fake_nulls);
			getData()[id >> 3] |= (1 << (id & 7));
		}

		void clearNull(USHORT id)
		{
			fb_assert(!m_fake_nulls);
			getData()[id >> 3] &= ~(1 << (id & 7));
		}

		bool isNull(USHORT id) const
		{
			if (m_fake_nulls)
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
			m_fake_nulls = false;
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
			m_fake_nulls = other->m_fake_nulls;

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
			m_fake_nulls = true;
		}

		bool isNull() const
		{
			return m_fake_nulls;
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

		bool isTempActive() const
		{
			return m_temp_active;
		}

		void setTempActive()
		{
			m_temp_active = true;
		}

		TraNumber getTransactionNumber() const
		{
		    return m_transaction_nr;
		}

		void setTransactionNumber(const TraNumber& transaction_nr)
		{
		    m_transaction_nr = transaction_nr;
		}

	private:
		PageStack m_precedence;			// stack of higher precedence pages/transactions
		Firebird::Array<UCHAR> m_data;	// space for record data
		const Format* m_format;			// what the data looks like
		TraNumber m_transaction_nr;		// transaction number for a record
		bool m_fake_nulls;				// all fields simulate being NULLs
		bool m_temp_active;				// record block in use for garbage collection or undo purposes
	};

	// Wrapper for reusable temporary records

	class AutoTempRecord
	{
	public:
		explicit AutoTempRecord(Record* record = NULL)
			: m_record(record)
		{
			// validate record and its flag
			fb_assert(!record || record->m_temp_active);
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
			fb_assert(!record || record->m_temp_active);

			m_record = record;
			return m_record;
		}

		void release()
		{
			if (m_record)
			{
				fb_assert(m_record->m_temp_active);
				m_record->m_temp_active = false;
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
} // namespace

#endif // JRD_RECORD_H
