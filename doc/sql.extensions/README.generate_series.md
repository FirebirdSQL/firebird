# GENERATE_SERIES function

The `GENERATE_SERIES` function creates a series of numbers within a specified interval. 

## Syntax

```
<generate_series_function> ::=
    GENERATE_SERIES(<start>, <limit> [, <step>]) [AS] <correlation name> [ ( <derived column name> ) ]
```

## Arguments

* `start` - The first value in the interval. `start` is specified as a variable, a literal, or a scalar expression of type 
`SMALLINT`, `INTEGER`, `BIGINT`, `INT128` or `NUMERIC/DECIMAL`.

* `limit` - The last value in the interval. `limit` is specified as a variable, a literal, or a scalar expression of 
type `SMALLINT`, `INTEGER`, `BIGINT`, `INT128` or `NUMERIC/DECIMAL`.

* `step` - Indicates the number of values to increment or decrement between steps in the series. `step` is an expression 
of type `SMALLINT`, `INTEGER`, `BIGINT`, `INT128` or `NUMERIC/DECIMAL`. 
`step` can be either negative or positive, but can't be zero (0). This argument is optional. The default value for `step` is 1.

## Returning type

The function `GENERATE_SERIES` returns a table with a `BIGINT`, `INT128` or `NUMERIC(18, x)/NUMERIC(38, x)` column, 
where the scale is determined by the maximum of the scales of the function arguments.

## Rules

* If `start > limit` and a positive `step` value is specified, an empty table is returned.

* If `start < limit` and a negative `step` value is specified, an empty table is returned.

* The series stops once the last generated step value exceeds the `limit` value.

* If the `step` argument is zero, an error is raised.

## Examples

```
SELECT n
FROM GENERATE_SERIES(1, 3) AS S(n);

SELECT n
FROM GENERATE_SERIES(3, 1, -1) AS S(n);

SELECT n
FROM GENERATE_SERIES(0, 9.9, 0.1) AS S(n);

SELECT 
  DATEADD(n MINUTE TO timestamp '2025-01-01 12:00') AS START_TIME,
  DATEADD(n MINUTE TO timestamp '2025-01-01 12:00:59.9999') AS FINISH_TIME
FROM GENERATE_SERIES(0, 59) AS S(n);
```

