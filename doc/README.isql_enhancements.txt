Isql enhancements in Firebird v2.
---------------------------------

Author: Claudio Valderrama C. <cvalde at usa.net>

1) Command line switch -b to bail out on error when used in non-interactive mode.

When using scripts as input in the command line, it may be totally unappropriate
to let isql continue executing a batch of commands after an error has happened.
Therefore, the "-b[ail]" option was created. It will stop at the first error it
can detect. Most cases have been covered, but if you find some error that's not
recognized by isql, you should inform the project, as this is a feature in progress.
When isql stops, it means it will no longer execute any command in the input script
and will return an error code to the operating system. At this time there aren't
different error codes. A return non-zero return code should be interpreted as failure.
Depending on other options (like -o, -m and -m2) , isql will show the error message
on screen or will send it to a file.

Some features:

- Even if isql is executing nested scripts, it will cease all execution and will
return to the operating system when it detects an error. Nested scripts happen
when a script A is used as isql input but in turn A contains an INPUT command to
load script B an so on. Isql doesn't check for direct or indirect recursion, thus
if the programmer makes a mistake and script A loads itself or loads script B that
in turn loads script A again, isql will run until it exhaust memory or an error
is returned from the database, at whose point -bail if activated will stop all
activity.

- The line number of the failure is not yet known. It has been a private test feature
for some years but needs more work to be included in the official isql.

- DML errors will be caught when being prepared or executed, depending on the type
of error.

- DDL errors will be caught when being prepared or executed by default, since isql
uses AUTODDL ON by default. However, if AUTO DLL is OFF, the server only complains
when the script does an explicit COMMIT and this may involve several SQL statements.

- The feature can be enabled/disabled interactively or from a script by means of the
SET BAIL [ON | OFF]
command. As it's the case with other SET commands, simply using SET BAIL will toggle
the state between activated and deactivated. Using SET will display the state of
the switch among many others.

- Even if BAIL is activated, it doesn't mean it will change isql behavior. An
additional requirement should be met: the session should be non-interactive. A
non-interactive session happens when the user calls isql in batch mode, giving it
a script as input. Example:
isql -b -i my_fb.sql -o results.log -m -m2
However, if the user loads isql interactively and later executes a script with the
input command, this is considered an interactive session even though isql knows it's
executing a script. Example:
isql
Use CONNECT or CREATE DATABASE to specify a database
SQL> set bail;
SQL> input my_fb.sql;
SQL> ^Z
Whatever contents the script has, it will be executed completely even with errors
and despite the BAIL option was enabled.


2) SET HEADING ON/OFF option.

Some people consider useful the idea of doing a SELECT inside isql and have the
output sent to a file, for additional processing later, specially if the number
of columns make isql display unpractical. However, isql by default prints column
headers and in this scenario, they are a nuisance. Therefore, the feature (that was
previously the fixed default) can be enabled/disabled interactively or from a script
by means of the
SET HEADing [ON | OFF]
command. As it's the case with other SET commands, simply using SET HEAD will toggle
the state between activated and deactivated. Using SET will display the state of
the switch among many others.
Note: this switch cannot be deactivated with a command line parameter.


3) Command line switch -m2 to send output of statistics and plans to the same file
than the rest of the output.

When the user specifies that the output should be sent to a file, two possibilities
have existed for years: either the command line switch -o followed by a file name
is used or once inside isql, the command OUTput followed by a file name is used at
any time, be it an interactive or a batch session. To return the output to the
console, simply typing OUTput; is enough. So far so good, but error messages
don't go to that file. They are shown in the console. Then isql developed the -m
command line switch to melt the error messages with the normal output to whatever
place the output was being redirected. This left still another case: statistics
about operations (SET STATs command) and SQL plans as the server returns them
(SET PLAN and SET PLANONLY commands) are considered diagnostic messages and thus
were sent always to the console. What the -m2 command line switch does is to
ensure that such information about stats and plans goes to the same file the output
has been redirected to.
Note: neither -m nor -m2 have interactive counterparts through the SET command.
They only can be specified in the command line switches for isql.

