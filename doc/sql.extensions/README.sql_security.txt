SQL SECURITY.

   Implements capability to run executable objects regarding SQL SECURITY clause.
   SQL Standard (2003, 2011) Feature.

Author:
   Red Soft, roman.simakov(at)red-soft.ru

Syntax is:

CREATE TABLE <TABLENAME> [SQL SECURITY {DEFINER | INVOKER}] ...
CREATE [OR ALTER] FUNCTION <FUNCTIONNAME> ... [SQL SECURITY {DEFINER | INVOKER}] AS ...
CREATE [OR ALTER] PROCEDURE <PROCEDURENAME> ... [SQL SECURITY {DEFINER | INVOKER}] AS ...
CREATE TRIGGER <TRIGGERNAME> ... [SQL SECURITY {DEFINER | INVOKER}] AS ...
CREATE PACKAGE <PACKAGENAME> [SQL SECURITY {DEFINER | INVOKER}] AS ...

Description:

Makes it possible to execute some objects with permissions of either definer or invoker.
By default INVOKER is used to keep backword compatibility.

If INVOKER is specified a current set of privileges of the current user will ba used.
If DEFINER - a set of privileges of object owner will be used to check an access to database objects used by this object.

Trigger inherits SQL SECURITY option from TABLE but can overwrite it by explicit specifying.

For procedures and functions defined in package explicit SQL SECURITY clause is prohibit.

Example 1. It's enought to grant only select privilege to user "us" for table.
In case of INVOKER it will require also EXECUTE for function F.

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

Example 2. It's enought to grant only execute privilege to user "us". In case of INVOKER it will require also INSERT for table T to either user "us" or procedure "p".

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

Example 3. In this example SQL SECURITY is used for table and provide access to calculated columns without explicit grant execute privilege of function f to user "us".

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

Example 4. It's enought to grant only execute privilege to user "us" for package pk.
In case of INVOKER it will require also INSERT for table T to either user "us".

create table t (i integer);
set term ^;
create package pk sql security definer
as
begin
    function f(i integer) returns int;
end^

create package body pk
as
begin
    function f(i integer) returns int
    as
    begin
      insert into t values (:i);
      return i + 1;
    end
end^
set term ;^
grant execute on package pk to user us;

commit;

connect 'localhost:/tmp/69.fdb' user us password 'pas';
select pk.f(3) from rdb$database;
