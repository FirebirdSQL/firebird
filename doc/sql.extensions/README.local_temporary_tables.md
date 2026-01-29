# Created Local Temporary Tables (FB 6.0)

Firebird 6.0 introduces support for SQL Created Local Temporary Tables (LTT). Unlike Global Temporary Tables (GTT),
which have permanent metadata stored in the system catalogue, Local Temporary Tables exist only within the connection
that created them. The table definition is private to the creating connection and is automatically discarded when the
connection ends. The data lifecycle depends on the `ON COMMIT` clause: with `ON COMMIT DELETE ROWS` (the default), data
is private to each transaction and deleted when the transaction ends; with `ON COMMIT PRESERVE ROWS`, data is shared
across all transactions in the connection and persists until the connection ends.

## Why Local Temporary Tables?

Local Temporary Tables are useful in scenarios where you need temporary storage without affecting the database metadata:

### Temporary storage in read-only databases

Since LTT definitions are not stored in the database, they can be created and used even when the database is read-only.

Global Temporary Tables (GTTs), on the other hand, cannot be created in read-only databases because their creation
requires metadata changes. Regarding usage (DML operations), only ON DELETE ROWS GTTs are allowed in a read-only
database.

### Session-private definitions

Each connection can create its own temporary tables with the same names without conflicts. The table definitions are
completely isolated between connections.

### Ad-hoc temporary storage

LTTs provide a quick way to create temporary storage during a session for intermediate results, data transformations,
or other temporary processing needs without leaving any trace in the database after disconnection.

## Comparison with Global Temporary Tables

| Feature                     | GTT                                  | LTT                                   |
|-----------------------------|--------------------------------------|---------------------------------------|
| Metadata storage            | System tables (`RDB$RELATIONS`, etc.)| Connection memory only                |
| Visibility of definition    | All connections                      | Creating connection only              |
| Persistence of definition   | Permanent (until explicitly dropped) | Until connection ends                 |
| Creation in read-only DB    | No                                   | Yes                                   |
| Used in read-only DB        | ON DELETE ROWS only                  | ON COMMIT ROWS / ON DELETE ROWS       |
| Schema support              | Yes                                  | Yes                                   |
| Indexes                     | Full support                         | Basic support (no expression/partial) |
| DDL triggers                | Fire on CREATE/DROP/ALTER            | Do not fire                           |
| DML triggers support        | Yes                                  | Not supported                         |
| Field level NOT NULL        | Supported                            | Supported                             |
| PK, FK, CHECK constraints   | Supported                            | Not supported                         |
| Named constraints           | Supported                            | Not supported                         |
| Explicit privileges         | Supported                            | Not supported                         |

## Syntax

### CREATE LOCAL TEMPORARY TABLE

```sql
{CREATE | RECREATE} LOCAL TEMPORARY TABLE [IF NOT EXISTS] [<schema name>.]<table name>
    (<column definitions>)
    [ON COMMIT {DELETE | PRESERVE} ROWS]
```

The `ON COMMIT` clause determines the data lifecycle:

- `ON COMMIT DELETE ROWS` (default): Data is private to each transaction and deleted when the transaction ends.
- `ON COMMIT PRESERVE ROWS`: Data persists until the connection ends.

Example:

```sql
-- Create a simple LTT with default ON COMMIT DELETE ROWS
create local temporary table temp_results (
    id integer not null,
    val varchar(100)
);

-- Create an LTT that preserves data across transactions
create local temporary table session_cache (
    key varchar(50) not null,
    data blob
) on commit preserve rows;

-- Create an LTT in a specific schema
create local temporary table my_schema.work_table (
    x integer,
    y integer
);
```

### ALTER TABLE

Local Temporary Tables support the following `ALTER TABLE` operations:

```sql
-- Add a column
ALTER TABLE <ltt name> ADD <column name> <data type> [NOT NULL];

-- Drop a column
ALTER TABLE <ltt name> DROP <column name>;

-- Rename a column
ALTER TABLE <ltt name> ALTER COLUMN <old name> TO <new name>;

-- Change column position
ALTER TABLE <ltt name> ALTER COLUMN <column name> POSITION <new position>;

-- Change nullability
ALTER TABLE <ltt name> ALTER COLUMN <column name> {DROP | SET} NOT NULL;

-- Change column type
ALTER TABLE <ltt name> ALTER COLUMN <column name> TYPE <new data type>;
```

Example:

```sql
create local temporary table temp_data (id integer);

alter table temp_data add name varchar(50) not null;
alter table temp_data alter column name position 1;
alter table temp_data alter column name to full_name;
alter table temp_data alter column id type bigint;
```

### DROP TABLE

```sql
DROP TABLE [IF EXISTS] [<schema name>.]<ltt name>
```

The same `DROP TABLE` statement is used for both regular tables and Local Temporary Tables. If the table name matches
an LTT in the current connection, the LTT is dropped.

### CREATE INDEX

```sql
CREATE [UNIQUE] [ASC[ENDING] | DESC[ENDING]] INDEX [IF NOT EXISTS] [<schema name>.]<index name>
    ON [<schema name>.]<ltt name> (<column list>)
```

