# Management of External Data \(FB 6.0\)

## Concept

Firebird 6.0 allows to managing external data \(distributed or heterogeneous queries\). Distributed queries allow managing data stored in other databases. Those databases can be located on the same or different servers and use the same or different DBMS. Distributed queries allow to integrate and process data from different sources, providing developers with the ability to work with them as a single logical unit.

External data management is defined by ISO/IEC 9075\-9:2023\(E\) \(SQL/MED\) standard. External data management implies both read and write access to external data. Access to foreign data is implemented using 4 objects: foreign-data wrapper, foreign server, user mapping, and foreign table.

## Syntax

### Foreign-data wrapper

In Firebird, the foreign-data wrapper can be implemented as a provider \(plugin\). To use a foreign-data wrapper, it should be defined in `plugins.conf`, for example:
```
Plugin = JDBC {
    Module = $(dir_plugins)/jdbc_provider
    Config = JDBC_config
}

Config = JDBC_config {
    Dir = $(this)/jdbc
}
```
To connect to Firebird databases, the Engine and Remote providers are available by default and require no additional definition in `plugins.conf`.

### Foreign server

A foreign server is a logical representation of a remote data source inside the DBMS. The DBMS connects to the foreign server to execute queries. The foreign server provides metadata and connection information about the remote data source. Usually, the external server stores the connection parameters such as connection string, port, database type, etc.

#### CREATE SERVER definition

```sql
CREATE SERVER [IF NOT EXISTS] <foreign server name>
        [{FOREIGN DATA WRAPPER | USING PLUGIN} <provider (plugin)>] [OPTIONS (<option> [,<option> ...] )]
    <option> ::= {
        CONNECTION_STRING = '<connection string>'
        | USER = '<user name>'
        | PASSWORD = '<password>'
        | ROLE = '<role>'
        | <option name> = 'value'
        | <option name> [{FILE | ENV}] 'value' }
```
The `SERVER` clause specifies the name of the server.
The `FOREIGN DATA WRAPPER` or `USING PLUGIN` clause specifies the provider to use to access foreign data. If no clause is specified, the providers specified in the Providers parameter of the `firebird.conf` configuration file will be tried for connection. The clause `USING PLUGIN` is not specified in the standard, but it is familiar to Firebird developers and users, so it is proposed as an extension to the standard definition of a foreign server.
The optional `OPTIONS` clause defines additional connection parameters to the foreign server. The required connection parameters depend on the provider \(plugin\) used to access the data. The option value can be read from a file or an environment variable. If it is necessary to read the value from a file, then `FILE` is specified after the option name or `ENV` is specified to read from the environment variable. The server should have the permissions to read the specified file. General options are:
* `CONNECTION_STRING` - connection string to the foreign server \(depends on the foreign server specification\), if the option is not specified, an attempt will be made to connect to the current database;
* `USER` - the user on whose name the connection will be established;
* `PASSWORD` - user password;
* `ROLE` - the role with which the specified user will connect to the foreign server.

The `CREATE SERVER` statement can be executed by administrators or users with the `CREATE SERVER` privilege.

#### ALTER SERVER definition

```sql
ALTER SERVER <foreign server name>
        [[DROP] FOREIGN DATA WRAPPER | [DROP] USING PLUGIN <provider(plugin)>] [OPTIONS(<option> [, <option> ...] )]
    <option> ::= {
        [DROP] <option name> [= 'value']
        | [DROP] <option name> [{FILE | ENV}] ['value'] }
```
If existing server options are specified, they will be updated and the others will not be affected.
The `ALTER SERVER` statement can be executed by administrators, owner or users with the `ALTER SERVER` privilege.

#### DROP SERVER definition

```sql
DROP SERVER <foreign server name>
```
The `DROP SERVER` statement can be executed by administrators, owner or users with the `DROP SERVER` privilege. If the server has dependencies, the `DROP SERVER` statement will fail with an execution error.

### User mappings

User mappings for foreign connections define the connection parameters to be used by the specified user when accessing a foreign server. The user mapping provides security and access control by specifying the credentials to connect to the foreign server. User mapping maps local user accounts to remote server credentials and defines the login and password for the connection.

#### CREATE USER MAPPING FOR definition

```sql
CREATE USER MAPPING FOR <user name> SERVER <server name> [OPTIONS (<option> [, <option> ...] )]
    <option> ::= {
        CONNECTION_STRING = '<connection string>'
        | USER = '<user name>'
        | PASSWORD = '<password>'
        | ROLE = '<role>'
        | <option name> = 'value'
        | <option name> [{FILE | ENV}] 'value' }
```
The general options are the same as for the `CREATE SERVER` statement. When connecting to a foreign server, the option values specified in the user mapping will be used. The connection parameters specified in the user mapping have a higher priority than those specified for the foreign server.

