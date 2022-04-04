-----------
Subroutines
-----------

Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>

Description:
    Support for PSQL subroutines (functions and procedures) inside functions, procedures, triggers
    and EXECUTE BLOCK. Subroutines are declared in the main routine and may be used from there or others subroutines.

Syntax:
    <declaration item> ::=
        DECLARE [VARIABLE] <variable name> <data type> [ = <value> ];
        |
        DECLARE [VARIABLE] CURSOR <cursor name> FOR (<query>);
        |
        <subroutine declaration>
        |
        <subroutine implementation>

    <subroutine declaration> ::=
        DECLARE FUNCTION <function name> [ (<input parameters>) ]
            RETURNS <data type>
            [ [ NOT ] DETERMINISTIC ] ;
        |
        DECLARE PROCEDURE <procedure name> [ (<input parameters>) ] [ RETURNS (<output parameters>) ] ;

    <subroutine implementation> ::=
        DECLARE FUNCTION <function name> [ (<input parameters>) ]
            RETURNS <data type>
            [ [ NOT ] DETERMINISTIC ]
        AS
            ...
        BEGIN
            ...
        END
        |
        DECLARE PROCEDURE <procedure name> [ (<input parameters>) ] [ RETURNS (<output parameters>) ]
        AS
            ...
        BEGIN
            ...
        END

Limitations:
    1) Subroutines may not be nested in another subroutine. They are only supported in the main
       routine.
    2) Currently, a subroutine may not directly access cursors of the main routine/block.
       This may be allowed in the future.
    3) Since FB 5 subroutines may use variables and parameters from the main routine/block.
    4) Variables and parameters that are accessed by subroutines may have a small performance
       penalty (even in the main routine) when being read.

Notes:
    1) Starting in FB 4, subroutines may be recursive or call others subroutines.

Examples:
    set term !;

    -- 1) Sub-procedures in execute block.

    execute block returns (name varchar(31))
    as
        declare procedure get_tables returns (table_name varchar(31))
        as
        begin
            for select rdb$relation_name
                  from rdb$relations
                  where rdb$view_blr is null
                  into table_name
            do
                suspend;
        end

        declare procedure get_views returns (view_name varchar(31))
        as
        begin
            for select rdb$relation_name
                  from rdb$relations
                  where rdb$view_blr is not null
                  into view_name
            do
                suspend;
        end
    begin
        for select table_name
              from get_tables
            union all
            select view_name
              from get_views
              into name
        do
            suspend;
    end!


    -- 2) Sub-function in a stored function.

    create or alter function func1 (n1 integer, n2 integer) returns integer
    as
        declare function subfunc (n1 integer, n2 integer) returns integer
        as
        begin
            return n1 + n2;
        end
    begin
        return subfunc(n1, n2);
    end!

    select func1(5, 6) from rdb$database!


    -- 3) Recursive sub-function in EXECUTE BLOCK.

    execute block returns (i integer, o integer)
    as
        -- Recursive function without forward declaration.
        declare function fibonacci(n integer) returns integer
        as
        begin
            if (n = 0 or n = 1) then
                return n;
            else
                return fibonacci(n - 1) + fibonacci(n - 2);
        end
    begin
        i = 0;

        while (i < 10)
        do
        begin
            o = fibonacci(i);
            suspend;
            i = i + 1;
        end
    end!


    -- 4) Example with forward declaration and parameter with default values.

    execute block returns (o integer)
    as
        -- Forward declaration of P1.
        declare procedure p1(i integer = 1) returns (o integer);

        -- Forward declaration of P2.
        declare procedure p2(i integer) returns (o integer);

        -- Implementation of P1 should not re-declare parameter default value.
        declare procedure p1(i integer) returns (o integer)
        as
        begin
            execute procedure p2(i) returning_values o;
        end

        declare procedure p2(i integer) returns (o integer)
        as
        begin
            o = i;
        end
    begin
        execute procedure p1 returning_values o;
        suspend;
    end!
