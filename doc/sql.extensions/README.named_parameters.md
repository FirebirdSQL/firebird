# Named parameters

## Overview

Besides the positional `?` placeholder, DSQL statements may use named parameters in the form `:name`.
A name may appear any number of times in the statement, but it is a single input parameter: the client
binds its value only once.

```sql
select name
    from employee
    where name = :name or full_name = :name
```

The statement above has exactly one input parameter (`NAME`).

## Rules

- `:name` follows the identifier rules: unquoted names are case-insensitive (stored in upper case) and quoted
  names (`:"name"`) are case-sensitive.
- Input parameters are numbered in the order of the first appearance of each name. In the API they are still
  bound by position, and the name of each parameter is reported by the input metadata
  (`IMessageMetadata::getField` / `getAlias`, or `sqlname` / `aliasname` in XSQLDA).
- Named and positional (`?`) parameters cannot be mixed in the same statement. Doing so raises the error
  `Named and positional parameters cannot be mixed`.
- Named parameters are recognized in DSQL statements (`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`,
  `UPDATE OR INSERT`, `EXECUTE PROCEDURE`, `CALL`, etc), including `FIRST :n` and `SKIP :n`. Inside PSQL
  code (`EXECUTE BLOCK` bodies, stored procedures, triggers, etc) `:name` continues to be a reference to a
  variable or parameter.
- When the data type of a named parameter cannot be determined, the error reports its name.
- They cannot be used in DDL statements.
- `FIRST :n` and `SKIP :n` also accept a variable or parameter in PSQL code.
- In `CALL`, `:name` used as an output argument is equivalent to `?` and is not an input parameter:
  `call p(:in1, :out1)`. A name used as output argument cannot be repeated nor be used as an input.
- The same named parameter, used with the same expression, matches itself in `GROUP BY`, for example
  `select cast(:a as integer) + id, count(*) from t group by cast(:a as integer) + id`.
  This is not possible with `?`, as each one is a different parameter.

## Data type

When the same parameter is used in different contexts, each context may infer a different data type or size
for it. The parameter receives a data type compatible with all of them, using the same rules as `UNION`
(for example, the largest string length or a numeric type able to hold all values).

In the example above, with `NAME VARCHAR(10)` and `FULL_NAME VARCHAR(40)`, the parameter is described as
`VARCHAR(40)`.

When the inferred data types are not compatible (for example `INTEGER` and `DATE`), an error is raised.
Use `CAST(:name AS <type>)` to make the type explicit.

## EXECUTE BLOCK and USING

The input parameters declared in the header of `EXECUTE BLOCK` and `USING` may be bound to a named parameter
instead of `?`:

```sql
execute block (x integer = :p)
    returns (r integer)
as
begin
    r = x * 10;
    suspend;
end
```

```sql
using (x integer = :p)
do select :x + 1 from rdb$database
```

In these statements the input parameter is named `P`, while `X` is the variable used inside the statement.

In the `DO` part of `USING`, `:name` refers to a declared parameter when one has that name. Otherwise it is
a named parameter, which may also be the one bound in the header:

```sql
using (x integer = :p)
do select :x + :p from rdb$database where :q = 2
```

This statement has the input parameters `P` and `Q`.

## EXECUTE STATEMENT

The engine reports the `fb_feature_named_parameters` feature (`fb_info_features`). When `EXECUTE STATEMENT`
with named arguments targets a server that has it, the SQL text is sent unchanged and the arguments are
matched with the names reported by the input metadata. Otherwise, the old client-side preprocessing that
replaces `:name` by `?` is used.

Note the compatibility difference: with the old preprocessing, each occurrence of a name became a separate `?`
with its own inferred data type. With named parameters in the server, repeated occurrences are one parameter
with a merged data type, so a statement that uses the same name in incompatible contexts (for example
`d = :v or id = :v`, with `d` a `DATE` and `id` an `INTEGER`) now raises an error. Use `CAST(:v AS <type>)` or
different names in such cases. On the other hand, text that uses `:name` and is executed with positional
arguments is now accepted.

```sql
execute block returns (cnt integer)
as
begin
    execute statement ('select count(*) from employee where name = :name or full_name = :name')
        (name := 'John') into cnt;
    suspend;
end
```
