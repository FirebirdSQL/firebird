/*
 *	PROGRAM:		Decimal 64 & 128 type.
 *	MODULE:			DecFloat.h
 *	DESCRIPTION:	Floating point with decimal exponent.
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
 *  Copyright (c) 2016 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_DECIMAL_FLOAT
#define FB_DECIMAL_FLOAT

#include "firebird/Interface.h"
#include "fb_exception.h"

#include <string.h>

#include "classes/fb_string.h"

extern "C"
{
#include "../../extern/decNumber/decQuad.h"
#include "../../extern/decNumber/decDouble.h"
}

namespace Firebird {

static const USHORT FB_DEC_Errors =
	DEC_IEEE_754_Division_by_zero |
	DEC_IEEE_754_Invalid_operation |
	DEC_IEEE_754_Overflow;

struct DecimalStatus
{
	DecimalStatus(USHORT exc)
		: decExtFlag(exc), roundingMode(DEC_ROUND_HALF_UP)
	{ }

	static const DecimalStatus DEFAULT;

	USHORT decExtFlag, roundingMode;
};

struct DecimalBinding
{
	DecimalBinding()
		: bind(DEC_NATIVE), numScale(0)
	{ }

	static const DecimalBinding DEFAULT;

	enum Bind { DEC_NATIVE, DEC_TEXT, DEC_DOUBLE, DEC_NUMERIC };

	Bind bind;
	SCHAR numScale;
};


class DecimalFixed;

class Decimal64
{
	friend class Decimal128;
	friend class DecimalFixed;
	friend class Decimal128Base;

public:
#if SIZEOF_LONG < 8
	Decimal64 set(int value, DecimalStatus decSt, int scale);
#endif
	Decimal64 set(SLONG value, DecimalStatus decSt, int scale);
	Decimal64 set(SINT64 value, DecimalStatus decSt, int scale);
	Decimal64 set(const char* value, DecimalStatus decSt);
	Decimal64 set(double value, DecimalStatus decSt);
	Decimal64 set(DecimalFixed value, DecimalStatus decSt, int scale);

	UCHAR* getBytes();
	Decimal64 abs() const;
	Decimal64 ceil(DecimalStatus decSt) const;
	Decimal64 floor(DecimalStatus decSt) const;
	Decimal64 neg() const;

	void toString(DecimalStatus decSt, unsigned length, char* to) const;
	void toString(string& to) const;

	int compare(DecimalStatus decSt, Decimal64 tgt) const;
	bool isInf() const;
	bool isNan() const;
	int sign() const;

	void makeKey(ULONG* key) const;
	void grabKey(ULONG* key);

	Decimal64 quantize(DecimalStatus decSt, Decimal64 op2) const;
	Decimal64 normalize(DecimalStatus decSt) const;
	short totalOrder(Decimal64 op2) const;
	short decCompare(Decimal64 op2) const;

#ifdef DEV_BUILD
	int show();
#endif

private:
	void setScale(DecimalStatus decSt, int scale);

	decDouble dec;
};

class Decimal128Base
{
	friend class Decimal128;
	friend class DecimalFixed;

public:
	double toDouble(DecimalStatus decSt) const;
	Decimal64 toDecimal64(DecimalStatus decSt) const;

	UCHAR* getBytes();
	int compare(DecimalStatus decSt, Decimal128Base tgt) const;

	void setScale(DecimalStatus decSt, int scale);

	bool isInf() const;
	bool isNan() const;
	int sign() const;

	void makeKey(ULONG* key) const;
	void grabKey(ULONG* key);
	static ULONG getIndexKeyLength();
	ULONG makeIndexKey(vary* buf);

#ifdef DEV_BUILD
	int show();
#endif

private:
	decQuad dec;
};

class Decimal128 : public Decimal128Base
{
	friend class Decimal64;

public:
	Decimal128 set(Decimal64 d64);
#if SIZEOF_LONG < 8
	Decimal128 set(int value, DecimalStatus decSt, int scale);
#endif
	Decimal128 set(SLONG value, DecimalStatus decSt, int scale);
	Decimal128 set(SINT64 value, DecimalStatus decSt, int scale);
	Decimal128 set(const char* value, DecimalStatus decSt);
	Decimal128 set(double value, DecimalStatus decSt);
	Decimal128 set(DecimalFixed value, DecimalStatus decSt, int scale);

	Decimal128 operator=(Decimal64 d64);

	void toString(DecimalStatus decSt, unsigned length, char* to) const;
	void toString(string& to) const;
	int toInteger(DecimalStatus decSt, int scale) const;
	SINT64 toInt64(DecimalStatus decSt, int scale) const;
	Decimal128 ceil(DecimalStatus decSt) const;
	Decimal128 floor(DecimalStatus decSt) const;
	Decimal128 abs() const;
	Decimal128 neg() const;
	Decimal128 add(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 sub(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 mul(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 div(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 mod(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 fma(DecimalStatus decSt, Decimal128 op2, Decimal128 op3) const;

	Decimal128 sqrt(DecimalStatus decSt) const;
	Decimal128 pow(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 ln(DecimalStatus decSt) const;
	Decimal128 log10(DecimalStatus decSt) const;

	Decimal128 quantize(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 normalize(DecimalStatus decSt) const;
	short totalOrder(Decimal128 op2) const;
	short decCompare(Decimal128 op2) const;

private:
	Decimal128 operator=(Decimal128Base d128b)
	{
		memcpy(&dec, &d128b.dec, sizeof(dec));
		return *this;
	}
};

class CDecimal128 : public Decimal128
{
public:
	CDecimal128(double value, DecimalStatus decSt)
	{
		set(value, decSt);
	}

	CDecimal128(SINT64 value, DecimalStatus decSt)
	{
		set(value, decSt, 0);
	}

	CDecimal128(int value)
	{
		set(value, DecimalStatus(0), 0);
	}
};

class DecimalFixed : public Decimal128Base
{
public:
#if SIZEOF_LONG < 8
	DecimalFixed set(int value)
	{
		return set(SLONG(value));
	}
#endif
	DecimalFixed set(SLONG value);
	DecimalFixed set(SINT64 value);
	DecimalFixed set(const char* value, int scale, DecimalStatus decSt);
	DecimalFixed set(double value, int scale, DecimalStatus decSt);

	int toInteger(DecimalStatus decSt) const;
	SINT64 toInt64(DecimalStatus decSt) const;
	void toString(DecimalStatus decSt, int scale, unsigned length, char* to) const;
	void toString(DecimalStatus decSt, int scale, string& to) const;

	DecimalFixed abs() const;
	DecimalFixed neg() const;
	DecimalFixed add(DecimalStatus decSt, DecimalFixed op2) const;
	DecimalFixed sub(DecimalStatus decSt, DecimalFixed op2) const;
	DecimalFixed mul(DecimalStatus decSt, DecimalFixed op2) const;
	DecimalFixed div(DecimalStatus decSt, DecimalFixed op2, int scale) const;
	DecimalFixed mod(DecimalStatus decSt, DecimalFixed op2) const;

	DecimalFixed operator=(Decimal128Base d128b)
	{
		memcpy(&dec, &d128b.dec, sizeof(dec));
		return *this;
	}

	void exactInt(DecimalStatus decSt, int scale);	// rescale & make it integer after conversions

private:
	Decimal128 scaled128(DecimalStatus decSt, int scale) const;
};

} // namespace Firebird


#endif // FB_DECIMAL_FLOAT
