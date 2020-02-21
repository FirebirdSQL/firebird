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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef FIREBIRD_MESSAGE_H
#define FIREBIRD_MESSAGE_H

#include "ibase.h"
#include "./Interface.h"
#include "./impl/boost/preprocessor/seq/for_each_i.hpp"
#include <assert.h>
#include <string.h>

#if defined(__linux__) && defined(__i386__)
#define FB__INT64_ALIGNAS alignas(8)
#else
#define FB__INT64_ALIGNAS
#endif

#define FB_MESSAGE(name, statusType, fields)	\
	FB__MESSAGE_I(name, statusType, 2, FB_BOOST_PP_CAT(FB__MESSAGE_X fields, 0), )

#define FB__MESSAGE_X(x, y) ((x, y)) FB__MESSAGE_Y
#define FB__MESSAGE_Y(x, y) ((x, y)) FB__MESSAGE_X
#define FB__MESSAGE_X0
#define FB__MESSAGE_Y0

#define FB_TRIGGER_MESSAGE(name, statusType, fields)	\
	FB__MESSAGE_I(name, statusType, 3, FB_BOOST_PP_CAT(FB_TRIGGER_MESSAGE_X fields, 0), \
		FB_TRIGGER_MESSAGE_MOVE_NAMES(name, fields))

#define FB_TRIGGER_MESSAGE_X(x, y, z) ((x, y, z)) FB_TRIGGER_MESSAGE_Y
#define FB_TRIGGER_MESSAGE_Y(x, y, z) ((x, y, z)) FB_TRIGGER_MESSAGE_X
#define FB_TRIGGER_MESSAGE_X0
#define FB_TRIGGER_MESSAGE_Y0

#define FB__MESSAGE_I(name, statusType, size, fields, moveNames)	\
	struct name	\
	{	\
		struct Type	\
		{	\
			FB_BOOST_PP_SEQ_FOR_EACH_I(FB__MESSAGE_FIELD, size, fields)	\
		};	\
		\
		static void setup(statusType* status, ::Firebird::IMetadataBuilder* builder)	\
		{	\
			unsigned index = 0;	\
			moveNames	\
			FB_BOOST_PP_SEQ_FOR_EACH_I(FB__MESSAGE_META, size, fields)	\
		}	\
		\
		name(statusType* status, ::Firebird::IMaster* master)	\
			: desc(master, status, FB_BOOST_PP_SEQ_SIZE(fields), setup)	\
		{	\
		}	\
		\
		::Firebird::IMessageMetadata* getMetadata() const	\
		{	\
			return desc.getMetadata();	\
		}	\
		\
		void clear()	\
		{	\
			memset(&data, 0, sizeof(data));	\
		}	\
		\
		Type* getData()	\
		{	\
			return &data;	\
		}	\
		\
		const Type* getData() const	\
		{	\
			return &data;	\
		}	\
		\
		Type* operator ->()	\
		{	\
			return getData();	\
		}	\
		\
		const Type* operator ->() const	\
		{	\
			return getData();	\
		}	\
		\
		Type data;	\
		::Firebird::MessageDesc desc;	\
	}

#define FB__MESSAGE_FIELD(r, _, i, xy)	\
	FB_BOOST_PP_CAT(FB__TYPE_, FB_BOOST_PP_TUPLE_ELEM(_, 0, xy)) FB_BOOST_PP_TUPLE_ELEM(_, 1, xy);	\
	ISC_SHORT FB_BOOST_PP_CAT(FB_BOOST_PP_TUPLE_ELEM(_, 1, xy), Null);

#define FB__MESSAGE_META(r, _, i, xy)	\
	FB_BOOST_PP_CAT(FB__META_, FB_BOOST_PP_TUPLE_ELEM(_, 0, xy))	\
	++index;

// Types - metadata

#define FB__META_FB_SCALED_SMALLINT(scale)	\
	builder->setType(status, index, SQL_SHORT);	\
	builder->setLength(status, index, sizeof(ISC_SHORT));	\
	builder->setScale(status, index, scale);

#define FB__META_FB_SCALED_INTEGER(scale)	\
	builder->setType(status, index, SQL_LONG);	\
	builder->setLength(status, index, sizeof(ISC_LONG));	\
	builder->setScale(status, index, scale);

#define FB__META_FB_SCALED_BIGINT(scale)	\
	builder->setType(status, index, SQL_INT64);	\
	builder->setLength(status, index, sizeof(ISC_INT64));	\
	builder->setScale(status, index, scale);

