/*
 *	PROGRAM:		Integer 128 type.
 *	MODULE:			Int128.cpp
 *	DESCRIPTION:	Big integer support.
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
 *  Copyright (c) 2019 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "Int128.h"

#include "StatusArg.h"
#include "gen/iberror.h"
#include "status.h"
#include "DecFloat.h"

#include <limits>

#include <stdlib.h>
#include <string.h>
#include <float.h>

using namespace Firebird;

namespace {

const CInt128 i64max(MAX_SINT64), i64min(MIN_SINT64);
const double p2_32 = 4294967296.0;
const I128limit i128limit;
const CInt128 minus1(-1);


} // anonymous namespace



namespace Firebird {

Int128 Int128::set(SLONG value, int scale)
{
	v = ttmath::sint(value);
	setScale(scale);
	return *this;
}

Int128 Int128::set(SINT64 value, int scale)
{
#ifdef TTMATH_PLATFORM32
	v = ttmath::slint(value);
#else
	v = ttmath::sint(value);
#endif
	setScale(scale);
	return *this;
}

Int128 Int128::set(const char* value)
{
// This is simplified method - it does not perform all what's needed for CVT_decompose
	v.FromString(value);
	return *this;
}

Int128 Int128::set(double value)
{
	bool sgn = false;
	if (value < 0.0)
	{
		value = -value;
		sgn = true;
	}

	double parts[4];
	for (int i = 0; i < 4; ++i)
	{
		parts[i] = value;
		value /= p2_32;
	}
	fb_assert(value < 1.0);

	unsigned dwords[4];
	value = 0.0;
	for (int i = 4; i--;)
	{
		dwords[i] = (parts[i] - value);
		value += p2_32 * dwords[i];
	}

	setTable32(dwords);
	if (sgn)
		v.ChangeSign();

	return *this;
}

Int128 Int128::set(DecimalStatus decSt, Decimal128 value)
{
	static CDecimal128 quant(1);
	value = value.quantize(decSt, quant);

	Decimal128::BCD bcd;
	value.getBcd(&bcd);
	fb_assert(bcd.exp == 0);

	v.SetZero();
	for (unsigned b = 0; b < sizeof(bcd.bcd); ++b)
	{
		v.MulInt(10);
		v.AddInt(bcd.bcd[b]);
	}
	if (bcd.sign < 0)
		v.ChangeSign();

	return *this;
}

void Int128::setScale(int scale)
{
	if (scale > 0)
	{
		ttmath::sint rem = 0;
		while (scale--)
			v.DivInt(10, scale == 0 ? &rem : nullptr);

		if (rem > 4)
			v++;
		else if (rem < -4)
			v--;
	}
	else if (scale < 0)
	{
		while (scale++) {
			if (v > i128limit.v || v < -i128limit.v)
				(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range)).raise();
			v.MulInt(10);
		}
	}
}

int Int128::toInteger(int scale) const
{
	Int128 tmp(*this);
	tmp.setScale(scale);
	int rc;
	if (tmp.v.ToInt(rc))
		overflow();
	return rc;
}

void Int128::toString(int scale, unsigned length, char* to) const
{
	string buffer;
	toString(scale, buffer);
	if (buffer.length() + 1 > length)
	{
		(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
			Arg::Gds(isc_trunc_limits) << Arg::Num(length) << Arg::Num(buffer.length() + 1)).raise();
	}
	buffer.copyTo(to, length);
}

void Int128::toString(int scale, string& to) const
{
	v.ToStringBase(to);
	bool sgn = to[0] == '-';
	if (sgn)
		to.erase(0, 1);

	if (scale)
	{
		if (scale < -38 || scale > 4)
		{
			string tmp;
			tmp.printf("E%d", scale);
			to += tmp;
		}
		else if (scale > 0)
		{
			string tmp(scale, '0');
			to += tmp;
		}
		else
		{
			unsigned posScale = -scale;
			if (posScale > to.length())
			{
				string tmp(posScale - to.length(), '0');
				to.insert(0, tmp);
			}
			if (posScale == to.length())
			{
				to.insert(0, "0.");
			}
			else
				to.insert(to.length() - posScale, ".");
		}
	}

	if (sgn)
		to.insert(0, "-");
}

SINT64 Int128::toInt64(int scale) const
{
	Int128 tmp(*this);
	tmp.setScale(scale);
	if (tmp.v < i64min.v || tmp.v > i64max.v)
		overflow();

	unsigned dwords[4];
	tmp.getTable32(dwords);
	SINT64 rc = int(dwords[1]);
	rc <<= 32;
	rc += dwords[0];

	return rc;
}

double Int128::toDouble() const
{
	unsigned dwords[4];
	getTable32(dwords);
	double rc = int(dwords[3]);
	for (int i = 3; i--;)
	{
		rc *= p2_32;
		rc += dwords[i];
	}

	return rc;
}

int Int128::compare(Int128 tgt) const
{
	return v < tgt.v ? -1 : v > tgt.v ? 1 : 0;
}

Int128 Int128::abs() const
{
	Int128 rc(*this);
	if (rc.v.Abs())
		overflow();
	return rc;
}

Int128 Int128::neg() const
{
	Int128 rc(*this);
	if (rc.v.ChangeSign())
		overflow();
	return rc;
}

Int128 Int128::add(Int128 op2) const
{
	Int128 rc(*this);
	if (rc.v.Add(op2.v))
		overflow();
	return rc;
}

Int128 Int128::sub(Int128 op2) const
{
	Int128 rc(*this);
	if (rc.v.Sub(op2.v))
		overflow();
	return rc;
}

Int128 Int128::mul(Int128 op2) const
{
	Int128 rc(*this);
	if (rc.v.Mul(op2.v))
		overflow();
	return rc;
}

Int128 Int128::div(Int128 op2, int scale) const
{
	if (compare(MIN_Int128) == 0 && op2.compare(minus1) == 0)
		Arg::Gds(isc_exception_integer_overflow).raise();

	static const CInt128 MIN_BY10(MIN_Int128 / 10);
	static const CInt128 MAX_BY10(MAX_Int128 / 10);

	// Scale op1 by as many of the needed powers of 10 as possible without an overflow.
	CInt128 op1(*this);
	int sign1 = op1.sign();
	while ((scale < 0) && (sign1 >= 0 ? op1.compare(MAX_BY10) <= 0 : op1.compare(MIN_BY10) >= 0))
	{
		op1 *= 10;
		++scale;
	}

	// Scale op2 shifting it to the right as long as only zeroes are thrown away.
	CInt128 tmp(op2);
	while (scale < 0)
	{
		ttmath::sint rem = 0;
		tmp.v.DivInt(10, &rem);
		if (rem)
			break;
		op2 = tmp;
		++scale;
	}

	if (op1.v.Div(op2.v))
		zerodivide();

	op1.setScale(scale);
	return op1;
}

Int128 Int128::mod(Int128 op2) const
{
	Int128 tmp(*this);
	Int128 rc;
	if (tmp.v.Div(op2.v, rc.v))
		zerodivide();
	return rc;
}

int Int128::sign() const
{
	return v.IsSign() ? -1 : v.IsZero() ? 0 : 1;
}

UCHAR* Int128::getBytes()
{
	return (UCHAR*)(v.table);
}

void Int128::getTable32(unsigned* dwords) const
{
	static_assert((sizeof(v.table[0]) == 4) || (sizeof(v.table[0]) == 8),
		"Unsupported size of integer in ttmath");

	if (sizeof(v.table[0]) == 4)
	{
		for (int i = 0; i < 4; ++i)
			dwords[i] = v.table[i];
	}
	else if (sizeof(v.table[0]) == 8)
	{
		for (int i = 0; i < 2; ++i)
		{
			dwords[i * 2] = v.table[i] & 0xFFFFFFFF;
			dwords[i * 2 + 1] = (v.table[i] >> 32) & 0xFFFFFFFF;
		}
	}
}

void Int128::setTable32(const unsigned* dwords)
{
	static_assert((sizeof(v.table[0]) == 4) || (sizeof(v.table[0]) == 8),
		"Unsupported size of integer in ttmath");

	if (sizeof(v.table[0]) == 4)
	{
		for (int i = 0; i < 4; ++i)
			v.table[i] = dwords[i];
	}
	else if (sizeof(v.table[0]) == 8)
	{
		for (int i = 0; i < 2; ++i)
		{
			v.table[i] = dwords[i * 2 + 1];
			v.table[i] <<= 32;
			v.table[i] += dwords[i * 2];
		}
	}
}

Int128 Int128::operator&=(FB_UINT64 mask)
{
	v.table[0] &= mask;
	unsigned i = 1;
	if (sizeof(v.table[0]) == 4)
	{
		i = 2;
		v.table[1] &= (mask >> 32);
	}

	for (; i < FB_NELEM(v.table); ++i)
		v.table[i] = 0;
	return *this;
}

Int128 Int128::operator&=(ULONG mask)
{
	v.table[0] &= mask;

	for (unsigned i = 1; i < FB_NELEM(v.table); ++i)
		v.table[i] = 0;
	return *this;
}

Int128 Int128::operator/(unsigned value) const
{
	Int128 rc(*this);
	rc.v.DivInt(value);
	return rc;
}

Int128 Int128::operator-() const
{
	return neg();
}

Int128 Int128::operator+=(unsigned value)
{
	v.AddInt(value);
	return *this;
}

Int128 Int128::operator*=(unsigned value)
{
	v.MulInt(value);
	return *this;
}

bool Int128::operator>(Int128 value) const
{
	return v > value.v;
}

bool Int128::operator>=(Int128 value) const
{
	return v >= value.v;
}

bool Int128::operator==(Int128 value) const
{
	return v == value.v;
}

void Int128::zerodivide()
{
	(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_integer_divide_by_zero)).raise();
}

void Int128::overflow()
{
	(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_integer_overflow)).raise();
}

#ifdef DEV_BUILD
const char* Int128::show()
{
	static char to[64];
	toString(0, sizeof(to), to);
	return to;
}
#endif

CInt128::CInt128(SINT64 value)
{
	set(value, 0);
}

CInt128::CInt128(minmax mm)
{
	switch(mm)
	{
	case MkMax:
		v.SetMax();
		break;
	case MkMin:
		v.SetMin();
		break;
	}
}

CInt128 MIN_Int128(CInt128::MkMin);
CInt128 MAX_Int128(CInt128::MkMax);

} // namespace Firebird
