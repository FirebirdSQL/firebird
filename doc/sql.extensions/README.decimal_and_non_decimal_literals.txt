==============================================
Decimal integer literals
Non-decimal integer literals (SQL:2023 T661)
Underscores in numeric literals (SQL:2023 T662)
==============================================

Supports unsigned hexadecimal integers, unsigned octal integers, and unsigned binary integers.
Also support for underscores in numeric and non-decimal literals

Authors:
    Alexey Chudaykin <chudaykinalex@gmail.com>

Syntax rules:

<plus sign> ::=
    + !! U+002B

<minus sign> ::=
    - !! U+002D

<period> ::=
    . !! U+002E

<underscore> ::=
    _ !! U+005F

<hexit> ::=
    0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F | a | b | c | d | e | f

<octal digit> ::=
    0 | 1 | 2 | 3 | 4 | 5 | 6 | 7

<binary digit> ::=
    0 | 1

<signed numeric literal> ::=
    [ <sign> ] <unsigned numeric literal>

<unsigned numeric literal> ::=
    <exact numeric literal> | <approximate numeric literal>

<exact numeric literal> ::=
    <unsigned integer> | <unsigned decimal integer> <period> [ <unsigned decimal integer> ] |
        <period> <unsigned decimal integer>

<sign> ::=
    <plus sign> | <minus sign>

<approximate numeric literal> ::=
    <mantissa> E <exponent>

<mantissa> ::=
    <unsigned decimal integer> [ <period> [ <unsigned decimal integer> ] ] |
        <period> <unsigned decimal integer>

<exponent> ::=
    <signed decimal integer>

<signed integer> ::=
    [ <sign> ] <unsigned integer>

<signed decimal integer> ::=
    [ <sign> ] <unsigned decimal integer>

<unsigned integer> ::=
    <unsigned decimal integer> | <unsigned hexadecimal integer> | <unsigned octal integer> |
        <unsigned binary integer>

<unsigned decimal integer> ::=
    <digit> [ { [ <underscore> ] <digit> }... ]

<unsigned hexadecimal integer> ::=
    0X { [ <underscore> ] <hexit> }...

<unsigned octal integer> ::=
    0O { [ <underscore> ] <octal digit> }...

<unsigned binary integer> ::=
    0B { [ <underscore> ] <binary digit> }...

The letters X, O, B and E may be written in either case.

Notes (non-decimal integer literals):
    1. The standard allows non-decimal literals in the mantissa of an exponential number entry.
       For example (0xAAAe10), but there may be a conflict (0xEEE10). Therefore, non-decimal
       literals are not allowed in the mantissa (see <mantissa> above): 0xAAAe10 is the
       hexadecimal integer 0xAAAE10 = 11185680.
    2. The <unsigned hexadecimal integer> value is a numeric value defined by applying the usual
       mathematical interpretation of positional hexadecimal notation to a string that is an
       unsigned hexadecimal integer. Similarly for an unsigned octal integer and an
       unsigned binary integer.
    3. To represent negative values, place a minus sign in front of an unsigned hexadecimal literal.
       Similarly for an unsigned octal integer and an unsigned binary integer.
    4. There is no non-decimal literal of type SMALLINT: even 0x1 evaluates to INTEGER. However,
       a value within the range 0x0000 (decimal zero) to 0x7FFF (decimal 32767) is converted to
       SMALLINT transparently when it is assigned to a SMALLINT column, variable or parameter.
       Similarly for an unsigned octal integer and an unsigned binary integer.
    5. The data type depends on the value of the literal, not on the number of digits
       (leading zeros do not matter):
           0 .. 0x7FFFFFFF                                     - INTEGER;
           0x80000000 .. 0x7FFFFFFFFFFFFFFF                    - BIGINT;
           0x8000000000000000 .. 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF - INT128.
       A greater value raises an error. Since the literal itself is unsigned, the minimal
       INT128 value cannot be written as -0x80000000000000000000000000000000, use the decimal
       literal -170141183460469231731687303715884105728 instead.
    6. Compatibility. Previously (see README.hex_literals) a hexadecimal literal was a signed
       two's complement value, and its data type depended on the number of <hexit>. Now the value
       is never negative (see note 2) and the data type depends on the value (see note 5):
           0xF0000000         was -268435456 (INTEGER), now 4026531840 (BIGINT);
           0xFFFFFFFFFFFFFFFF was -1 (BIGINT),          now 18446744073709551615 (INT128).
       Use a minus sign to write negative values, e.g. -0x10000000 instead of 0xF0000000.

Notes (decimal literals):
    1. Data types of decimal literals are not changed by this feature. Underscores do not affect
       the data type and the scale: 1_000.5 is NUMERIC(18, 1), 12_345_678_901_234_567_890 is INT128.

Notes (underscores in numeric literals):
    1. Limitations for non-decimal integer literals:
        1.1. It is considered unacceptable for there to be two or more consecutive underscores;
        1.2. Underscores are not allowed after the last character.
       An underscore is allowed right after the prefix: 0x_FF.
    2. Limitations for decimal literals:
        2.1. Underscores before the first character and after the last character are not allowed;
        2.2. It is considered unacceptable for there to be two or more consecutive underscores;
        2.3. Underscores are not permitted before or after the <period> symbol;
        2.4. Underscores are not allowed before or after the <E> character and the exponent sign.

Examples (non-decimal integer literals):
    1. Unsigned binary integer:
        1.1. select  0b11010100, 0B11010100 from rdb$database;  --> 212;
        1.2. select  0b0000000  from rdb$database;  --> 0;
        1.3. select  -0b11010100, -0B11010100 from rdb$database; --> -212.
    2. Unsigned octal integer:
        2.1. select  0o12345670, 0O12345670 from rdb$database; --> 2739128;
        2.2. select  0o00000000 from rdb$database; --> 0;
        2.3. select  -0o12345670, -0O12345670 from rdb$database;   --> -2739128.
    3. Unsigned hexadecimal integer:
        3.1. select  0xABC123, 0XABC123 from rdb$database; --> 11256099;
        3.2. select  0x00000000 from rdb$database; --> 0;
        3.3. select  -0xABC123, -0XABC123 from rdb$database;   --> -11256099.
        3.4. select 0x7FFFFFFF from rdb$database; -->  2147483647 type INTEGER
        3.5. select 0x80000000 from rdb$database; -->  2147483648 type BIGINT
        3.6. select 0x7FFFFFFFFFFFFFFF from rdb$database; -->  9223372036854775807 type BIGINT
        3.7. select 0x8000000000000000 from rdb$database; -->  9223372036854775808 type INT128
        3.8. select -0x80000000 from rdb$database; -->  -2147483648 type INTEGER
        3.9. select -0x8000000000000000 from rdb$database; -->  -9223372036854775808 type BIGINT

Examples (underscores in numeric literals):
    1. For non-decimal integer literals:
        1.1 Permitted: select 0x_FFFF, 0xFF_FF, 0x_FF_FF from rdb$database;
        1.2 Forbidden: select 0x_FF__FF, 0xFFFFFF_, 0x_FF_FF_FF_ from rdb$database;
    2. For decimal literals:
        2.1 Permitted: select 10_10, 10_10.10_10, 10.10E-10_0 from rdb$database;
        2.2 Forbidden: select _1010, 100_, 10__10, 1010._1010, 1010_.1, 10.10E_-100_ from rdb$database;
