# Schemas (FB 6.0)

Firebird 6.0 introduces support for schemas in the database. Schemas are not an optional feature, so every Firebird 6 
database has at least a `SYSTEM` schema, reserved for Firebird system objects (`RDB$*` and `MON$*`).

User objects live in different schemas, which may be the automatically created `PUBLIC` schema or user-defined ones. It 
is not allowed (except for indexes) to create or modify objects in the `SYSTEM` schema.

This documentation explains how schemas work in Firebird, how to use them, and what may differ when migrating a 
database from previous versions to Firebird 6.

## Why schemas?

Schemas allow the logical grouping of database objects (such as tables, views, and indexes), providing a clear 
structure to the database. They are primarily used for two purposes.

### Schemas for database object organization

Schemas help in organizing database objects modularly, making the database easier to manage and maintain. By dividing 
the database into different schemas, developers and administrators can focus on specific areas, improving team 
scalability and reducing complexity.

For example, the `SYSTEM` schema separates objects created by two distinct groups (the Firebird DBMS core team and the 
Firebird users). Firebird users can organize objects in custom schemas like `FINANCE` and `MARKETPLACE`.

### Schemas for data isolation

In multi-tenant applications, schemas can provide data isolation for different clients or tenants. By assigning a 
unique schema to each tenant, tables and other objects can share the same names in different schemas, reducing data 
leakage risks and sometimes improving performance. Applications can set the schema search path for the current selected 
customer.

This approach simplifies database management and scaling since each tenant's data is isolated, making maintenance, 
updates, and backups straightforward. Example schema names could be `CUSTOMER_1` and `CUSTOMER_2`.

## Schema-less and schema-bound objects

Database objects fall into two categories: schema-less and schema-bound.

### Schema-less objects

These objects exist outside schemas and function as before:
- Users
- Roles
- Blob filters
- Schemas

### Schema-bound objects

These objects are always contained within a schema:
- Tables
- Views
- Triggers
- Procedures
- Exceptions
- Domains
- Indexes
- Character sets
- Sequences / Generators
- Functions
- Collations
- Packages

Some objects are highly dependent on their parents, like table-based triggers and indexes depending on the table. 
In this case the child object always resides in the same schema of its parent.

## Search Path

A Firebird session is started with an initial search path, a list of schemas used to resolve unqualified object names. 
By default, this path is set to `PUBLIC, SYSTEM`, but it can be customized using the `isc_dpb_search_path` parameter 
in the API.

The initial search path serves as the basis for the current search path, which is actively used during object 
resolution. The current search path can be dynamically updated using the `SET SEARCH_PATH TO` statement. If needed, 
you can reset the current search path to its initial configuration with the `ALTER SESSION RESET` statement.

Nonexistent schemas can be included in the search path but are ignored during name resolution.

The first existing schema in the search path is referred to as the **current schema** and is exclusively used in some 
operations.

Binding unqualified objects to a schema typically occurs at **statement preparation time**. An exception to this is 
the `MAKE_DBKEY` function when its first argument is an expression (not a simple literal), in which case the table 
resolution happens at **execution time**.

Object names can now be explicitly qualified with their schema, such as `SCHEMA_NAME.TABLE_NAME`, 
`SCHEMA_NAME.TABLE_NAME.COLUMN_NAME`, or `SCHEMA_NAME.PACKAGE_NAME.PROCEDURE_NAME`. However, the schema qualifier is 
optional. When omitted, the search path is used to resolve unqualified names, and the behavior depends on the context 
in which the name appears.

For `CREATE`, `CREATE OR ALTER`, and `RECREATE` statements, the system searches only the **current schema** (the first 
valid schema in the search path) for an existing object, and the new object is created in this same schema. 
This rule also applies to `GRANT` and `REVOKE` statements for DDL operations without the `ON SCHEMA` subclause. If no 
**current schema** is available (i.e., no valid schema exists in the search path), an error is raised.

