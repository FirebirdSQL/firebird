# Name resolution (FB 6.0)

With the introduction of schemas in Firebird 6.0, the syntax `<name>.<name>` - used for tables, views, procedures,
and functions (both standalone and packaged) - introduces ambiguity when resolving object names using the schema
search path. The ambiguity arises between:

- `<schema>.<object>` (a schema and its object)
- `<package>.<object>` (a package and its object)

This document focuses on name resolution rules for tables, views, procedures, and functions within queries and
code blocks.

## Scope specifier (`%`)

To resolve these ambiguities, Firebird introduces a **scope specifier**, represented by the `%` symbol. This
allows unambiguous referencing of objects.

### Syntax

```sql
<name> % { SCHEMA | PACKAGE } . <name>
```

### Examples

```sql
select *
    from plg$profiler%schema.plg$prof_sessions;

execute procedure rdb$profiler%package.pause_session;

call rdb$profiler%package.pause_session();

select rdb$time_zone_util%package.database_version()
    from system%schema.rdb$database;

select *
    from rdb$time_zone_util%package.transitions('America/Sao_Paulo', timestamp '2017-01-01', timestamp '2019-01-01');
```

## Detailed name resolution rules

Firebird resolves object names following a structured sequence of rules. Once an object is located, the resolution
process halts, ensuring no ambiguity errors occur.

- **`name1.name2.name3`**
  1. Look for routine `name3` inside package `name2`, inside schema `name1`.

- **`name1%schema.name2`**
  1. Look for object `name2` inside schema `name1`.

- **`name1%package.name2`**
  1. Look for object `name2` inside a package `name1` using the schema search path.

- **`name1.name2`**
  1. If inside a package named `name1`, look for routine `name2` in the same package.
  2. Look in schema `name1` for object `name2`.
  3. Look for object `name2` inside a package `name1` using the schema search path.

- **`name`**
  1. Look for subroutine `name`.
  2. If inside a package, look for routine `name` in the same package.
  3. Look for object `name` using the schema search path.

> **_Note:_** Object resolution also depends on the context in which they are used. For example, in
`select * from name1.name2`, `name2` could be a table, view, or procedure. However, in
`execute procedure name1.name2`, `name2` must be a procedure. This distinction means that an `execute procedure`
command versus a `select` command can resolve to different objects.
