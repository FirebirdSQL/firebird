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
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *					Alex Peshkoff, 2017
 *					Claudio Valderrama, 2001
 */

#define FB_UDR_STATUS_TYPE ::Firebird::ThrowStatusWrapper

#include "firebird.h"

#include <ibase.h>
#include <firebird/UdrCppEngine.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_TIMEB_H
# include <sys/timeb.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <limits>

using namespace Firebird;


//------------------------------------------------------------------------------

/***
create function div (
    n1 integer,
    n2 integer
) returns double precision
    external name 'udf_compat!UC_div'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_div)
	FB_UDR_MESSAGE(InMessage,
		(FB_INTEGER, n1)
		(FB_INTEGER, n2)
	);

	FB_UDR_MESSAGE(OutMessage,
		(FB_DOUBLE, result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
		if (in->n1Null || in->n2Null)
		{
			out->resultNull = FB_TRUE;
			out->result = 0;
		}
		else
		{
			out->resultNull = FB_FALSE;
			if (in->n2)
				out->result = div(in->n1, in->n2).quot;
			else
			{
				out->result = std::numeric_limits<double>::infinity();
				ISC_STATUS_ARRAY statusVector = {isc_arg_gds, isc_arith_except,
					isc_arg_gds, isc_exception_integer_divide_by_zero, isc_arg_end};
				FbException::check(isc_exception_integer_divide_by_zero, status, statusVector);
			}
		}
	}
FB_UDR_END_FUNCTION

/***
create function frac (
    val double precision
) returns double precision
    external name 'udf_compat!UC_frac'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_frac)
	FB_UDR_MESSAGE(InMessage,
		(FB_DOUBLE, val)
	);

	FB_UDR_MESSAGE(OutMessage,
		(FB_DOUBLE, result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
		if (in->valNull)
		{
			out->resultNull = FB_TRUE;
			out->result = 0;
		}
		else
		{
			out->resultNull = FB_FALSE;
			out->result = in->val > 0 ? in->val - floor(in->val) :
				in->val < 0 ? in->val - ceil(in->val) : 0;
		}
	}
FB_UDR_END_FUNCTION


namespace
{
	enum day_format {day_short, day_long};
	const FB_SIZE_T day_len[] = {13, 53};
	const char* day_fmtstr[] = {"%a", "%A"};

	void decode_timestamp(IUtil* u, const FbTimestamp* from, tm* to, unsigned* fractions)
	{
		// decode firebird timestamp format
		memset(to, 0, sizeof(tm));
		from->date.decode(u, (unsigned*)&to->tm_year, (unsigned*)&to->tm_mon, (unsigned*)&to->tm_mday);
		to->tm_year -= 1900;
		to->tm_mon--;
		from->time.decode(u, (unsigned*)&to->tm_hour, (unsigned*)&to->tm_min, (unsigned*)&to->tm_sec, fractions);

		// set wday/yday
		time_t tt = mktime(to);
#if defined(HAVE_LOCALTIME_R)
		localtime_r(&tt, to);
#elif defined(HAVE_LOCALTIME_S)
		localtime_s(to, &tt);
#else
		error: missing thread-safe version of localtime()
#endif
	}

	template <typename VC>
	void get_DOW(IUtil* u, const FbTimestamp* v, VC* rc, const day_format df)
	{
		// decode firebird timestamp format
		tm times;
		decode_timestamp(u, v, &times, NULL);

		const int dow = times.tm_wday;
		if (dow >= 0 && dow <= 6)
		{
			FB_SIZE_T name_len = day_len[df];
			const char* name_fmt = day_fmtstr[df];
			// There should be a better way to do this than to alter the thread's locale.
			if (!strcmp(setlocale(LC_TIME, NULL), "C"))
				setlocale(LC_ALL, "");
			name_len = static_cast<FB_SIZE_T>(strftime(rc->str, name_len, name_fmt, &times));
			if (name_len)
			{
				// There's no clarity in the docs whether '\0' is counted or not; be safe.
				if (!rc->str[name_len - 1])
					--name_len;
				rc->length = name_len;
				return;
			}
		}
		rc->length = df == day_long ? 5 : 3;
		memcpy(rc->str, "ERROR", rc->length);
	}

	void encode_timestamp(IUtil* u, const tm* from, const unsigned fractions, FbTimestamp* to)
	{
		tm times = *from;

		// decode firebird timestamp format
		times.tm_year += 1900;
		times.tm_mon++;

		to->date.encode(u, times.tm_year, times.tm_mon, times.tm_mday);
		to->time.encode(u, times.tm_hour, times.tm_min, times.tm_sec, fractions);
	}
} // anonymous namespace

/***
create function dow (
    val timestamp
) returns varchar(53) character set utf8
    external name 'udf_compat!UC_dow'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_dow)
	FB_UDR_MESSAGE(InMessage,
		(FB_TIMESTAMP, val)
	);

	FB_UDR_MESSAGE(OutMessage,
		(FB_VARCHAR(53), result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
		out->resultNull = in->valNull;
		if (!out->resultNull)
			get_DOW(master->getUtilInterface(), &in->val, &out->result, day_long);
	}
FB_UDR_END_FUNCTION

/***
create function sdow (
    val timestamp
) returns varchar(13) character set utf8
    external name 'udf_compat!UC_sdow'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_sdow)
	FB_UDR_MESSAGE(InMessage,
		(FB_TIMESTAMP, val)
	);

	FB_UDR_MESSAGE(OutMessage,
		(FB_VARCHAR(13), result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
		out->resultNull = in->valNull;
		if (!out->resultNull)
			get_DOW(master->getUtilInterface(), &in->val, &out->result, day_short);
	}
FB_UDR_END_FUNCTION

/***
create function getExactTimestampUTC
	returns timestamp
    external name 'udf_compat!UC_getExactTimestampUTC'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_getExactTimestampUTC)
	FB_UDR_MESSAGE(OutMessage,
		(FB_TIMESTAMP, result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
#if defined(HAVE_GETTIMEOFDAY)
		timeval tv;
		GETTIMEOFDAY(&tv);
		const time_t seconds = tv.tv_sec;

		tm timex;
#if defined(HAVE_GMTIME_R)
		tm* times = gmtime_r(&seconds, &timex);
#else
		timeMutex.enter();
		tm* times = gmtime(&seconds);
		if (times)
		{
			// Copy to local variable before we exit the mutex.
			timex = *times;
			times = &timex;
		}
		timeMutex.leave();
#endif // gmtime_r

		if (times)
		{
			encode_timestamp(master->getUtilInterface(), times, tv.tv_usec / 100, &out->result);
			out->resultNull = false;
		}

#else // gettimeofday

		_timeb timebuffer;
		_ftime(&timebuffer);
		// gmtime uses thread local storage in NT, no need to lock threads.
		// Of course, this facility is only available in multithreaded builds.
		tm* times = gmtime(&timebuffer.time);

		if (times)
		{
			encode_timestamp(master->getUtilInterface(), times, timebuffer.millitm * 10, &out->result);
			out->resultNull = false;
		}

#endif

		else
		{
			out->resultNull = true;
			out->result.date.value = 0;
			out->result.time.value = 0;
		}
	}
FB_UDR_END_FUNCTION

/***
create function isLeapYear (
    val timestamp
) returns boolean
    external name 'udf_compat!UC_isLeapYear'
    engine udr;
***/
FB_UDR_BEGIN_FUNCTION(UC_isLeapYear)
	FB_UDR_MESSAGE(InMessage,
		(FB_TIMESTAMP, val)
	);

	FB_UDR_MESSAGE(OutMessage,
		(FB_BOOLEAN, result)
	);

	FB_UDR_EXECUTE_FUNCTION
	{
		if (in->valNull)
		{
			out->result = FB_FALSE;
			out->resultNull = true;
			return;
		}

		const int ly = in->val.date.getYear(master->getUtilInterface());
		out->result = ((ly % 4 == 0 && ly % 100 != 0) || ly % 400 == 0) ? FB_TRUE : FB_FALSE;
		out->resultNull = false;
	}
FB_UDR_END_FUNCTION

//------------------------------------------------------------------------------


// This should be used in only one of the UDR library files.
// Build must export firebird_udr_plugin function.
FB_UDR_IMPLEMENT_ENTRY_POINT
