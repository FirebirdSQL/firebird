=================
Builtin functions
=================

Builtin functions below (except DECODE) are used only if there is no
UDF declared with the same name.
This choice of UDF or system function is decided when compiling the
statement and isn't changed if the statement is stored (trigger / SP).

Authors:
    Adriano dos Santos Fernandes <adrianosf@uol.com.br>
    Oleg Loa <loa@mail.ru>
    Alexey Karyakin <aleksey.karyakin@mail.ru>
    Claudio Valderrama C. <cvalde at usa.net>
    Alexander Peshkov <peshkoff@mail.ru>


---
ABS
---

Function:
    Returns the absolute value of a number.

Format:
    ABS( <number> )

Example:
    select abs(amount) from transactions;


----
ACOS
----

Function:
    Returns the arc cosine of a number.

Format:
    ACOS( <number> )

Notes:
    Argument to ACOS must be in the range -1 to 1 and returns a value in
    the range 0 to PI.

Example:
    select acos(x) from y;


-----
ACOSH
-----

Function:
    Returns the hyperbolic arc cosine of a number (expressed in radians).

Format:
    ACOSH( <number> )

Example:
    select acosh(x) from y;


----------
ASCII_CHAR
----------

Function:
    Returns the ASCII character with the specified code.

Format:
    ASCII_CHAR( <number> )

Notes:
    Argument to ASCII_CHAR must be in the range 0 to 255 and returns a value
    with NONE character set.

Example:
    select ascii_char(x) from y;


---------
ASCII_VAL
---------

Function:
    Returns the ASCII code of the first character of the specified string.

Format:
    ASCII_VAL( <string> )

Notes:
    1) Returns 0 if the string is empty.
    2) Throws error if the first character is multi-byte.

Example:
    select ascii_val(x) from y;


----
ASIN
----

Function:
    Returns the arc sine of a number.

Format:
    ASIN( <number> )

Notes:
    Argument to ASIN must be in the range -1 to 1 and returns a value in
    the range -PI / 2 to PI / 2.

Example:
    select asin(x) from y;


-----
ASINH
-----

Function:
    Returns the hyperbolic arc sine of a number (expressed in radians).

Format:
    ASINH( <number> )

Example:
    select asinh(x) from y;


----
ATAN
----

Function:
    Returns the arc tangent of a number.

Format:
    ATAN( <number> )

Notes:
    Returns a value in the range -PI / 2 to PI / 2.

Example:
    select atan(x) from y;


-----
ATAN2
-----

Function:
    Returns the arc tangent of the first number / the second number.

Format:
    ATAN( <number>, <number> )

Notes:
    Returns a value in the range -PI to PI.

Example:
    select atan2(x, y) from z;


-----
ATANH
-----

Function:
    Returns the hyperbolic arc tangent of a number (expressed in radians).

Format:
    ATANH( <number> )

Example:
    select atanh(x) from y;


-----------------------------
BASE64_ENCODE / BASE64_DECODE
-----------------------------

Function:
	Encodes / decodes input data to / from BASE64 representation. Works with character strings and blobs.

Format:
	BASE64_ENCODE( <binary data> )
	BASE64_DECODE( <base64 data> )

Example:
	select base64_encode(public_key) from clients;


-------
BIN_AND
-------

Function:
    Returns the result of a binary AND operation performed on all arguments.

Format:
    BIN_AND( <number>, <number> [, <number> ...] )

Example:
    select bin_and(flags, 1) from x;


-------
BIN_NOT
-------

Function:
    Returns the result of a bitwise NOT operation performed on its argument.

Format:
    BIN_NOT( <number> )

Example:
    select bin_not(flags) from x;


------
BIN_OR
------

Function:
    Returns the result of a binary OR operation performed on all arguments.

Format:
    BIN_OR( <number>, <number> [, <number> ...] )

Example:
    select bin_or(flags1, flags2) from x;


-------
BIN_SHL
-------

Function:
    Returns the result of a binary shift left operation performed on the arguments (first << second).

Format:
    BIN_SHL( <number>, <number> )

Example:
    select bin_shl(flags1, 1) from x;


-------
BIN_SHR
-------

Function:
    Returns the result of a binary shift right operation performed on the arguments (first >> second).

Format:
    BIN_SHR( <number>, <number> )

Example:
    select bin_shr(flags1, 1) from x;


-------
BIN_XOR
-------

Function:
    Returns the result of a binary XOR operation performed on all arguments.

