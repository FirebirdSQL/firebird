-----------------------------------------
Generic user and system context variables
-----------------------------------------

Function:
    New built-in functions give access to some information about current
    connection and current transaction. Also they provide means to associate
    and retrieve user context data with transaction or connection.

Author:
    Nickolay Samofatov <nickolay at broadviewsoftware dot com>

Format:
    RDB$SET_CONTEXT( <namespace>, <variable>, <value> )
    RDB$GET_CONTEXT( <namespace>, <variable> )

Returned value:
    INTEGER for RDB$SET_CONTEXT
    VARCHAR(32765) for RDB$GET_CONTEXT

Usage:

  RDB$SET_CONTEXT and RDB$GET_CONTEXT set and retrieve current value for the
  context variables. Namespace name identifies a group of context variables with
  similar properties. Access rules such as the fact if variables may be read and
  written to and by whom are determined by namespace which they belong to.

  Namespace and variable names are case-sensitive.

  Value of a variable is stored as a string, its length is limited by 32765 bytes.

  RDB$GET_CONTEXT retrieves current value of a variable. If variable does not
  exist in namespace return value for the function is NULL.

  RDB$SET_CONTEXT sets a value for specific variable. Function returns value of
  1 if variable existed before the call and 0 otherwise. To delete variable from
  context set its value to NULL.

  Currently, there is a fixed number of pre-defined namespaces you may use.

  USER_SESSION namespace offers access to session-specific user-defined
  variables. You can define and set value for variable with any name in this
  context. USER_TRANSACTION namespace offers the same possibilities for
  individual transactions.

  SYSTEM namespace provides read-only access to the following variables.

   Variable name                  Value
  ------------------------------------------------------------------------------
   NETWORK_PROTOCOL	            | The network protocol used by client to connect. Currently
                                | used values: "TCPv4", "TCPv6", "WNET", "XNET" and NULL.
                                |
   WIRE_COMPRESSED              | Compression status of current connection.
                                | If connection is compressed - returns "TRUE", if it is
                                | not compressed - returns "FALSE".
                                | If connection is embedded - returns NULL.
                                |
   WIRE_ENCRYPTED               | Encryption status of current connection.
                                | Value is the same as for compression status above.
                                |
   WIRE_CRYPT_PLUGIN            | If connection is encrypted - returns name of current plugin,
                                | otherwise NULL.
                                |
   CLIENT_ADDRESS               | The wire protocol address and port number of remote client
                                | represented as string. Value is IP address concatenated with
                                | port number using the '/' separator character. Value is
                                | returned for TCPv4 and TCPv6 protocols only, for all other
                                | protocols NULL is returned.
                                |
   CLIENT_HOST                  | The wire protocol host name of remote client. Value is
                                | returned for all supported protocols.
                                |
   CLIENT_PID                   | Process ID of remote client application
                                |
   CLIENT_PROCESS               | Process name of remote client application
                                |
   DB_NAME                      | Canonical name of current database. It is either alias
                                | name if connectivity via file names is not allowed or
                                | fully expanded database file name otherwise.
                                |
   DB_GUID                      | GUID of the current database
                                |
   DB_FILE_ID                   | Unique filesystem-level ID of the current database
                                |
   ISOLATION_LEVEL              | Isolation level for current transaction. Returned values
                                | are "READ COMMITTED", "CONSISTENCY", "SNAPSHOT".
                                |
   TRANSACTION_ID               | Numeric ID for current transaction. Returned value is the
                                | same as of CURRENT_TRANSACTION pseudo-variable.
                                |
   LOCK_TIMEOUT                 | Lock timeout value specified for current transaction
                                |
   READ_ONLY                    | Returns "TRUE" if current transaction is read-only and
                                | "FALSE" otherwise
                                |
   SESSION_ID                   | Numeric ID for current session. Returned value is the
                                | same as of CURRENT_CONNECTION pseudo-variable.
                                |
   CURRENT_USER                 | Current user for the connection. Returned value is the
                                | same as of CURRENT_USER pseudo-variable.
                                |
   EFFECTIVE_USER               | Effective user for now. It indicates privileges of
                                | which user is currently used to execute function, procedure, trigger.
                                |
   CURRENT_ROLE                 | Current role for the connection. Returned value is the
                                | same as of CURRENT_ROLE pseudo-variable.
                                |
   ENGINE_VERSION               | Engine version number, e.g. "2.1.0" (since V2.1)
                                |
   GLOBAL_CN                    | Most current value of global Commit Number counter
                                |
   SNAPSHOT_NUMBER              | Value of Snapshot Number of currently database snapshot: either
                                | transaction level (for SNAPSHOT or CONSISTENCY transaction),
                                | or request level (for READ COMMITTED READ CONSISTENCY
                                | transaction). NULL, if snapshot is not exist.
                                |
   SESSION_IDLE_TIMEOUT         | Current value of idle connection timeout
                                |
   STATEMENT_TIMEOUT            | Current value of statement execution timeout
                                |
   REPLICATION_SEQUENCE         | Current replication sequence (number of the latest segment
                                | written to the replication journal)
                                |
   REPLICA_MODE                 | Replica mode of the database. Possible values are
                                | "READ-ONLY", "READ-WRITE" and NULL.
                                |
   EXT_CONN_POOL_SIZE           | Pool size (number of connections inside the pool)
                                |
   EXT_CONN_POOL_IDLE_COUNT     | Count of currently inactive connections in the pool
                                |
   EXT_CONN_POOL_ACTIVE_COUNT   | Count of active connections, associated with pool
                                |
   EXT_CONN_POOL_LIFETIME       | Idle connection lifetime, in seconds
                                |
   SESSION_TIMEZONE             | Current session time zone.

Notes:
   To prevent DoS attacks against Firebird Server you are not allowed to have
   more than 1000 variables stored in each transaction or session context.

Example(s):

create procedure set_context(User_ID varchar(40), Trn_ID integer) as
begin
  RDB$SET_CONTEXT('USER_TRANSACTION', 'Trn_ID', Trn_ID);
  RDB$SET_CONTEXT('USER_TRANSACTION', 'User_ID', User_ID);
end;

create table journal (
   jrn_id integer not null primary key,
   jrn_lastuser varchar(40),
   jrn_lastaddr varchar(255),
   jrn_lasttransaction integer
);

CREATE TRIGGER UI_JOURNAL FOR JOURNAL BEFORE INSERT OR UPDATE
as
begin
  new.jrn_lastuser = rdb$get_context('USER_TRANSACTION', 'User_ID');
  new.jrn_lastaddr = rdb$get_context('SYSTEM', 'CLIENT_ADDRESS');
  new.jrn_lasttransaction = rdb$get_context('USER_TRANSACTION', 'Trn_ID');
end;

execute procedure set_context('skidder', 1);

insert into journal(jrn_id) values(0);

commit;
