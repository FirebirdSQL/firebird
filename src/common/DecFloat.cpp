/*
 *	PROGRAM:		Decimal 64 & 128 type.
 *	MODULE:			DecFloat.cpp
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

#include "firebird.h"
#include "DecFloat.h"
#include "Int128.h"

#include "StatusArg.h"
#include "iberror.h"
#include "status.h"

#include <limits>

extern "C"
{
#include "../../extern/decNumber/decimal128.h"
#include "../../extern/decNumber/decimal64.h"
#include "../../extern/decNumber/decNumber.h"
}

#include <stdlib.h>
#include <string.h>
#include <float.h>

using namespace Firebird;

const DecimalStatus DecimalStatus::DEFAULT(FB_DEC_Errors);
const NumericBinding NumericBinding::DEFAULT;

namespace {

struct Dec2fb
{
	USHORT decError;
	ISC_STATUS fbError;
	ISC_STATUS fbDoubleError;
};

Dec2fb dec2fb[] = {
	{ DEC_IEEE_754_Division_by_zero, isc_decfloat_divide_by_zero, isc_exception_float_divide_by_zero },
	{ DEC_IEEE_754_Inexact, isc_decfloat_inexact_result, isc_exception_float_inexact_result },
	{ DEC_IEEE_754_Invalid_operation, isc_decfloat_invalid_operation, isc_exception_float_invalid_operand },
	{ DEC_IEEE_754_Overflow, isc_decfloat_overflow, isc_exception_float_overflow },
	{ DEC_IEEE_754_Underflow, isc_decfloat_underflow, isc_exception_float_underflow },
	{ 0, 0 }
};

class DecimalContext : public decContext
{
public:
	DecimalContext(const Decimal64*, DecimalStatus ds, bool dblErr = false)
		: decSt(ds), dblError(dblErr)
	{
		init(DEC_INIT_DECIMAL64);
	}

	DecimalContext(const Decimal128*, DecimalStatus ds, bool dblErr = false)
		: decSt(ds), dblError(dblErr)
	{
		init(DEC_INIT_DECIMAL128);
	}

	~DecimalContext() NOEXCEPT_ARG(false)
	{
		// Typically exceptions should better be not thrown from destructors.
		// But in our case there should never be any exception raised inside
		// Decimal64/128 functions - C library never throw, i.e. dtor will
		// be never called due to exception processing.
		// Therefore checking status in destructor is safe.
		checkForExceptions();
	}

	void checkForExceptions()
	{
		USHORT unmaskedExceptions = decSt.decExtFlag & decContextGetStatus(this);
		if (!unmaskedExceptions)
			return;

		decContextZeroStatus(this);

		for (Dec2fb* e = dec2fb; e->decError; ++e)
		{
			// Arg::Gds(isc_arith_except) as first vector element ?
			if (e->decError & unmaskedExceptions)
				Arg::Gds(dblError ? e->fbDoubleError : e->fbError).raise();
		}
	}

private:
	DecimalStatus decSt;
	bool dblError;			// Raise double proecision related errors instead decfloat

	void init(int kind)
	{
		decContextDefault(this, kind);
		fb_assert(decSt.roundingMode < USHORT(DEC_ROUND_MAX));
		enum rounding rMode = rounding(decSt.roundingMode);
		decContextSetRounding(this, rMode);
		traps = 0;		// do not raise SIGFPE
	}
};

const CDecimal128 pow2_32("4294967296", DecimalStatus(0));
const CDecimal128 pow2_64("18446744073709551616", DecimalStatus(0));
const CDecimal128 dmax(DBL_MAX, DecimalStatus(0)), dmin(-DBL_MAX, DecimalStatus(0));
const CDecimal128 dzup(DBL_MIN, DecimalStatus(0)), dzlw(-DBL_MIN, DecimalStatus(0));
// be careful with order of initialization: pow2_32 is used in i64max constructor
const CDecimal128 i64max(MAX_SINT64, DecimalStatus(0)), i64min(MIN_SINT64, DecimalStatus(0));
const CDecimal128 c1(1);

unsigned digits(const unsigned pMax, unsigned char* const coeff, int& exp)
{
	for (unsigned i = 0; i < pMax; ++i)
	{
		if (coeff[i])
		{
			if (i)
			{
				memmove(coeff, &coeff[i], pMax - i);
				memset(&coeff[pMax - i], 0, i);
				exp -= i;
			}

			i = pMax - i;

			while (!coeff[i - 1])
			{
				fb_assert(i > 0);
				--i;
			}

			return i;
		}
	}

	return 0;
}

// offsets down from MAX_SLONG
const ULONG OFF_inf = 3;
const ULONG OFF_snan = 2;
const ULONG OFF_nan = 1;

void make(ULONG* key,
	const unsigned pMax, const int bias, const unsigned decSize,
	unsigned char* coeff, int sign, int exp, decClass cl)
{
	ULONG offset = 0;

	// special cases
	switch(cl)
	{
	case DEC_CLASS_SNAN:
		offset = OFF_snan;
		break;
	case DEC_CLASS_QNAN:
		offset = OFF_nan;
		break;
	case DEC_CLASS_NEG_INF:
		offset = OFF_inf;
		sign = DECFLOAT_Sign;
		break;
	case DEC_CLASS_POS_INF:
		offset = OFF_inf;
		sign = 0;
		break;
	default:
		break;
	}

	if (offset)		// special value
	{
		unsigned dig = decSize / sizeof(ULONG);
		fb_assert(!(decSize % sizeof(ULONG)));
		while (dig--)
			*key++ = sign ? ~MAX_SLONG : MAX_SLONG;
		*key = MAX_SLONG - offset;
		if (sign)
			*key = ~*key;
		return;
	}

	// normalize coeff & exponent
	unsigned dig = digits(pMax, coeff, exp);

	// exponent bias and sign
	if (!dig)
	{
		exp = 0;
		sign = 0;
	}
	else
	{
		exp += (bias + 2);
		if (sign)
			exp = -exp;
	}

	*key++ = exp;

	// convert to SLONG
	fb_assert(pMax / 9 < decSize / sizeof(ULONG));
	memset(key, 0, decSize);

	for (unsigned i = 0; i < pMax; ++i)
	{
		unsigned c = i / 9;
		key[c] *= 10;
		key[c] += (sign ? 9 - coeff[i] : coeff[i]);
	}
}

void grab(ULONG* key,
	const unsigned pMax, const int bias, const unsigned decSize,
	unsigned char* bcd, int& sign, int& exp, decClass& cl)
{
	exp = *key++;
	sign = 0;

	if (exp == MAX_SLONG || exp == ~MAX_SLONG)	// special value
	{
		unsigned dig = decSize / sizeof(ULONG);
		fb_assert(!(decSize % sizeof(ULONG)));
		ULONG offset = key[dig - 1];

		if (exp == ~MAX_SLONG)
		{
			sign = DECFLOAT_Sign;
			offset = ~offset;
		}

		offset = MAX_SLONG - offset;
		switch(offset)
		{
		case OFF_inf:
			cl = sign ? DEC_CLASS_NEG_INF : DEC_CLASS_POS_INF;
			break;
		case OFF_snan:
			cl = DEC_CLASS_SNAN;
			break;
		case OFF_nan:
			cl = DEC_CLASS_QNAN;
			break;
		default:
			(Arg::Gds(isc_random) << "Invalid class of special decfloat value in sort key").raise();
		}

		return;
	}

	// normal value
	// here we ignore differences in class for SUBNORMAL, ZERO and NEG
	cl = DEC_CLASS_POS_NORMAL;

	// parse exp
	if (exp < 0)
	{
		sign = DECFLOAT_Sign;
		exp = -exp;
	}

	if (exp != 0)
		exp -= (bias + 2);

	// convert from SLONG
	for (int i = pMax; i--;)
	{
		int c = i / 9;
		bcd[i] = key[c] % 10;
		key[c] /= 10;

		if (sign)
			bcd[i] = 9 - bcd[i];
	}

	// normalize
	for (unsigned i = pMax; i--; )
	{
		if (bcd[i])
		{
			if (i < pMax - 1)
			{
				memmove(&bcd[pMax - 1 - i], bcd, i + 1);
				memset(bcd, 0, pMax - 1 - i);
				exp += (pMax - 1 - i);
			}

			break;
		}
	}
}

void setSpecial(decNumber* number, decClass cl, int sign)
{
	decNumberZero(number);

	switch(cl)
	{
	case DEC_CLASS_SNAN:
		number->bits |= DECSNAN;
		break;

	case DEC_CLASS_QNAN:
		number->bits |= DECNAN;
		break;

	case DEC_CLASS_NEG_INF:
	case DEC_CLASS_POS_INF:
		number->bits |= DECINF;
		break;

	default:
		fb_assert(false);
		break;
	}

	if (sign)
		number->bits |= DECNEG;
}

} // anonymous namespace



namespace Firebird {

void Decimal64::setScale(DecimalStatus decSt, int scale)
{
	if (scale)
	{
		DecimalContext context(this, decSt);
		scale += decDoubleGetExponent(&dec);
		decDoubleSetExponent(&dec, &context, scale);
	}
}

#if SIZEOF_LONG < 8
Decimal64 Decimal64::set(int value, DecimalStatus decSt, int scale)
{
	return set(SLONG(value), decSt, scale);
}
#endif

Decimal64 Decimal64::set(SLONG value, DecimalStatus decSt, int scale)
{
	decDoubleFromInt32(&dec, value);
	setScale(decSt, -scale);

	return *this;
}

Decimal64 Decimal64::set(Int128 value, DecimalStatus decSt, int scale)
{
	Decimal128 tmp;
	tmp.set(value, decSt, scale);
	*this = tmp.toDecimal64(decSt);

	return *this;
}

Decimal64 Decimal64::set(SINT64 value, DecimalStatus decSt, int scale)
{
	{
		char s[30];		// for sure enough for int64
		sprintf(s, "%" SQUADFORMAT, value);
		DecimalContext context(this, decSt);
		decDoubleFromString(&dec, s, &context);
	}

	setScale(decSt, -scale);

	return *this;
}

Decimal64 Decimal64::set(const char* value, DecimalStatus decSt)
{
	DecimalContext context(this, decSt);
	decDoubleFromString(&dec, value, &context);

	return *this;
}

Decimal64 Decimal64::set(double value, DecimalStatus decSt)
{
	char s[50];
	sprintf(s, "%.016e", value);
	DecimalContext context(this, decSt);
	decDoubleFromString(&dec, s, &context);

	return *this;
}

void Decimal64::toString(DecimalStatus decSt, unsigned length, char* to) const
{
	DecimalContext context(this, decSt);

	if (length)
	{
		--length;
		char s[IDecFloat16::STRING_SIZE];
		memset(s, 0, sizeof(s));
		decDoubleToString(&dec, s);

		if (strlen(s) > length)
			decContextSetStatus(&context, DEC_Invalid_operation);
		else
			length = strlen(s);

		memcpy(to, s, length + 1);
	}
	else
		decContextSetStatus(&context, DEC_Invalid_operation);
}

void Decimal64::toString(string& to) const
{
	to.grow(IDecFloat16::STRING_SIZE);
	toString(DecimalStatus(0), to.length(), to.begin());		// provide long enough string, i.e. no traps
	to.recalculate_length();
}

UCHAR* Decimal64::getBytes()
{
	return dec.bytes;
}

Decimal64 Decimal64::abs() const
{
	Decimal64 rc;
	decDoubleCopyAbs(&rc.dec, &dec);
	return rc;
}

Decimal64 Decimal64::ceil(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal64 rc;
	decDoubleToIntegralValue(&rc.dec, &dec, &context, DEC_ROUND_CEILING);
	return rc;
}

Decimal64 Decimal64::floor(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal64 rc;
	decDoubleToIntegralValue(&rc.dec, &dec, &context, DEC_ROUND_FLOOR);
	return rc;
}

int Decimal64::compare(DecimalStatus decSt, Decimal64 tgt) const
{
	DecimalStatus cmpStatus(decSt);
	cmpStatus.decExtFlag &= ~DEC_IEEE_754_Invalid_operation;
	DecimalContext context(this, cmpStatus);
	decDouble r;
	decDoubleCompare(&r, &dec, &tgt.dec, &context);
	return decDoubleToInt32(&r, &context, DEC_ROUND_HALF_UP);
}

bool Decimal64::isInf() const
{
	switch (decDoubleClass(&dec))
	{
	case DEC_CLASS_NEG_INF:
	case DEC_CLASS_POS_INF:
		return true;
	}

	return false;
}

bool Decimal64::isNan() const
{
	switch (decDoubleClass(&dec))
	{
    case DEC_CLASS_SNAN:
    case DEC_CLASS_QNAN:
		return true;
	}

	return false;
}

int Decimal64::sign() const
{
	if (decDoubleIsZero(&dec))
		return 0;
	if (decDoubleIsSigned(&dec))
		return -1;
	return 1;
}

#ifdef DEV_BUILD
int Decimal64::show()
{
	decDoubleShow(&dec, "");
	return 0;
}
#endif

Decimal64 Decimal64::neg() const
{
	Decimal64 rc;
	decDoubleCopyNegate(&rc.dec, &dec);
	return rc;
}

void Decimal64::makeKey(ULONG* key) const
{
	unsigned char coeff[DECDOUBLE_Pmax];
	int sign = decDoubleGetCoefficient(&dec, coeff);
	int exp = decDoubleGetExponent(&dec);
	decClass dc = decDoubleClass(&dec);

	make(key, DECDOUBLE_Pmax, DECDOUBLE_Bias, sizeof(dec), coeff, sign, exp, dc);
}

void Decimal64::grabKey(ULONG* key)
{
	int exp, sign;
	unsigned char bcd[DECDOUBLE_Pmax];
	decClass cl;

	grab(key, DECDOUBLE_Pmax, DECDOUBLE_Bias, sizeof(dec), bcd, sign, exp, cl);

	if (cl == DEC_CLASS_POS_NORMAL)
		decDoubleFromBCD(&dec, exp, bcd, sign);
	else
	{
		DecimalContext context(this, 0);
    	decNumber number;

    	setSpecial(&number, cl, sign);
		decDoubleFromNumber(&dec, &number, &context);
	}
}

Decimal64 Decimal64::quantize(DecimalStatus decSt, Decimal64 op2) const
{
	DecimalContext context(this, decSt);
	Decimal64 rc;
	decDoubleQuantize(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal64 Decimal64::normalize(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal64 rc;
	decDoubleReduce(&rc.dec, &dec, &context);
	return rc;
}

short Decimal64::totalOrder(Decimal64 op2) const
{
	decDouble r;
	decDoubleCompareTotal(&r, &dec, &op2.dec);
	fb_assert(!decDoubleIsNaN(&r));

	DecimalContext context2(this, 0);
	return decDoubleToInt32(&r, &context2, DEC_ROUND_HALF_UP);
}

/*
 *	decCompare() implements SQL function COMPARE_DECFLOAT() which has non-traditional return values.
 *	COMPARE_DECFLOAT (X, Y)
 *		0 - X == Y
 *		1 - X < Y
 *		2 - X > Y
 *		3 - values unordered
 */

