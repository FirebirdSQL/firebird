# Custom Aggregate Functions

Custom aggregate functions extend `CREATE FUNCTION` with an `AGGREGATE` form implemented in PSQL
or by an external routine engine.

They allow users to define aggregate routines with their own per-group or per-window-frame local state, step logic and
final result calculation.

## Syntax

```sql
{ recreate | { create [or alter] } | alter } aggregate function <function-name>
    [ ( <input-parameter> [, <input-parameter> ...] ) ]
    returns <return-type>
    [sql security {invoker | definer}]
as
    [<local declarations>]
begin
    [on start do <statement>]

    on accumulate do <statement>

    on group do <statement>

    [on finish do <statement>]
end
```

External aggregate functions use the same header followed by:

```sql
external name '<external-name>' engine <engine-name>
```

Local aggregate sub-functions may be declared inside PSQL local declarations:

```sql
declare aggregate function <function-name>
    [ ( <input-parameter> [, <input-parameter> ...] ) ]
    returns <return-type>;

declare aggregate function <function-name>
    [ ( <input-parameter> [, <input-parameter> ...] ) ]
    returns <return-type>
as
    [<local declarations>]
begin
    [on start do <statement>]

    on accumulate do <statement>

    on group do <statement>

    [on finish do <statement>]
end
```

Where:

- `ON START DO` is optional
- `ON ACCUMULATE DO` is required
- `ON GROUP DO` is required
- `ON FINISH DO` is optional
- the `ON ... DO` sections apply only to the PSQL form

The sections must appear in this order: optional `ON START DO`, then `ON ACCUMULATE DO`, then
required `ON GROUP DO`, then optional `ON FINISH DO`.

## Execution Model

An aggregate function is invoked once for each aggregate instance:
- once per group in grouped aggregation
- once per window frame in windowed aggregation

For each instance:
1. A private execution context is allocated
2. Local variables declared in the routine body start as `NULL`
3. `ON START DO` is executed once, if present
4. `ON ACCUMULATE DO` is executed for each qualifying row
5. `ON GROUP DO` is executed to compute the current aggregate result
6. `ON FINISH DO` is executed once when the aggregate instance is retired and may perform destructive cleanup

Local variables declared in the routine body are visible to `ON START DO`, `ON ACCUMULATE DO` and
`ON GROUP DO`, and if present `ON FINISH DO`, and persist for the full lifetime of the same aggregate instance.

Formal input parameters of the aggregate function are visible only to `ON ACCUMULATE DO`. They are not visible to
`ON START DO`, `ON GROUP DO` or `ON FINISH DO`.

`ON GROUP DO` is repeatable and non-terminal. In windowed execution, the engine may call `ON GROUP DO`,
then continue the same aggregate instance with more `ON ACCUMULATE DO` calls, then call `ON GROUP DO` again.
Use `ON FINISH DO` for terminal destructive cleanup.

## Invocation

Aggregate functions are called using normal function call syntax:

```sql
<aggregate-name>(<args>)
<aggregate-name>(<args>) filter (where <condition>)
<aggregate-name>(<args>) over (<window-specification>)
```

They are valid only in aggregate-capable SQL contexts, such as:
- the select list of aggregate queries
- `HAVING`
- `ORDER BY`
- windowed `OVER (...)` usage

They are not valid as scalar function calls.

## Notes

- Custom aggregate functions share the normal function namespace.

## Restrictions

- `DETERMINISTIC` is not allowed for aggregate functions
- only `ON ACCUMULATE DO` may reference aggregate function input parameters
- `ON START DO`, `ON ACCUMULATE DO` and `ON FINISH DO` may not use `RETURN`; use `EXIT` for an early exit instead
- `ON GROUP DO` must use `RETURN <value>` to produce the aggregate result
- `ON GROUP DO` may not use `EXIT`

## `RETURN` And `EXIT` Semantics

- In `ON START DO`, `ON ACCUMULATE DO` and `ON FINISH DO`, `EXIT;` exits only the current section early
- In those three sections, any form of `RETURN` is invalid
- In `ON GROUP DO`, `RETURN <value>;` produces the aggregate result
- In `ON GROUP DO`, `EXIT;` is invalid

