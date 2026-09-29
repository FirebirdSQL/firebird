# PERCENTILE_DISC and PERCENTILE_CONT functions

The `PERCENTILE_CONT` and `PERCENTILE_DISC` functions are known as inverse distribution functions. 
These functions operate on an ordered set. Both functions can be used as aggregate or window functions.

## PERCENTILE_DISC

`PERCENTILE_DISC` is an inverse distribution function that assumes a discrete distribution model. 
It takes a percentile value and a sort specification and returns an element from the set. 
Nulls are ignored in the calculation.

Syntax for the `PERCENTILE_DISC` function as an aggregate function.

```
PERCENTILE_DISC(<percent>) WITHIN GROUP (ORDER BY <expr> [ASC | DESC])
```

Syntax for the `PERCENTILE_DISC` function as an window function.

```
PERCENTILE_DISC(<percent>) WITHIN GROUP (ORDER BY <expr> [ASC | DESC])
  OVER (PARTITION BY <part_expr>)
```

The first argument `<percent>` must evaluate to a numeric value between 0 and 1, because it is a percentile value. 
This expression must be constant within each aggregate group. 
The `ORDER BY` clause takes a single expression that can be of any type that can be sorted.

The function `PERCENTILE_DISC` returns a value of the same type as the argument in `ORDER BY`.

For a given percentile value `P`, `PERCENTILE_DISC` sorts the values of the expression in the `ORDER BY` clause and 
returns the value with the smallest `CUME_DIST` value (with respect to the same sort specification) 
that is greater than or equal to `P`.

### Analytic Example

```sql
SELECT 
  DEPT_NO, 
  SALARY,
  CUME_DIST() OVER(PARTITION BY DEPT_NO ORDER BY SALARY) AS "CUME_DIST",
  PERCENTILE_DISC(0.5) WITHIN GROUP(ORDER BY SALARY) 
    OVER(PARTITION BY DEPT_NO) AS MEDIAN_DISC
FROM EMPLOYEE
WHERE DEPT_NO < 600
ORDER BY 1, 2;
```

```
DEPT_NO                SALARY               CUME_DIST           MEDIAN_DISC
======= ===================== ======================= =====================
000                  53793.00      0.5000000000000000              53793.00
000                 212850.00       1.000000000000000              53793.00
100                  44000.00      0.5000000000000000              44000.00
100                 111262.50       1.000000000000000              44000.00
110                  61637.81      0.5000000000000000              61637.81
110                  68805.00       1.000000000000000              61637.81
115                6000000.00      0.5000000000000000            6000000.00
115                7480000.00       1.000000000000000            6000000.00
120                  22935.00      0.3333333333333333              33620.63
120                  33620.63      0.6666666666666666              33620.63
120                  39224.06       1.000000000000000              33620.63
121                 110000.00       1.000000000000000             110000.00
123                  38500.00       1.000000000000000              38500.00
125                  33000.00       1.000000000000000              33000.00
130                  86292.94      0.5000000000000000              86292.94
130                 102750.00       1.000000000000000              86292.94
140                 100914.00       1.000000000000000             100914.00
180                  42742.50      0.5000000000000000              42742.50
180                  64635.00       1.000000000000000              42742.50
```

## PERCENTILE_CONT

`PERCENTILE_CONT` is an inverse distribution function that assumes a continuous distribution model. 
It takes a percentile value and a sort specification and returns an element from the set. 
Nulls are ignored in the calculation.

Syntax for the `PERCENTILE_CONT` function as an aggregate function.

```
PERCENTILE_CONT(<percent>) WITHIN GROUP (ORDER BY <expr> [ASC | DESC])
```

Syntax for the `PERCENTILE_CONT` function as an window function.

```
PERCENTILE_CONT(<percent>) WITHIN GROUP (ORDER BY <expr> [ASC | DESC])
  OVER (PARTITION BY <part_expr>)
```

The first argument `<percent>` must evaluate to a numeric value between 0 and 1, because it is a percentile value. 
This expression must be constant within each aggregate group. 
The `ORDER BY` clause takes a single expression, which must be of numeric type to perform interpolation.