Format:
    BIN_XOR( <number>, <number> [, <number> ...] )

Example:
    select bin_xor(flags1, flags2) from x;


--------------
CEIL | CEILING
--------------

Function:
    Returns a value representing the smallest integer that is greater
    than or equal to the input argument.

Format:
    { CEIL | CEILING }( <number> )

Example:
    1) select ceil(val) from x;
    2) select ceil(2.1), ceil(-2.1) from rdb$database;  -- returns 3, -2


------------
CHAR_TO_UUID
------------

Function:
    Converts the CHAR(32) ASCII representation of an UUID
    (XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX) to the CHAR(16) OCTETS
    representation (optimized for storage).

Format:
    CHAR_TO_UUID( <string> )

Important (for big-endian servers):
    It has been discovered that before Firebird 2.5.2, CHAR_TO_UUID and UUID_TO_CHAR work
    incorrectly in big-endian servers. In these machines, bytes/characters are swapped and go in
    wrong positions when converting. This bug was fixed in 2.5.2 and 3.0, but that means these
    functions now return different values (for the same input parameter) than before.

Example:
    select char_to_uuid('93519227-8D50-4E47-81AA-8F6678C096A1') from rdb$database;

See also: GEN_UUID and UUID_TO_CHAR


---
COS
---

Function:
    Returns the cosine of an angle (expressed in radians).

Format:
    COS( <number> )

Notes:
    The angle is specified in radians and returns a value in the range -1 to 1.

Example:
    select cos(x) from y;


----
COSH
----

Function:
    Returns the hyperbolic cosine of an angle (expressed in radians).

Format:
    COSH( <number> )

Example:
    select cosh(x) from y;


---
COT
---

Function:
    Returns 1 / tan(argument).

Format:
    COT( <number> )

Example:
    select cot(x) from y;


-----
CRC32
-----

Function:
	Returns CRC-32 with the polynomial 0x04C11DB7. Accepts argument of any type.

Format:
	CRC32( <any value> )

Example:
	select crc32(job_title) from job;


-------
DATEADD
-------

Function:
    Returns a date/time/timestamp value increased (or decreased, when negative)
    by the specified amount of time.

Format:
    DATEADD( <number> <timestamp_part> TO <date_time> )
    DATEADD( <timestamp_part>, <number>, <date_time> )

    timestamp_part ::= { YEAR | MONTH | DAY | WEEK | HOUR | MINUTE | SECOND | MILLISECOND }

Notes:
    1) WEEKDAY and YEARDAY cannot be used. It doesn't make sense.
    2) YEAR, MONTH and DAY could not be used with time values.
    3) All timestamp_part values could be used with timestamp values.
    4) When using HOUR, MINUTE, SECOND and MILLISECOND for DATEADD and dates, the quantity added or
        subtracted should account at least for one day to produce effect (IE adding 23 hours to a date
        doesn't increment it).
    5) When using YEAR or MONTH and the input day is greater than the maximum possible day in the
       result year/month, the result day is returned in the last day of the result year/month.

Example:
    select dateadd(-1 day to current_date) as yesterday
	    from rdb$database;


--------
DATEDIFF
--------

Function:
    Returns an exact numeric value representing the amount of time from the first
    date/time/timestamp value to the second one.

Format:
    DATEDIFF( <timestamp_part> FROM <date_time> TO <date_time> )
    DATEDIFF( <timestamp_part>, <date_time>, <date_time> )

    timestamp_part ::= { YEAR | MONTH | DAY | WEEK | HOUR | MINUTE | SECOND | MILLISECOND }

Notes:
    1) Returns positive value if the second value is greater than the first one,
	   negative when the first one is greater or zero when they are equal.
    2) Comparison of date with time values is invalid.
    3) WEEKDAY and YEARDAY cannot be used. It doesn't make sense.
    4) YEAR, MONTH and DAY could not be used with time values.
    5) All timestamp_part values could be used with timestamp values.

Example:
    select datediff(week from cast('yesterday' as timestamp) - 7 to current_timestamp)
	    from rdb$database;


------
DECODE
------

Function:
    DECODE is a shortcut to CASE ... WHEN ... ELSE expression.

