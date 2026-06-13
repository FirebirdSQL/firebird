# Declared Local Temporary Tables in PSQL (FB 6.0)

Firebird 6.0 supports declaring local temporary tables inside PSQL blocks, procedures, functions and triggers.

Declared Local Temporary Tables are local to the compiled PSQL object or `EXECUTE BLOCK`. They are not stored in
database metadata and they do not require an `ON COMMIT` clause. Their data is scoped to the current execution frame.

This feature is distinct from:

- SQL-created Local Temporary Tables (`CREATE LOCAL TEMPORARY TABLE ...`), whose definitions are attachment-private
  DDL objects.
- Packaged temporary tables (`TEMPORARY TABLE ...` inside packages), whose definitions are persistent package-owned
  metadata.

## Syntax

Declared Local Temporary Tables are declared in the same declaration section as variables, cursors and subroutines.

```sql
DECLARE LOCAL TEMPORARY TABLE <table_name>
(
    <column_definition> [, ...]
);
```

There is no `ON COMMIT` clause.

Example in `EXECUTE BLOCK`:

```sql
set term !;

execute block returns (n integer)
as
    declare local temporary table t (
        id integer not null,
        val varchar(20)
    );
begin
    insert into t(id, val) values (1, 'a');
    insert into t(id, val) values (2, 'b');

    update t set id = id + 10 where id = 1;
    delete from t where id = 2;

    select count(*) from t into n;
    suspend;
end!

set term ;!
```

Example in a procedure:

```sql
set term !;

create procedure p_count_values returns (n integer)
as
    declare local temporary table t (
        id integer not null
    );
begin
    insert into t(id) values (1);
    insert into t(id) values (2);

    select count(*) from t into n;
    suspend;
end!

set term ;!
```

## Semantics

Declared Local Temporary Tables have execution-frame data scope.

- The table structure is part of the compiled PSQL statement, procedure, function or trigger.
- Rows are private to the current execution frame.
- Rows are discarded when that execution frame finishes.
- Transaction commit or rollback does not define the table lifetime; there is no `ON COMMIT` behavior.
- An autonomous transaction block inside the same execution frame sees the same rows.
- Local subroutines may access declared local temporary tables from the containing PSQL scope. They see and modify the
  rows of the current containing execution frame.
- Local subroutines may also declare their own local temporary tables. Those rows are scoped to the subroutine execution
  frame.
- Recursive calls reuse the same compiled table structure, but each recursive execution frame has separate rows.

Recursive example:

```sql
set term !;

create procedure p_ltt_rec(d integer)
    returns (depth integer, cnt_before integer, cnt_after integer)
as
    declare local temporary table t (
        id integer
    );
begin
    insert into t values (:d);
    select count(*) from t into cnt_before;

    if (d > 0) then
    begin
        for
            select depth, cnt_before, cnt_after
                from p_ltt_rec(:d - 1)
                into depth, cnt_before, cnt_after
        do
            suspend;
    end

    select count(*) from t into cnt_after;
    depth = d;
    suspend;
end!

set term ;!
```

Each row returned by `p_ltt_rec(2)` reports `cnt_before = 1` and `cnt_after = 1`, because every recursive frame has
its own table data.

Autonomous transaction example:

```sql
set term !;

execute block returns (n integer)
as
    declare local temporary table t (
        id integer
    );
begin
    insert into t values (10);

    in autonomous transaction do
    begin
        select count(*) from t into n;
    end

    suspend;
end!

set term ;!
```

The autonomous block sees the row inserted by its parent execution frame.

Local subroutine example:

```sql
set term !;

execute block returns (n integer, s integer)
as
    declare local temporary table t (
        id integer
    );

    declare procedure p_add(v integer)
    as
    begin
        insert into t values (:v);
    end

    declare function f_count returns integer
    as
        declare variable ret integer;
    begin
        select count(*) from t into ret;
        return ret;
    end
begin
    insert into t values (1);
    execute procedure p_add(2);

    n = f_count();
    select sum(id) from t into s;
    suspend;
end!

set term ;!
```

The local procedure and function access `t` declared by the outer `EXECUTE BLOCK`. They use the same row set as the
current outer execution frame.

## Visibility and Name Resolution

Declared Local Temporary Tables are visible only to SQL statements compiled inside the declaring PSQL scope and its
local subroutines.

- Unqualified references to the declared name resolve to the declared local table.
- SQL statements in local subroutines may also reference declared local temporary tables from the containing PSQL scope.
- The declaration name cannot be schema-qualified or package-qualified.
- The table is not present in `RDB$RELATIONS`, `RDB$RELATION_FIELDS`, monitoring metadata or other persistent metadata.
- Dynamic SQL, including `EXECUTE STATEMENT`, is parsed separately and does not see declared local temporary tables.

The name shares the local declaration namespace. It cannot duplicate an input parameter, output parameter, local
variable, cursor or another declared local table in the same declaration scope.

## Supported Operations

Declared Local Temporary Tables can be used by regular DML statements compiled in the same PSQL scope:

```sql
insert into t (...) values (...);
select ... from t ...;
update t set ... where ...;
delete from t where ...;
```

They can be used in subqueries, joins and cursor loops like other record sources, subject to the restrictions below.

## Restrictions

Declared Local Temporary Tables intentionally support a small table definition surface.

Column restrictions:

- `DEFAULT` values are not supported.
- `COMPUTED BY` columns are not supported.
- `IDENTITY` columns are not supported.
- Array columns are not supported.
- Field-level `NOT NULL` is supported, but only as an unnamed constraint.

Constraint restrictions:

- Table constraints are not supported.
- Primary keys are not supported.
- Foreign keys are not supported.
- Check constraints are not supported.
- Named constraints are not supported.

Other restrictions:

- Indexes are not supported.
- Triggers on declared local temporary tables are not supported.
- Explicit privileges are not supported.
- `ALTER TABLE`, `DROP TABLE`, `CREATE INDEX`, `ALTER INDEX` and `DROP INDEX` are not valid for declared local
  temporary tables.
- Declared local temporary tables cannot be referenced by persistent metadata objects outside the PSQL object where
  they are declared.

## Notes

Declared Local Temporary Tables are useful for temporary intermediate data that should not be exposed through database
metadata and should be automatically isolated per PSQL execution frame.

Use SQL-created Local Temporary Tables when the temporary table definition must be created and addressed dynamically by
the attachment. Use packaged temporary tables when the definition must be part of package metadata and visible through
package name resolution.
