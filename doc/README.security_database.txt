Initializing the Security Database
----------------------------------
The security database (security4.fdb) has no predefined users. This is intentional.
Having user with well known predefined password and full access rights is serious security risk.

Firebird starting with version 3.0 does not require presence of SYSDBA user. One can use another name
for god-like user, have them different per-database or even work without users in security database using
authentication plugins that do not require it. That all is out of scope here: this document describes
step by step how to manually prepare security database for usage similar to what one could see in previous
firebird (and/or interbase) versions. Normally this task is performed by installers but in a case you wish
to perform manual installation or complete failed on creation user installer - this text is for you.

You will need to create the user SYSDBA and set up the password for it
using SQL CREATE USER command syntax in embedded mode as your first step to getting
remote access to databases.

Initialization is performed in embedded mode using the isql utility. For an embedded connection, an authentication
password is not required and will be ignored if you provide one. An embedded connection will work fine
with no login credentials and "log you in" using your host credentials if you omit a user name. However, even
though the user name is not subject to authentication, creating or modifying anything in the existing security
database requires that the user be SYSDBA; otherwise, isql will throw a privilege error for the CREATE USER
request.

The SQL user management commands will work with any open database. Because the sample database employee.fdb
is present in your installation and already aliased in databases.conf, it is convenient to use
it for the user management task.

1. Stop the Firebird server. Firebird caches connections to the security database aggressively. The presence
   of server connections may prevent isql from establishing an embedded connection.
2. In a suitable shell, start an isql interactive session, opening the employee database via its alias:
    > isql -user sysdba employee
3. Create the SYSDBA user:
    WARNING! Do not just copy and paste! Generate your own strong password!

    SQL> create user SYSDBA password 'StrongPassword';
    SQL> exit;

	WARNING! Do not just copy and paste! Generate your own strong password!
4. To complete the initialization, start the Firebird server again. Now you will be able to perform a network
   login to databases using login SYSDBA and the password you assigned to it.

An effective password with authentication plugin Srp can be up to 20 characters, Srp256 provides efficient
passwords up to 32 characters or up to 64 characters when Srp512 is used. At the same time a password of up to
255 characters will be valid but in a case of brute force attack it's highly possible that duplicated password
with length approximately equal to efficient one can be found.

The initialization can also be scripted using the file input option of isql with the content being same as interactive usage.
> isql -i init.sql -user sysdba employee
