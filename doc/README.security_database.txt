Initializing the Security Database
----------------------------------
The security database (security4.fdb) has no predefined users. This is intentional.
You will need to create the user SYSDBA and set up the password for it
using SQL CREATE USER command syntax in embedded mode as your first step to getting
access to databases and utilities.

Initialization is performed in embedded mode using the isql utility. For an embedded connection, an authentication
password is not required and will be ignored if you provide one. An embedded connection will work fine
with no login credentials and "log you in" using your host credentials if you omit a user name. However, even
though the user name is not subject to authentication, creating or modifying anything in the existing security
database requires that the user be SYSDBA; otherwise, isql will throw a privilege error for the CREATE USER
request.

The SQL user management commands will work with any open database. Because the sample database employee.fdb
is present in your installation and already aliased in databases.conf, it is convenient to use
it for the user management task.

1. Stop the Firebird server. Firebird 4 caches connections to the security database aggressively. The presence
   of server connections may prevent isql from establishing an embedded connection.
2. In a suitable shell, start an isql interactive session, opening the employee database via its alias:
    > isql -user sysdba employee
3. Create the SYSDBA user:
    WARNING! Do not just copy and paste! Generate your own strong password!

    SQL> create or alter user SYSDBA password 'StrongPassword';
    SQL> commit;
    SQL> quit;

	WARNING! Do not just copy and paste! Generate your own strong password!
4. To complete the initialization, start the Firebird server again. Now you will be able to perform a network
   login to databases, including the security database, using the password you assigned to SYSDBA.

An effective password, using the default user manager Srp, can be up to 20 characters, although a password
of up to 255 characters will be valid.

The initialization can also be scripted using the file input option of isql with the content being same as interactive usage.
> isql -i init.sql -user sysdba employee
