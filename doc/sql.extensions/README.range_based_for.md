# Range-based FOR statement (FB 6.0)

## Description

The range-based `FOR` statement iterates over a range of numeric values. The iteration is performed in
increasing order with the `TO` clause, or in decreasing order with the `DOWNTO` clause.

## Syntax

```
[<label> :]
FOR <variable> = <initial value> {TO | DOWNTO} <final value> [BY <step value>] DO
    <compound statement>
```

## Notes

- If omitted, `<step value>` is `1`.
- `<variable>` also accepts parameters.
- `<variable>`, `<initial value>`, `<final value>` and `<step value>` must be expressions of exact numeric types.
- `BREAK` or `LEAVE [<label>]` exits the loop.
- `CONTINUE [<label>]` skips the remainder of the loop body and starts the next iteration.
- `<variable>` can be modified by the loop body.

## Execution

- `<initial value>` is evaluated and assigned to `<variable>`. If `NULL`, the loop exits immediately.
- `<final value>` is evaluated and assigned to a temporary variable. If `NULL`, the loop exits immediately.
- `<step value>` (or its default value `1`) is evaluated and assigned to a temporary variable.
  If `NULL`, the loop exits immediately. If zero or negative, an error is raised.
- Loop starts:
  - If not the first iteration:
    - If `<variable>` is `NULL`, the loop exits.
    - `<variable>` is incremented (`TO`) or decremented (`DOWNTO`) by the cached `<step value>`.
  - `<variable>` is compared (less than or equal for `TO` or greater than or equal for `DOWNTO`) to the cached
    `<final value>` and if false, the loop exits.
  - The body (`<compund statement>`) of the loop is executed.
  - The loop continues to the next iteration.

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