Examples using this rule:

```sql
create table TABLE1 (ID integer);
recreate table TABLE1 (ID integer);
create or alter function F1 returns integer as begin end;
grant create table to user USER1;
```

For `ALTER`, `DROP`, and others statements, the system searches for the specified object across all schemas in the 
search path. The reference is bound to the first matching object found. If no matching object exists in any schema, an 
error is raised.

Examples using this rule:

```sql
alter table TABLE1 add X integer;
alter function FUNCTION1 returns integer as begin end;
select * from TABLE1;
```

The behavior of search paths differs between DML and DDL statements.

For DML statements, the search path is used to locate all referenced unqualified objects. For example:

```sql
insert into TABLE1 values (1);

execute block returns (out DOMAIN1)
as
begin
    select val from TABLE2 into out;
end;
```

In this case, the search path is used to locate `TABLE1`, `DOMAIN1`, and `TABLE2`.

For DDL statements, the search path operates similarly, but with a subtle difference. Once the object being created or 
modified is bound to a schema during statement preparation, the search path is implicitly and temporarily modified. 
This adjustment sets the search path to the schema of the object. Additionally, if the `SYSTEM` schema was already 
present in the search path, it is appended as the last schema.

```sql
create schema SCHEMA1;
create schema SCHEMA2;

create domain SCHEMA1.DOMAIN1 integer;

-- DOMAIN1 is bound to SCHEMA1 even without it being in the search path, as the table being created is bound to SCHEMA1
create table SCHEMA1.TABLE1 (id DOMAIN1);

set search_path to SCHEMA2, SCHEMA1;
-- Error: even if SCHEMA1 is in the search path, TABLE2 is bound to SCHEMA2,
-- so DOMAIN1 is searched in SCHEMA2 and SYSTEM schemas
create table TABLE2 (id DOMAIN1);

set search_path to SYSTEM;

create procedure SCHEMA1.PROC1
as
begin
    -- TABLE1 is bound to SCHEMA1 as PROC1
    insert into TABLE1 values (1);
end;
```

### Resolving between `PACKAGE.OBJECT` and `SCHEMA.OBJECT`

The syntax `<name>.<name>` introduces ambiguity between `<package>.<object>` and `<schema>.<object>` when referring to 
procedures and functions. See [name resolution](README.name_resolution.md) for details.

## Permissions

Permissions for managing and using schema-bound objects are now influenced by the permissions assigned to schemas.

A schema, like other database objects, has an owner. The schema owner can manage and use any object within the schema, 
including objects created by other users in that schema.

To manipulate objects within a schema owned by another user, specific DDL permissions are required. While DDL 
permissions existed in previous versions, they are now more granular. For example:

```sql
grant create table on schema SCHEMA1 to user USER1;
grant alter any procedure on schema SCHEMA1 to PUBLIC;
```

The `ON SCHEMA <name>` clause is optional. If omitted, the **current schema** is implicitly assumed.

Previously, using an object required having specific permissions, such as `EXECUTE` or `USAGE`, granted for the object. 
Now, in addition to these object-level permissions, the `USAGE` permission must also be granted for the schema 
containing the object. For example:

```sql
-- Connected as USER1
create schema SCHEMA1;
create table SCHEMA1.TABLE1 (ID integer);

grant usage on schema SCHEMA1 to user USER2;
grant select on table SCHEMA1.TABLE1 to user USER2;
```

## The SYSTEM schema

All system schema-bound objects (e.g., `RDB$*` and `MON$*`) are now created in a dedicated schema called `SYSTEM`. 
The `SYSTEM` schema has a default `USAGE` permission granted to `PUBLIC` and is included in the default search path. 
This ensures backward compatibility with previous Firebird versions.

While the `SYSTEM` schema allows operations like index creation and manipulation of those indexes, it is otherwise 
locked for DDL changes. Modifying objects within the `SYSTEM` schema through DDL operations is strongly discouraged.

