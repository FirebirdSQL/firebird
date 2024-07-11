# DECLARE VARIABLE initializer enhancements (FB 6.0)

Up to Firebird 5.0, variables could be declared and initialized in the same statement, however only simple
expressions (the same as allowed in a DOMAIN's DEFAULT clause) were allowed as initializers.
This limitation has been removed in Firebird 6.0, allowing the use of any value expression as initializer.

## Syntax

```
DECLARE [VARIABLE] <varname> <type> [{ = | DEFAULT } <value>];
```

## Notes

Previously declared variables can be used in the initializer expression of following variables.

A variable initializer may call subroutines, and these subroutines may read and write previously
declared variables.

A subroutine used in an initializer may also write to variables declared after the one being
initialized, but in this case their values will be overwritten when their initializers are
executed, even if they don't have explicit initializers.

```
-- This block will return (<null>, 2) as values of v1 and v2 assigned in sf1 will
-- be overwritten by their initializer after sf1 is called.
execute block returns (o1 integer, o2 integer)
as
    declare function sf1 returns integer;

    declare v0 integer = sf1();
    declare v1 integer;
    declare v2 integer = 2;

    declare function sf1 returns integer
    as
    begin
        v1 = 10;
        v2 = 20;
        return 0;
    end
begin
    o1 = v1;
    o2 = v2;
    suspend;
end
```

It's an error if a subroutine reads a variable declared after the one being initialized like in the
following example.

```
-- When sf1 is called, v1 is not yet initialized.
execute block
as
    declare function sf1 returns integer;

    declare v0 integer = sf1();
    declare v1 integer;

    declare function sf1 returns integer
    as
    begin
        return v1;
    end
begin
end
```