short Decimal64::decCompare(Decimal64 op2) const
{
	if (decDoubleIsNaN(&dec) || decDoubleIsNaN(&op2.dec))
		return 3;

	switch (totalOrder(op2))
	{
	case -1:
		return 1;
	case 0:
		return 0;
	case 1:
		return 2;
	default:
		fb_assert(false);
	}

	// warning silencer
	return 3;
}

Decimal128 Decimal128::set(Decimal64 d64)
{
	decDoubleToWider(&d64.dec, &dec);

	return *this;
}

#if SIZEOF_LONG < 8
Decimal128 Decimal128::set(int value, DecimalStatus decSt, int scale)
{
	return set(SLONG(value), decSt, scale);
}
#endif

Decimal128 Decimal128::set(SLONG value, DecimalStatus decSt, int scale)
{
	decQuadFromInt32(&dec, value);
	setScale(decSt, -scale);

	return *this;
}

Decimal128 Decimal128::set(Int128 value, DecimalStatus decSt, int scale)
{
	unsigned dwords[4];
	value.getTable32(dwords);

	DecimalContext context(this, decSt);
	decQuadFromInt32(&dec, dwords[3]);
	for (int i = 3; i--; )
	{
		decQuad dw;
		decQuadFromUInt32(&dw, dwords[i]);
		decQuadFMA(&dec, &dec, &pow2_32.dec, &dw, &context);
	}

	setScale(decSt, -scale);
	return *this;
}

