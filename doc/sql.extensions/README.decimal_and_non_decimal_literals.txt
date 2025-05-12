=============================================
Decimal integer literals
Non-decimal integer literals (SQ:2023 T661)
Underscores in numeric literals (SQ:2023 T662)
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
    <exact numeric literal>

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

Notes (non-decimal integer literals):
    1. The standard allows non-decimal literals in the mantissa of an exponential number entry.
       For example (0xAAAe10), but there may be a conflict (0xEEE10). Therefore, a restriction on
       the use of non-decimal literals in the exponential number record is introduced.
    2. The <unsigned hexadecimal integer> value is a numeric value defined by applying the usual
       mathematical interpretation of positional hexadecimal notation to a string that is an
       unsigned hexadecimal integer. Similarly for an unsigned octal integer and an
       unsigned binary integer.
    3. To represent negative values, place a minus sign in front of an unsigned hexadecimal literal.
       Similarly for an unsigned octal integer and an unsigned binary integer. Similarly for an
       unsigned octal integer and an unsigned binary integer.
    4. Numbers with type SMALLINT cannot be written in hexadecimal, strictly speaking,
       since even 0x1 evaluates to INTEGER. However, if you write a positive integer within
       the 16-bit range 0x0000 (decimal zero) to 0x7FFF (decimal 32767), it will be converted to
       SMALLINT transparently. Similarly for an unsigned octal integer and an
       unsigned binary integer.

Notes (underscores in numeric literals):
    1. Limitations for non-decimal integer literals:
        1.1. It is considered unacceptable for there to be two or more consecutive underscores;
        1.2. Underscores are not allowed after the last character.
    2. Limitations for decimal integer literals:
        1.1. Underscores before the first character and after the last character are not allowed;
        1.2. Underscores are not permitted before or after the <period> symbol;
        1.3. Underscores are not allowed before or after the <E> character;
        1.4. Underscores are not allowed after the last character.

Examples (non-decimal integer literals):
    1. Unsigned binary integer:
        1.1. select  0b11010100, 0B11010100 from rdb$database;  --> 212;
        1.2. select  0b0000000  from rdb$database;  --> 0;
        1.3. select  -0b11010100, -0B11010100 from rdb$database; --> -212.
    2. Unsigned octal intege:
        2.1. select  0o12345670, 0O12345670 from rdb$database; --> 2739128;
        2.2. select  0o00000000 from rdb$database; --> 0;
        2.3. select  -0o12345670, -0O12345670 from rdb$database;   --> -2739128.
    3. Unsigned hexadecimal integer:
        3.1. select  0xABC123, 0XABC123 from rdb$database; --> 11256099;
        3.2. select  0x00000000 from rdb$database; --> 0;
        3.3. select  -0xABC123, -0XABC123 from rdb$database;   --> -11256099.
        3.4. select 0x7FFFFFFFFF from rdb$database; -->  2147483647 type INTEGER
        3.5. select 0x80000000 from rdb$database; -->  2147483648 type BIGINT
        3.5. select 0x7FFFFFFFFFFFFFFF from rdb$database; -->  9223372036854775807 type BIGINT
        3.6. select 0x8000000000000000 from rdb$database; -->  9223372036854775808 type INT128

Examples (underscores in numeric literals):
    1. For non-decimal integer literals:
        1.1 Permitted: select 0x_FFFF, 0xFF_FF, 0x_FF_FF, 0x_FF_FF from rdb$database;
        1.2 Forbidden: select 0x_FF__FF, 0xFFFFFF_, 0x_FF_FF_FF_ from rdb$database;
    2. For decimal integer literals:
        2.1 Permitted: select 10_10, 10_10.10_10, 10.10E-10_0 from rdb$database;
        2.2 Forbidden: select _1010, 100_, 1010._1010, 1010_.1, 10.10E_-100_ from rdb$database;
