Cumulative roles.

   Implements capability to grant role to role.

Author:
   Red Soft Corporation, roman.simakov(at)red-soft.biz

Syntax is:

GRANT [DEFAULT] <role name> TO [USER | ROLE] <user/role name> [WITH ADMIN OPTION];
REVOKE [DEFAULT] <role name> FROM [USER | ROLE] <user/role name> [WITH ADMIN OPTION];

Description:

Makes it possible to grant a role to user or another role.

If DEFAULT keyword is used the role will be used every time for user even if it's not specified explicitly.
While connecting user will get permissions of all roles which were granted to him with DEFAULT keyword and
permissions of all roles also granted to them with DEFAULT keyword specified.
If user specify a role in connection he will also get permissions of this role (if granted of course) and
permissions of all roles granted to it, etc.

When some user want go grant a role to another user or role ADMIN OPTION will be checked. In this case user can grant
a role cumulatively granted to him only if every role in sequence has ADMIN OPTION.

REVOKE works as usual except if DEFAULT is specified only default option will be revoked. In other words
role skill be granted but like without DEFAULT.

Let:
"->" grant without ADMIN OPTION
"=>" grant with ADMIN OPTION

Consider 3 options:
1) WORKER->MANAGER->Joe
2) WORKER->MANAGER=>Joe
3) WORKER=>MANAGER->Joe
4) WORKER=>MANAGER=>Joe

Joe can grant role MANAGER in 2 and 4 options and role WORKER only in 4 option. In 1 and 3 options Joe cannot grant
nothing even in 3 option WORKER granted to MANAGER with ADMIN OPTION.

Sample:

CREATE DATABASE 'LOCALHOST:/TMP/CUMROLES.FDB';
CREATE TABLE T(I INTEGER);
CREATE ROLE TINS;
CREATE ROLE CUMR;
GRANT INSERT ON T TO TINS;
GRANT DEFAULT TINS TO CUMR WITH ADMIN OPTION;
GRANT CUMR TO USER US WITH ADMIN OPTION;
CONNECT 'LOCALHOST:/TMP/CUMROLES.FDB' USER 'US' PASSWORD 'PAS';
INSERT INTO T VALUES (1);
GRANT TINS TO US2;

Use RDB$ROLE_IN_USE function to check if privileges of specified role are currently available to the current user.

Syntax:
RDB$ROLE_IN_USE(role_name varchar(N)) RETURNS BOOLEAN

Pay attention - role name should be entered exactly as it's returned by CURRENT_ROLE, function is case-sensitive!

Sample:
To get a list of currently active roles you can run:
SELECT * FROM RDB$ROLES WHERE RDB$ROLE_IN_USE(RDB$ROLE_NAME)