Decimal128 Decimal128::set(SINT64 value, DecimalStatus decSt, int scale)
{
	{
		int high = value >> 32;
		unsigned low = value & 0xFFFFFFFF;

		DecimalContext context(this, decSt);
		decQuad up, down;
		decQuadFromInt32(&up, high);
		decQuadFromUInt32(&down, low);
		decQuadFMA(&dec, &up, &pow2_32.dec, &down, &context);
	}

	setScale(decSt, -scale);

	return *this;
}

Decimal128 Decimal128::set(const char* value, DecimalStatus decSt)
{
	DecimalContext context(this, decSt);
	decQuadFromString(&dec, value, &context);

	return *this;
}

Decimal128 Decimal128::set(double value, DecimalStatus decSt)
{
	char s[50];
	sprintf(s, "%.016e", value);
	DecimalContext context(this, decSt);
	decQuadFromString(&dec, s, &context);

	return *this;
}

Decimal128 Decimal128::operator=(Decimal64 d64)
{
	decDoubleToWider(&d64.dec, &dec);
	return *this;
}

int Decimal128::toInteger(DecimalStatus decSt, int scale) const
{
	Decimal128 tmp(*this);
	tmp.setScale(decSt, -scale);
	DecimalContext context(this, decSt);
	enum rounding rMode = decContextGetRounding(&context);
	return decQuadToInt32(&tmp.dec, &context, rMode);
}

