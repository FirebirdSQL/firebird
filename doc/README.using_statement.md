# The `USING` Statement

## Overview

The `USING` statement is a new DSQL extension designed to bridge the gap between standard DSQL statements and the
powerful but verbose `EXECUTE BLOCK`.

When adapting a standard DSQL command to use `EXECUTE BLOCK` (for instance, to utilize sub-routines or reuse a single
input parameter in multiple places), the developer is currently forced to explicitly declare all input parameters and,
more tediously, all output fields.

The `USING` statement simplifies this workflow. It provides the ability to declare parameters and sub-routines while
allowing the engine to infer outputs automatically from the contained SQL command.

## Syntax

```sql
USING [ ( <input_parameter_list> ) ]
    [ <subroutines> ]
DO <sql_command>
```

**Note:** At least one of `<input_parameter_list>` or `<subroutines>` must be present. A `USING ... DO` statement
without parameters and without subroutines is invalid.

### Components

*  **`<input_parameter_list>`**: A strictly typed list of parameters. These can be bound to values using the `?`
   placeholder.
*  **`<subroutines>`**: Standard PSQL function or procedure declarations.
*  **`<sql_command>`**: The DSQL statement to execute. Supported statements include:
   *   `SELECT`
   *   `INSERT` (with or without `RETURNING`)
   *   `UPDATE` (with or without `RETURNING`)
   *   `UPDATE OR INSERT` (with or without `RETURNING`)
   *   `DELETE` (with or without `RETURNING`)
   *   `MERGE` (with or without `RETURNING`)
   *   `CALL`
   *   `EXECUTE PROCEDURE`

## Key Features

1. **Inferred Outputs**: Unlike `EXECUTE BLOCK`, you do not need to explicitly declare a `RETURNS (...)` clause. The
   output columns are automatically inferred from the `<sql_command>` in the `DO` clause.
2. **Statement Type Transparency**: The API returns the statement type of the inner `<sql_command>` (e.g., if the
   inner command is a `SELECT`, the client sees a `SELECT` statement).
3. **Parameter Reuse**: Input parameters declared in the `USING` clause can be used multiple times within the script
   using named references (e.g., `:p1`), while only requiring a single bind from the client application.
4. **Mixed Parameter Binding**: You can mix declared parameters (bound via `?` in the declaration) and direct
   positional parameters (using `?` inside the `DO` command).

## Examples

### 1. Basic Parameter Reuse and Subroutines

This example demonstrates declaring typed parameters, defining local functions/procedures, and using them in a query.

```sql
using (p1 integer = ?, p2 integer = ?)
    -- Declare a local function
    declare function subfunc (i1 integer) returns integer
    as
    begin
        return i1;
    end

    -- Declare a local procedure
    declare procedure subproc (i1 integer) returns (o1 integer)
    as
    begin
        o1 = i1;
        suspend;
    end
do
-- The main query
select subfunc(:p1) + o1
    from subproc(:p2 + ?)
```

In this scenario:
1. The client binds values to `p1` and `p2`.
2. The client binds a third value to the `?` inside the `DO` clause.
3. The result set structure is inferred from the `SELECT` statement.

### 2. Simplifying Parameter Reuse

Without `USING`, inserting the same bind value into multiple columns requires sending the data twice.

**Standard DSQL:**
```sql
insert into generic_table (col_a, col_b) values (?, ?);
-- Client must bind: [100, 100]
```

**With `USING`:**
```sql
using (val integer = ?)
do insert into generic_table (col_a, col_b) values (:val, :val);
-- Client binds: [100]
```

## Comparison

| Feature                 | Standard DSQL                   | `EXECUTE BLOCK`                               | `USING`                                       |
| :---------------------- | :------------------------------ | :-------------------------------------------- | :-------------------------------------------- |
| **Subroutines**         | No                              | Yes                                           | Yes                                           |
| **Input Declarations**  | Implicit (Positional)           | Explicit                                      | Hybrid (implicit and explicit)                |
| **Output Declarations** | Inferred                        | Explicit (`RETURNS`)                          | Inferred                                      |
| **Verbosity**           | Low                             | High                                          | Medium                                        |
| **Use Case**            | Simple queries                  | Complex logic, loops, no result set inference | Reusing params, subroutines, standard queries |