Example:

```sql
create local temporary table temp_orders (
    order_id integer not null,
    customer_id integer,
    order_date date
);

create unique index idx_temp_orders_pk on temp_orders (order_id);
create descending index idx_temp_orders_date on temp_orders (order_date);
create index idx_temp_orders_cust on temp_orders (customer_id);
```

### ALTER INDEX

```sql
ALTER INDEX <index name> {ACTIVE | INACTIVE}
```

Indexes on LTTs can be deactivated and reactivated, which may be useful when performing bulk inserts in
`ON COMMIT PRESERVE ROWS` tables.

### DROP INDEX

```sql
DROP INDEX [IF EXISTS] <index name>
```

## Limitations

Local Temporary Tables have the following restrictions:

### Column restrictions

- **DEFAULT values**: Columns cannot have default values.
- **COMPUTED BY columns**: Computed columns are not supported.
- **IDENTITY columns**: Identity (auto-increment) columns are not supported.
- **ARRAY types**: Array type columns are not supported.
- **Domain changes**: Columns can be defined using domains, but changes to the domain definition are not propagated to
  existing LTT columns. The column retains the domain's characteristics as they were at the time the column was created.

### Constraint restrictions

- **PRIMARY KEY**: Primary key constraints are not supported.
- **FOREIGN KEY**: Foreign key constraints are not supported.
- **CHECK constraints**: Check constraints are not supported.
- **NOT NULL constraints**: Only unnamed NOT NULL constraints are supported.

### Index restrictions

- **Expression-based indexes**: Indexes based on expressions are not supported.
- **Partial indexes**: Partial (filtered) indexes are not supported.

### Other restrictions

- **EXTERNAL FILE**: LTTs cannot be linked to external files.
- **SQL SECURITY clause**: The SQL SECURITY clause is not applicable to LTTs.
- **Max number of LTTs**: The maximum number of active Local Temporary Tables per connection is 1024.
- **Persistent metadata references**: LTTs cannot be directly referenced in stored procedures, triggers, views, or
  other persistent database objects. Attempting to do so will raise an error. However, LTTs can be accessed via
  `EXECUTE STATEMENT` inside persistent objects, since the SQL text is parsed at runtime.

## Data Lifecycle

### ON COMMIT DELETE ROWS

When a Local Temporary Table is created with `ON COMMIT DELETE ROWS` (the default), data is private to each transaction
and automatically deleted when the transaction ends, whether by commit or rollback. Since Firebird supports multiple
concurrent transactions within the same connection, each transaction has its own isolated view of the data.

```sql
create local temporary table temp_work (id integer);

insert into temp_work values (1);
insert into temp_work values (2);
select count(*) from temp_work;  -- Returns 2

commit;

select count(*) from temp_work;  -- Returns 0 (data was deleted)
```

### ON COMMIT PRESERVE ROWS

When created with `ON COMMIT PRESERVE ROWS`, data persists across transaction boundaries and remains available until
the connection ends.

```sql
create local temporary table session_data (id integer) on commit preserve rows;

insert into session_data values (1);
commit;

insert into session_data values (2);
commit;

select count(*) from session_data;  -- Returns 2 (data preserved across commits)
```

### COMMIT/ROLLBACK RETAINING

Similar to Global Temporary Tables, `COMMIT RETAINING` and `ROLLBACK RETAINING` preserve the data in LTTs with
`ON COMMIT DELETE ROWS`.

## Transactional DDL

DDL operations on Local Temporary Tables behave the same as DDL on persistent tables with respect to transactions.
Changes to the table structure (CREATE, ALTER, DROP) are only made permanent when the transaction commits. If the
transaction is rolled back, any LTT structural changes made within that transaction are undone.

Savepoints are also supported. If a savepoint is rolled back, any LTT changes made after that savepoint are undone.

```sql
set autoddl off;  -- ISQL feature

create local temporary table t1 (id integer);

savepoint sp1;

alter table t1 add name varchar(50);

rollback to savepoint sp1;

-- The ALTER TABLE is undone; column 'name' does not exist
```

This applies to all DDL operations on LTTs.

## Schema Integration

Local Temporary Tables are schema-bound objects, just like regular tables. They follow the same schema resolution
rules:

- When creating an LTT without a schema qualifier, it is created in the current schema (the first valid schema in the
  search path).
- When referencing an LTT without a schema qualifier, the search path is used to resolve the name.
- Index names for LTTs must be unique within the schema and cannot conflict with persistent index names.

```sql
set search_path to my_schema;

-- Creates LTT in my_schema
create local temporary table temp_data (id integer);

-- References my_schema.temp_data
select * from temp_data;

-- Explicit schema qualification
create local temporary table other_schema.temp_work (x integer);
```

## Implementation Notes

Local Temporary Tables store their data and indexes in temporary files, similar to Global Temporary Tables. Each
connection has its own temporary file space for LTT data.

When a connection ends (either normally or due to an error), all Local Temporary Tables created by that connection
are automatically discarded along with their data. No explicit cleanup is required.