Format:
    DECODE( <expression>, <search>, <result> [ , <search>, <result> ... ] [, <default> ]

Example:
	select decode(state, 0, 'deleted', 1, 'active', 'unknown') from things;


-------------------
ENCRYPT and DECRYPT
-------------------

Function:
    Encrypts/decrypts data using symmetric cipher.

Format:
    {ENCRYPT | DECRYPT} ( <string | blob> USING <algorithm> [MODE <mode>] KEY <string>
    	[IV <string>] [<endianness>] [CTR_LENGTH <smallint>] [COUNTER <bigint>])

    algorithm ::= { block_cipher | stream_cipher }
    block_cipher ::= { AES | ANUBIS | BLOWFISH | KHAZAD | RC5 | RC6 | SAFER+ | TWOFISH | XTEA }
    stream_cipher ::= { CHACHA20 | RC4 | SOBER128 }
    mode ::= { CBC | CFB | CTR | ECB | OFB }
    endianness ::= { CTR_BIG_ENDIAN | CTR_LITTLE_ENDIAN }

Important:
    - Mode should be specified for block ciphers.
    - Initialization vector (IV) should be specified for block ciphers in all modes except ECB and
      all stream ciphers except RC4.
    - Endianness may be specified only in CTR mode, default is little endian counter.
    - Counter length (CTR_LENGTH, bytes) may be specified only in CTR mode, default is the size of IV.
    - Initial counter value (COUNTER) may be specified only for CHACHA20 cipher, default is 0.
	- Sizes of data strings passed to this functions are according to selected algorithm and mode
	  requirements.
	- Functions return BLOB when first argument is blob and varbinary for all text types.

Example:
    select encrypt('897897' using sober128 key 'AbcdAbcdAbcdAbcd' iv '01234567') from rdb$database;
    select decrypt(x'0154090759DF' using sober128 key 'AbcdAbcdAbcdAbcd' iv '01234567') from rdb$database;
    select decrypt(secret_field using aes mode ofb key '0123456701234567' iv init_vector) from secure_table;


---
EXP
---

Function:
    Returns the exponential e to the argument.

Format:
    EXP( <number> )

Example:
    select exp(x) from y;


---------
FIRST_DAY
---------

Function:
    Returns a date/timestamp with the first day of the year/month/week of a given
    date/timestamp value.

Format:
    FIRST_DAY( OF { YEAR | MONTH | WEEK } FROM <date_or_timestamp> )

Notes:
    1) The first day of the week is considered as Sunday, per the same rules of EXTRACT with WEEKDAY.
    2) When a timestamp is passed the return value preserves the time part.

Example:
    select first_day(of month from current_date) from rdb$database;
    select first_day(of year from current_timestamp) from rdb$database;
    select first_day(of week from date '2017-11-01') from rdb$database;


-----
FLOOR
-----

Function:
    Returns a value representing the largest integer that is less
    than or equal to the input argument.

Format:
    FLOOR( <number> )

Example:
    1) select floor(val) from x;
    2) select floor(2.1), floor(-2.1) from rdb$database;  -- returns 2, -3


--------
GEN_UUID
--------

Function:
    Returns an universal unique number in CHAR(16) OCTETS type.

Format:
    GEN_UUID()

Important:
    Before Firebird 2.5.2, GEN_UUID was returning completely random strings. This is not compliant
    with the RFC-4122 (UUID specification).
    This was fixed in Firebird 2.5.2 and 3.0. Now GEN_UUID returns a compliant UUID version 4
    string, where some bits are reserved and the others are random. The string format of a compliant
    UUID is XXXXXXXX-XXXX-4XXX-YXXX-XXXXXXXXXXXX, where 4 is fixed (version) and Y is 8, 9, A or B.

Example:
    insert into records (id) value (gen_uuid());

See also: CHAR_TO_UUID and UUID_TO_CHAR


----
HASH
----

Function:
    Returns a HASH of a string using a specified algorithm.

Format:
    HASH( <string> [ USING <algorithm> ] )

    algorithm ::= { MD5 | SHA1 | SHA256 | SHA512 }

Important:
    - The syntax without USING is very discouraged and maintained for backward compatibility.
    It returns a 64 bit integer and produces very bad hashes that easily result in collisions.
    - The syntax with USING is introduced in FB 4.0 and returns VARCHAR strings with OCTETS charset.

Example:
    select hash(x) from y;
    select hash(x using sha256) from y;


--------
LAST_DAY
--------

Function:
    Returns a date/timestamp with the last day of the year/month/week of a given
    date/timestamp value.

Format:
    LAST_DAY( OF { YEAR | MONTH | WEEK } FROM <date_or_timestamp> )

