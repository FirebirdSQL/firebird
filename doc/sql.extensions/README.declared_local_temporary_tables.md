# Declared Local Temporary Tables in Packages (FB 6.0)

Firebird 6.0 supports declaring local temporary tables in SQL packages.

These tables are declared in package metadata. Internally, they are stored as package-owned persistent temporary table
metadata, identified through `RDB$PACKAGE_NAME`. Their data remains temporary: transaction-local for
`ON COMMIT DELETE ROWS` and connection-local for `ON COMMIT PRESERVE ROWS`.

## Syntax

Declared local temporary tables can be used in package declarations.

```sql
CREATE PACKAGE <package_name>
AS
BEGIN
    [{<package_item> ;}...]
END

<package_item> ::=
    <declared_local_temporary_table> |
    <package_procedure_declaration> |
    <package_function_declaration>

<declared_local_temporary_table> ::=
    DECLARE LOCAL TEMPORARY TABLE <table_name>
    (
        <column_definition> [, ...]
    )
    [ON COMMIT {DELETE | PRESERVE} ROWS]
    [{[UNIQUE] [ASC | DESC] INDEX <index_name> (<column_name>)}...]
```

They can also appear in the declaration section of `CREATE PACKAGE BODY`.

```sql
CREATE PACKAGE BODY <package_name>
AS
BEGIN
    [{<package_item> ;}...]
END

<package_body_item> ::=
    <declared_local_temporary_table> |
    <package_procedure_definition> |
    <package_function_definition>
```

## Semantics

- `ON COMMIT DELETE ROWS` (default): rows are transaction-local and cleared at transaction end.
- `ON COMMIT PRESERVE ROWS`: rows are connection-local and preserved across transactions in the same attachment.

The table definition is part of package metadata and is persistent like other package members; table data remains
temporary.

## Visibility and Name Resolution

Visibility depends on where the table is declared:

- Tables declared in `CREATE PACKAGE` (header) are public package members.
- Tables declared in `CREATE PACKAGE BODY` are private to that package body.
- Unqualified references to a matching declared table name inside package routines resolve to the package table.

External SQL access rules:

- Header tables can be accessed externally as `[{schema_name}.package_name.table_name` (for example, `pkg.t_pub`).
- Body tables cannot be accessed externally and are only valid inside routines of the same package body.

Index DDL rules:

- Packaged declared local temporary table indexes must be declared inline in `DECLARE LOCAL TEMPORARY TABLE`.
- Standalone index DDL commands are not allowed for packaged tables:
  `CREATE INDEX`, `ALTER INDEX`, `DROP INDEX`, `SET STATISTICS INDEX`.

## Name Isolation

Declared local temporary table names are isolated by package context.

- Different packages may declare tables with the same name.
- A package table name may also match a regular table name in the same schema.

Resolution inside package routines prefers the package-local declaration.

## Dependencies and DDL lifecycle

Declared local temporary tables participate in package dependency handling.

- Package routines depending on declared table columns are tracked as package dependencies.
- `DROP PACKAGE` removes package body/header dependencies and package members (routines and declared tables) in package
  scope.
- `ALTER PACKAGE` and `CREATE OR ALTER PACKAGE` recreate packaged declared local temporary tables that belong to the
  package header.
- `RECREATE PACKAGE BODY` and `CREATE OR ALTER PACKAGE BODY` recreate packaged declared local temporary tables that
  belong to the package body.

## System metadata changes

Packaged declared local temporary tables add package ownership and visibility information to system metadata. Tools
that inspect metadata should use these columns when present.

| Table                | Column             | Meaning                                                         |
|----------------------|--------------------|-----------------------------------------------------------------|
| `RDB$RELATIONS`      | `RDB$PACKAGE_NAME` | Owning package of the declared temporary table                  |
| `RDB$RELATIONS`      | `RDB$PRIVATE_FLAG` | `PUBLIC` (`0`) for header tables, `PRIVATE` (`1`) for body tables |
| `RDB$RELATION_FIELDS`| `RDB$PACKAGE_NAME` | Owning package of the declared temporary table columns          |
| `RDB$INDICES`        | `RDB$PACKAGE_NAME` | Owning package of inline indexes declared for packaged tables   |
| `RDB$INDEX_SEGMENTS` | `RDB$PACKAGE_NAME` | Owning package of the packaged table index segments             |
| `MON$TABLE_STATS`    | `MON$PACKAGE_NAME` | Owning package reported in runtime table statistics             |

## Example

```sql
set term !;

recreate package pkg
as
begin
    declare local temporary table t_pub(
        id integer
    ) on commit preserve rows
      index idx_t_pub_id (id);

    procedure p1(n integer);
    procedure p2 returns (n integer);
end!

create package body pkg
as
begin
    declare local temporary table t_priv(
        id integer
    ) on commit preserve rows
      unique index uq_t_priv_id (id);

    procedure p1(n integer)
    as
    begin
        insert into t_pub(id) values (:n);
        insert into t_priv(id) values (:n);
    end

    procedure p2 returns (n integer)
    as
    begin
        for select id from t_pub into :n do
            suspend;
    end
end!

set term ;!

-- use package routines
execute procedure pkg.p1(10);
select * from pkg.p2;

-- header-declared table: allowed
select * from pkg.t_pub;

-- body-declared table: not allowed
-- select * from pkg.t_priv;

-- not allowed for packaged tables:
-- create index idx_cmd on pkg.t_pub(id);
-- alter index idx_t_pub_id active;
-- drop index idx_t_pub_id;
```

## Notes

- This feature is distinct from SQL-created local temporary tables (`CREATE LOCAL TEMPORARY TABLE ...`), which are
  attachment-private DDL objects.
- Packaged declared temporary tables are not attachment-private created LTTs. They use persistent temporary-table
  metadata associated with the package through `RDB$PACKAGE_NAME`.
- Declared package temporary tables are package DDL items and follow package compilation, visibility, and dependency
  rules.
