# Floating point types

Firebird has two forms of floating point types:

- approximate numeric types (or binary floating point types)
- decimal floating point types

## Approximate numeric types

The approximate numeric types supported by Firebird are a 32 bit single 
precision and a 64 bit double precision floating point type. These types are
available with the following SQL standard type names:

- `REAL` - 32 bit single precision (synonym for `FLOAT`) _(1)_
- `FLOAT` - 32 bit single precision
- `FLOAT(p)` where `p` is the precision of the significand in binary digits _(2, 3)_
	- 1 <= `p` <= 24 - 32 bit single precision (synonym for `FLOAT`)
	- 25 <= `p` <= 53 - 64 bit double precision (synonym for `DOUBLE PRECISION`)
- `DOUBLE PRECISION` - 64 bit double precision

In addition the following non-standard type names are supported:

- `LONG FLOAT` - 64 bit double precision (synonym for `DOUBLE PRECISION`)
- `LONG FLOAT(p)` where `p` is the precision of the significand in binary digits _(4, 5)_
	- 1 <= `p` <= 53 - 64 bit double precision (synonym for `DOUBLE PRECISION`)

These non-standard type names are deprecated and they may be removed in a future 
version.

## Decimal floating point types

The decimal floating point types supported by Firebird are a 64 bit Decimal64
with a precision of 16 decimals and a 128 bit Decimal128 with a precision of
34 decimals. These types are available with the following SQL standard type 
names:

- `DECFLOAT` - 128 bit Decimal128 (synonym for `DECFLOAT(34)`)
- `DECFLOAT(16)` - 64 bit Decimal64
- `DECFLOAT(34)` - 128 bit Decimal128

## Notes

1. `REAL` has been available as a synonym for `FLOAT` since Firebird 1 and even 
earlier, but was never documented.
2. Firebird 3 and earlier supported `FLOAT(p)` where `p` was the 
approximate precision in decimal digits, and 0 <= `p` <= 7 mapped to 32 bit 
single precision and `p` > 7 to 64 bit double precision. This syntax was never 
documented.
3. For `p` in `FLOAT(p)`, the values 1 <= `p` <= 24 are all treated as 
`p` = 24, values 25 <= `p` <= 53 are all handled as `p` = 53.
4. Firebird 3 and earlier supported `LONG FLOAT(p)` where `p` was the 
approximate precision in decimal digits, where any value for `p` mapped to 
64 bit double precision. This type name and syntax were never documented.
3. For `p` in `LONG FLOAT(p)`, the values 1 <= `p` <= 53 are all handled as 
`p` = 53.