#define FB__META_FB_FLOAT	\
	builder->setType(status, index, SQL_FLOAT);	\
	builder->setLength(status, index, sizeof(float));

#define FB__META_FB_DOUBLE	\
	builder->setType(status, index, SQL_DOUBLE);	\
	builder->setLength(status, index, sizeof(double));

#define FB__META_FB_DECFLOAT16	\
	builder->setType(status, index, SQL_DEC16);	\
	builder->setLength(status, index, sizeof(FB_DEC16));

#define FB__META_FB_DECFLOAT34	\
	builder->setType(status, index, SQL_DEC34);	\
	builder->setLength(status, index, sizeof(FB_DEC34));

#define FB__META_FB_BLOB	\
	builder->setType(status, index, SQL_BLOB);	\
	builder->setLength(status, index, sizeof(ISC_QUAD));

#define FB__META_FB_BOOLEAN	\
	builder->setType(status, index, SQL_BOOLEAN);	\
	builder->setLength(status, index, sizeof(FB_BOOLEAN));

#define FB__META_FB_DATE	\
	builder->setType(status, index, SQL_DATE);	\
	builder->setLength(status, index, sizeof(::Firebird::FbDate));

#define FB__META_FB_TIME	\
	builder->setType(status, index, SQL_TIME);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTime));

#define FB__META_FB_TIME_TZ	\
	builder->setType(status, index, SQL_TIME_TZ);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTimeTz));

#define FB__META_FB_TIME_TZ_EX	\
	builder->setType(status, index, SQL_TIME_TZ_EX);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTimeTzEx));

#define FB__META_FB_TIMESTAMP	\
	builder->setType(status, index, SQL_TIMESTAMP);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTimestamp));

#define FB__META_FB_TIMESTAMP_TZ	\
	builder->setType(status, index, SQL_TIMESTAMP_TZ);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTimestampTz));

#define FB__META_FB_TIMESTAMP_TZ_EX	\
	builder->setType(status, index, SQL_TIMESTAMP_TZ_EX);	\
	builder->setLength(status, index, sizeof(::Firebird::FbTimestampTzEx));

#define FB__META_FB_CHAR(len)	\
	builder->setType(status, index, SQL_TEXT);	\
	builder->setLength(status, index, len);

#define FB__META_FB_VARCHAR(len)	\
	builder->setType(status, index, SQL_VARYING);	\
	builder->setLength(status, index, len);

#define FB__META_FB_INTL_CHAR(len, charSet)	\
	builder->setType(status, index, SQL_TEXT);	\
	builder->setLength(status, index, len);	\
	builder->setCharSet(status, index, charSet);

#define FB__META_FB_INTL_VARCHAR(len, charSet)	\
	builder->setType(status, index, SQL_VARYING);	\
	builder->setLength(status, index, len);	\
	builder->setCharSet(status, index, charSet);

#define FB__META_FB_SMALLINT			FB__META_FB_SCALED_SMALLINT(0)
#define FB__META_FB_INTEGER				FB__META_FB_SCALED_INTEGER(0)
#define FB__META_FB_BIGINT				FB__META_FB_SCALED_BIGINT(0)

// Types - struct

#define FB__TYPE_FB_SCALED_SMALLINT(x)			ISC_SHORT
#define FB__TYPE_FB_SCALED_INTEGER(x)			ISC_LONG
#define FB__TYPE_FB_SCALED_BIGINT(x)			FB__INT64_ALIGNAS ISC_INT64
#define FB__TYPE_FB_SMALLINT					ISC_SHORT
#define FB__TYPE_FB_INTEGER						ISC_LONG
#define FB__TYPE_FB_BIGINT						FB__INT64_ALIGNAS ISC_INT64
#define FB__TYPE_FB_FLOAT						float
#define FB__TYPE_FB_DOUBLE						double
#define FB__TYPE_FB_DECFLOAT16					FB__INT64_ALIGNAS FB_DEC16
#define FB__TYPE_FB_DECFLOAT34					FB__INT64_ALIGNAS FB_DEC34
#define FB__TYPE_FB_BLOB						ISC_QUAD
#define FB__TYPE_FB_BOOLEAN						ISC_UCHAR
#define FB__TYPE_FB_DATE						::Firebird::FbDate
#define FB__TYPE_FB_TIME						::Firebird::FbTime
#define FB__TYPE_FB_TIME_TZ						::Firebird::FbTimeTz
#define FB__TYPE_FB_TIME_TZ_EX					::Firebird::FbTimeTzEx
#define FB__TYPE_FB_TIMESTAMP					::Firebird::FbTimestamp
#define FB__TYPE_FB_TIMESTAMP_TZ				::Firebird::FbTimestampTz
#define FB__TYPE_FB_TIMESTAMP_TZ_EX				::Firebird::FbTimestampTzEx
#define FB__TYPE_FB_CHAR(len)					::Firebird::FbChar<(len)>
#define FB__TYPE_FB_VARCHAR(len)				::Firebird::FbVarChar<(len)>
#define FB__TYPE_FB_INTL_CHAR(len, charSet)		::Firebird::FbChar<(len)>
#define FB__TYPE_FB_INTL_VARCHAR(len, charSet)	::Firebird::FbVarChar<(len)>