## The PUBLIC schema

A schema named `PUBLIC` is automatically created in new databases, with a default `USAGE` permission granted to 
`PUBLIC`. However, only the database or schema owner has default permissions to manipulate objects within this schema.

Unlike the `SYSTEM` schema, the `PUBLIC` schema is not a system object and can be dropped by the database owner 
or by a user with `DROP ANY SCHEMA` permission. If restoring a Firebird 6 or later backup using `gbak`, and the 
`PUBLIC` schema was not present in the original database, the restored database will also exclude it.

## New statements and expressions

### CREATE SCHEMA

```sql
{CREATE [IF NOT EXISTS] | CREATE OR ALTER | RECREATE} SCHEMA <schema name>
    [DEFAULT CHARACTER SET <character set name>]
    [DEFAULT SQL SECURITY {DEFINER | INVOKER}]
```

Schemas may have an optional default character set and SQL security setting, which serve as the defaults for objects 
contained within them. When these defaults are not specified or are dropped, the database's default character set and 
SQL security settings are used, as in previous versions.

Unlike the automatically created `PUBLIC` schema, newly created schemas grant `USAGE` permission only to their owners 
and not to `PUBLIC`.

Schema names `INFORMATION_SCHEMA` and `DEFINITION_SCHEMA` are reserved and cannot be used for new schemas.

### ALTER SCHEMA

```sql
ALTER SCHEMA <schema name>
    <alter schema option>...

<alter schema option> ::=
    SET DEFAULT CHARACTER SET <character set name> |
    SET DEFAULT SQL SECURITY {DEFINER | INVOKER} |
    DROP DEFAULT CHARACTER SET |
    DROP DEFAULT SQL SECURITY
```

### DROP SCHEMA

```sql
DROP SCHEMA [IF EXISTS] <schema name>
```

Currently, only empty schemas can be dropped. In the future, a `CASCADE` sub-clause will be introduced, allowing 
schemas to be dropped along with all their contained objects.

### CURRENT_SCHEMA

`CURRENT_SCHEMA` returns the name of the first valid schema in the search path of the current session. If no valid 
schema exists, it returns `NULL`.

### SET SEARCH_PATH TO

```sql
SET SEARCH_PATH TO <schema name> [, <schema name>]...
```

### RDB$GET_CONTEXT

#### CURRENT_SCHEMA (SYSTEM)

`RDB$GET_CONTEXT('SYSTEM', 'CURRENT_SCHEMA')` returns the same value as the `CURRENT_SCHEMA` expression.

#### SEARCH_PATH (SYSTEM)

`RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH')` returns the current session search path, including invalid schemas in the 
list. To get separate records for each entry, you can use:

```sql
select NAME
    from SYSTEM.RDB$SQL.PARSE_UNQUALIFIED_NAMES(RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH'))
```

#### SCHEMA_NAME (DDL_TRIGGER)

`RDB$GET_CONTEXT('DDL_TRIGGER', 'SCHEMA_NAME')` returns the schema name of the affected object within a DDL trigger.

## Monitoring

The monitoring tables now include schema-related information:

`MON$ATTACHMENTS`
- `MON$SEARCH_PATH`: search path of the attachment

`MON$TABLE_STATS`
- `MON$SCHEMA_NAME`: schema of the table

`MON$CALL_STACK`
- `MON$SCHEMA_NAME`: schema of the routine

`MON$COMPILED_STATEMENTS`
- `MON$SCHEMA_NAME`: schema of the routine

## Queries

Field names can now be qualified with the schema, in addition to aliases or table names. This includes cases where the 
schema is implicitly determined by the search path. It is also possible to qualify the table name with the schema and 
refer to the field using just the table name. For example:

