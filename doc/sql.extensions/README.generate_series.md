# GENERATE_SERIES function

The `GENERATE_SERIES` function creates a series of numbers within a specified interval. 
The interval and the step between series values ​​are defined by the user.

## Syntax

```
<generate_series_function> ::=
    GENERATE_SERIES(<start>, <finish> [, <step>]) [AS] <correlation name> [ ( <derived column name> ) ]
```

## Arguments

* `start` - The first value in the interval. `start` is specified as a variable, a literal, or a scalar expression of type 
`smallint`, `integer` or `bigint`.

* `finish` - The last value in the interval. `finish` is specified as a variable, a literal, or a scalar expression of 
type `smallint`, `integer` or `bigint`. The series stops once the last generated step value exceeds the `finish` value.

* `step` - Indicates the number of values to increment or decrement between steps in the series. `step` is an expression 
of type `smallint`, `integer` or `bigint`. `step` can be either negative or positive, but can't be zero (0). This 
argument is optional. The default value for `step` is 1.

## Returning type

The function `GENERATE_SERIES` returns a set with a `BIGINT` column.

## Rules

* If `start > finish` and a negative `step` value is specified, an empty set is returned.

* If `start < finish` and a positive `step` value is specified, an empty set is returned.

* If the `step` argument is zero, an error is thrown.

## Examples

```
SELECT n
FROM GENERATE_SERIES(1, 3) AS S(n);

SELECT n
FROM GENERATE_SERIES(3, 1, 1) AS S(n);

SELECT 
  DATEADD(n MINUTE TO timestamp '2025-01-01 12:00') AS START_TIME,
  DATEADD(n MINUTE TO timestamp '2025-01-01 12:00:59.9999') AS FINISH_TIME
FROM GENERATE_SERIES(0, 59) AS S(n);
```

