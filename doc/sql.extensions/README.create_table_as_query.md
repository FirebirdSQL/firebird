# `CREATE TABLE ... AS <query>` (FB 6.0)

Tables may be created from a query result using the following syntax:

```sql
CREATE [{GLOBAL | LOCAL} TEMPORARY TABLE] [IF NOT EXISTS] TABLE <table name>
  [ (<column name> [, <column name> ...]) ]
  AS <query expression>
  [WITH [NO] DATA]
  [ON COMMIT {DELETE | PRESERVE} ROWS]
```

The new table columns are derived from the query select list. If a column name list is specified, it must have
the same number of columns as the query result.

If no column name list is specified, column names are taken from the query output names. Unnamed expressions must
be explicitly aliased.

`WITH DATA` is the default and inserts the query result into the newly created table. `WITH NO DATA` creates only
the table definition.

For global and local temporary tables, normal temporary-table data lifetime rules apply. Package temporary tables
do not support this syntax.

## Character Sets

For `CHAR` and `VARCHAR` columns, the database default character set is not used. The character set and collation
of the new column are copied from the corresponding query expression.

String literals use the connection character set, so columns created from string literals inherit the connection
character set.

## Examples

```sql
CREATE TABLE employee_copy AS
  SELECT emp_no, first_name, last_name
    FROM employee;

CREATE TABLE employee_names (id, full_name) AS
  SELECT emp_no, first_name || ' ' || last_name
    FROM employee
  WITH NO DATA;

CREATE GLOBAL TEMPORARY TABLE session_report AS
  SELECT emp_no, salary
    FROM employee
  WITH DATA
  ON COMMIT PRESERVE ROWS;

CREATE LOCAL TEMPORARY TABLE tx_work AS
  SELECT emp_no, salary
    FROM employee
  WITH NO DATA;
```
