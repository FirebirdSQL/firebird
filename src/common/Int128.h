/*
 *	PROGRAM:		Integer 128 type.
 *	MODULE:			Int128.h
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

#ifndef FB_INT128
#define FB_INT128

#include "firebird/Interface.h"
#include "fb_exception.h"

#include <string.h>

#include "classes/fb_string.h"

#ifdef FB_USE_ABSEIL_INT128

#include "absl/numeric/int128.h"

namespace Firebird {

class Decimal64;
class Decimal128;
struct DecimalStatus;

class Int128 //: public Decimal128Base
{
public:
#if SIZEOF_LONG < 8
	Int128 set(int value, int scale)
	{
		return set(SLONG(value), scale);
	}
#endif

	Int128 set(SLONG value, int scale)
	{
		v = value;
		setScale(scale);
		return *this;
	}

	Int128 set(SINT64 value, int scale)
	{
		v = value;
		setScale(scale);
		return *this;
	}

	Int128 set(double value)
	{
		v = absl::int128(value);
		return *this;
	}

	Int128 set(DecimalStatus decSt, Decimal128 value);

	Int128 set(Int128 value)
	{
		v = value.v;
		return *this;
	}

	Int128 operator=(SINT64 value)
	{
		set(value, 0);
		return *this;
	}

#ifdef DEV_BUILD
	const char* show();
#endif

	int toInteger(int scale) const
	{
		Int128 tmp(*this);
		tmp.setScale(scale);
		int rc = int(tmp.v);
		if (tmp.v != rc)
			overflow();
		return rc;
	}

	SINT64 toInt64(int scale) const
	{
		Int128 tmp(*this);
		tmp.setScale(scale);
		SINT64 rc = SINT64(tmp.v);
		if (tmp.v != rc)
			overflow();
		return rc;
	}

	void toString(int scale, unsigned length, char* to) const;
	void toString(int scale, string& to) const;

	double toDouble() const
	{
		return double(v);
	}

	Int128 operator&=(FB_UINT64 mask)
	{
		v &= mask;
		return *this;
	}

	Int128 operator&=(ULONG mask)
	{
		v &= mask;
		return *this;
	}

	Int128 operator-() const
	{
		Int128 rc;
		rc.v = -v;
		return rc;
	}

	Int128 operator/(unsigned value) const
	{
		Int128 rc;
		rc.v = v / value;
		return rc;
	}

	Int128 operator+=(unsigned value)
	{
		v += value;
		return *this;
	}

	Int128 operator-=(unsigned value)
	{
		v -= value;
		return *this;
	}

	Int128 operator*=(unsigned value)
	{
		v *= value;
		return *this;
	}

	Int128 operator<<(int value) const
	{
		Int128 rc;
		rc.v = v << value;
		return rc;
	}

	Int128 operator>>(int value) const
	{
		Int128 rc;
		rc.v = v >> value;
		return rc;
	}

	int compare(Int128 tgt) const
	{
		return v < tgt.v ? -1 : v > tgt.v ? 1 : 0;
	}

	bool operator>(Int128 value) const
	{
		return v > value.v;
	}

	bool operator>=(Int128 value) const
	{
		return v >= value.v;
	}

	bool operator==(Int128 value) const
	{
		return v == value.v;
	}

	bool operator!=(Int128 value) const
	{
		return v != value.v;
	}

	Int128 operator&=(Int128 value)
	{
		v &= value.v;
		return *this;
	}

	Int128 operator|=(Int128 value)
	{
		v |= value.v;
		return *this;
	}

	Int128 operator^=(Int128 value)
	{
		v ^= value.v;
		return *this;
	}

	Int128 operator~() const
	{
		Int128 rc;
		rc.v = ~v;
		return rc;
	}

	int sign() const
	{
		return v < 0 ? -1 : v == 0 ? 0 : 1;
	}

	Int128 abs() const;
	Int128 neg() const;

	Int128 add(Int128 op2) const
	{
		Int128 rc;
		rc.v = v + op2.v;

		// see comment ArithmeticNode::add2()
		if (sign() == op2.sign() && op2.sign() != rc.sign())
			overflow();

		return rc;
	}

	Int128 sub(Int128 op2) const
	{
		Int128 rc;
		rc.v = v - op2.v;

		// see comment ArithmeticNode::add2()
		if (sign() != op2.sign() && op2.sign() == rc.sign())
			overflow();

		return rc;
	}

	Int128 mul(Int128 op2) const
	{
		Int128 rc;
		rc.v = v * op2.v;

		if (rc.v / v != op2.v)
			overflow();

		return rc;
	}

	Int128 div(Int128 op2, int scale) const;

	Int128 mod(Int128 op2) const
	{
		if (op2.v == 0)
			zerodivide();

		Int128 rc;
		rc.v = v % op2.v;
		return rc;
	}

	// returns internal data in per-32bit form
	void getTable32(unsigned* dwords) const
	{
		absl::int128 vv = v;
		for (int i = 0; i < 4; ++i)
		{
			dwords[i] = unsigned(vv);
			vv >>= 32;
		}
	}

	void setScale(int scale);

	UCHAR* getBytes()
	{
		return (UCHAR*)(&v); 
	}

protected:
	absl::int128 v;

	static void overflow();
	static void zerodivide();

	Int128 set(const char* value);
};

class CInt128 : public Int128
{
public:
	enum minmax {MkMax, MkMin};

	CInt128(SINT64 value);
	CInt128(minmax mm);
	CInt128(const Int128& value)
	{
		set(value);
	}
};

extern CInt128 MAX_Int128, MIN_Int128;

class I128limit : public Int128
{
public:
	I128limit()
	{
		v = 1;
		for (int i = 0; i < 126; ++i)
			v *= 2;
		v *= 5;
	}
};

} // namespace Firebird

#else // FB_USE_ABSEIL_INT128

#include "../../extern/ttmath/ttmath.h"

namespace Firebird {

class Decimal64;
class Decimal128;
struct DecimalStatus;

class Int128 //: public Decimal128Base
{
public:
#if SIZEOF_LONG < 8
	Int128 set(int value, int scale)
	{
		return set(SLONG(value), scale);
	}
#endif
	Int128 set(SLONG value, int scale);
	Int128 set(SINT64 value, int scale);
	Int128 set(double value);
	Int128 set(DecimalStatus decSt, Decimal128 value);
	Int128 set(Int128 value)
	{
		v = value.v;
		return *this;
	}

	Int128 operator=(SINT64 value)
	{
		set(value, 0);
		return *this;
	}

#ifdef DEV_BUILD
	const char* show();
#endif

	int toInteger(int scale) const;
	SINT64 toInt64(int scale) const;
	void toString(int scale, unsigned length, char* to) const;
	void toString(int scale, string& to) const;
	double toDouble() const;

	Int128 operator&=(FB_UINT64 mask);
	Int128 operator&=(ULONG mask);

	int compare(Int128 tgt) const
	{
		return v < tgt.v ? -1 : v > tgt.v ? 1 : 0;
	}

	Int128 operator/ (unsigned value) const
	{
		Int128 rc (*this);
		rc.v.DivInt (value);
		return rc;
	}

	Int128 operator<< (int value) const
	{
		Int128 rc (*this);
		rc.v <<= value;
		return rc;
	}

	Int128 operator>> (int value) const
	{
		Int128 rc (*this);
		rc.v >>= value;
		return rc;
	}

	Int128 operator&= (Int128 value)
	{
		v &= value.v;
		return *this;
	}

	Int128 operator|= (Int128 value)
	{
		v |= value.v;
		return *this;
	}

	Int128 operator^= (Int128 value)
	{
		v ^= value.v;
		return *this;
	}

	Int128 operator~ () const
	{
		Int128 rc (*this);
		rc.v.BitNot ();
		return rc;
	}

	Int128 operator- () const
	{
		return neg ();
	}

	Int128 operator+= (unsigned value)
	{
		v.AddInt (value);
		return *this;
	}

	Int128 operator-= (unsigned value)
	{
		v.SubInt (value);
		return *this;
	}

	Int128 operator*= (unsigned value)
	{
		v.MulInt (value);
		return *this;
	}

	bool operator> (Int128 value) const
	{
		return v > value.v;
	}

	bool operator>= (Int128 value) const
	{
		return v >= value.v;
	}

	bool operator== (Int128 value) const
	{
		return v == value.v;
	}

	bool operator!= (Int128 value) const
	{
		return v != value.v;
	}

	int sign() const
	{
		return v.IsSign() ? -1 : v.IsZero() ? 0 : 1;
	}

	Int128 abs() const
	{
		Int128 rc(*this);
		if (rc.v.Abs())
			overflow();
		return rc;
	}

	Int128 neg() const
	{
		Int128 rc(*this);
		if (rc.v.ChangeSign())
			overflow();
		return rc;
	}

	Int128 add(Int128 op2) const
	{
		Int128 rc(*this);
		if (rc.v.Add(op2.v))
			overflow();
		return rc;
	}

	Int128 sub(Int128 op2) const
	{
		Int128 rc(*this);
		if (rc.v.Sub(op2.v))
			overflow();
		return rc;
	}

	Int128 mul(Int128 op2) const;
	Int128 div(Int128 op2, int scale) const;

	Int128 mod(Int128 op2) const
	{
		Int128 tmp(*this);
		Int128 rc;
		if (tmp.v.Div(op2.v, rc.v))
			zerodivide();
		return rc;
	}

	void getTable32(unsigned* dwords) const;		// internal data in per-32bit form
	void setTable32(const unsigned* dwords);
	void setScale(int scale);

	UCHAR* getBytes()
	{
		return (UCHAR*)(v.table);
	}

protected:
	ttmath::Int<TTMATH_BITS(128)> v;

	static void overflow();
	static void zerodivide();

	Int128 set(const char* value);
};

class CInt128 : public Int128
{
public:
	enum minmax {MkMax, MkMin};

	CInt128(SINT64 value);
	CInt128(minmax mm);
	CInt128(const Int128& value)
	{
		set(value);
	}
};

extern CInt128 MAX_Int128, MIN_Int128;

class I128limit : public Int128
{
public:
	I128limit()
	{
		v.SetOne();
		for (int i = 0; i < 126; ++i)
			v.MulInt(2);
		v.DivInt(5);
	}
};

} // namespace Firebird

#endif // FB_USE_ABSEIL_INT128

#endif // FB_INT128