The `PERCENTILE_CONT` function returns a value of type `DOUBLE PRECISION` or `DECFLOAT(34)` depending on the type 
of the argument in the `ORDER BY` clause. A value of type `DECFLOAT(34)` is returned if `ORDER BY` contains 
an expression of one of the types `INT128`, `NUMERIC(38, x)` or `DECFLOAT(16 | 34)`, otherwise - `DOUBLE PRECISION`.

The result of `PERCENTILE_CONT` is computed by linear interpolation between values after ordering them. 
Using the percentile value (`P`) and the number of rows (`N`) in the aggregation group, you can compute 
the row number you are interested in after ordering the rows with respect to the sort specification. 
This row number (`RN`) is computed according to the formula `RN = (1 + (P * (N - 1))`. 
The final result of the aggregate function is computed by linear interpolation between the values from rows 
at row numbers `CRN = CEILING(RN)` and `FRN = FLOOR(RN)`.

```
function f(N) ::= value of expression from row at N

if (CRN = FRN = RN) then
  return f(RN)
else
  return (CRN - RN) * f(FRN) + (RN - FRN) * f(CRN)
```

### Analytic Example

```sql
SELECT 
  DEPT_NO, 
  SALARY,
  PERCENT_RANK() OVER(PARTITION BY DEPT_NO ORDER BY SALARY) AS "PERCENT_RANK",
  PERCENTILE_CONT(0.5) WITHIN GROUP(ORDER BY SALARY) 
    OVER(PARTITION BY DEPT_NO) AS MEDIAN_CONT
FROM EMPLOYEE
WHERE DEPT_NO < 600
ORDER BY 1, 2;
```

```
DEPT_NO                SALARY            PERCENT_RANK             MEDIAN_CONT
======= ===================== ======================= =======================
000                  53793.00       0.000000000000000       133321.5000000000
000                 212850.00       1.000000000000000       133321.5000000000
100                  44000.00       0.000000000000000       77631.25000000000
100                 111262.50       1.000000000000000       77631.25000000000
110                  61637.81       0.000000000000000       65221.40500000000
110                  68805.00       1.000000000000000       65221.40500000000
115                6000000.00       0.000000000000000       6740000.000000000
115                7480000.00       1.000000000000000       6740000.000000000
120                  22935.00       0.000000000000000       33620.63000000000
120                  33620.63      0.5000000000000000       33620.63000000000
120                  39224.06      0.2500000000000000       33620.63000000000
121                 110000.00       0.000000000000000       110000.0000000000
123                  38500.00       0.000000000000000       38500.00000000000
125                  33000.00       0.000000000000000       33000.00000000000
130                  86292.94       0.000000000000000       94521.47000000000
130                 102750.00       1.000000000000000       94521.47000000000
140                 100914.00       0.000000000000000       100914.0000000000
180                  42742.50       0.000000000000000       53688.75000000000
180                  64635.00       1.000000000000000       53688.75000000000
```

## An example of using both aggregate functions

```sql
SELECT 
  DEPT_NO, 
  PERCENTILE_CONT(0.5) WITHIN GROUP(ORDER BY SALARY) AS MEDIAN_CONT,
  PERCENTILE_DISC(0.5) WITHIN GROUP(ORDER BY SALARY) AS MEDIAN_DISC
FROM EMPLOYEE 
GROUP BY DEPT_NO;
```

```
DEPT_NO             MEDIAN_CONT           MEDIAN_DISC
======= ======================= =====================
000           133321.5000000000              53793.00
100           77631.25000000000              44000.00
110           65221.40500000000              61637.81
115           6740000.000000000            6000000.00
120           33620.63000000000              33620.63
121           110000.0000000000             110000.00
123           38500.00000000000              38500.00
125           33000.00000000000              33000.00
130           94521.47000000000              86292.94
140           100914.0000000000             100914.00
180           53688.75000000000              42742.50
600           66450.00000000000              27000.00
621           71619.75000000000              62550.00
622           53167.50000000000              53167.50
623           60000.00000000000              60000.00
670           71268.75000000000              31275.00
671           81810.19000000000              81810.19
672           45647.50000000000              35000.00
900           92791.31500000000              69482.63
```