```sql
create schema SCHEMA1;

create table SCHEMA1.TABLE1 (ID integer);

set search_path to SCHEMA1;

select TABLE1.ID from SCHEMA1.TABLE1;
select SCHEMA1.TABLE1.ID from TABLE1;
select SCHEMA1.TABLE1.ID from SCHEMA1.TABLE1;
```

If the same table name exists in multiple schemas, fields must be qualified with schema names or table aliases to 
avoid ambiguity. For example:

```sql
create schema SCHEMA1;
create schema SCHEMA2;

create table SCHEMA1.TABLE1 (ID integer);
create table SCHEMA2.TABLE1 (ID integer);

select SCHEMA1.TABLE1.ID, SCHEMA2.TABLE1.ID from SCHEMA1.TABLE1, SCHEMA2.TABLE1;
select S1.ID, S2.ID from SCHEMA1.TABLE1 S1, SCHEMA2.TABLE1 S2;
```

### Plans

Plans (both legacy and detailed) now include schema names in their reports, resulting in slight changes to their format.

For example, the query below:
```sql
with q1 as (
    select *
        from t1
),
q2 as (
    select *
        from q1
)
select *
    from q2 x;
```

Previously produced the following plan:
```
PLAN (X Q1 T1 NATURAL)

Select Expression
    -> Table "T1" as "X Q1 T1" Full Scan
```

Now, the plan explicitly includes the schema, and each name is quoted separately:
```
PLAN ("X" "Q1" "PUBLIC"."T1" NATURAL)

Select Expression
    -> Table "PUBLIC"."T1" as "X" "Q1" "PUBLIC"."T1" Full Scan
```

## New DPB items

### `isc_dpb_search_path`

`isc_dpb_search_path` is a string DPB parameter, similar to `isc_dpb_user_name`, used to set the initial schema search 
path for a session.

## New TPB items

### `isc_tpb_lock_table_schema`

`isc_tpb_lock_table_schema` could be used with `isc_tpb_lock_read` and `isc_tpb_lock_write`, after the table name and 
before `isc_tpb_shared`, `isc_tpb_protected` or `isc_tpb_exclusive`. The format of `isc_tpb_lock_table_schema` is 
a single byte length followed by the schema name.

## Array support

### `isc_sdl_schema`

When working with arrays using SDL (Slice Description Language), the `isc_sdl_schema` parameter can now be used to 
explicitly qualify the schema. Its format is equivalent to `isc_sdl_relation`.

## Utilities

### isql

#### Option `-(SE)ARCH_PATH`

This option enables ISQL to pass the search path argument as `isc_dpb_search_path` with every established attachment.

```
isql -search_path x,y test.fdb
select RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "X", "Y"
set search_path to y;
select RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "Y"

connect 't2.fdb';
select RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "X", "Y"
```

```
isql -search_path '"x", "y"' test.fdb
select RDB$GET_CONTEXT('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "x", "y"
```