#define FB_TRIGGER_MESSAGE_MOVE_NAMES(name, fields)	\
	FB_TRIGGER_MESSAGE_MOVE_NAMES_I(name, 3, FB_BOOST_PP_CAT(FB_TRIGGER_MESSAGE_MOVE_NAMES_X fields, 0))

#define FB_TRIGGER_MESSAGE_MOVE_NAMES_X(x, y, z) ((x, y, z)) FB_TRIGGER_MESSAGE_MOVE_NAMES_Y
#define FB_TRIGGER_MESSAGE_MOVE_NAMES_Y(x, y, z) ((x, y, z)) FB_TRIGGER_MESSAGE_MOVE_NAMES_X
#define FB_TRIGGER_MESSAGE_MOVE_NAMES_X0
#define FB_TRIGGER_MESSAGE_MOVE_NAMES_Y0

#define FB_TRIGGER_MESSAGE_MOVE_NAMES_I(name, size, fields)	\
	FB_BOOST_PP_SEQ_FOR_EACH_I(FB_TRIGGER_MESSAGE_MOVE_NAME, size, fields)	\
	builder->truncate(status, index);	\
	index = 0;

#define FB_TRIGGER_MESSAGE_MOVE_NAME(r, _, i, xy)	\
	builder->moveNameToIndex(status, FB_BOOST_PP_TUPLE_ELEM(_, 2, xy), index++);


namespace Firebird {


template <unsigned N>
struct FbChar
{
	char str[N];
};

template <unsigned N>
struct FbVarChar
{
	ISC_USHORT length;
	char str[N];

	void set(const char* s)
	{
		size_t len = strlen(s);
		assert(len <= N);
		length = (ISC_USHORT) len;
		memcpy(str, s, (length <= N ? length : N));
	}
};

// This class has memory layout identical to ISC_DATE.
class FbDate
{
public:
	void decode(IUtil* util, unsigned* year, unsigned* month, unsigned* day) const
	{
		util->decodeDate(value, year, month, day);
	}

	unsigned getYear(IUtil* util) const
	{
		unsigned year;
		decode(util, &year, NULL, NULL);
		return year;
	}

	unsigned getMonth(IUtil* util) const
	{
		unsigned month;
		decode(util, NULL, &month, NULL);
		return month;
	}

	unsigned getDay(IUtil* util) const
	{
		unsigned day;
		decode(util, NULL, NULL, &day);
		return day;
	}

	void encode(IUtil* util, unsigned year, unsigned month, unsigned day)
	{
		value = util->encodeDate(year, month, day);
	}

public:
	FbDate& operator=(ISC_DATE& val)
	{
		*(this) = *(FbDate*) &val;
		return *this;
	}

	operator ISC_DATE&()
	{
		return *(ISC_DATE*) this;
	}

	operator const ISC_DATE&() const
	{
		return *(ISC_DATE*) this;
	}

public:
	ISC_DATE value;
};

// This class has memory layout identical to ISC_TIME.
class FbTime
{
public:
	void decode(IUtil* util, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions) const
	{
		util->decodeTime(value, hours, minutes, seconds, fractions);
	}

	unsigned getHours(IUtil* util) const
	{
		unsigned hours;
		decode(util, &hours, NULL, NULL, NULL);
		return hours;
	}

	unsigned getMinutes(IUtil* util) const
	{
		unsigned minutes;
		decode(util, NULL, &minutes, NULL, NULL);
		return minutes;
	}

	unsigned getSeconds(IUtil* util) const
	{
		unsigned seconds;
		decode(util, NULL, NULL, &seconds, NULL);
		return seconds;
	}

