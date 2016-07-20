SQL SECURITY.

   Implements capability to run executable objects regarding SQL SECURITY clause.
   SQL Standard (2003, 2011) Feature.

Author:
   Red Soft, roman.simakov(at)red-soft.ru

Syntax is:

CREATE TABLE <TABLENAME> (...) [SQL SECURITY {DEFINER | INVOKER}]
ALTER TABLE <TABLENAME> ... [{ALTER SQL SECURITY {DEFINER | INVOKER} | DROP SQL SECURITY}]
CREATE [OR ALTER] FUNCTION <FUNCTIONNAME> ... [SQL SECURITY {DEFINER | INVOKER}] AS ...
CREATE [OR ALTER] PROCEDURE <PROCEDURENAME> ... [SQL SECURITY {DEFINER | INVOKER}] AS ...
CREATE [OR ALTER] TRIGGER <TRIGGERNAME> ... [SQL SECURITY {DEFINER | INVOKER}] [AS ...]
CREATE [OR ALTER] PACKAGE <PACKAGENAME> [SQL SECURITY {DEFINER | INVOKER}] AS ...

Description:

Makes it possible to execute some objects with permissions of either definer or invoker.
By default INVOKER is used to keep backword compatibility.

If INVOKER is specified a current set of privileges of the current user will ba used.
If DEFINER - a set of privileges of object owner will be used to check an access to database objects used by this object.

Trigger inherits SQL SECURITY option from TABLE but can overwrite it by explicit specifying.

For procedures and functions defined in package explicit SQL SECURITY clause is prohibit.


Example 1. It's enought to grant only SELECT privilege to user US for table T.
In case of INVOKER it will require also EXECUTE for function F.

set term ^;
create function f() returns int
as
begin
    return 3;
end^
set term ;^
create table t (i integer, c computed by (i + f())) sql security definer;
insert into t values (2);
grant select on table t to user us;

commit;

connect 'localhost:/tmp/7.fdb' user us password 'pas';
select * from t;


Example 2. It's enough to grant EXECUTE privilege to user US for function F.
In case of INVOKER it will require also INSERT for table T.

set term ^;
create function f (i integer) returns int sql security definer
as
begin
  insert into t values (:i);
  return i + 1;
end^
set term ;^
grant execute on function f to user us;

commit;

connect 'localhost:/tmp/59.fdb' user us password 'pas';
select f(3) from rdb$database;


Example 3. It's enought to grant only EXECUTE privilege to user US for procedure P.
In case of INVOKER it will require also INSERT for table T to either user US or procedure P.

set term ^;
create procedure p (i integer) sql security definer
as
begin
  insert into t values (:i);
end^
set term ;^

grant execute on procedure p to user us;
commit;

connect 'localhost:/tmp/17.fdb' user us password 'pas';
execute procedure p(1);


Example 4. It's enought to grant only INSERT privilege to user US for table TR.
In case of INVOKER it will require also INSERT for table T to user US.

create table tr (i integer);
create table t (i integer);
set term ^;
create trigger tr_ins for tr after insert sql security definer
as
begin
  insert into t values (NEW.i);
end^
set term ;^
grant insert on table tr to user us;

commit;

connect 'localhost:/tmp/29.fdb' user us password 'pas';
insert into tr values(2);

the same result if specify SQL SECURITY DEFINER for table TR.

create table tr (i integer) sql security definer;
create table t (i integer);
set term ^;
create trigger tr_ins for tr after insert
as
begin
  insert into t values (NEW.i);
end^
set term ;^
grant insert on table tr to user us;

commit;

connect 'localhost:/tmp/29.fdb' user us password 'pas';
insert into tr values(2);


Example 5. It's enought to grant only EXECUTE privilege to user US for package PK.
In case of INVOKER it will require also INSERT for table T to user US.

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
