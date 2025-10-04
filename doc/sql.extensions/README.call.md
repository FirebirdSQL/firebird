# CALL statement (FB 6.0)

`CALL` statement is similar to `EXECUTE PROCEDURE`, but allow the caller to get specific output parameters, or none.

When using the positional or mixed parameter passing, output parameters follows the input ones.

When passing `NULL` to output parameters, they are ignored, and in the case of DSQL, not even returned.

In DSQL output parameters are specified using `?`, and in PSQL using the target variables or parameters.

## Syntax

```
<call statement> ::=
    CALL [<package name> .] <procedure name> (<arguments>)

<arguments> ::=
    <positional arguments> |
    [ {<positional arguments>,} ] <named arguments>

<positional arguments> ::=
    <value or default> [ {, <value or default>}... ]

<named arguments> ::=
    <named argument> [ {, <named argument>}... ]

<named argument> ::=
    <argument name> => <value or default>

<value or default> ::=
    <value> |
    DEFAULT
```

## Examples

```
create or alter procedure insert_customer (
    last_name varchar(30),
    first_name varchar(30)
) returns (
    id integer,
    full_name varchar(62)
)
as
begin
    insert into customers (last_name, first_name)
        values (:last_name, :first_name)
        returning id, last_name || ', ' || first_name
        into :id, :full_name;
end
```

```
-- Not all output parameters are necessary.
call insert_customer(
    'LECLERC',
    'CHARLES',
    ?)
```

```
-- Ignore first output parameter (using NULL) and get the second.
call insert_customer(
    'LECLERC',
    'CHARLES',
    null,
    ?)
```

```
-- Ignore ID output parameter.
call insert_customer(
    'LECLERC',
    'CHARLES',
    full_name => ?)
```

```
-- Pass inputs and get outputs using named arguments.
call insert_customer(
    last_name => 'LECLERC',
    first_name => 'CHARLES',
    last_name => ?,
    id => ?)
```

```
create or alter procedure do_something_and_insert_customer returns (
    out_id integer,
    out_full_name varchar(62)
)
as
    declare last_name varchar(30);
    declare first_name varchar(30);
begin
    call insert_customer(
        last_name,
        first_name,
        out_id,
        full_name => out_full_name);
end
```
