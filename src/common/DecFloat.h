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

#include "classes/fb_string.h"

extern "C"
{
#include "../../extern/decNumber/decQuad.h"
#include "../../extern/decNumber/decDouble.h"
}

namespace Firebird {

struct DecimalStatus
{
	DecimalStatus(USHORT exc)
		: decExtFlag(exc), roundingMode(DEC_ROUND_HALF_UP)
	{ }

	USHORT decExtFlag, roundingMode;
};

struct DecimalBinding
{
	DecimalBinding()
		: bind(DEC_NATIVE), numScale(0)
	{ }

	enum Bind { DEC_NATIVE, DEC_TEXT, DEC_DOUBLE, DEC_NUMERIC };

	Bind bind;
	SCHAR numScale;
};

class Decimal64
{
	friend class Decimal128;

public:
	Decimal64 set(int value, DecimalStatus decSt, int scale);
	Decimal64 set(long value, DecimalStatus decSt, int scale);
	Decimal64 set(SINT64 value, DecimalStatus decSt, int scale);
	Decimal64 set(const char* value, DecimalStatus decSt);
	Decimal64 set(double value, DecimalStatus decSt);

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
	decDouble dec;

	void setScale(DecimalStatus decSt, int scale);
};

class Decimal128
{
	friend class Decimal64;

public:
	Decimal128 set(Decimal64 d64);
	Decimal128 set(int value, DecimalStatus decSt, int scale);
	Decimal128 set(long value, DecimalStatus decSt, int scale);
	Decimal128 set(SINT64 value, DecimalStatus decSt, int scale);
	Decimal128 set(const char* value, DecimalStatus decSt);
	Decimal128 set(double value, DecimalStatus decSt);

	Decimal128 operator=(Decimal64 d64);

	int toInteger(DecimalStatus decSt, int scale) const;
	void toString(DecimalStatus decSt, unsigned length, char* to) const;
	void toString(string& to) const;
	double toDouble(DecimalStatus decSt) const;
	SINT64 toInt64(DecimalStatus decSt, int scale) const;
	UCHAR* getBytes();
	Decimal64 toDecimal64(DecimalStatus decSt) const;
	Decimal128 abs() const;
	Decimal128 ceil(DecimalStatus decSt) const;
	Decimal128 floor(DecimalStatus decSt) const;
	Decimal128 add(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 sub(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 mul(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 div(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 neg() const;
	Decimal128 fma(DecimalStatus decSt, Decimal128 op2, Decimal128 op3) const;
	Decimal128 sqrt(DecimalStatus decSt) const;
	Decimal128 pow(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 ln(DecimalStatus decSt) const;
	Decimal128 log10(DecimalStatus decSt) const;

	int compare(DecimalStatus decSt, Decimal128 tgt) const;
	bool isInf() const;
	bool isNan() const;

	void makeKey(ULONG* key) const;
	void grabKey(ULONG* key);
	static ULONG getIndexKeyLength();
	ULONG makeIndexKey(vary* buf);

	Decimal128 quantize(DecimalStatus decSt, Decimal128 op2) const;
	Decimal128 normalize(DecimalStatus decSt) const;
	short totalOrder(Decimal128 op2) const;
	short decCompare(Decimal128 op2) const;

#ifdef DEV_BUILD
	int show();
#endif

private:
	decQuad dec;

	void setScale(DecimalStatus decSt, int scale);
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

} // namespace Firebird


#endif // FB_DECIMAL_FLOAT