void Decimal128::toString(DecimalStatus decSt, unsigned length, char* to) const
{
	DecimalContext context(this, decSt);

	if (length)
	{
		--length;
		char s[IDecFloat34::STRING_SIZE];
		memset(s, 0, sizeof(s));
		decQuadToString(&dec, s);

		if (strlen(s) > length)
			decContextSetStatus(&context, DEC_Invalid_operation);
		else
			length = strlen(s);

		memcpy(to, s, length + 1);
	}
	else
		decContextSetStatus(&context, DEC_Invalid_operation);
}

void Decimal128::toString(string& to) const
{
	to.grow(IDecFloat34::STRING_SIZE);
	toString(DecimalStatus(0), to.length(), to.begin());		// provide long enough string, i.e. no traps
	to.recalculate_length();
}

double Decimal128::toDouble(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt, true);

	if (compare(decSt, dmin) < 0)
	{
		decContextSetStatus(&context, DEC_Overflow);
		return std::numeric_limits<double>::has_infinity ? -std::numeric_limits<double>::infinity() : 0.0;
	}
	if (compare(decSt, dmax) > 0)
	{
		decContextSetStatus(&context, DEC_Overflow);
		return std::numeric_limits<double>::has_infinity ? std::numeric_limits<double>::infinity() : 0.0;
	}

	if ((!decQuadIsZero(&dec)) && compare(decSt, dzlw) > 0 && compare(decSt, dzup) < 0)
	{
		decContextSetStatus(&context, DEC_Underflow);
		return 0.0;
	}

	char s[IDecFloat34::STRING_SIZE];
	decQuadToString(&dec, s);
	return atof(s);
}

