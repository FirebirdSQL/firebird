# v5.0 Beta 1 (planned)

## New features

* [#7350](https://github.com/FirebirdSQL/firebird/pull/7350): SKIP LOCKED clause for SELECT WITH LOCK, UPDATE and DELETE  
  Reference(s): [/doc/sql.extensions/README.skip_locked.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.skip_locked.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [#7216](https://github.com/FirebirdSQL/firebird/pull/7216): New built-in function `BLOB_APPEND`  
  Reference(s): [/doc/sql.extensions/README.blob_append.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.blob_append.md)  
  Contributor(s): Vlad Khorsun

* [#7144](https://github.com/FirebirdSQL/firebird/pull/7144): Compiled statement cache  
  Contributor(s): Adriano dos Santos Fernandes

* [#7086](https://github.com/FirebirdSQL/firebird/pull/7086): PSQL and SQL profiler  
  Reference(s): [/doc/sql.extensions/README.profiler.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.profiler.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [#7050](https://github.com/FirebirdSQL/firebird/pull/7050): Add table `MON$COMPILED_STATEMENTS` and column `MON$COMPILED_STATEMENT_ID` to both `MON$STATEMENTS` and `MON$CALL_STACK` tables  
  Reference(s): [/doc/README.monitoring_tables](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.monitoring_tables)  
  Contributor(s): Adriano dos Santos Fernandes

* [#6910](https://github.com/FirebirdSQL/firebird/issues/6910): Add way to retrieve statement BLR with `Statement::getInfo` and _ISQL's_ `SET EXEC_PATH_DISPLAY` BLR  
  Contributor(s): Adriano dos Santos Fernandes

* [#6798](https://github.com/FirebirdSQL/firebird/issues/6798): Add built-in functions `UNICODE_CHAR` and `UNICODE_VAL` to convert between Unicode code point and character  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [#6713](https://github.com/FirebirdSQL/firebird/issues/6713): System table `RDB$KEYWORDS` with keywords [CORE6482]  
  Contributor(s): Adriano dos Santos Fernandes

* [#6681](https://github.com/FirebirdSQL/firebird/issues/6681): Support for `WHEN NOT MATCHED BY SOURCE` for `MERGE` statement [CORE6448]  
  Reference(s): [/doc/sql.extensions/README.merge.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.merge.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [#3750](https://github.com/FirebirdSQL/firebird/issues/3750): Partial indices [CORE3384]  
  Reference(s): [/doc/sql.extensions/README.partial_indices](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.partial_indices)  
  Contributor(s): Dmitry Yemanov, Vlad Khorsun

## Improvements

* [#7331](https://github.com/FirebirdSQL/firebird/issues/7331): Cost-based choice between nested loop join and hash join  
  Contributor(s): Dmitry Yemanov

* [#7294](https://github.com/FirebirdSQL/firebird/issues/7294): Allow FB-known macros in replication.conf  
  Contributor(s): Dmitry Yemanov

* [#7293](https://github.com/FirebirdSQL/firebird/pull/7293): Create Android packages with all necessary files in all architectures (x86, x64, arm32, arm64)  
  Contributor(s): Adriano dos Santos Fernandes

* [#7284](https://github.com/FirebirdSQL/firebird/pull/7284): Change release filenames as the following examples  
  Contributor(s): Adriano dos Santos Fernandes

* [#7259](https://github.com/FirebirdSQL/firebird/issues/7259): Remove _TcpLoopbackFastPath_ and use of _SIO_LOOPBACK_FAST_PATH_  
  Contributor(s): Vlad Khorsun

* [#7208](https://github.com/FirebirdSQL/firebird/issues/7208): Trace: provide performance statistics for DDL statements  
  Contributor(s): Vlad Khorsun

* [#7194](https://github.com/FirebirdSQL/firebird/issues/7194): Make it possible to avoid fbclient dependency in pascal programs using firebird.pas  
  Contributor(s): Alexander Peshkov

* [#7186](https://github.com/FirebirdSQL/firebird/issues/7186): _Nbackup_ `RDB$BACKUP_HISTORY` cleanup  
  Contributor(s): Vlad Khorsun

* [#7169](https://github.com/FirebirdSQL/firebird/issues/7169): Improve _ICU_ version mismatch diagnostics  
  Contributor(s): Adriano dos Santos Fernandes

* [#7168](https://github.com/FirebirdSQL/firebird/issues/7168): Ignore missing UDR libraries during restore  
  Contributor(s): Adriano dos Santos Fernandes

* [#7165](https://github.com/FirebirdSQL/firebird/issues/7165): Provide ability to see in the trace log events related to missing security context  
  Contributor(s): Vlad Khorsun

* [#7161](https://github.com/FirebirdSQL/firebird/issues/7161): Update _zlib_ to 1.2.12  
  Contributor(s): Vlad Khorsun

* [#7093](https://github.com/FirebirdSQL/firebird/issues/7093): Improve indexed lookup speed of strings when the last keys characters are part of collated contractions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7092](https://github.com/FirebirdSQL/firebird/issues/7092): Improve performance of `CURRENT_TIME`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7083](https://github.com/FirebirdSQL/firebird/issues/7083): `ResultSet::getInfo()` implementation  
  Contributor(s): Dmitry Yemanov

* [#7065](https://github.com/FirebirdSQL/firebird/issues/7065): Connection hangs after delivery of 256GB of data  
  Contributor(s): Alexander Peshkov

* [#7051](https://github.com/FirebirdSQL/firebird/issues/7051): Network support for bi-directional cursors  
  Contributor(s): Dmitry Yemanov

* [#7046](https://github.com/FirebirdSQL/firebird/issues/7046): Make ability to add comment to mapping `('COMMENT ON MAPPING ... IS ...')`  
  Contributor(s): Alexander Peshkov

* [#7042](https://github.com/FirebirdSQL/firebird/issues/7042): `ON DISCONNECT` triggers are not executed during forced attachment shutdown  
  Contributor(s): Ilya Eremin

* [#7041](https://github.com/FirebirdSQL/firebird/issues/7041): Firebird port for _Apple M1_  
  Contributor(s): Adriano dos Santos Fernandes

* [#7038](https://github.com/FirebirdSQL/firebird/issues/7038): Improve performance of `STARTING WITH` with insensitive collations  
  Contributor(s): Adriano dos Santos Fernandes

* [#7025](https://github.com/FirebirdSQL/firebird/issues/7025): Results of negation must be the same for each datatype (smallint / int / bigint / int128) when argument is least possible value for this type  
  Contributor(s): Alexander Peshkov

* [#6992](https://github.com/FirebirdSQL/firebird/issues/6992): Transform `OUTER` joins into `INNER` ones if the `WHERE` condition violates the outer join rules  
  Contributor(s): Dmitry Yemanov

* [#6959](https://github.com/FirebirdSQL/firebird/issues/6959): `IBatch::getInfo()` implementation  
  Contributor(s): Alexander Peshkov

* [#6957](https://github.com/FirebirdSQL/firebird/issues/6957): Add database creation time to the output of _ISQL's_ command `SHOW DATABASE`  
  Contributor(s): Vlad Khorsun

* [#6954](https://github.com/FirebirdSQL/firebird/issues/6954): _fb_info_protocol_version_ support  
  Contributor(s): Alexander Peshkov

* [#6929](https://github.com/FirebirdSQL/firebird/issues/6929): Add support of _PKCS v.1.5_ padding to RSA functions, needed for backward compatibility with old systems.  
  Contributor(s): Alexander Peshkov

* [#6915](https://github.com/FirebirdSQL/firebird/issues/6915): Allow attribute DISABLE-COMPRESSIONS in UNICODE collations  
  Contributor(s): Adriano dos Santos Fernandes

* [#6903](https://github.com/FirebirdSQL/firebird/issues/6903): Unable to create ICU-based collation with locale keywords  
  Contributor(s): tkeinz, Adriano dos Santos Fernandes

* [#6874](https://github.com/FirebirdSQL/firebird/issues/6874): Literal 65536 (interpreted as int) can not be multiplied by itself w/o cast if result more than 2^63-1  
  Contributor(s): Alexander Peshkov

* [#6873](https://github.com/FirebirdSQL/firebird/issues/6873): `SIMILAR TO` should use index when pattern starts with non-wildcard character (as `LIKE` does)  
  Contributor(s): Adriano dos Santos Fernandes

* [#6872](https://github.com/FirebirdSQL/firebird/issues/6872): Faster execution of indexed `STARTING WITH` with _UNICODE_ collation  
  Contributor(s): Adriano dos Santos Fernandes

* [#6815](https://github.com/FirebirdSQL/firebird/issues/6815): Support multiple rows for DML `RETURNING`  
  Contributor(s): Adriano dos Santos Fernandes

* [#6810](https://github.com/FirebirdSQL/firebird/issues/6810): Use precise limit of salt length when signing messages and verifying the sign  
  Contributor(s): Alexander Peshkov

* [#6809](https://github.com/FirebirdSQL/firebird/issues/6809): Integer hex-literal support for INT128  
  Contributor(s): Alexander Peshkov

* [#6794](https://github.com/FirebirdSQL/firebird/pull/6794): Improvement: add `MON$SESSION_TIMEZONE` to `MON$ATTACHMENTS`  
  Contributor(s): Adriano dos Santos Fernandes

* [#6740](https://github.com/FirebirdSQL/firebird/issues/6740): Allow parenthesized query expression for standard-compliance [CORE6511]  
  Contributor(s): Adriano dos Santos Fernandes

* [#6730](https://github.com/FirebirdSQL/firebird/issues/6730): Trace: provide ability to see _STATEMENT RESTART_ events (or their count) [CORE6500]  
  Contributor(s): Vlad Khorsun

* [#6571](https://github.com/FirebirdSQL/firebird/issues/6571): Improve memory consumption of statements and requests [CORE6330]  
  Contributor(s): Adriano dos Santos Fernandes

* [#5589](https://github.com/FirebirdSQL/firebird/issues/5589): Support full SQL standard character string literal syntax [CORE5312]  
  Contributor(s): Adriano dos Santos Fernandes

* [#5588](https://github.com/FirebirdSQL/firebird/issues/5588): Support full SQL standard binary string literal syntax [CORE5311]  
  Contributor(s): Adriano dos Santos Fernandes

* [#4769](https://github.com/FirebirdSQL/firebird/issues/4769): Allow sub-routines to access variables/parameters defined at the outer/parent level [CORE4449]  
  Contributor(s): Adriano dos Santos Fernandes

* [#4723](https://github.com/FirebirdSQL/firebird/issues/4723): Optimize the record-level _RLE_ algorithm for a denser compression of shorter-than-declared strings and sets of subsequent _NULLs_ [CORE4401]  
  Contributor(s): Dmitry Yemanov

* [#1708](https://github.com/FirebirdSQL/firebird/issues/1708): Avoid data retrieval if the `WHERE` clause always evaluates to `FALSE` [CORE1287]  
  Contributor(s): Dmitry Yemanov

## Bugfixes

* [#7314](https://github.com/FirebirdSQL/firebird/issues/7314): Multitreaded activating indices restarts server process  
  Contributor(s): Vlad Khorsun

* [#7304](https://github.com/FirebirdSQL/firebird/issues/7304): Events in system attachments (like garbage collector) are not traced  
  Contributor(s): Alexander Peshkov

* [#7298](https://github.com/FirebirdSQL/firebird/issues/7298): Info result parsing  
  Contributor(s): Alexander Peshkov

* [#7296](https://github.com/FirebirdSQL/firebird/issues/7296): During shutdown _op_disconnect_ may be sent to invalid handle  
  Contributor(s): Alexander Peshkov

* [#7295](https://github.com/FirebirdSQL/firebird/issues/7295): Unexpected message 'Error reading data from the connection' when fbtracemgr is closed using _Ctrl-C_  
  Contributor(s): Alexander Peshkov

* [#7283](https://github.com/FirebirdSQL/firebird/issues/7283): Suspicious error message during install  
  Contributor(s): Alexander Peshkov

* [#7262](https://github.com/FirebirdSQL/firebird/issues/7262): Repeated _op_batch_create_ leaks the batch  
  Contributor(s): Alexander Peshkov

* [#7045](https://github.com/FirebirdSQL/firebird/issues/7045): International characters in table or alias names causes queries of `MON$STATEMENTS` to fail  
  Contributor(s): Adriano dos Santos Fernandes

* [#6968](https://github.com/FirebirdSQL/firebird/issues/6968): On Windows, engine may hung when works with corrupted database and read after the end of file  
  Contributor(s): Vlad Khorsun

* [#6854](https://github.com/FirebirdSQL/firebird/issues/6854): Crash occurs when use `SIMILAR TO`  
  Contributor(s): Adriano dos Santos Fernandes

* [#6845](https://github.com/FirebirdSQL/firebird/issues/6845): Result type of `AVG` over `BIGINT` column results in type `INT128`  
  Contributor(s): Alexander Peshkov

* [#6838](https://github.com/FirebirdSQL/firebird/issues/6838): Deleting multiple rows from a view with triggers may cause triggers to fire just once  
  Contributor(s): Dmitry Yemanov

* [#6836](https://github.com/FirebirdSQL/firebird/issues/6836): `fb_shutdown()` does not wait for self completion in other thread  
  Contributor(s): Alexander Peshkov

* [#6832](https://github.com/FirebirdSQL/firebird/issues/6832): Segfault using "commit retaining" with GTT  
  Contributor(s): Alexander Peshkov

* [#6825](https://github.com/FirebirdSQL/firebird/pull/6825): Correct error message for `DROP VIEW`  
  Contributor(s): Ilya Eremin

* [#6817](https://github.com/FirebirdSQL/firebird/issues/6817): _-fetch_password passwordfile_ does not work with gfix  
  Contributor(s): Alexander Peshkov

* [#6807](https://github.com/FirebirdSQL/firebird/issues/6807): Regression in FB 4.x : "Unexpected end of command" with incorrect line/column info  
  Contributor(s): Adriano dos Santos Fernandes

* [#6801](https://github.com/FirebirdSQL/firebird/issues/6801): Error recompiling a package with some combination of nested functions  
  Contributor(s): Adriano dos Santos Fernandes

* [#5749](https://github.com/FirebirdSQL/firebird/issues/5749): Token unknown error on formfeed in query [CORE5479]  
  Contributor(s): Adriano dos Santos Fernandes

* [#5534](https://github.com/FirebirdSQL/firebird/issues/5534): String truncation exception on `UPPER/LOWER` functions, UTF8 database and some multibyte characters [CORE5255]  
  Contributor(s): Adriano dos Santos Fernandes

* [#5173](https://github.com/FirebirdSQL/firebird/issues/5173): Compound `ALTER TABLE` statement with `ADD` and `DROP` the same constraint failed if this constraint involves index creation (PK/UNQ/FK) [CORE4878]  
  Contributor(s): Ilya Eremin

* [#5082](https://github.com/FirebirdSQL/firebird/issues/5082): Exception "too few key columns found for index" raises when attempt to create table with PK and immediatelly drop this PK within the same transaction [CORE4783]  
  Contributor(s): Ilya Eremin

* [#4893](https://github.com/FirebirdSQL/firebird/issues/4893): Syntax error when `UNION` subquery ("query primary") in parentheses [CORE4577]  
  Contributor(s): Adriano dos Santos Fernandes

* [#4085](https://github.com/FirebirdSQL/firebird/issues/4085): `RDB$INDICES` information stored inconsistently after a `CREATE INDEX` [CORE3741]  
  Contributor(s): Dmitry Yemanov

* [#3886](https://github.com/FirebirdSQL/firebird/issues/3886): `RECREATE TABLE T` with PK or UK is impossible after duplicate typing w/o commit when _ISQL_ is launched in _AUTODDL=OFF_ mode [CORE3529]  
  Contributor(s): Ilya Eremin

* [#3812](https://github.com/FirebirdSQL/firebird/issues/3812): Query with SP doesn't accept explicit plan [CORE3451]  
  Contributor(s): Dmitry Yemanov

* [#3357](https://github.com/FirebirdSQL/firebird/issues/3357): Bad execution plan if some stream depends on multiple streams via a function [CORE2975]  
  Contributor(s): Dmitry Yemanov

* [#1210](https://github.com/FirebirdSQL/firebird/issues/1210): Server hangs on I/O error during "open" operation for file "/tmp/firebird/fb_trace_ksVDoc" [CORE2917]  
  Contributor(s): Alexander Peshkov

* [#3218](https://github.com/FirebirdSQL/firebird/issues/3218): Optimizer fails applying stream-local predicates before merging [CORE2832]  
  Contributor(s): Dmitry Yemanov

## Cleanup

* [#7082](https://github.com/FirebirdSQL/firebird/issues/7082): Remove the WNET protocol  
  Contributor(s): Dmitry Yemanov

* [#6840](https://github.com/FirebirdSQL/firebird/issues/6840): Remove QLI  
  Contributor(s): Adriano dos Santos Fernandes
