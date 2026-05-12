# Packaged Temporary Tables (FB 6.0)

Firebird 6.0 supports declaring temporary tables in SQL packages.

Internally, they are stored as package-owned persistent temporary table metadata, identified through `RDB$PACKAGE_NAME`.
Their data remains temporary: transaction-local for `ON COMMIT DELETE ROWS` and connection-local for
`ON COMMIT PRESERVE ROWS`.

## Syntax

Packaged temporary tables can be used in package header and body.

```sql
CREATE PACKAGE <package_name>
AS
BEGIN
    [{<package_item> ;}...]
END

<package_item> ::=
    <package_temporary_table_declaration> |
    <package_procedure_declaration> |
    <package_function_declaration> |
	<package_constant_declaration>

<package_temporary_table_declaration> ::=
    TEMPORARY TABLE <table_name>
    (
        <column_definition> [, ...]
    )
    [ON COMMIT {DELETE | PRESERVE} ROWS]
    [{[UNIQUE] [ASC | DESC] INDEX <index_name> (<column_name>)}...]
```

```sql
CREATE PACKAGE BODY <package_name>
AS
BEGIN
    [{<package_item> ;}...]
END

<package_body_item> ::=
    <package_temporary_table_declaration> |
    <package_procedure_definition> |
    <package_function_definition> |
	<package_constant_declaration>
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

- Header tables can be accessed externally as `package_name.table_name` or `package_name%package.table_name`
  (for example, `pkg.t_pub` or `pkg%package.t_pub`).
- Schema-qualified packaged table access uses the three-part form `{schema_name}.package_name.table_name`.
- Body tables cannot be accessed externally and are only valid inside routines of the same package body, including
  when using `%package`.

Permissions:

- Packaged temporary table permissions are attached to the package, not to individual packaged tables.
- Direct access to public header tables requires the appropriate package privilege for the requested operation, such
  as `SELECT`, `INSERT`, `UPDATE` or `DELETE`.
- Private body tables cannot be made externally accessible with package grants.
- If the package is created in a non-public schema, the caller also needs `USAGE` on that schema.

Example:

```sql
grant select, insert on package pkg to user some_user;
```

Index DDL rules:

- Packaged temporary table indexes must be declared inline in `TEMPORARY TABLE`.
- Standalone index DDL commands are not allowed for packaged tables:
  `CREATE INDEX`, `ALTER INDEX`, `DROP INDEX`, `SET STATISTICS INDEX`.

`COMMENT ON` support:

- `COMMENT ON TABLE package_name.table_name IS ...` is supported for packaged temporary tables.
- `COMMENT ON TABLE package_name%package.table_name IS ...` is also supported.
- `COMMENT ON COLUMN package_name.table_name.column_name IS ...` is supported for columns of packaged
  temporary tables.
- `COMMENT ON COLUMN package_name%package.table_name.column_name IS ...` is also supported.
- This applies to both header-declared public tables and body-declared private tables.
- Descriptions are stored in the regular metadata fields
  `RDB$RELATIONS.RDB$DESCRIPTION` and `RDB$RELATION_FIELDS.RDB$DESCRIPTION`.

## Name Isolation

The table names are isolated by package context.

- Different packages may declare tables with the same name.
- A package table name may also match a regular table name in the same schema.

Resolution inside package routines prefers the package-local declaration.

## Dependencies and DDL lifecycle

`DROP PACKAGE` removes package body/header dependencies and package members (routines and declared tables) in package
scope.

`ALTER PACKAGE` and `CREATE OR ALTER PACKAGE` recreate packaged declared local temporary tables that belong to the
package header.
Existing private packaged tables from the body definition are dropped.

`RECREATE PACKAGE BODY` and `CREATE OR ALTER PACKAGE BODY` recreate packaged declared local temporary tables that
belong to the package body.
Existing private packaged tables from the previous body definition are dropped before the new body tables are created.

## System metadata changes

Packaged temporary tables add package ownership and visibility information to system metadata.
Tools that inspect metadata should use these columns when present.

| Table                | Column             | Meaning                                                           |
|----------------------|--------------------|-------------------------------------------------------------------|
| `RDB$RELATIONS`      | `RDB$PACKAGE_NAME` | Owning package of the declared temporary table                    |
| `RDB$RELATIONS`      | `RDB$PRIVATE_FLAG` | `PUBLIC` (`0`) for header tables, `PRIVATE` (`1`) for body tables |
| `RDB$RELATION_FIELDS`| `RDB$PACKAGE_NAME` | Owning package of the declared temporary table columns            |
| `RDB$INDICES`        | `RDB$PACKAGE_NAME` | Owning package of inline indexes declared for packaged tables     |
| `RDB$INDEX_SEGMENTS` | `RDB$PACKAGE_NAME` | Owning package of the packaged table index segments               |
| `MON$TABLE_STATS`    | `MON$PACKAGE_NAME` | Owning package reported in runtime table statistics               |

In monitoring, packaged temporary tables are reported as `GLOBAL TEMPORARY` in `MON$TABLE_STATS.MON$TABLE_TYPE`.

## Example

```sql
set term !;

recreate package pkg
as
begin
    temporary table t_pub(
        id integer
    ) on commit preserve rows
      index idx_t_pub_id (id);

    procedure p1(n integer);
    procedure p2 returns (n integer);
end!

create package body pkg
as
begin
    temporary table t_priv(
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
select * from pkg%package.t_pub;

-- body-declared table: not allowed
-- select * from pkg.t_priv;
-- select * from pkg%package.t_priv;

comment on table pkg.t_pub is 'Public packaged temporary table';
comment on column pkg.t_pub.id is 'Identifier in public packaged temporary table';
comment on table pkg.t_priv is 'Private packaged temporary table';
comment on column pkg.t_priv.id is 'Identifier in private packaged temporary table';
comment on table pkg%package.t_pub is 'Public packaged temporary table via %package';
comment on column pkg%package.t_pub.id is 'Identifier via %package';
comment on table pkg%package.t_priv is 'Private packaged temporary table via %package';
comment on column pkg%package.t_priv.id is 'Identifier in private table via %package';

-- not allowed for packaged tables:
-- create index idx_cmd on pkg.t_pub(id);
-- alter index idx_t_pub_id active;
-- drop index idx_t_pub_id;
```

## Notes

- This feature is distinct from SQL-created local temporary tables (`CREATE LOCAL TEMPORARY TABLE ...`), which are
  attachment-private DDL objects.
- Packaged temporary tables are not attachment-private created LTTs. They use persistent temporary-table metadata
  associated with the package through `RDB$PACKAGE_NAME`.
- Packaged temporary tables follow package compilation, visibility, and dependency rules.