SINT64 Decimal128::toInt64(DecimalStatus decSt, int scale) const
{
	static CDecimal128 quant(1);

	Decimal128 wrk(*this);
	wrk.setScale(decSt, -scale);
	wrk = wrk.quantize(decSt, quant);

	if (wrk.compare(decSt, i64min) < 0 || wrk.compare(decSt, i64max) > 0)
	{
		DecimalContext context(this, decSt, true);
		decContextSetStatus(&context, DEC_Invalid_operation);
		return 0;	// in case of no trap on invalid operation
	}

	unsigned char coeff[DECQUAD_Pmax];
	int sign = decQuadGetCoefficient(&wrk.dec, coeff);
	SINT64 rc = 0;

	for (int i = 0; i < DECQUAD_Pmax; ++i)
	{
		rc *= 10;
		if (sign)
			rc -= coeff[i];
		else
			rc += coeff[i];
	}

	return rc;
}

UCHAR* Decimal128::getBytes()
{
	return dec.bytes;
}

Decimal64 Decimal128::toDecimal64(DecimalStatus decSt) const
{
	Decimal64 rc;
	DecimalContext context(this, decSt);
	decDoubleFromWider(&rc.dec, &dec, &context);
	return rc;
}

void Decimal128::setScale(DecimalStatus decSt, int scale)
{
	if (scale)
	{
		DecimalContext context(this, decSt);
		scale += decQuadGetExponent(&dec);
		decQuadSetExponent(&dec, &context, scale);
	}
}

