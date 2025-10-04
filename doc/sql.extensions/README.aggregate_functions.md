# Aggregate Functions


## ANY_VALUE (Firebird 6.0)

`ANY_VALUE` is a non-deterministic aggregate function that returns its expression for an arbitrary
record from the grouped rows.

`NULLs` are ignored. It's returned only in the case of none evaluated records having a non-null value.

Syntax:

```
<any value> ::= ANY_VALUE(<expression>)
```

Example:

```
select department,
       any_value(employee) employee
    from employee_department
    group by department
```
