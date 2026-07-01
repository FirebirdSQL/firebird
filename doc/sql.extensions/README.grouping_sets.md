# Grouping Sets

`ROLLUP`, `CUBE` and `GROUPING SETS` extend `GROUP BY` with multiple grouping
levels in a single query. They are useful for subtotals, grand totals and
cross-tab style summaries.

The related `GROUPING(<expr> [, <expr> ...])` function returns `0` when a
single `<expr>` participates in the current grouping set, and `1` when it has
been replaced by a subtotal `NULL`. With multiple arguments it returns these
bits as a single integer. `GROUPING_ID(<expr> [, <expr> ...])` is an equivalent
Firebird extension for the bit-mask form.

## Syntax

```
<group by clause> ::=
  GROUP BY [ALL | DISTINCT] <grouping element> [, <grouping element> ...]

<grouping element> ::=
    <value expression>
  | (<value expression> [, <value expression> ...])
  | ()
  | ROLLUP(<grouping element> [, <grouping element> ...])
  | CUBE(<grouping element> [, <grouping element> ...])
  | GROUPING SETS(<grouping set> [, <grouping set> ...])

<grouping set> ::=
    <value expression>
  | (<value expression> [, <value expression> ...])
  | ()
  | ROLLUP(<grouping element> [, <grouping element> ...])
  | CUBE(<grouping element> [, <grouping element> ...])

<grouping function> ::=
    GROUPING(<value expression> [, <value expression> ...])
  | GROUPING_ID(<value expression> [, <value expression> ...])
```

`GROUP BY ()` specifies a single empty grouping set, producing one aggregate
group for the whole input, including empty input.

## ROLLUP

`ROLLUP(a, b, c)` expands to:

```
(a, b, c)
(a, b)
(a)
()
```

Composite grouping items are treated as one rollup unit. For example,
`ROLLUP(a, (b, c), d)` expands to:

```
(a, b, c, d)
(a, b, c)
(a)
()
```

Example:

```sql
select
    department,
    product,
    grouping(department) as g_department,
    grouping(product) as g_product,
    sum(amount) as total_amount
from sales
group by rollup(department, product)
order by
    grouping(department),
    department,
    grouping(product),
    product;
```

## CUBE

`CUBE(a, b, c)` expands to all subsets:

```
(a, b, c)
(a, b)
(a, c)
(a)
(b, c)
(b)
(c)
()
```

Composite grouping items are also supported. `CUBE((a, b), c)` expands to:

```
(a, b, c)
(a, b)
(c)
()
```

Because `CUBE` grows exponentially, Firebird rejects statements producing more
than 4096 grouping sets.

## GROUPING SETS

`GROUPING SETS` lists grouping sets explicitly and preserves their order:

```sql
select a, b, sum(x)
from t
group by grouping sets ((a, b), (a), ());
```

Top-level grouping elements are combined as a Cartesian product. For example:

```sql
select a, b, c, sum(x)
from t
group by a, grouping sets ((b), (c));
```

is equivalent to:

```
(a, b)
(a, c)
```

## ALL and DISTINCT

`ALL` preserves duplicate grouping sets. This is the default.

```sql
select a, sum(x)
from t
group by all grouping sets ((a), (a));
```

`DISTINCT` removes duplicate grouping sets before execution:

```sql
select a, sum(x)
from t
group by distinct grouping sets ((a), (a));
```

## GROUPING()

`GROUPING(<expr> [, <expr> ...])` may be used in the select list, `HAVING` and
`ORDER BY`. Its arguments must match grouping expressions from `ROLLUP`, `CUBE`
or `GROUPING SETS`.

It is not allowed in `WHERE`, in `GROUP BY`, in aggregate function arguments or
without an extended grouping context. The same rules apply to `GROUPING_ID`.

With one argument, `GROUPING` returns an `INTEGER` flag. Example distinguishing
a real `NULL` from a subtotal `NULL`:

```sql
create table t (a integer, x integer);

insert into t values (null, 10);
insert into t values (1, 20);

select
    a,
    grouping(a) as g_a,
    sum(x) as sx
from t
group by rollup(a)
order by grouping(a), a;
```

Result:

```
A       G_A     SX
<null>  0       10
1       0       20
<null>  1       30
```

Rows returned without `ORDER BY` do not have a guaranteed order.

Window functions are evaluated after the grouped result has been produced.
They may appear in the same query block as `ROLLUP`, `CUBE` or
`GROUPING SETS`, and they can also be applied in an outer query over a grouped
result.

With multiple arguments, `GROUPING` returns a `BIGINT` mask. The rightmost
argument is the least significant bit, the same as `GROUPING_ID`. This form
implements SQL optional feature T433, multi-argument `GROUPING`:

```sql
select
    department,
    product,
    grouping(department, product) as gid,
    sum(amount) as total_amount
from sales
group by rollup(department, product)
order by gid, department, product;
```

## GROUPING_ID()

`GROUPING_ID(<expr> [, <expr> ...])` returns the same `BIGINT` bit mask as
multi-argument `GROUPING`. For each argument, the corresponding bit is `0` when
the expression is present in the current grouping set and `1` when it is rolled
up. The last argument is the least significant bit.

Example:

```sql
select
    department,
    product,
    grouping_id(department, product) as gid,
    sum(amount) as total_amount
from sales
group by rollup(department, product)
order by gid, department, product;
```

For `ROLLUP(department, product)`, `GROUPING_ID(department, product)` returns:

```
0 for (department, product)
1 for (department)
3 for ()
```

`GROUPING_ID` accepts at most 63 arguments.
