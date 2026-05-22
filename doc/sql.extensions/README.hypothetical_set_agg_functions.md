# Hypothetical-Set Aggregate Functions

Hypothetical set functions include ranking and rank distribution functions, which you are already familiar with as window functions, but they are applied to groups to enter values ​​in hypothetical form.

There are two sorted set ranking functions: `RANK` and `DENSE_RANK`. There are also two sorted set rank distribution functions: `PERCENT_RANK` and `CUME_DIST`. Window functions and ordered set functions differ in the way they order. The former order within a window, while the latter order within a group.

Syntax of aggregate functions of a hypothetical set:

```
<hypothetical-set-agg-function> ::=
      RANK(<args>) WITHIN GROUP (ORDER BY <sorted_args>)
    | DENSE_RANK(<args>) WITHIN GROUP (ORDER BY <sorted_args>)
    | PERCENT_RANK(<args>) WITHIN GROUP (ORDER BY <sorted_args>)
    | CUME_DIST(<args>) WITHIN GROUP (ORDER BY <sorted_args>)
```

Each of the "hypothetical-set" aggregates is associated with a window function of the same name. In each case, the aggregate's result is the value that the associated window function would have returned for the "hypothetical" row constructed from args, if such a row had been added to the sorted group of rows represented by the sorted_args. For each of these functions, the list of direct arguments given in args must match the number and types of the aggregated arguments given in sorted_args. Unlike most built-in aggregates, these aggregates are not strict, that is they do not drop input rows containing nulls. `NULL` values sort according to the rule specified in the `ORDER BY` clause.

The values ​​of the input arguments must be constant within each group.

The number of input arguments of the hypothetical set functions must match the number of sorting elements in the `WITHIN GROUP (ORDER BY <sorted_args>)` clause.

An example of using hypothetical set functions without grouping:

```sql
SELECT
  RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS R1,
  DENSE_RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS R2,
  PERCENT_RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS PR,
  CUME_DIST(35500) WITHIN GROUP(ORDER BY E.SALARY) AS CD
FROM EMPLOYEE E;
```

```
                   R1                    R2                      PR                      CD
===================== ===================== ======================= =======================
                    9                     8      0.1904761904761905      0.2093023255813954
```

An example of using hypothetical set functions with grouping:

```sql
SELECT
  E.JOB_COUNTRY,
  RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS R1,
  DENSE_RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS R2,
  PERCENT_RANK(35500) WITHIN GROUP(ORDER BY E.SALARY) AS PR,
  CUME_DIST(35500) WITHIN GROUP(ORDER BY E.SALARY) AS CD
FROM EMPLOYEE E
GROUP BY E.JOB_COUNTRY;
```

```
JOB_COUNTRY                        R1                    R2                      PR                      CD
=============== ===================== ===================== ======================= =======================
Canada                              1                     1       0.000000000000000      0.5000000000000000
England                             3                     3      0.6666666666666666      0.7500000000000000
France                              1                     1       0.000000000000000      0.5000000000000000
Italy                               2                     2       1.000000000000000       1.000000000000000
Japan                               1                     1       0.000000000000000      0.3333333333333333
Switzerland                         1                     1       0.000000000000000      0.5000000000000000
USA                                 6                     5      0.1515151515151515      0.1764705882352941
```

An example of using hypothetical functions with two arguments:

```sql
SELECT
  RANK('England', 35500) WITHIN GROUP(ORDER BY E.JOB_COUNTRY, E.SALARY) AS R1,
  DENSE_RANK('England', 35500) WITHIN GROUP(ORDER BY E.JOB_COUNTRY, E.SALARY) AS R2,
  PERCENT_RANK('England', 35500) WITHIN GROUP(ORDER BY E.JOB_COUNTRY, E.SALARY) AS PR,
  CUME_DIST('England', 35500) WITHIN GROUP(ORDER BY E.JOB_COUNTRY, E.SALARY) AS CD
FROM EMPLOYEE E;
```

```
                   R1                    R2                      PR                      CD
===================== ===================== ======================= =======================
                    4                     4     0.07142857142857142     0.09302325581395349
```
