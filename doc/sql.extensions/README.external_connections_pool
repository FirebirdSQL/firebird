  The pool of external connections.

  To avoid delays when external connections established frequently, external 
data source (EDS) subsystem is supplemented by the pool of external connections.
The pool keeps unused external connections for some time and allows to avoid the 
cost of connecting / disconnecting for frequently used connection strings.

Author:
   Vlad Khorsun <hvlad@users.sourceforge.net>

How pool works:
- every external connection is associated with a pool when created
- pool maintains two lists: idle connections and active connections
- when some connection become unused (i.e. it have no active requests and no 
  active transactions), it is reset and placed into idle list (on successful 
  reset) or closed (if reset failed).
  Connection is reset using ALTER SESSION RESET statement. It is considered 
  successful if no error occurs. Note, if external data source not supported
  ALTER SESSION RESET statement - it is not considered as error and such
  connection will be placed into pool.
- if the pool has reached max. size, the oldest idle connection is closed
- when engine ask to create a new external connection, the pool first looks 
  for candidate at the idle list. The search is based on 4 parameters:
  - connection string,
  - username,
  - password,
  - role.
  The search is case sensitive.
- if suitable connection is found, then it tested if it is still alive
  - if it did not pass the check, it is deleted and the search is repeated
    (the error is not reported to the user)
- found (and alive) connection is moved from idle list to active list and 
  returned to the caller
- if there are several suitable connections, the most recently used is chosen
- if there is no suitable connection, the new one is created (and put into 
  active list)
- when idle connection gets expired (exceeded the lifetime), it is deleted from 
  the pool and closed.

Key characteristics:
- absence of "eternal" external connections
- limited number of inactive (idle) external connections at the pool
- support a quick search among the connections (using 4 parameters above)
- the pool is common for all external databases
- the pool is common for all local connections handled by the given Firebird 
  process

Pool parameters:
- connection life time: the time interval from the moment of the last usage of 
  connection after which it will be forcibly closed
- pool size: the maximum allowed number of idle connections in the pool

Pool management:
  New SQL statement is introduced to manage the pool:

	ALTER EXTERNAL CONNECTIONS POOL. 

  When prepared it is described as DDL statement but have immediate effect: i.e. 
it is executed immediately and completely, not waiting for transaction commit.
Changes applied to the in-memory instance of the pool in current Firebird 
process. Therefore change in one Classic process doesn't affect other Classic
processes. Changes is not persistent and after restart Firebird will use pool 
settings at firebird.conf (see below). 

  New system privilege "MODIFY_EXT_CONN_POOL" is required to run the statement.

The full syntax is:

- ALTER EXTERNAL CONNECTIONS POOL SET SIZE <int>
  set maximum number of idle connections. 

  Valid values are from 0 to 1000.
  Value of zero means that pool is disabled. 
  Default value is set in firebird.conf (see below).

- ALTER EXTERNAL CONNECTIONS POOL SET LIFETIME <int> <time_part>,
    where <time_part> is SECOND | MINUTE | HOUR

  Set idle connection lifetime, in seconds. 
  Valid values are from 1 SECOND to 24 HOUR.
  Default value is set in firebird.conf (see below).

- ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL
  Closes all idle connections. 
  Disassociates all active connections off the pool (such connections will be 
  closed immediately when gets unused).

- ALTER EXTERNAL CONNECTIONS POOL CLEAR OLDEST
  Closes expired idle connections. 

  The state of external connections pool could be queried using new context
variables in 'SYSTEM' namespace:
- EXT_CONN_POOL_SIZE			pool size
- EXT_CONN_POOL_LIFETIME		idle connection lifetime, in seconds
- EXT_CONN_POOL_IDLE_COUNT		count of currently inactive connections
- EXT_CONN_POOL_ACTIVE_COUNT	count of active connections, associated with pool


  Firebird configuration (firebird.conf) got two new settings related with pool:

- ExtConnPoolSize = 0, pool size, and
- ExtConnPoolLifeTime = 7200, idle connection lifetime, seconds