int Decimal128::compare(DecimalStatus decSt, Decimal128 tgt) const
{
	DecimalContext context(this, decSt);
	decQuad r;
	decQuadCompare(&r, &dec, &tgt.dec, &context);
	return decQuadToInt32(&r, &context, DEC_ROUND_HALF_UP);
}

bool Decimal128::isInf() const
{
	switch(decQuadClass(&dec))
	{
	case DEC_CLASS_NEG_INF:
	case DEC_CLASS_POS_INF:
		return true;
	}

	return false;
}

bool Decimal128::isNan() const
{
	switch(decQuadClass(&dec))
	{
    case DEC_CLASS_SNAN:
    case DEC_CLASS_QNAN:
		return true;
	}

	return false;
}

int Decimal128::sign() const
{
	if (decQuadIsZero(&dec))
		return 0;
	if (decQuadIsSigned(&dec))
		return -1;
	return 1;
}

Decimal128 Decimal128::ceil(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadToIntegralValue(&rc.dec, &dec, &context, DEC_ROUND_CEILING);
	return rc;
}

Decimal128 Decimal128::floor(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadToIntegralValue(&rc.dec, &dec, &context, DEC_ROUND_FLOOR);
	return rc;
}

Decimal128 Decimal128::modf(DecimalStatus decSt, Decimal128* ipart) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc, ip;
	decQuadToIntegralValue(&ip.dec, &dec, &context, DEC_ROUND_DOWN);
	decQuadSubtract(&rc.dec, &dec, &ip.dec, &context);
	*ipart = ip;
	return rc;
}

#ifdef DEV_BUILD
int Decimal128::show()
{
	decQuadShow(&dec, "");
	return 0;
}
#endif

Decimal128 Decimal128::abs() const
{
	Decimal128 rc;
	decQuadCopyAbs(&rc.dec, &dec);
	return rc;
}

Decimal128 Decimal128::neg() const
{
	Decimal128 rc;
	decQuadCopyNegate(&rc.dec, &dec);
	return rc;
}

