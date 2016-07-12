SQL SECURITY.

   Implements capability to run executable objects regarding SQL SECURITY clause.
   SQL Standard (2003, 2011) Feature.

Author:
   Red Soft, roman.simakov(at)red-soft.ru

Syntax is:

CREATE TABLE <TABLENAME> [SQL SECURITY {DEFINER | INVOKER}] ...
CREATE FUNCTION <FUNCTIONNAME> [SQL SECURITY {DEFINER | INVOKER}] ...
CREATE PROCEDURE <PROCEDURENAME> [SQL SECURITY {DEFINER | INVOKER}] ...
CREATE TRIGGER <TRIGGERNAME> [SQL SECURITY {DEFINER | INVOKER}] ...

Description:

Makes it possible to execute some objects with permissions of either definer or invoker.
By default INVOKER is used to keep backword compatibility.

If INVOKER is specified a current set of privileges of the current user will ba used.
If DEFINER - a set of privileges of object owner will be used to check an access to database objects used by this object.

Example 1. It's enought to grant only execute privilege to user "us". In case of INVOKER it will require also INSERT for table T to either user "us" or procedure "p".

create table t (i integer);
set term ^;
create procedure p sql security definer (i integer)
as
begin
  insert into t values (:i);
end^
set term ;^

grant execute on procedure p to user us;
commit;

connect 'localhost:/tmp/17.fdb' user us password 'pas';
execute procedure p(1);

Example 2. In this example SQL SECURITY is used for table and provide access to calculated columns without explicit grant execute privilege of function f to user "us".

set term ^;
create function f() returns int
as
begin
    return 3;
end^
set term ;^
create table t sql security definer (i integer, c computed by (i + f()));
insert into t values (2);
grant select on table t to user us;

commit;

connect 'localhost:/tmp/7.fdb' user us password 'pas';
select * from t;

