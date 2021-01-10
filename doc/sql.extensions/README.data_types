---------------------
Native SQL data types
---------------------


BIGINT (FB 1.5)
--------------

  Function:
    SQL99-compliant exact numeric type.

  Author:
    Dmitry Yemanov <yemanov@yandex.ru>

  Syntax rules:
    BIGINT

  Storage:
    64-bit, signed

  Example(s):
    1. DECLARE VARIABLE VAR1 BIGINT;
    2. CREATE TABLE TABLE1 (FIELD1 BIGINT);

  Note(s):
    1. Available in Dialect 3 only.
    2. Quote from the SQL-99 specification:

	SMALLINT, INTEGER, and BIGINT specify the data type exact numeric,
	with scale of 0 (zero) and binary or decimal precision. The choice
	of binary versus decimal precision is implementation-defined, but
	the same radix shall be chosen for all three data types. The precision
	of SMALLINT shall be less than or equal to the precision of INTEGER,
	and the precision of BIGINT shall be greater than or equal to the
	precision of INTEGER.


BOOLEAN (FB 3.0)
--------------

  Function:
    SQL2008-compliant boolean type.

  Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>

  Syntax rules:
    BOOLEAN

  Storage:
    8-bit

  Example(s):
    1. DECLARE VARIABLE VAR1 BOOLEAN = TRUE;
    2. CREATE TABLE TABLE1 (FIELD1 BOOLEAN);

  Note(s):
    1. Quote from the SQL-2008 specification:

       The data type boolean comprises the distinct truth values TRUE and FALSE. Unless prohibited
       by a NOT NULL constraint, the boolean data type also supports the truth value UNKNOWN as the
       null value. This specification does not make a distinction between the null value of the
       boolean data type and the truth value Unknown that is the result of an SQL <predicate>,
       <search condition>, or <boolean value expression>; they may be used interchangeably to mean
       exactly the same thing.

    2. Represented in the API with the FB_BOOLEAN type and FB_TRUE and FB_FALSE constants.

    3. The value TRUE is greater than the value FALSE.

    4. Non-booleans values are not implicitly convertible to boolean in boolean-specific expressions
       like predicates and arguments for operators NOT, AND, OR and IS.

    5. It's allowed to test booleans without compare with TRUE or FALSE. For example,
       "field1 OR field2" and "NOT field1" are valid expressions. It's also allowed to compare with
       others operators, including the new IS operator: "field1 IS FALSE".


BINARY, VARBINARY, BINARY VARYING (FB 4.0)
------------------------------------------

  Function:
    Alias for CHAR, VARCHAR, VARYING CHAR with CHARACTER SET OCTETS.

  Author:
    Dimitry Sibiryakov

  Syntax rules:
    BINARY[(length)]
    VARBINARY(length)
    BINARY VARYING(length)

  Example(s):
    1. DECLARE VARIABLE VAR1 VARBINARY(10);
    2. CREATE TABLE TABLE1 (FIELD1 BINARY(16),
                            FIELD2 VARBINARY(100),
                            FIELD3 BINARY VARYING(1000));

  Note(s):
    1. If length is omitted for type BINARY, it is considered to be 1.
    2. Can be distinguished from text types by value 1 in RDB$FIELD_SUB_TYPE.
    3. Character set is set to OCTETS for backward compatibility.
    4. In API are similar to corresponding text types, getSubType() returns 0.


