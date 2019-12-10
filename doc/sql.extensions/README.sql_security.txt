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
CREATE [OR ALTER] TRIGGER <TRIGGERNAME> ... [SQL SECURITY {DEFINER | INVOKER} | DROP SQL SECURITY] [AS ...]
CREATE [OR ALTER] PACKAGE <PACKAGENAME> [SQL SECURITY {DEFINER | INVOKER}] AS ...

ALTER DATABASE SET DEFAULT SQL SECURITY {DEFINER | INVOKER}

Description:

Makes it possible to execute some objects with permissions of either definer or invoker.
By default INVOKER is used to keep backward compatibility. You can change this behavior and be more compatible
with SQL STANDARD by using ALTER DATABASE SET DEFAULT SQL SECURITY statement.

If INVOKER is specified a current set of privileges of the current user will be used.
If DEFINER - a set of privileges of object owner will be used to check an access to database objects
used by this object.

Trigger inherits SQL SECURITY option from TABLE but can overwrite it by explicit specifying. If SQL SECURITY option
will be changed for table, existing triggers without explicitly specified option will not use new value immediately
it will take effect next time trigger will be loaded into metadata cache.

For procedures and functions defined in package explicit SQL SECURITY clause is prohibit.

In stored procedures, functions or triggers you may check which user if really effective and which privileges
are applying to accessed objects by using the system context variable EFFECTIVE_USER from SYSTEM namespace.
select RDB$GET_CONTEXT('SYSTEM', 'EFFECTIVE_USER') from RDB$DATABASE;

Note: now the same object may be called in different security contexts and requires different privileges.
For example we have:
- a stored procedure INV with SQL SECURITY INVOKER which insert records in a table T
- a stored procedure DEF with SQL SECURITY DEFINER defined by SYSDBA

If a user U calls INV an access to T will require an INSERT privile to be granted to U (and EXECUTE on INV of course).
In this case U is EFFECTIVE_USER due INV running.
If user U calls DEF an access to T will require an INSERT privilege to be granted to SYSDBA (end EXECUTE on DEF).
In this case SYSDBA is EFFECTIVE_USER due INV running as well as due DEF running.

Example 1. It's enough to grant only SELECT privilege to user US for table T.
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


Example 6. Altering explicit option SQL SECURITY for triggers.
To remove explicit SQL SECURITY OPTION from trigger you can execute:
alter trigger tr_ins drop sql security;

To set it again to SQL SECURITY INVOKER you can:
alter trigger tr_ins sql security invoker;