The `CREATE USER MAPPING FOR` statement can be executed by administrators and database owner.

#### ALTER USER MAPPING FOR definition

```sql
ALTER USER MAPPING FOR <user name> SERVER <server name> [OPTIONS (<option> [, <option> ...] )]
    <option> ::= {
        [DROP] <option name> [= 'value']
        | [DROP] <option name> [{FILE | ENV}] ['value'] }
```
If existing server options are specified, they will be updated and the others will not be affected.
The `ALTER USER MAPPING FOR` statement can be executed by administrators and database owner.

#### DROP USER MAPPING FOR definition

```sql
DROP USER MAPPING FOR <user name> SERVER <server name>
```
The `DROP USER MAPPING FOR` statement can be executed by administrators and database owner.

### Access to foreign data

Foreign server data can be managed in two ways: using the `EXECUTE STATEMENT` statement or via a foreign table.

### Foreign table

A foreign table is a table that is physically stored on a foreign server but is accessible by the local DBMS. The foreign table allows executing SQL queries to the data contained in the table as local ones. The foreign table includes the table structure description and column mapping between the foreign and local table.

#### CREATE FOREIGN TABLE definition

```sql
CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table name>
            (<foreign column definition> [, {<foreign column definition> | <table constraint>}...])
            SERVER <server name> [OPTIONS (<option> [, <option> ...] )]
    <foreign column definition> ::=
        <regular column definition>
        | <identity column definition>
        [OPTIONS (<option> [, <option> ...] )]
    <regular column definition> ::=
        <column name> { <data type> | <domain name>}
        [DEFAULT {<literal> | NULL | <context variable>}]
        [NOT NULL]
        [<column constraint>]
        [COLLATE <collation name>]
    <identity column definition> ::=
        <column name> [<data type>]
        GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [(<identity column option> [<identity column option>])]
        [<column constraint>]
    <identity column option> ::= START WITH <start value>
        | INCREMENT [BY] <increment value>
    <column constraint> ::=
        [CONSTRAINT <constraint name>] CHECK (<check condition>)
    <table constraint> ::=
        [CONSTRAINT <constraint name>] CHECK (<check condition>)
    <option> ::= {
        <option name> [= 'value']
        | <option name> ['value'] }
```
Rules:
* The name of the foreign table must be unique;
* The required parameter is the name of the foreign server;
* If the specified foreign server does not exist, the creation of the foreign table will fail;
* The table name and column names should be the same as the object names on the foreign server. Can be remapped using the `OPTIONS` clause;
* A foreign column cannot be `UNIQUE`, `PRIMARY KEY`, or `FOREIGN KEY`;
* A foreign column cannot be computed \(`COMPUTED [BY] | GENERATED ALWAYS AS`\);
* Foreign columns can have `NOT NULL` and `CHECK` integrity constraints, but this does not guarantee that the foreign server will check these constraints;
* Otherwise, the rules for creating a foreign table are the same as for a regular table.

The optional `OPTIONS` clause specifies additional foreign column options and table options. The required foreign column and table options depend on the provider \(plugin\) used to access table data. General table options are:
* SCHEMA_NAME - the schema name where the table is located on the foreign server;
* TABLE_NAME - name of the table on the foreign server.
General column options are:
* COLUMN_NAME - the name of the column in the foreign table;
* PRIMARY_KEY - an identifier explicitly specifying that the column is used in the primary key.

The table name should include the full schema name and the table name on the foreign server \(if not specified in the options\). The table names should match. Column names should match \(if not specified in the options\). The number of defined columns should be less than or equal to the columns in the table on the foreign server.

The `CREATE FOREIGN TABLE` statement can be executed by administrators and users with the `CREATE FOREIGN TABLE` privilege and. The user executing the `CREATE FOREIGN TABLE` statement becomes the owner of the foreign table.

#### ALTER FOREIGN TABLE definition