4) Ability to show the line number where an error happened in a script.

In previous versions, the only reasonable way to know where a script had caused
an error was using the switched -e for echoing commands, -o to send the output
to a file and -m to merge the error output to the same file. This way, you could
observe the commands isql executed and the errors if they exist. The script continued
executing to the end. The server only gives a line number related to the single command
(statement) that it's executing, for some DSQL failures. For other errors, you
only know the statement caused problems.

With the addition of -b for bail as described in (1), the user is given the power
to tell isql to stop executing scripts when an error happens, but you still need to
echo the commands to the output file to discover which statement caused the failure.

Now, the ability to signal a script-related line number of a failure enables the
user to go to the script directly and find the offending statement. When the server
provides line and column information, you will be told the exact line in the script
that caused the problem. When the server only indicates a failure, you will be told
the starting line of the statement that caused the failure, related to the whole script.

This feature works even if there are nested scripts, namely, if script SA includes
script SB and SB causes a failure, the line number is related to SB. When SB is
read completely, isql continues executing SA and then isql continues counting lines
related to SA, since each file gets a separate line counter. A script SA includes
SB when SA uses the INPUT command to load SB.

Lines are counted according to what the underlying IO layer considers separate lines.
For ports using EDITLINE, a line is what readline() provides in a single call.
Line length is limited to 32767 bytes, but this has been always the limit.

5) SHOW SYSTEM command shows predefined UDFs.

The SHOW <object_type> command is meant to show user objects of that type.
The SHOW SYSTEM commmand is meant to show system objects, but until now it
only showed system tables. Now it lists the predefined, system UDFs incorporated
into FB 2. It may be enhanced to list system views if we create some of them
in the future.

6) -r2 command line parameter.

The sole objective of this parameter is to specify a case-sensitive role name.
With -r, the default switch, roles provided in the command line are uppercased.
With -r2, the role is passed to the engine exactly as typed in the command line.

7) Binary text is shown in hex.

This feature was contributed before the firt FB2 alpha. It will show content
from CHAR/VARCHAR columns in hex when the character set is binary (octets).
This feature is currently hardcoded: it can't be disabled.

8) SET SQLDA_DISPLAY ON/OFF option.

This option exists long before FB1 and it was available previously in DEBUG builds
only. Now this has been made public. It shows the raw SQLVARs information.
Each SQLVAR represents a field in the XSQLDA, the main structure used in the FB API
to talk to clients, transferring data into and out of the server. This option
is not accounted for when you type
SET;
in isql to see the state for most options.



Isql enhancements in Firebird v3.
---------------------------------

9) SET KEEP_TRAN_PARAMS option.

Author: Vladyslav Khorsun <hvlad at users sourceforge net>

When set to ON, isql keeps text of following successful SET TRANSACTION statement and
new DML transactions is started using the same SQL text (instead of defaul CONCURRENCY
WAIT mode).
When set to OFF, isql start new DML transaction as usual.
Name KEEP_TRAN_PARAMS could be cut down to the KEEP_TRAN.

In Firebird 3 KEEP_TRAN_PARAMS value is OFF by default, preserving backward compatibility
with old behaviour.
In Firebird 4 KEEP_TRAN_PARAMS is ON by default to make isql behaviour more logical.


Example:

-- check current value
SQL> SET;
...
Keep transaction params: OFF

-- toggle value
SQL> SET KEEP_TRAN;
SQL> SET;
...
Keep transaction params: ON
SET TRANSACTION


SQL>commit;

-- start new transaction, check KEEP_TRAN value and actual transaction's
-- parameters
SQL>SET TRANSACTION READ COMMITTED WAIT;
SQL>SET;
...
Keep transaction params: ON
  SET TRANSACTION READ COMMITTED WAIT
SQL> SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') FROM RDB$DATABASE;

RDB$GET_CONTEXT

=============================================================
READ COMMITTED

SQL> commit;

-- start new transaction, ensure is have parameters as KEEP_TRAN value
SQL> SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') FROM RDB$DATABASE;

RDB$GET_CONTEXT

