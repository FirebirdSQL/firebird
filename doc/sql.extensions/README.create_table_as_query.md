# `CREATE TABLE ... AS <query>` (FB 6.0)

Tables may be created from a query result using the following syntax:

```sql
CREATE [{GLOBAL | LOCAL} TEMPORARY] TABLE [IF NOT EXISTS] <table name>
  [ (<column name> [, <column name> ...]) ]
  AS <query expression>
  [WITH [NO] DATA]
  [ON COMMIT {DELETE | PRESERVE} ROWS]
```

The new table columns are derived from the query select list. If a column name list is specified, it must have
the same number of columns as the query result.

If no column name list is specified, column names are taken from the query output names. Unnamed expressions must
be explicitly aliased.

`WITH DATA` is the default and inserts the query result into the newly created table in the same transaction.
`WITH NO DATA` creates only the table definition.

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

## ISQL behavior

ISQL in AUTODDL mode (the default) uses separate transactions for DDL and DML statements.
When `CREATE TABLE ... AS <query>` is executed with `WITH DATA`, the table creation and data population occur in the
DDL transaction.

For regular tables, the inserted rows are not visible to the DML transaction until the DDL transaction is committed.
For temporary tables, this behavior is even more surprising because the rows belong to the DDL transaction and are
therefore not visible to the DML transaction at all.

For example:

```sql
SQL> CREATE TABLE T1 AS SELECT 1 A FROM RDB$DATABASE;
SQL> SELECT * FROM T1;

SQL> COMMIT;
SQL> SELECT * FROM T1;

           A
============
           1
```

With a temporary table:

```sql
SQL> CREATE GLOBAL TEMPORARY TABLE T1 AS SELECT 1 A FROM RDB$DATABASE;
SQL> SELECT * FROM T1;

SQL> COMMIT;
SQL> SELECT * FROM T1;
```

The table exists, but no rows are returned because the data was inserted in the DDL transaction and is not visible to
the DML transaction.

To avoid this behavior, disable AUTODDL before executing the statement:

```sql
SQL> SET AUTODDL OFF;

SQL> CREATE GLOBAL TEMPORARY TABLE T1 AS SELECT 1 A FROM RDB$DATABASE;
SQL> SELECT * FROM T1;

           A
============
           1
```

When AUTODDL is disabled, both the table creation and data population occur in the current transaction, making the
inserted rows immediately visible.