```sql
ALTER FOREIGN TABLE <table name> <modification operation> [, <modification operation>...]
      [OPTIONS (<option> [, <option> ...] )]
    <modification operation> ::=
        ADD <foreign column definition>
        | ADD <table constraint>
        | DROP <column name>
        | DROP CONSTRAINT <column or table constraint>
        | ALTER [COLUMN] <column name> <foreign column modification>
        | [OPTIONS (<option> [, <option> ...] )]
    <foreign column definition> ::=
        <regular column definition>
        | <identity column definition>
        [OPTIONS (<option> [, <option> ...] )]
    <regular column definition> ::=
        <column name> { <data type> | <domain name>}
        [DEFAULT {<literal> | NULL | <context variable>}]
        [NOT NULL]
        [<column constraint>]
        [COLLATE <collation name>]
    <identity column definition> ::=
        <column name> [<data type>]
        GENERATED {ALWAYS|BY DEFAULT} AS IDENTITY [(<identity column option> [<identity column option>])]
    [<column constraint>]
    <foreign column modification> ::=
        TO <new column name>
        | POSITION <new position>
        | <regular column modification>
        | <identity column definition>
    <regular column modification> ::=
        TYPE { <data type> | <domain name> }
        | SET DEFAULT { <literal> | NULL | <context variable>}
        | DROP DEFAULT
        | SET NOT NULL
        | DROP NOT NULL
    <identity column definition> ::=
        <identity column option>
        | SET GENERATED {ALWAYS|BY DEFAULT} [ <identity column option> ...]
        | DROP IDENTITY
    <identity column option> ::= {
        RESTART [ WITH <start value> ]
        | SET INCREMENT [BY] <increment value> }
    <option> ::=
        [DROP] <option name> [= 'value']
        | [DROP] <option name> ['value']
```
The mandatory parameters are the table name and the modification operation definition. If new values of existing options of a foreign column or foreign table are specified, they will be updated and the others will not be affected. Otherwise, the rules for altering a foreign table are the same as for a regular table.

The `ALTER FOREIGN TABLE` statement can be executed by administrators, the owner of the table and users with the `ALTER FOREIGN TABLE` privilege.

#### DROP FOREIGN TABLE definition

```sql
DROP FOREIGN TABLE <table name>
```

A foreign table that is referenced in triggers cannot be deleted, except for triggers written by the user specifically for this table. Also, a foreign table that is used in a stored procedure or view cannot be deleted. When a foreign table is dropped, all its triggers and options \(include column options\) will be deleted as well.

The `DROP TABLE` statement can be executed by administrators, the owner of the table and users with the `DROP FOREIGN TABLE` privilege.

### EXECUTE STATEMENT changes

```sql
<execute_statement> ::= EXECUTE STATEMENT <argument>
      [<option> ...]
      [INTO <variables>];
    <argument> ::= <paramless_stmt>
                | (<paramless_stmt>)
                | (<stmt_with_params>) (<param_values>)
    <param_values> ::= <named_values> | <positional_values>
    <named_values> ::= <named_value> [, <named_value> ...]
    <named_value> ::= [EXCESS] paramname := <value_expr>
    <positional_values> ::= <value_expr> [, <value_expr> ...]
    <option> ::=
        WITH {AUTONOMOUS | COMMON} TRANSACTION
      | WITH CALLER PRIVILEGES
      | AS USER user
      | PASSWORD password
      | ROLE role
      | ON EXTERNAL { [DATA SOURCE] <connection_string> | SERVER <server name> }
    <connection_string> ::=
      !! See <filespec> in the CREATE DATABASE syntax !!
    <variables> ::= [:]varname [, [:]varname ...]
```

The mandatory option for the `ON EXTERNAL SERVER` clause is the name of the foreign server. If the specified foreign server does not exist, creating an object with this `EXECUTE STATEMENT` will fail. The connection parameters specified in the `EXECUTE STATEMENT` statement have a higher priority than those specified in the user mapping or specified for the foreign server.

## System metadata changes

A new field is added for the `RDB$RELATIONS` table that stores the foreign server name:
| Field                      | Type              | Description                                                |
|----------------------------|-------------------|------------------------------------------------------------|
| RDB$FOREIGN_SERVER_NAME    | CHAR \(63\)         | The foreign server name                                    |

If the `RDB$FOREIGN_SERVER_NAME` field contains a value other than NULL, the metadata is considered to match the foreign table.

The following system tables have been added:

`RDB$FOREIGN_SERVERS` - contains a description of all foreign servers defined in the database:

| Field                      | Type                 | Description                                                |
|----------------------------|----------------------|------------------------------------------------------------|
| RDB$FOREIGN_SERVER_NAME    | CHAR \(63\)          | The foreign server name                                    |
| RDB$FOREIGN_SERVER_WRAPPER | CHAR \(63\)          | Provider (plugin) used to connect to a foreign server      |
| RDB$SECURITY_CLASS         | CHAR \(63\)          | Security class                                             |
| RDB$OWNER_NAME             | CHAR \(63\)          | The username of the user who created the server originally |