DECFLOAT (FB 4.0)
--------------

  Function:
    DB2-compliant numeric type. DECFLOAT precisely (unlike FLOAT or DOUBLE PRECISION that provide
    binary approximation) stores decimal values being therefore ideal choice for business appli-
    cations. Firebird according to IEEE standard has both 16- and 34-digit decimal float encodings.
    All intermediate calculations are performed with 34-digit values.

  Author:
    Alex Peshkoff <peshkoff@mail.ru>

  Syntax rules:
    DECFLOAT
    DECFLOAT(16)
    DECFLOAT(34)

  Storage:
    64-bit / 128-bit, format according to IEEE 754 Decimal64/Decimal128

  Example(s):
    1. DECLARE VARIABLE VAR1 DECFLOAT(34);
    2. CREATE TABLE TABLE1 (FIELD1 DECFLOAT(16));

  Note(s):
    1.	If no precision has been specified in the type declaration, the precision is 34.
    2.	A number of standard functions can be used with DECFLOAT datatype. It is:
    	ABS, CEILING, EXP, FLOOR, LN, LOG, LOG10, POWER, SIGN, SQRT.
    	Agregate functions SUM, AVG, MAX and MIN also work with DECFLOAT data.
    	All statistics aggregates (like but not limited to STDDEV or CORR) work with DECFLOAT data.

    3.	Firebird supports four functions, specially designed to support DECFLOAT data:
    	- COMPARE_DECFLOAT - compares two DECFLOAT values to be equal, different or unordered.
    		Returns SMALLINT value which can be as follows:
    			0 - values are equal
    			1 - first value is less than second
    			2 - first value is greater than second
    			3 - values unordered (i.e. one or both is NAN / SNAN)
    		Unlike comparison operators ('<', '=', '>', etc.) comparison is exact - i.e.
    		COMPARE_DECFLOAT(2.17, 2.170) returns 2, not 0.
    	- NORMALIZE_DECFLOAT - has single DECFLOAT argument returned in it's simplest form.	That
    		means that for any nonzero value trailing zero are removed with appropriate correction
    		of an exponent. For example NORMALIZE_DECFLOAT(12.00) returns 12 and
    		NORMALIZE_DECFLOAT(120) returns 1.2E+2.
    	- QUANTIZE - has two DECFLOAT arguments. The returned value is first argument scaled using
    		second value as a pattern. For example QUANTIZE(1234, 9.999) returns 1234.000.
    	- TOTALORDER - compares two DECFLOAT values including any special value. The comparison is
    		exact. Returns SMALLINT value which can be as follows:
    			-1 - first value is less than second
    			 0 - values are equal
    			 1 - first value is greater than second
    		DECFLOAT values are ordered as follows:
    			-nan < -snan < -inf < -0.1 < -0.10 < -0 < 0 < 0.10 < 0.1 < inf < snan < nan

	4.	Firebird supports new session control operator SET DECFLOAT. It has following forms:
			SET DECFLOAT ROUND <mode> - controls rounding mode used in operations with DECFLOAT
				values. Valid modes are: CEILING (towards +infinity), UP (away from 0), HALF_UP
				(to nearest, if equidistant - up), HALF_EVEN (to nearest, if equidistant - ensure
				last digit in the result to be even), HALF_DOWN (to nearest, if equidistant - down),
				DOWN (towards 0), FLOOR (towards -infinity), REROUND (up if digit to be rounded is
				0 or 5, down in other cases). HALF_UP rounding is used by default.
				The initial configuration may be specified with DPB isc_dpb_decfloat_round followed
				by a string with its value (case does not matter).

			SET DECFLOAT TRAPS TO <comma-separated traps list - may be empty> - controls which
				exceptional conditions cause a trap. Valid traps are: Division_by_zero, Inexact,
				Invalid_operation, Overflow and Underflow. By default traps are set to:
				Division_by_zero, Invalid_operation, Overflow.
				The initial configuration may be specified with DPB isc_dpb_decfloat_traps followed
				by a comma-separated string with its value (case does not matter; using a single
				optional space after commas).

	5.	The length of DECFLOAT literals are limited to 1024 characters. For longer values, you will
			need to use the scientific notation. For example, the 0.0<1020 zeroes>11 cannot be used
			as a literal, instead you can use the equivalent in scientific notation: 1.1E-1022.
			Similarly 10<1022 zeroes>0 can be presented as 1.0E1024.


Enhancement in precision of calculations with NUMERIC/DECIMAL (FB 4.0)
--------------

  Function:
    Maximum precision of NUMERIC and DECIMAL data types is increased to 38 digits.

  Author:
    Alex Peshkoff <peshkoff@mail.ru>

  Syntax rules:
    INT128
    NUMERIC ( P {, N} )
    DECIMAL ( P {, N} )
		where P is precision (P <= 38, was limited prior with 18 digits) and N is optional number
		of digits after decimal separator (as before).

  Storage:
    128-bit signed integer.

  Example(s):
    1. DECLARE VARIABLE VAR1 DECIMAL(25);
    2. CREATE TABLE TABLE1 (FIELD1 NUMERIC(38, 19));
    3. CREATE PROCEDURE PROC1 (PAR1 INT128) AS BEGIN END;

  Note(s):
    Numerics with precision less than 19 digits use SMALLINT, INTEGER, BIGINT or DOUBLE PRECISION
    as base datatype depending upon number of digits and dialect. When precision is between 19 and
    38 digits 128-bit integer is used for it. Actual precision is always increased to 38 digits.
    For complex calculations such digits are casted (internally) to DECFLOAT(34) and the result of
    various math (log, exp, etc.) and aggregate functions using high precision numeric argument is
    DECFLOAT(34).