	unsigned getFractions(IUtil* util) const
	{
		unsigned fractions;
		decode(util, NULL, NULL, NULL, &fractions);
		return fractions;
	}

	void encode(IUtil* util, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
	{
		value = util->encodeTime(hours, minutes, seconds, fractions);
	}

public:
	FbTime& operator=(ISC_TIME& val)
	{
		*(this) = *(FbTime*) &val;
		return *this;
	}

	operator ISC_TIME&()
	{
		return *(ISC_TIME*) this;
	}

	operator const ISC_TIME&() const
	{
		return *(ISC_TIME*) this;
	}

public:
	ISC_TIME value;
};

// This class has memory layout identical to ISC_TIME_TZ.
class FbTimeTz
{
public:
	FbTimeTz& operator=(ISC_TIME_TZ& val)
	{
		*(this) = *(FbTimeTz*) &val;
		return *this;
	}

	operator ISC_TIME_TZ&()
	{
		return *(ISC_TIME_TZ*) this;
	}

	operator const ISC_TIME_TZ&() const
	{
		return *(ISC_TIME_TZ*) this;
	}

public:
	FbTime utcTime;
	ISC_USHORT timeZone;
};

// This class has memory layout identical to ISC_TIME_TZ_EX.
class FbTimeTzEx
{
public:
	FbTimeTzEx& operator=(ISC_TIME_TZ_EX& val)
	{
		*(this) = *(FbTimeTzEx*) &val;
		return *this;
	}

	operator ISC_TIME_TZ_EX&()
	{
		return *(ISC_TIME_TZ_EX*) this;
	}

	operator const ISC_TIME_TZ_EX&() const
	{
		return *(ISC_TIME_TZ_EX*) this;
	}

public:
	FbTime utcTime;
	ISC_USHORT timeZone;
	ISC_SHORT extOffset;
};

// This class has memory layout identical to ISC_TIMESTAMP.
class FbTimestamp
{
public:
	FbTimestamp& operator=(ISC_TIMESTAMP& val)
	{
		*(this) = *(FbTimestamp*) &val;
		return *this;
	}

	operator ISC_TIMESTAMP&()
	{
		return *(ISC_TIMESTAMP*) this;
	}

	operator const ISC_TIMESTAMP&() const
	{
		return *(ISC_TIMESTAMP*) this;
	}

public:
	FbDate date;
	FbTime time;
};

// This class has memory layout identical to ISC_TIMESTAMP_TZ.
class FbTimestampTz
{
public:
	FbTimestampTz& operator=(ISC_TIMESTAMP_TZ& val)
	{
		*(this) = *(FbTimestampTz*) &val;
		return *this;
	}

	operator ISC_TIMESTAMP_TZ&()
	{
		return *(ISC_TIMESTAMP_TZ*) this;
	}

	operator const ISC_TIMESTAMP_TZ&() const
	{
		return *(ISC_TIMESTAMP_TZ*) this;
	}

public:
	FbTimestamp utcTimestamp;
	ISC_USHORT timeZone;
};

// This class has memory layout identical to ISC_TIMESTAMP_TZ_EX.
class FbTimestampTzEx
{
public:
	FbTimestampTzEx& operator=(ISC_TIMESTAMP_TZ_EX& val)
	{
		*(this) = *(FbTimestampTzEx*) &val;
		return *this;
	}

	operator ISC_TIMESTAMP_TZ_EX&()
	{
		return *(ISC_TIMESTAMP_TZ_EX*) this;
	}

	operator const ISC_TIMESTAMP_TZ_EX&() const
	{
		return *(ISC_TIMESTAMP_TZ_EX*) this;
	}

public:
	FbTimestamp utcTimestamp;
	ISC_USHORT timeZone;
	ISC_SHORT extOffset;
};

class MessageDesc
{
public:
	template <typename StatusType>
	MessageDesc(IMaster* master, StatusType* status, unsigned count,
		void (*setup)(StatusType*, IMetadataBuilder*))
	{
		IMetadataBuilder* builder = master->getMetadataBuilder(status, count);

		setup(status, builder);

		metadata = builder->getMetadata(status);

		builder->release();
	}

	~MessageDesc()
	{
		metadata->release();
	}

	IMessageMetadata* getMetadata() const
	{
		return metadata;
	}

private:
	IMessageMetadata* metadata;
};


}	// namespace Firebird

#endif	// FIREBIRD_MESSAGE_H
