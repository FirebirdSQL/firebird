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
#include "classes/MetaName.h"

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
	Int128 operator-() const;
	Int128 operator/(unsigned value) const;
	Int128 operator+=(unsigned value);
	Int128 operator*=(unsigned value);

	int compare(Int128 tgt) const;
	bool operator>(Int128 value) const;
	bool operator>=(Int128 value) const;
	bool operator==(Int128 value) const;
	int sign() const;

	Int128 abs() const;
	Int128 neg() const;
	Int128 add(Int128 op2) const;
	Int128 sub(Int128 op2) const;
	Int128 mul(Int128 op2) const;
	Int128 div(Int128 op2, int scale) const;
	Int128 mod(Int128 op2) const;

	void getTable32(unsigned* dwords) const;		// internal data in per-32bit form
	void setTable32(const unsigned* dwords);
	void setScale(int scale);
	UCHAR* getBytes();

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


#endif // FB_INT128
