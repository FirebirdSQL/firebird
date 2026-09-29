# Aggregate Functions


## ANY_VALUE (Firebird 6.0)

`ANY_VALUE` is a non-deterministic aggregate function that returns its expression for an arbitrary
record from the grouped rows.

`NULLs` are ignored. It's returned only in the case of none evaluated records having a non-null value.

Syntax:

```
<any value> ::= ANY_VALUE(<expression>)
```

Example:

```
select department,
       any_value(employee) employee
    from employee_department
    group by department
```

## Bitwise aggregates (Firebird 6.0)

The `BIN_AND_AGG`, `BIN_OR_AGG`, and `BIN_XOR_AGG` aggregate functions perform bit operations.

`NULLs` are ignored. It's returned only in the case of none evaluated records having a non-null value.

The input argument must be one of the integer types (`SMALLINT`, `INTEGER`, `BIGINT`, or `INT128`). 
The output result is of the same type as the input argument.

Syntax:

```
<bin_add agg> ::= BIN_AND_AGG(<expression>)

<bin_or agg> ::= BIN_OR_AGG(<expression>)

<bin_xor agg> ::= BIN_XOR_AGG([ALL | DISTINCT] <expression>)
```

The `BIN_AND_AGG` and `BIN_OR_AGG` functions do not support the `DISTINCT` keyword, since eliminating duplicates does 
not affect the result. However, for `BIN_XOR_AGG`, you can specify `DISTINCT` to exclude duplicates from processing.

Example:

```
SELECT
  name,
  BIN_AND_AGG(n) AS F_AND,
  BIN_OR_AGG(n) AS F_OR,
  BIN_XOR_AGG(n) AS F_XOR,
  BIN_XOR_AGG(ALL n) AS F_XOR_A,
  BIN_XOR_AGG(DISTINCT n) AS F_XOR_D
FROM acl_masks
GROUP BY name
```