## External Aggregate Functions

External aggregate functions follow the same phases as PSQL aggregate functions:
 - START is called once before any row is accumulated;
 - ACCUMULATE is called once per qualifying input row;
 - GROUP writes the current aggregate result and may be called more than once;
 - FINISH is called when the aggregate instance is discarded.

Example UDR aggregate function:

    /***
    create aggregate function sum_positive (
        n integer
    ) returns integer
        external name 'udrcpp_example!sum_positive'
        engine udr;
    ***/
    FB_UDR_BEGIN_AGGREGATE_FUNCTION(sum_positive)
        FB_UDR_MESSAGE(InMessage,
            (FB_INTEGER, n)
        );

        FB_UDR_MESSAGE(OutMessage,
            (FB_INTEGER, result)
        );

        FB_UDR_CONSTRUCTOR
            // , total(0)
        {
        }

        FB_UDR_START_AGGREGATE_FUNCTION
        {
            // total = 0;
        }

        FB_UDR_ACCUMULATE_AGGREGATE_FUNCTION
        {
            if (!in->nNull && in->n > 0)
                total += in->n;
        }

        FB_UDR_GROUP_AGGREGATE_FUNCTION
        {
            out->resultNull = FB_FALSE;
            out->result = total;
        }

        FB_UDR_FINISH_AGGREGATE_FUNCTION
        {
            // total = 0;
        }

        SINT64 total = 0;
    FB_UDR_END_AGGREGATE_FUNCTION

## Examples

### Sum Ignoring `NULL`

```sql
set term ^;

create or alter aggregate function sum_i(v integer)
    returns bigint
as
    declare total bigint;
    declare seen boolean;
begin
    on start do
    begin
        total = 0;
        seen = false;
    end

    on accumulate do
    begin
        if (v is not null) then
        begin
            total = total + v;
            seen = true;
        end
    end

    on group do
    begin
        if (not seen) then
            return null;

        return total;
    end
end^

set term ;^
```

Usage:

```sql
select sum_i(salary)
    from employee;
```

### Count Non-`NULL` Values Without `ON START DO`

```sql
set term ^;

create or alter aggregate function count_not_null(v integer)
    returns bigint
as
    declare cnt bigint;
begin
    on accumulate do
    begin
        if (v is not null) then
        begin
            if (cnt is null) then
                cnt = 0;

            cnt = cnt + 1;
        end
    end

    on group do
    begin
        return coalesce(cnt, 0);
    end
end^

set term ;^
```

Usage:

```sql
select department,
       count_not_null(salary) as employees_with_salary
    from employee
    group by department;
```

### Window Usage

```sql
select department,
       salary,
       sum_i(salary) over (partition by department) as dept_sum
    from employee;
```

### Using `FILTER`

```sql
select sum_i(amount) filter (where status = 'A') as approved_amount,
       sum_i(amount) filter (where status = 'P') as pending_amount
    from payments;
```

### Packaged Aggregate Function

```sql
set term ^;

create package agg_pkg
as
begin
    aggregate function sum_i(v integer) returns bigint;
end^

create package body agg_pkg
as
begin
    aggregate function sum_i(v integer)
        returns bigint
    as
        declare total bigint;
    begin
        on accumulate do
            total = coalesce(total, 0) + coalesce(v, 0);

        on group do
            return total;
    end
end^

set term ;^
```

Usage:

```sql
select agg_pkg.sum_i(salary)
    from employee;
```

### Local Aggregate Sub-Function

```sql
set term ^;

execute block returns (total bigint)
as
    declare aggregate function sum_i(v integer)
        returns bigint
    as
        declare state bigint;
    begin
        on start do
            state = 0;

        on accumulate do
        begin
            if (v is not null) then
                state = state + v;
        end

        on group do
            return state;
    end
begin
    select sum_i(n)
        from (
            select 1 n from rdb$database
            union all
            select 2 n from rdb$database
        )
        into total;

    suspend;
end^

set term ;^
```