=============================================================
READ COMMITTED

-- disable KEEP_TRAN, current transaction is not changed
SQL> SET KEEP_TRAN OFF;
SQL> SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') FROM RDB$DATABASE;

RDB$GET_CONTEXT

=============================================================
READ COMMITTED

SQL> commit;

-- start new transaction, ensure is have default parameters (SNAPSHOT)
SQL> SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') FROM RDB$DATABASE;

RDB$GET_CONTEXT

=============================================================
SNAPSHOT

SQL> SET;
...
Keep transaction params: OFF
SQL>



Isql enhancements in Firebird v4.0.1.
---------------------------------

10) SET EXEC_PATH_DISPLAY BLR/OFF

Retrieves the execution path of a DML statement formatted as BLR text.

It requires server v4.0.1 or greater to work.

Warning: this feature is very tied to engine internals and its usage is discouraged
if you do not understand very well how these internals are subject to change between
versions.



Isql enhancements in Firebird v5.
---------------------------------

11) SET PER_TABLE_STATS option.

Author: Vladyslav Khorsun <hvlad at users sourceforge net>

When set to ON show per-table run-time statistics after query execution.
It is set to OFF by default. Also, it is independent of SET STATS option.
The name PER_TABLE_STATS could be shortened up to PER_TAB. Tables in output
are sorted by its relation id's.

Example:

-- check current value
SQL> SET;
...
Print per-table stats:   OFF
...

-- turn per-table stats on
SQL> SET PER_TABLE_STATS ON;
SQL>
SQL> SELECT COUNT(*) FROM RDB$RELATIONS JOIN RDB$RELATION_FIELDS USING (RDB$RELATION_NAME);

                COUNT
=====================
                  534

Per table statistics:
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
 Table name                     | Natural | Index   | Insert  | Update  | Delete  | Backout | Purge   | Expunge |
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
RDB$INDICES                     |         |        3|         |         |         |         |         |         |
RDB$RELATION_FIELDS             |         |      534|         |         |         |         |         |         |
RDB$RELATIONS                   |       59|         |         |         |         |         |         |         |
RDB$SECURITY_CLASSES            |         |        3|         |         |         |         |         |         |
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+

Note, here are present some system tables that was not listed in query - it is
because engine reads some metadata when preparing the query.

-- turn common stats on
SQL> SET STATS ON;
SQL> SELECT COUNT(*) FROM RDB$RELATIONS JOIN RDB$RELATION_FIELDS USING (RDB$RELATION_NAME);

                COUNT
=====================
                  534

Current memory = 3828960
Delta memory = 208
Max memory = 3858576
Elapsed time = 0.001 sec
Buffers = 256
Reads = 0
Writes = 0
Fetches = 715
Per table statistics:
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
 Table name                     | Natural | Index   | Insert  | Update  | Delete  | Backout | Purge   | Expunge |
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
RDB$RELATION_FIELDS             |         |      534|         |         |         |         |         |         |
RDB$RELATIONS                   |       59|         |         |         |         |         |         |         |
--------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+

-- turn per-table stats off, using shortened name
SQL> SET PER_TAB OFF;



12) SET WIRE_STATS option.

Author: Vladyslav Khorsun <hvlad at users sourceforge net>

  When set to ON shows wire (network) statistics after query execution.
It is set to OFF by default. The name WIRE_STATS could be shortened up to WIRE.

The statistics counters shown in two groups: 'logical' and 'physical':
  - logical counters show numbers of packets in terms of Firebird wire protocol
	and number of bytes send before compression and received after decompression;
  - physical counters show number of physical packets and bytes send and 
	received over the wire, number of bytes could be affected by wire compression, 
	if present. Also, number of network roundtrips is shown: it is number of
	changes of IO direction from 'send' to 'receive'.

  Note, wire statistics is gathered by Remote provider only, i.e. it is always
zero for embedded connections. Also, it is collected by client and IO direction
(send, receive) is shown from client point of view.

Examples:

1. INET protocol with wire compression.
Set WireCompression = true in firebird.conf

>isql inet://employee