Notes:
    1) The last day of the week is considered as Saturday, per the same rules of EXTRACT with WEEKDAY.
    2) When a timestamp is passed the return value preserves the time part.

Example:
    select last_day(of month from current_date) from rdb$database;
    select last_day(of year from current_timestamp) from rdb$database;
    select last_day(of week from date '2017-11-01') from rdb$database;


----
LEFT
----

Function:
    Returns the substring of a specified length that appears at the start of a string.

Format:
    LEFT( <string>, <number> )

Example:
    select left(name, char_length(name) - 10)
        from people
        where name like '% FERNANDES';


--
LN
--

Function:
    Returns the natural logarithm of a number.

Format:
    LN( <number> )

Example:
    select ln(x) from y;


---
LOG
---

Function:
    LOG(x, y) returns the logarithm base x of y.

Format:
    LOG( <number>, <number> )

Example:
    select log(x, 10) from y;


-----
LOG10
-----

Function:
    Returns the logarithm base ten of a number.

Format:
    LOG10( <number> )

Example:
    select log10(x) from y;


----
LPAD
----

Function:
    LPAD(string1, length, string2) appends string2 to the beginning of
    string1 until length of the result string becomes equal to length.

Format:
    LPAD( <string>, <number> [, <string> ] )

Notes:
    1) If the second string is omitted the default value is one space.
    2) The second string is truncated when the result string will
	   become larger than length.
    3) The first string is truncated if its length is greater than the length
	   parameter.

Example:
    select lpad(x, 10) from y;


--------
MAXVALUE
--------

Function:
    Returns the maximum value of a list of values.

Format:
    MAXVALUE( <value> [, <value> ...] )

Example:
    select maxvalue(v1, v2, 10) from x;


--------
MINVALUE
--------

Function:
    Returns the minimun value of a list of values.

Format:
    MINVALUE( <value> [, <value> ...] )

Example:
    select minvalue(v1, v2, 10) from x;


---
MOD
---

Function:
    MOD(X, Y) returns the remainder part of the division of X by Y.

Format:
    MOD( <number>, <number> )

Example:
    select mod(x, 10) from y;


-------
OVERLAY
-------

Function:
    OVERLAY( <string1> PLACING <string2> FROM <start> [ FOR <length> ] ) returns
    string1 replacing the substring FROM start FOR length by string2.

Format:
    OVERLAY( <string> PLACING <string> FROM <number> [ FOR <number> ] )

Notes:
    1) If <length> is not specified, CHAR_LENGTH( <string2> ) is implied.
    2) The OVERLAY function is equivalent to:
           SUBSTRING(<string1> FROM 1 FOR <start> - 1) ||
           <string2> ||
           SUBSTRING(<string1> FROM <start> + <length>)


--
PI
--

Function:
    Returns the PI constant (3.1459...).

Format:
    PI()

Example:
    val = PI();


--------
POSITION
--------

Function:
    Returns the position of the first string inside the second string starting at
    an offset (or from the beginning when omitted). When not found, returns 0.

Format:
    POSITION( <string> IN <string> )
    POSITION( <string>, <string> [, <number>] )

Example:
    select rdb$relation_name
        from rdb$relations
        where position('RDB$' IN rdb$relation_name) = 1;


-----
POWER
-----

Function:
    POWER(X, Y) returns X to the power of Y.

Format:
    POWER( <number>, <number> )

Example:
    select power(x, 10) from y;


----
RAND
----

Function:
    Returns a random number between 0 and 1.

Format:
    RAND()

Example:
    select * from x order by rand();


----------------------
RDB$GET_TRANSACTION_CN
----------------------

(FB4 extension)
Function:
    Returns commit number of given transaction. Result type is BIGINT.

	Note, engine internally uses unsigned 8-byte integer for commit numbers,
	and unsigned 6-byte integer for transaction numbers. Thus, despite of
	SQL language have no unsigned integers and RDB$GET_TRANSACTION_CN returns
	signed BIGINT, it is impossible to see negative commit numbers except of
	few special values used for non-committed transactions.
	Summary, numbers returned by RDB$GET_TRANSACTION_CN could have values below:

	-2 - transaction is dead (rolled back)
	-1 - transaction is in limbo
	 0 - transaction is active,
	 1 - transaction committed before database started or less than database
		 Oldest Interesting Transaction
	>1 - transaction committed after database started
	NULL - given transaction number is NULL or greater than database Next Transaction

	See also README.read_consistency.md