Decimal128 Decimal128::add(DecimalStatus decSt, Decimal128 op2) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadAdd(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal128 Decimal128::sub(DecimalStatus decSt, Decimal128 op2) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadSubtract(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal128 Decimal128::mul(DecimalStatus decSt, Decimal128 op2) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadMultiply(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal128 Decimal128::div(DecimalStatus decSt, Decimal128 op2) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadDivide(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal128 Decimal128::fma(DecimalStatus decSt, Decimal128 op2, Decimal128 op3) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadFMA(&rc.dec, &op2.dec, &op3.dec, &dec, &context);
	return rc;
}

Decimal128 Decimal128::sqrt(DecimalStatus decSt) const
{
	decNumber dn;
	decQuadToNumber(&dec, &dn);

	DecimalContext context(this, decSt);
	decNumberSquareRoot(&dn, &dn, &context);

	Decimal128 rc;
	decQuadFromNumber(&rc.dec, &dn, &context);
	return rc;
}

Decimal128 Decimal128::pow(DecimalStatus decSt, Decimal128 op2) const
{
	decNumber dn, dn2;
	decQuadToNumber(&dec, &dn);
	decQuadToNumber(&op2.dec, &dn2);

	DecimalContext context(this, decSt);
	decNumberPower(&dn, &dn, &dn2, &context);

	Decimal128 rc;
	decQuadFromNumber(&rc.dec, &dn, &context);
	return rc;
}

Decimal128 Decimal128::ln(DecimalStatus decSt) const
{
	decNumber dn;
	decQuadToNumber(&dec, &dn);

	DecimalContext context(this, decSt);
	decNumberLn(&dn, &dn, &context);

	Decimal128 rc;
	decQuadFromNumber(&rc.dec, &dn, &context);
	return rc;
}

Decimal128 Decimal128::log10(DecimalStatus decSt) const
{
	decNumber dn;
	decQuadToNumber(&dec, &dn);

	DecimalContext context(this, decSt);
	decNumberLog10(&dn, &dn, &context);

	Decimal128 rc;
	decQuadFromNumber(&rc.dec, &dn, &context);
	return rc;
}

void Decimal128::makeKey(ULONG* key) const
{
	unsigned char coeff[DECQUAD_Pmax];
	int sign = decQuadGetCoefficient(&dec, coeff);
	int exp = decQuadGetExponent(&dec);
	decClass dc = decQuadClass(&dec);

	make(key, DECQUAD_Pmax, DECQUAD_Bias, sizeof(dec), coeff, sign, exp, dc);
}

void Decimal128::grabKey(ULONG* key)
{
	int exp, sign;
	unsigned char bcd[DECQUAD_Pmax];
	decClass cl;

	grab(key, DECQUAD_Pmax, DECQUAD_Bias, sizeof(dec), bcd, sign, exp, cl);

	if (cl == DEC_CLASS_POS_NORMAL)
		decQuadFromBCD(&dec, exp, bcd, sign);
	else
	{
		DecimalContext context(this, 0);
    	decNumber number;

    	setSpecial(&number, cl, sign);
		decQuadFromNumber(&dec, &number, &context);

#ifdef NOT_USED_OR_REPLACED
		// decQuad / decDouble API for setting special values appears VERY unstable for FB4
		// if sometimes it becomes better approximately following code may be used

    	decQuadZero(&dec);
		switch(cl)
		{
		case DEC_CLASS_SNAN:
			DFWORD(&dec, 0) = DECFLOAT_sNaN;
			break;
		case DEC_CLASS_QNAN:
			DFWORD(&dec, 0) = DECFLOAT_qNaN;
			break;
		case DEC_CLASS_NEG_INF:
		case DEC_CLASS_POS_INF:
			DFWORD(&dec, 0) = DECFLOAT_Inf;
			show();
			break;
		default:
			fb_assert(false);
			break;
		}
		if (sign)
			DFWORD(&dec, 0) |= DECFLOAT_Sign;
#endif //NOT_USED_OR_REPLACED
	}
}

ULONG Decimal128::makeIndexKey(vary* buf)
{
	unsigned char coeff[DECQUAD_Pmax + 2];
	int sign = decQuadGetCoefficient(&dec, coeff);
	int exp = decQuadGetExponent(&dec);
	const int bias = DECQUAD_Bias;
	const unsigned pMax = DECQUAD_Pmax;

	// normalize coeff & exponent
	unsigned dig = digits(pMax, coeff, exp);

	// exponent bias and sign
	exp += (bias + 1);
	if (!dig)
		exp = 0;
	if (sign)
		exp = -exp;
	exp += 2 * (bias + 1);	// make it positive
	fb_assert(exp >= 0 && exp < 64 * 1024);

	// encode exp
	char* k = buf->vary_string;
	*k++ = exp >> 8;
	*k++ = exp & 0xff;

	// invert negative
	unsigned char* const end = &coeff[dig];
	if (sign && dig)
	{
		fb_assert(end[-1]);
		--end[-1];

		for (unsigned char* p = coeff; p < end; ++p)
			*p = 9 - *p;
	}

	// Some 0's in the end - caller, do not forget to reserve additional space on stack
	end[0] = end[1] = 0;

	// Avoid bad data in k in case when coeff is zero
	*k = 0;

	// Shifts for moving 10-bit values to bytes buffer
	struct ShiftTable { UCHAR rshift, lshift; };
	static ShiftTable table[4] =
	{
		{ 2, 6 },
		{ 4, 4 },
		{ 6, 2 },
		{ 8, 0 }
	};

	// compress coeff - 3 decimal digits (999) per 10 bits (1023)
	unsigned char* p = coeff;
	for (ShiftTable* t = table; p < end; p += 3)
	{
		USHORT val = p[0] * 100 + p[1] * 10 + p[2];
		fb_assert(val < 1000);	// 1024, 10 bit
		*k |= (val >> t->rshift);
		++k;
		*k = (val << t->lshift);
		if (!t->lshift)
		{
			++k;
			*k = 0;
			t = table;
		}
		else
			++t;
	}
	if (*k)
		++k;

	// done
	buf->vary_length = k - buf->vary_string;
	return buf->vary_length;
}

Decimal128 Decimal128::quantize(DecimalStatus decSt, Decimal128 op2) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadQuantize(&rc.dec, &dec, &op2.dec, &context);
	return rc;
}

Decimal128 Decimal128::normalize(DecimalStatus decSt) const
{
	DecimalContext context(this, decSt);
	Decimal128 rc;
	decQuadReduce(&rc.dec, &dec, &context);
	return rc;
}

short Decimal128::totalOrder(Decimal128 op2) const
{
	decQuad r;
	decQuadCompareTotal(&r, &dec, &op2.dec);
	fb_assert(!decQuadIsNaN(&r));

	DecimalContext context2(this, 0);
	return decQuadToInt32(&r, &context2, DEC_ROUND_HALF_UP);
}

/*
 *	decCompare() implements SQL function COMPARE_DECFLOAT() which has non-traditional return values.
 *	COMPARE_DECFLOAT (X, Y)
 *		0 - X == Y
 *		1 - X < Y
 *		2 - X > Y
 *		3 - values unordered
 */

short Decimal128::decCompare(Decimal128 op2) const
{
	if (decQuadIsNaN(&dec) || decQuadIsNaN(&op2.dec))
		return 3;

	switch (totalOrder(op2))
	{
	case -1:
		return 1;
	case 0:
		return 0;
	case 1:
		return 2;
	default:
		fb_assert(false);
	}

	// warning silencer
	return 3;
}

void Decimal128::getBcd(BCD* bcd) const
{
	bcd->sign = decQuadToBCD(&dec, &bcd->exp, bcd->bcd);
}

} // namespace Firebird
