# Named arguments for function and procedure calling (FB 6.0)

Named arguments allows you to specify function and procedure arguments by their names, rather than only
by their positions.

It is especially useful when the routine have a lot of parameters and you want to specify them in arbitrary
order or not specify some of those who have default values.

As the positional syntax, all arguments without default values are required to be present in the call.

A call can use positional, named or mixed arguments. In mixed syntax, positional arguments must appear before
named arguments.

## Syntax

```
<function call> ::=
    [<package name> .] <function_name>( [<arguments>] )

<procedure selection> ::=
    [<package name> .] <procedure_name> [( <arguments> )]

<execute procedure> ::=
    EXECUTE PROCEDURE [<package name> .] <procedure name>
    [{ (<arguments>) | <arguments> }]
    [RETURNING_VALUES ...]

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
select function_name(parameter2 => 'Two', parameter1 => 1)
  from rdb$database
```

```
select function_name(1, parameter2 => 'Two')
  from rdb$database
```

```
select function_name(default, parameter2 => 'Two')
  from rdb$database
```

```
execute procedure insert_customer(
  last_name => 'SCHUMACHER',
  first_name => 'MICHAEL')
```

```
select *
  from get_customers(city_id => 10, last_name => 'SCHUMACHER')
```