Format:
    RDB$GET_TRANSACTION_CN( <transaction number> )

Examples:
	select rdb$get_transaction_cn(current_transaction) from rdb$database;
	select rdb$get_transaction_cn(123) from rdb$database;


--------------------
RDB$SYSTEM_PRIVILEGE
--------------------

(FB4 extension)
Function:
    Returns true if current attachment has given system privilege.

Format:
    RDB$SYSTEM_PRIVILEGE( <privilege> )

Example:
	select rdb$system_privilege(user_management) from rdb$database;


-------
REPLACE
-------

Function:
    REPLACE(searched, find, replacement) replaces all occurences of "find"
    in "searched" by "replacement".

Format:
    REPLACE( <string>, <string>, <string> )

Example:
    select replace(x, ' ', ',') from y;


-------
REVERSE
-------

Function:
    Returns a string in reverse order.

Format:
    REVERSE( <value> )

Notes:
    REVERSE is an useful function to index strings from right to left.

Example:
    create index people_email on people computed by (reverse(email));
    select * from people where reverse(email) starting with reverse('.br');


-----
RIGHT
-----

Function:
    RIGHT(string, length)
    Returns the substring of a specified length that appears at the end of a string.

Format:
    RIGHT( <string>, <number> )

Example:
    select right(rdb$relation_name, char_length(rdb$relation_name) - 4)
        from rdb$relations
        where rdb$relation_name like 'RDB$%';


-----
ROUND
-----

Function:
    ROUND(number, scale)
    Returns a number rounded to the specified scale.

Format:
    ROUND( <number> [, <number> ] )

Notes:
    If the scale (second parameter) is negative, the integer part of
    the value is rounded. Ex: ROUND(123.456, -1) returns 120.000.

Examples:
    select round(salary * 1.1, 0) from people;


----
RPAD
----

Function:
    RPAD(string1, length, string2) appends string2 to the end of
    string1 until length of the result string becomes equal to length.

Format:
    RPAD( <string>, <number> [, <string> ] )

Notes:
    1) If the second string is omitted the default value is one space.
    2) The second string is truncated when the result string will
	   become larger than length.
    3) The first string is truncated if its length is greater than the length
	   parameter.

Example:
    select rpad(x, 10) from y;


-----------
RSA_PRIVATE
-----------

Function:
    Returns RSA private key of specified length (in bytes) in PKCS#1 format as VARBINARY string.

Format:
    RSA_PRIVATE ( <smallint> )

Example:
    select rdb$set_context('USER_SESSION', 'private_key', rsa_private(256)) from rdb$database;


----------
RSA_PUBLIC
----------

Function:
    Returns RSA public key for specified RSA private key, all keys are in PKCS#1 format.

Format:
    RSA_PUBLIC ( <private key> )

Example:
    (tip - start running samples one by one from RSA_PRIVATE function)
    select rdb$set_context('USER_SESSION', 'public_key',
        rsa_public(rdb$get_context('USER_SESSION', 'private_key'))) from rdb$database;


-----------
RSA_ENCRYPT
-----------

Function:
    Pads data using OAEP padding and encrypts using RSA public key. Normally used to encrypt
    short symmetric keys which are then used in block ciphers to encrypt a message.

Format:
    RSA_ENCRYPT ( <string> KEY <public key> [LPARAM <string>] [HASH <hash>] )
        KEY should be a value, returhed by RSA_PUBLIC function.
        LPARAM is an additional system specific tag that can be applied to identify which
            system encoded the message. Default value is NULL.
        hash ::= { MD5 | SHA1 | SHA256 | SHA512 } Default is SHA256.

Example:
    (tip - start running samples one by one from RSA_PRIVATE function)
    select rdb$set_context('USER_SESSION', 'msg', rsa_encrypt('Some message'
        key rdb$get_context('USER_SESSION', 'public_key'))) from rdb$database;


-----------
RSA_DECRYPT
-----------

Function:
    Decrypts using RSA private key and OAEP de-pads the resulting data.

Format:
    RSA_DECRYPT ( <string> KEY <private key> [LPARAM <string>] [HASH <hash>] )
        KEY should be a value, returhed by RSA_PRIVATE function.
        LPARAM is the same variable passed to RSA_ENCRYPT. If it does not match
          what was used during encoding this function will not decrypt the packet.
        hash ::= { MD5 | SHA1 | SHA256 | SHA512 } Default is SHA256.