Special names with double quotes require different handling depending on the operating system and, specifically, the 
shell interpreter. On Linux with `bash`, double quotes must be escaped using the backslash (`\`) character. On Windows 
with `cmd`, double quotes need to be duplicated. The example below demonstrates how to use the same search path on both 
platforms.

Linux / bash:
```
isql -search_path "x, \"y\", z, \"q a\", system" test.fdb

select rdb$get_context('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "X", "y", "Z", "q a", "SYSTEM"
```

Windows / cmd:
```
isql -search_path "x, ""y"", z, ""q a"", system" test.fdb

select rdb$get_context('SYSTEM', 'SEARCH_PATH') from system.rdb$database;
-- Result: "X", "y", "Z", "q a", "SYSTEM"
```

### gbak

To use databases created in earlier Firebird versions with Firebird 6, you must restore a backup using the Firebird 6 
`gbak` utility. In the restored database, all user objects will be placed in the `PUBLIC` schema.

Firebird includes several built-in plugins that create database objects. Prior to Firebird 6, schemas did not exist, 
so these objects were only distinguished by their `PLG$` prefix. When restoring a pre-Firebird 6 backup, `gbak` 
automatically attempts to recreate these objects (and their data) in specific schemas — `PLG$PROFILER`, `PLG$SRP`, and 
`PLG$LEGACY_SEC` — while removing those located in the `PUBLIC` schema. This process may fail if user-defined objects 
reference these plugin objects. In that case, `gbak` will display a warning and halt the plugin schema migration 
process.

#### Options

`gbak` now includes the `-INCLUDE_SCHEMA_D(ATA)` and `-SKIP_SCHEMA_D(ATA)` switches, which complement the previously
existing `-INCLUDE_SCHEMA` and `-SKIP_SCHEMA` options. If `-INCLUDE_SCHEMA_D(ATA)` is not specified, all schemas are
included by default. Likewise, if `-SKIP_SCHEMA_D(ATA)` is not provided, no schemas are skipped.

Example:
```shell
# Include data only from tables of the schema S1
gbak -c -include_schema_data S1 database.fbk database.fdb

# Include data only from tables named T1 from any schema
gbak -c -include_data T1 database.fbk database.fdb

# Include data only from table T1 of the schema S1
gbak -c -include_schema_data S1 -include_data T1 database.fbk database.fdb

# Include data from tables of all schemas except S1
gbak -c -skip_schema_data S1 database.fbk database.fdb
```

### gstat

`gstat` now includes the `-sch <schemaname>` option, which complement the previously existing `-t <tablename>`.
If `-sch` is not specified, all schemas are analyzed by default.

### fbsvcmgr

`fbsvcmgr` now include some options forwarded to their equivalent services/utilities. This table shows the options and
their equivalents:

| Option                  | Equivalent                                       |
|-------------------------|--------------------------------------------------|
| bkp_skip_schema_data    | `gbak -skip_schema_data` when backing up         |
| bkp_include_schema_data | `gbak -include_schema_data` when backing up      |
| res_skip_schema_data    | `gbak -skip_schema_data` when restoring          |
| res_include_schema_data | `gbak -include_schema_data` when restoring       |
| sts_schema              | `gstat -sch`                                     |
| val_sch_incl            | analogous to `fbsvcmgr -val_tab_incl` for schema |
| val_sch_excl            | analogous to `fbsvcmgr -val_tab_excl` for schema |

## System metadata changes

The following fields have been added to system tables. It is essential for applications and tools that read metadata 
to utilize these fields where appropriate. For example, consider using `RDB$SCHEMA_NAME` when joining tables.

| Table                    | Column                          |
|--------------------------|---------------------------------|
| MON$ATTACHMENTS          | MON$SEARCH_PATH                 |
| MON$CALL_STACK           | MON$SCHEMA_NAME                 |
| MON$COMPILED_STATEMENTS  | MON$SCHEMA_NAME                 |
| MON$TABLE_STATS          | MON$SCHEMA_NAME                 |
| RDB$CHARACTER_SETS       | RDB$DEFAULT_COLLATE_SCHEMA_NAME |
| RDB$CHARACTER_SETS       | RDB$SCHEMA_NAME                 |
| RDB$CHECK_CONSTRAINTS    | RDB$SCHEMA_NAME                 |
| RDB$COLLATIONS           | RDB$SCHEMA_NAME                 |
| RDB$DATABASE             | RDB$CHARACTER_SET_SCHEMA_NAME   |
| RDB$DEPENDENCIES         | RDB$DEPENDED_ON_SCHEMA_NAME     |
| RDB$DEPENDENCIES         | RDB$DEPENDENT_SCHEMA_NAME       |
| RDB$EXCEPTIONS           | RDB$SCHEMA_NAME                 |
| RDB$FIELDS               | RDB$SCHEMA_NAME                 |
| RDB$FIELD_DIMENSIONS     | RDB$SCHEMA_NAME                 |
| RDB$FUNCTIONS            | RDB$SCHEMA_NAME                 |
| RDB$FUNCTION_ARGUMENTS   | RDB$FIELD_SOURCE_SCHEMA_NAME    |
| RDB$FUNCTION_ARGUMENTS   | RDB$RELATION_SCHEMA_NAME        |
| RDB$FUNCTION_ARGUMENTS   | RDB$SCHEMA_NAME                 |
| RDB$GENERATORS           | RDB$SCHEMA_NAME                 |
| RDB$INDEX_SEGMENTS       | RDB$SCHEMA_NAME                 |
| RDB$INDICES              | RDB$FOREIGN_KEY_SCHEMA_NAME     |
| RDB$INDICES              | RDB$SCHEMA_NAME                 |
| RDB$PACKAGES             | RDB$SCHEMA_NAME                 |
| RDB$PROCEDURES           | RDB$SCHEMA_NAME                 |
| RDB$PROCEDURE_PARAMETERS | RDB$FIELD_SOURCE_SCHEMA_NAME    |
| RDB$PROCEDURE_PARAMETERS | RDB$RELATION_SCHEMA_NAME        |
| RDB$PROCEDURE_PARAMETERS | RDB$SCHEMA_NAME                 |
| RDB$PUBLICATION_TABLES   | RDB$TABLE_SCHEMA_NAME           |
| RDB$REF_CONSTRAINTS      | RDB$CONST_SCHEMA_NAME_UQ        |
| RDB$REF_CONSTRAINTS      | RDB$SCHEMA_NAME                 |
| RDB$RELATIONS            | RDB$SCHEMA_NAME                 |
| RDB$RELATION_CONSTRAINTS | RDB$SCHEMA_NAME                 |
| RDB$RELATION_FIELDS      | RDB$FIELD_SOURCE_SCHEMA_NAME    |
| RDB$RELATION_FIELDS      | RDB$SCHEMA_NAME                 |
| RDB$SCHEMAS              | RDB$CHARACTER_SET_NAME          |
| RDB$SCHEMAS              | RDB$CHARACTER_SET_SCHEMA_NAME   |
| RDB$SCHEMAS              | RDB$SQL_SECCURITY               |
| RDB$SCHEMAS              | RDB$DESCRIPTION                 |
| RDB$SCHEMAS              | RDB$OWNER_NAME                  |
| RDB$SCHEMAS              | RDB$SCHEMA_NAME                 |
| RDB$SCHEMAS              | RDB$SECURITY_CLASS              |
| RDB$SCHEMAS              | RDB$SYSTEM_FLAG                 |
| RDB$TRIGGERS             | RDB$SCHEMA_NAME                 |
| RDB$TRIGGER_MESSAGES     | RDB$SCHEMA_NAME                 |
| RDB$USER_PRIVILEGES      | RDB$RELATION_SCHEMA_NAME        |
| RDB$USER_PRIVILEGES      | RDB$USER_SCHEMA_NAME            |
| RDB$VIEW_RELATIONS       | RDB$RELATION_SCHEMA_NAME        |
| RDB$VIEW_RELATIONS       | RDB$SCHEMA_NAME                 |

## Differences with previous versions

### CREATE SCHEMA in `IAttachment::executeCreateDatabase` and `isc_create_database`

In earlier versions, `CREATE SCHEMA` served as an alias for `CREATE DATABASE`, when using the API functions 
`IAttachment::executeCreateDatabase` and `isc_create_database` to create databases. This is no longer the case; 
the only valid syntax now is `CREATE DATABASE`.

### Object names in error messages and expressions

Object names in error and informational messages are now consistently qualified and quoted within message parameters, 
even in DIALECT 1 databases.

```sql
SQL> create table TABLE1 (ID integer);
SQL> create table TABLE1 (ID integer);
Statement failed, SQLSTATE = 42S01
unsuccessful metadata update
-CREATE TABLE "PUBLIC"."TABLE1" failed
-Table "PUBLIC"."TABLE1" already exists

SQL> create schema "Weird ""Schema""";
SQL> create schema "Weird ""Schema""";
Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE SCHEMA "Weird ""Schema""" failed
-Schema "Weird ""Schema""" already exists
```

The `RDB$ERROR(EXCEPTION)` expression now returns the fully qualified and quoted name of the exception as well.

### Object name parsing outside SQL

When working with object names in `isc_dpb_search_path`, `isc_dpb_lc_ctype`, `isc_dpb_set_db_charset` and `MAKE_DBKEY`, 
the names follow the same rules as in SQL. 
This means that names containing special characters or lowercase letters must be enclosed in quotes.

With `isc_dpb_lc_ctype` and `isc_dpb_set_db_charset`, names not qualified with a schema are resolved using the `SYSTEM` 
schema.

With `MAKE_DBKEY`, unqualified names are resolved using the current search path. In earlier versions, `MAKE_DBKEY` 
required an exact table name as its first parameter and did not support the use of double quotes for special 
characters.

### Minimum page size

The minimum database page size has been increased from 4096 to 8192 bytes. This change was necessary because the 
previous minimum could no longer accommodate updates done in the system indexes.

### System function overrides

In Firebird versions prior to 6, user-defined functions could override system functions that use standard syntax, 
such as `ABS` and `MOD`. This behavior is no longer allowed.

Starting with Firebird 6, system functions must be called without quotes and without a schema name. If a user-defined 
function shares a name with a system function, it must be explicitly called using quotes or qualified with the schema 
name.

### Built-in plugins

Built-in plugins are migrated by `gbak` to their own schemas. Stored routines referenceing them should be updated to
include their schema names before changes.

## Replication

### Config parameters `include_schema_filter` and `exclude_schema_filter`

`replication.conf` now includes the `include_schema_filter` and `exclude_schema_filter` parameters, which complement
the previously existing `include_filter` and `exclude_filter`. If `include_schema_filter` is not specified,
all schemas are included by default. Likewise, if `exclude_schema_filter` is not provided, no schemas are excluded.

### Config parameter `schema_search_path`

A master database running in Firebird < 6.0 can be asynchronously replicated to a slave database using Firebird ≥ 6.0.
Since the master database has no schema awareness, the `schema_search_path` config parameter is introduced on the
slave side. This allows mapping database objects to a specific schema using a search path.

## System packages and functions

Firebird includes system packages such as `RDB$TIME_ZONE_UTIL`, `RDB$PROFILER`, and others. These system packages 
are now located in the `SYSTEM` schema. If the `SYSTEM` schema is not included in the search path, their usage 
requires explicit qualification with `SYSTEM`, as with any other object bound to the `SYSTEM` schema.

In contrast, Firebird also provides non-packaged built-in functions like `RDB$GET_CONTEXT`, `ABS`, and `DATEDIFF`. 
These functions are not listed in the database metadata (`RDB$FUNCTIONS`) and neither require nor accept the 
`SYSTEM` qualifier for usage.

## Downgrade compatibility

It is expected that Firebird 6 databases that do not even use multiple users schemas (for example, a Firebird 5 
database just migrated to Firebird 6 by `gbak`) may not always be downgradable to **unmodified previous versions** of 
Firebird using `gbak`. This issue can occur if user objects reference system objects.

The Firebird team plans to backport essential internal changes to Firebird 5 to enable such downgrades. 
It should also be possible to downgrade databases with multiple user schemas, as long as objects with 
the same name do not exist in multiple schemas.

This documentation will be updated once these changes are implemented.

## Limitations

DPB and TPB parameters cannot be greater than 255 bytes. That imposes a limit for usage of multiple schemas in 
`isc_dpb_search_path` or fully qualified names with multi-byte characters in `isc_dpb_lc_ctype` and 
`isc_dpb_set_db_charset`.