`RDB$FOREIGN_SERVER_OPTIONS` - contains a description of all options defined for the foreign servers in the database:

| Field                      | Type                 | Description                                                |
|----------------------------|----------------------|------------------------------------------------------------|
| RDB$FOREIGN_SERVER_NAME    | CHAR \(63\)          | The foreign server name                                    |
| RDB$FOREIGN_OPTION_NAME    | CHAR \(63\)          | Option name                                                |
| RDB$FOREIGN_OPTION_VALUE   | VARCHAR \(32765\)    | Option value                                               |
| RDB$FOREIGN_OPTION_TYPE    | SHORT                | Option type                                                |

`RDB$FOREIGN_USER_MAPPINGS` - contains a description of all user mappings defined in the database:

| Field                      | Type                 | Description                                                |
|----------------------------|----------------------|------------------------------------------------------------|
| RDB$USER                   | CHAR \(63\)          | The user name to map                                       |
| RDB$FOREIGN_SERVER_NAME    | CHAR \(63\)          | The foreign server name to map                             |

`RDB$FOREIGN_MAPPING_OPTIONS` - contains a description of all options defined for user mappings to foreign servers in the database:

| Field                      | Type                 | Description                                                |
|----------------------------|----------------------|------------------------------------------------------------|
| RDB$USER                   | CHAR \(63\)          | The user name to map                                       |
| RDB$FOREIGN_SERVER_NAME    | CHAR \(63\)          | The foreign server name                                    |
| RDB$FOREIGN_OPTION_NAME    | CHAR \(63\)          | Option name                                                |
| RDB$FOREIGN_OPTION_VALUE   | VARCHAR \(32765\)    | Option value                                               |
| RDB$OWNER_NAME             | SHORT                | Option type                                                |

This table completely absorbs table `RDB$FOREIGN_USER_MAPPINGS`, but the ISO/IEC 9075\-9:2023\(E\) \(SQL/MED\) standard requires a separate table for user mappings and user mapping options.

`RDB$FOREIGN_TABLE_FIELD_OPTIONS` - contains a description of all options defined for foreign table columns in the database:

| Field                      | Type                 | Description                                                |
|----------------------------|----------------------|------------------------------------------------------------|
| RDB$SCHEMA_NAME            | CHAR \(63\)          | The schema name                                            |
| RDB$TABLE_NAME             | CHAR \(63\)          | The table name                                             |
| RDB$FIELD_NAME             | CHAR \(63\)          | The field name                                             |
| RDB$FOREIGN_OPTION_NAME    | CHAR \(63\)          | Option name                                                |
| RDB$FOREIGN_OPTION_VALUE   | VARCHAR \(32765\)    | Option value                                               |

`RDB$FOREIGN_TABLE_OPTIONS` - contains a description of all options defined for foreign tables in the database:

| Field                      | Type                | Description                                                |
|----------------------------|---------------------|------------------------------------------------------------|
| RDB$SCHEMA_NAME            | CHAR \(63\)         | The schema name                                            |
| RDB$TABLE_NAME             | CHAR \(63\)         | The table name                                             |
| RDB$FOREIGN_OPTION_NAME    | CHAR \(63\)         | Option name                                                |
| RDB$FOREIGN_OPTION_VALUE   | VARCHAR \(32765\)   | Option value                                               |

For `RDB$FOREIGN_SERVERS` a unique index is created on the field `RDB$FOREIGN_SERVER_NAME`.
For `RDB$FOREIGN_SERVERS_OPTIONS` creates a unique index on the fields `RDB$FOREIGN_SERVER_NAME` and `RDB$FOREIGN_OPTION_NAME`.
For `RDB$FOREIGN_TABLE_OPTIONS` a unique index is created on the `RDB$SCHEMA_NAME`, `RDB$TABLE_NAME` and `RDB$FOREIGN_OPTION_NAME` fields.
For `RDB$FOREIGN_USER_MAPPINGS` a unique index is created for the fields `RDB$USER` and `RDB$FOREIGN_SERVER_NAME`.
For `RDB$FOREIGN_MAPPING_OPTIONS` a unique index is created on the `RDB$USER`, `RDB$FOREIGN_SERVER_NAME`, and `RDB$FOREIGN_OPTION_NAME` fields.
For `RDB$FOREIGN_TABLE_FIELD_OPTIONS` a unique index is created for the `RDB$SCHEMA_NAME`, `RDB$TABLE_NAME`, `RDB$FIELD_NAME`, and `RDB$FOREIGN_OPTION_NAME` fields.

Connections to foreign servers are created using the external data source \(EDS\) subsystem.

*This documentation will be updated.*