SQL> SET;
Print statistics:        OFF
Print per-table stats:   OFF
Print wire stats:        OFF
...

SQL> SET WIRE;
SQL>
SQL> SELECT COUNT(*) FROM RDB$RELATIONS;

                COUNT
=====================
                   67

Wire logical statistics:
  send packets =        6
  recv packets =        5
  send bytes   =      184
  recv bytes   =      224
Wire physical statistics:
  send packets =        3
  recv packets =        2
  send bytes   =      123
  recv bytes   =       88
  roundtrips   =        2

  Note difference due to wire compression in send/recv bytes for logical and
physical stats.


2. XNET protocol (wire compression is not used).

>isql xnet://employee

SQL> SET WIRE;
SQL>
SQL> SELECT COUNT(*) FROM RDB$RELATIONS;

                COUNT
=====================
                   67

Wire logical statistics:
  send packets =        5
  recv packets =        6
  send bytes   =      176
  recv bytes   =      256
Wire physical statistics:
  send packets =        5
  recv packets =        5
  send bytes   =      176
  recv bytes   =      256
  roundtrips   =        5

  Note, send/recv bytes for logical and physical stats are equal.


3. Embedded connection (wire statistics is absent).

SQL> SET WIRE;
SQL>
SQL> select count(*) from rdb$relations;

                COUNT
=====================
                   67

Wire logical statistics:
  send packets =        0
  recv packets =        0
  send bytes   =        0
  recv bytes   =        0
Wire physical statistics:
  send packets =        0
  recv packets =        0
  send bytes   =        0
  recv bytes   =        0
  roundtrips   =        0



13) SHOW WIRE_STATISTICS command.

Author: Vladyslav Khorsun <hvlad at users sourceforge net>

  New ISQL command that shows accumulated wire statistics. There is also
shortened alias WIRE_STATS.

  The command show values of wire statistics counters, accumulated since the
connection start time. Format is the same as of SET STATS above.



Isql enhancements in Firebird v6.
---------------------------------

14) EXPLAIN statement.

Author: Adriano dos Santos Fernandes

A new ISQL statement was created to easily show a query plan without execute it.

Note: If SET STATS is ON, stats are still shown.

Examples:

SQL> explain select * from employees where id = ?;

SQL> set term !;
SQL>
SQL> explain
CON> execute block
CON> as
CON>     declare id integer;
CON> begin
CON>     select id from employees where id = ? into id;
CON> end!
SQL>
SQL> set term ;!


15) SET AUTOTERM ON/OFF

Author: Adriano dos Santos Fernandes

When set to ON, terminator defined with SET TERM is changed to semicolon and a new logic
for TERM detection is used, where engine helps ISQL to detect valid usage of semicolons
inside statements.

At each semicolon (outside quotes or comments), ISQL prepares the query buffer with
engine using flag IStatement::PREPARE_REQUIRE_SEMICOLON.

If engine prepares the statement correctly, it's run and ISQL is put in new statement
mode.

If engine returns error isc_command_end_err2, then ISQL is put in statement
continuation mode and asks for another line, repeating the process.

If engine returns a different error, the error is shown and ISQL is put in new statement
mode.

Notes:
- This option can also be activated with command line parameter -autot(erm)
- It can only be used with Firebird engine/server v6 or later
- SET TERM command automatically sets AUTOTERM to OFF
- SET AUTOTERM ON command automatically sets TERM to semicolon
- While AUTOTERM ON can be used in non-interactive scripts, at each semicolon,
  statement may be tried to be compiled using the server/engine.
  That may be slow for big scripts with PSQL statements spanning many lines.

Examples:

SQL> SET AUTOTERM ON;

SQL> execute block returns (o1 integer)
CON> as
CON> begin
CON>     o1 = 1;
CON>     suspend;
CON> end;

          O1
============
           1

SQL> select 1 from rdb$database;

    CONSTANT
============
           1

SQL> select 1
CON>     from rdb$database;

    CONSTANT
============
           1

SQL> select 1
CON>     from rdb$database
CON>     where true;

    CONSTANT
============
           1