Example:
    (tip - start running samples one by one from RSA_PRIVATE function)
    select rsa_decrypt(rdb$get_context('USER_SESSION', 'msg')
        key rdb$get_context('USER_SESSION', 'private_key')) from rdb$database;


--------
RSA_SIGN
--------

Function:
    Performs PSS encoding of message digest to be signed and signs using RSA private key.

Format:
    RSA_SIGN ( <string> KEY <private key> [HASH <hash>] [SALT_LENGTH <smallint>] )
        KEY should be a value, returhed by RSA_PRIVATE function.
        hash ::= { MD5 | SHA1 | SHA256 | SHA512 } Default is SHA256.
        SALT_LENGTH indicates the length of the desired salt, and should typically be small.
            A good value is between 8 and 16.

Example:
    (tip - start running samples one by one from RSA_PRIVATE function)
    select rdb$set_context('USER_SESSION', 'msg', rsa_sign(hash('Test message' using sha256)
        key rdb$get_context('USER_SESSION', 'private_key'))) from rdb$database;


----------
RSA_VERIFY
----------

Function:
    Performs PSS encoding of message digest to be signed and verifies it's digital signature
        using RSA public key.

Format:
    RSA_VERIFY ( <string> SIGNATURE <string> KEY <public key> [HASH <hash>] [SALT_LENGTH <smallint>] )
        SIGNATURE should be a value, returhed by RSA_SIGN function.
        KEY should be a value, returhed by RSA_PUBLIC function.
        hash ::= { MD5 | SHA1 | SHA256 | SHA512 } Default is SHA256.
        SALT_LENGTH indicates the length of the desired salt, and should typically be small.
            A good value is between 8 and 16.

Example:
    (tip - start running samples one by one from RSA_PRIVATE function)
    select rsa_verify(hash('Test message' using sha256) signature rdb$get_context('USER_SESSION', 'msg')
        key rdb$get_context('USER_SESSION', 'public_key')) from rdb$database;


----
SIGN
----

Function:
    Returns 1, 0, or -1 depending on whether the input value is positive, zero or
    negative, respectively.

Format:
    SIGN( <number> )

Example:
    select sign(x) from y;


---
SIN
---

Function:
    Returns the sine of an angle (expressed in radians).

Format:
    SIN( <number> )

Notes:
    Argument to SIN must be specified in radians.

Example:
    select sin(x) from y;


----
SINH
----

Function:
    Returns the hyperbolic sine of an angle (expressed in radians).

Format:
    SINH( <number> )

Example:
    select sinh(x) from y;


----
SQRT
----

Function:
    Returns the square root of a number.

Format:
    SQRT( <number> )

Example:
    select sqrt(x) from y;


---
TAN
---

Function:
    Returns the tangent of an angle (expressed in radians).

Format:
    TAN( <number> )

Notes:
    Argument to TAN must be specified in radians.

Example:
    select tan(x) from y;


----
TANH
----

Function:
    Returns the hyperbolic tangent of an angle (expressed in radians).

Format:
    TANH( <number> )

Example:
    select tanh(x) from y;


-----
TRUNC
-----

Function:
    TRUNC(number, scale)
    Returns the integral part (up to the specified scale) of a number.

Format:
    TRUNC( <number> [, <number> ] )

Notes:
    If the scale (second parameter) is negative, the integer part of
    the value is truncated. Ex: TRUNC(123.456, -1) returns 120.000.

Example:
    1) select trunc(x) from y;
    2) select trunc(-2.8), trunc(2.8) from rdb$database;  -- returns -2, 2
    3) select trunc(987.65, 1), trunc(987.65, -1) from rdb$database;  -- returns 987.60, 980.00


------------
UUID_TO_CHAR
------------

Function:
    Converts a CHAR(16) OCTETS UUID (that's returned by GEN_UUID) to the
    CHAR(32) ASCII representation (XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX).

Format:
    UUID_TO_CHAR( <string> )

Important (for big-endian servers):
    It has been discovered that before Firebird 2.5.2, CHAR_TO_UUID and UUID_TO_CHAR work
    incorrectly in big-endian servers. In these machines, bytes/characters are swapped and go in
    wrong positions when converting. This bug was fixed in 2.5.2 and 3.0, but that means these
    functions now return different values (for the same input parameter) than before.

Example:
    select uuid_to_char(gen_uuid()) from rdb$database;

See also: GEN_UUID and CHAR_TO_UUID
