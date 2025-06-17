# Range-based FOR statement (FB 6.0)

## Description

The range-based `FOR` statement is used to iterate over a range of numeric values. The iteration is performed in
increasing order when used with `TO` clause and in decreasing order when used with `DOWNTO` clause.

## Syntax

```
[<label> :]
FOR <variable> = <initial value> {TO | DOWNTO} <final value> [BY <by value>] DO
    <statement>
```

## Notes

- If omitted, `<by value>` is `1`.
- `<variable>` also accepts parameters.
- `<variable>`, `<initial value>`, `<final value>` and `<by value>` must be expressions of exact numeric types.
- `BREAK [<label>]` can be used to exit the loop.
- `CONTINUE [<label>]` can be used to restart the next loop iteration.
- `<variable>` can be assigned by user code inside the loop.

## Execution

- `<initial value>` is evaluated and assigned to `<variable>`. If it is `NULL`, the loop is not executed.
- `<final value>` is evaluated and assigned to a temporary variable. If it is `NULL`, the loop is not executed.
- `<by value>` (or its default `1` value) is evaluated and assigned to a temporary variable.
  If it is `NULL`, the loop is not executed. If it is zero or negative, an error is raised.
- Loop starts:
  - If it is not the first iteration:
    - If `<variable>` is `NULL`, the loop is exited.
    - `<variable>` is incremented (`TO`) or decremented (`DOWNTO`) by the cached `<by value>`.
  - `<variable>` is compared (less than or equal for `TO` or greater than or equal for `DOWNTO`) to the cached
    `<final value>` and if it is out of range, the loop is exited.
  - Loop continues to the next iteration.

## Examples

```
execute block returns (out integer)
as
begin
    for out = 1 to 3 do
        suspend;
end

/* Result:
1
2
3
*/
```

```
execute block returns (out integer)
as
begin
    for out = 9 downto 7 do
        suspend;
end

/* Result:
9
8
7
*/
```

```
execute block returns (out integer)
as
begin
    for out = 5 to 3 do
        suspend;
end

/* Result:
*/
```

```
execute block returns (out numeric(5,2))
as
begin
    for out = 9 downto 7 by 0.5 do
        suspend;
end

/* Result:
9.00
8.50
8.00
7.50
7.00
*/
```

```
execute block
as
    declare i integer;
begin
    for i = 1 to 10 do
    begin
        insert into table1 values (i);
        insert into table2 values (i);
    end
end

/* Result:
10 records inserted into table1
10 records inserted into table2
*/
```
