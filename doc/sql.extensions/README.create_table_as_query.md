# `CREATE TABLE ... AS <query>` (FB 6.0)

Tables may be created from a query result using the following syntax:

```sql
CREATE [{GLOBAL | LOCAL} TEMPORARY] TABLE [IF NOT EXISTS] <table name>
  [ (<column name> [, <column name> ...]) ]
  AS { (<query expression>) | <query expression> }
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

## Datatypes and Domains

When a select list item is a direct reference to a source table or view column, the new column copies the source
column's exact datatype instead of one derived only from the query's runtime result (which would lose information
such as declared `NUMERIC`/`DECIMAL` precision).

* If the source column is based on a named domain, the new column references that domain directly, as if it had
  been declared `<column name> <domain name>`.
* If the source column is based on an auto-generated (implicit) domain, its exact type (precision/scale, character
  set, collation, etc.) is copied.

For any other select list item (an expression, a literal, or an aggregate, for example), the new column's datatype
is derived from the query result.

## Character Sets

For `CHAR` and `VARCHAR` columns not covered by the direct column-reference rule above, the database default
character set is not used. The character set and collation of the new column are copied from the corresponding
query expression.

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

CREATE TABLE high_salary AS
  (WITH avg_salary AS (SELECT AVG(salary) AS avg_salary FROM employee)
   SELECT e.emp_no, e.salary
     FROM employee e, avg_salary a
     WHERE e.salary > a.avg_salary);
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
