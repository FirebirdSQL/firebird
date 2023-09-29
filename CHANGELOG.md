# v5.0 Release Candidate 1

## New features

* [#7682](https://github.com/FirebirdSQL/firebird/issues/7682): Use _ParallelWorkers_ setting from firebird.conf as default for all parallelised operations  
  Contributor(s): Vlad Khorsun

* [#7469](https://github.com/FirebirdSQL/firebird/pull/7469): Make Android port (client / embedded) work inside apps  
  Contributor(s): Adriano dos Santos Fernandes

* [#5959](https://github.com/FirebirdSQL/firebird/issues/5959): Add support for `QUARTER` to `EXTRACT`, `FIRST_DAY` and `LAST_DAY`  
  Contributor(s): Adriano dos Santos Fernandes

## Improvements

* [#7720](https://github.com/FirebirdSQL/firebird/pull/7720): MacOS: build _libicu_ and static _libc++_ using _vcpkg_  
  Contributor(s): Adriano dos Santos Fernandes

* [#7707](https://github.com/FirebirdSQL/firebird/pull/7707): Better processing and optimization if `IN <list>` predicates  
  Contributor(s): Dmitry Yemanov

* [#7692](https://github.com/FirebirdSQL/firebird/issues/7692): Make trace config parser resolve symlinks in database file path in trace configuration  
  Contributor(s): Vlad Khorsun

* [#7688](https://github.com/FirebirdSQL/firebird/issues/7688): Profiler should not miss query's top-level access paths nodes  
  Contributor(s): Adriano dos Santos Fernandes

* [#7687](https://github.com/FirebirdSQL/firebird/issues/7687): Add `LEVEL` column to `PLG$PROF_RECORD_SOURCES` and `PLG$PROF_RECORD_SOURCE_STATS_VIEW`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7685](https://github.com/FirebirdSQL/firebird/issues/7685): Add overload `FbVarChar::set` function for non null-terminated string  
  Contributor(s): Adriano dos Santos Fernandes

* [#7680](https://github.com/FirebirdSQL/firebird/pull/7680): Make boot build on Windows a bit more user-friendly  
  Contributor(s): Vlad Khorsun

* [#7652](https://github.com/FirebirdSQL/firebird/issues/7652): Make the profiler store aggregated requests by default, with option for detailed store  
  Contributor(s): Adriano dos Santos Fernandes

* [#7642](https://github.com/FirebirdSQL/firebird/issues/7642): Getting the current `DECFLOAT ROUND/TRAPS` settings  
  Contributor(s): Alexander Peshkov

* [#7637](https://github.com/FirebirdSQL/firebird/issues/7637): Run as application not specifying switch -a  
  Contributor(s): Vlad Khorsun

* [#7634](https://github.com/FirebirdSQL/firebird/issues/7634): Include Performance Cores only in default affinity mask  
  Contributor(s): Vlad Khorsun

* [#7576](https://github.com/FirebirdSQL/firebird/issues/7576): Allow nested parenthesized joined table  
  Contributor(s): Mark Rotteveel

* [#7559](https://github.com/FirebirdSQL/firebird/pull/7559): Optimize creation of expression and partial indices  
  Contributor(s): Dmitry Yemanov

* [#7550](https://github.com/FirebirdSQL/firebird/issues/7550): Add support for _-parallel_ in combination with _gfix -icu_  
  Contributor(s): Vlad Khorsun

* [#7542](https://github.com/FirebirdSQL/firebird/issues/7542): Compiler warnings raise when build cloop generated Firebird.pas in RAD Studio 11.3  
  Contributor(s): Vlad Khorsun

* [#7539](https://github.com/FirebirdSQL/firebird/issues/7539): `RDB$GET/SET_CONTEXT()`: enclosing in apostrophes or double quotes  of a missed namespace/variable will make output more readable  
  Contributor(s): Vlad Khorsun

* [#7536](https://github.com/FirebirdSQL/firebird/issues/7536): Add ability to query current value of parallel workers for an attachment  
  Contributor(s): Vlad Khorsun

* [#7506](https://github.com/FirebirdSQL/firebird/pull/7506): Reduce output of the `SHOW GRANTS` command  
  Contributor(s): Artyom Ivanov

* [#7494](https://github.com/FirebirdSQL/firebird/issues/7494): Firebird performance issue - unnecessary index reads  
  Contributor(s): Vlad Khorsun

* [#7475](https://github.com/FirebirdSQL/firebird/issues/7475): `SHOW SYSTEM` command: provide list of functions belonging to system packages  
  Contributor(s): Alexander Peshkov

* [#7466](https://github.com/FirebirdSQL/firebird/pull/7466): Add _COMPILE_ trace events for procedures/functions/triggers  
  Contributor(s): Dmitry Yemanov

* [#7425](https://github.com/FirebirdSQL/firebird/issues/7425): Add _REPLICA MODE_ to the output of the _isql_ `SHOW DATABASE` command  
  Contributor(s): Dmitry Yemanov

* [#7405](https://github.com/FirebirdSQL/firebird/pull/7405): Surface internal optimization modes (all rows vs first rows) at the SQL and configuration levels  
  Contributor(s): Dmitry Yemanov

* [#7213](https://github.com/FirebirdSQL/firebird/pull/7213): Use Windows private namespace for kernel objects used in server-to-server IPC  
  Contributor(s): Vlad Khorsun

* [#7046](https://github.com/FirebirdSQL/firebird/issues/7046): Make ability to add comment to mapping (`COMMENT ON MAPPING ... IS ...`)  
  Contributor(s): Alexander Peshkov

* [#7001](https://github.com/FirebirdSQL/firebird/issues/7001): _ISQL_ showing publication status  
  Contributor(s): Dmitry Yemanov

## Bugfixes

* [#7747](https://github.com/FirebirdSQL/firebird/pull/7747): Fix an issue where the garbage collection in indexes and blobs is not performed in _VIO_backout_  
  Contributor(s): Ilya Eremin

* [#7738](https://github.com/FirebirdSQL/firebird/issues/7738): Crash on multiple connections/disconnections  
  Contributor(s): Alexander Peshkov

* [#7737](https://github.com/FirebirdSQL/firebird/pull/7737): Fix cases where the precedence relationship between a record page and a blob page is not set  
  Contributor(s): Ilya Eremin

* [#7731](https://github.com/FirebirdSQL/firebird/issues/7731): Display length of timestamp with timezone is wrong in dialect 1  
  Contributor(s): Alexander Peshkov

* [#7730](https://github.com/FirebirdSQL/firebird/issues/7730): Server ignores the size of VARCHAR when performing `SET BIND ... TO VARCHAR(N)`  
  Contributor(s): Alexander Peshkov

* [#7729](https://github.com/FirebirdSQL/firebird/issues/7729): `SET BIND OF TS WITH TZ TO VARCHAR(128)` uses the date format of dialect 1  
  Contributor(s): Alexander Peshkov

* [#7727](https://github.com/FirebirdSQL/firebird/issues/7727): Index for integer column cannot be used when `INT128/DECFLOAT` value is being searched  
  Contributor(s): Dmitry Yemanov

* [#7723](https://github.com/FirebirdSQL/firebird/issues/7723): Wrong error message on login if the user doesn't exists and WireCrypt is disabled  
  Contributor(s): Alexander Peshkov

* [#7713](https://github.com/FirebirdSQL/firebird/issues/7713): `FOR SELECT` statement can not see any changes made in `DO` block  
  Contributor(s): Vlad Khorsun

* [#7710](https://github.com/FirebirdSQL/firebird/issues/7710): Expression index - more than one null value cause attempt to store duplicate value error - FB5.0 beta 2  
  Contributor(s): Vlad Khorsun

* [#7703](https://github.com/FirebirdSQL/firebird/issues/7703): Requests leak in _AutoCacheRequest_  
  Contributor(s): Alexander Peshkov

* [#7696](https://github.com/FirebirdSQL/firebird/issues/7696): `select from external procedure` validates output parameters even when fetch method returns false  
  Contributor(s): Adriano dos Santos Fernandes

* [#7694](https://github.com/FirebirdSQL/firebird/pull/7694): Fix false positives of "missing entries for record X" error during index validation when a deleted record version is committed and has a backversion  
  Contributor(s): Ilya Eremin

* [#7691](https://github.com/FirebirdSQL/firebird/issues/7691): `with caller privileges` has no effect in triggers   
  Contributor(s): Alexander Peshkov

* [#7683](https://github.com/FirebirdSQL/firebird/issues/7683): `rdb$time_zone_util.transitions` returns an infinite resultset  
  Contributor(s): Adriano dos Santos Fernandes

* [#7676](https://github.com/FirebirdSQL/firebird/issues/7676): "Attempt to evaluate index expression recursively"  
  Contributor(s): Dmitry Yemanov

* [#7670](https://github.com/FirebirdSQL/firebird/issues/7670): Cursor name can duplicate parameter and variable names in procedures and functions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7665](https://github.com/FirebirdSQL/firebird/issues/7665): Wrong result ordering in `LEFT JOIN` query  
  Contributor(s): Dmitry Yemanov

* [#7664](https://github.com/FirebirdSQL/firebird/issues/7664): `DROP TABLE` executed for a table with big records may lead to "wrong page type" or "end of file" error  
  Contributor(s): Vlad Khorsun, Ilya Eremin

* [#7662](https://github.com/FirebirdSQL/firebird/pull/7662): Fix performance issues in _prepare_update()_  
  Contributor(s): Ilya Eremin

* [#7661](https://github.com/FirebirdSQL/firebird/issues/7661): Classic Server rejects new connections  
  Contributor(s): Vlad Khorsun

* [#7658](https://github.com/FirebirdSQL/firebird/issues/7658): Segfault when closing database in valgrind-enabled build  
  Contributor(s): Alexander Peshkov

* [#7649](https://github.com/FirebirdSQL/firebird/issues/7649): Switch Linux performance counter timer to CLOCK_MONOTONIC_RAW  
  Contributor(s): Adriano dos Santos Fernandes

* [#7641](https://github.com/FirebirdSQL/firebird/pull/7641): Fix wrong profiler measurements due to overflow.  
  Contributor(s): Adriano dos Santos Fernandes

* [#7638](https://github.com/FirebirdSQL/firebird/issues/7638): `OVERRIDING USER VALUE` should be allowed for `GENERATED ALWAYS AS IDENTITY`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7627](https://github.com/FirebirdSQL/firebird/issues/7627): The size of the database with big records becomes bigger after backup/restore  
  Contributor(s): Ilya Eremin

* [#7626](https://github.com/FirebirdSQL/firebird/issues/7626): Segfault when new attachment is done to shutting down database  
  Contributor(s): Alexander Peshkov

* [#7611](https://github.com/FirebirdSQL/firebird/issues/7611): Can't backup/restore database from v3 to v4 with `SEC$USER_NAME` field longer than 10 characters  
  Contributor(s): Adriano dos Santos Fernandes

* [#7610](https://github.com/FirebirdSQL/firebird/issues/7610): Uninitialized/random value assigned to `RDB$ROLES` -> `RDB$SYSTEM PRIVILEGES` when restoring from FB3 backup  
  Contributor(s): Adriano dos Santos Fernandes

* [#7604](https://github.com/FirebirdSQL/firebird/issues/7604): PSQL functions do not convert the output BLOB to the connection character set.  
  Contributor(s): Adriano dos Santos Fernandes

* [#7603](https://github.com/FirebirdSQL/firebird/issues/7603): `BIN_SHR` on `INT128` does not apply sign extension  
  Contributor(s): Alexander Peshkov

* [#7599](https://github.com/FirebirdSQL/firebird/issues/7599): Conversion of text with '\0' to `DECFLOAT` without errors  
  Contributor(s): Alexander Peshkov

* [#7598](https://github.com/FirebirdSQL/firebird/issues/7598): DDL statements hang when the compiled statements cache is enabled  
  Contributor(s): Vlad Khorsun

* [#7582](https://github.com/FirebirdSQL/firebird/issues/7582): Missing _isc_info_end_ in _Firebird.pas_  
  Contributor(s): Alexander Peshkov

* [#7579](https://github.com/FirebirdSQL/firebird/issues/7579): Cannot _nbackup_ a firebird 3.0 database in firebird 4.0 service with _engine12_ setup in _Providers_  
  Contributor(s): Alexander Peshkov

* [#7574](https://github.com/FirebirdSQL/firebird/issues/7574): Derived table syntax allows dangling `AS`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7569](https://github.com/FirebirdSQL/firebird/issues/7569): Multi-level order by and offset/fetch ignored on parenthesized query expressions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7562](https://github.com/FirebirdSQL/firebird/issues/7562): Profiler elapsed times are incorrect in Windows  
  Contributor(s): Adriano dos Santos Fernandes

* [#7556](https://github.com/FirebirdSQL/firebird/issues/7556): FB Classic can hang when attempts to attach DB while it is starting to encrypt/decrypt  
  Contributor(s): Alexander Peshkov

* [#7555](https://github.com/FirebirdSQL/firebird/issues/7555): Invalid configuration for random fresh created database may be used after drop of another one with alias in databases.conf  
  Contributor(s): Alexander Peshkov

* [#7554](https://github.com/FirebirdSQL/firebird/issues/7554): Firebird 5 partial index creation causes server hang up  
  Contributor(s): Vlad Khorsun

* [#7553](https://github.com/FirebirdSQL/firebird/issues/7553): Firebird 5 profiler error with subselects  
  Contributor(s): Adriano dos Santos Fernandes

* [#7548](https://github.com/FirebirdSQL/firebird/issues/7548): `SET BIND OF TIMESTAMP WITH TIME ZONE TO CHAR` is not working with UTF8 connection charset  
  Contributor(s): Adriano dos Santos Fernandes

* [#7537](https://github.com/FirebirdSQL/firebird/issues/7537): Wrong name in error message when unknown namespace is passed into RDB$SET_CONTEXT()  
  Contributor(s): Vlad Khorsun

* [#7535](https://github.com/FirebirdSQL/firebird/issues/7535): High CPU usage connect to Firebird 3 database using Firebird 4 Classic and SuperClassic service  
  Contributor(s): Vlad Khorsun

* [#7514](https://github.com/FirebirdSQL/firebird/issues/7514): Segfault when detaching after deleting shadow on Classic  
  Contributor(s): Alexander Peshkov

* [#7504](https://github.com/FirebirdSQL/firebird/issues/7504): Segfault when closing SQL statement in remote provider during shutdown  
  Contributor(s): Alexander Peshkov

* [#7499](https://github.com/FirebirdSQL/firebird/issues/7499): Problem with restore  
  Contributor(s): Vlad Khorsun

* [#7488](https://github.com/FirebirdSQL/firebird/issues/7488): Invalid real to string cast   
  Contributor(s): Alexander Peshkov, Artyom Abakumov

* [#7486](https://github.com/FirebirdSQL/firebird/issues/7486): No initialization of rpb's runtime flags causes problems with `SKIP LOCKED` when config _ReadConsistency = 0_ and SuperServer  
  Contributor(s): Adriano dos Santos Fernandes

* [#7484](https://github.com/FirebirdSQL/firebird/issues/7484): External engine `SYSTEM` not found  
  Contributor(s): Adriano dos Santos Fernandes

* [#7480](https://github.com/FirebirdSQL/firebird/issues/7480): Firebird server stops accepting new connections after some time  
  Contributor(s): Alexander Peshkov

* [#7472](https://github.com/FirebirdSQL/firebird/issues/7472): Window functions may lead to crash interacting with others exceptions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7464](https://github.com/FirebirdSQL/firebird/issues/7464): Crash on repeating update in 5.0  
  Contributor(s): Adriano dos Santos Fernandes

* [#7456](https://github.com/FirebirdSQL/firebird/issues/7456): Impossible drop function in package with name of PSQL-function  
  Contributor(s): Adriano dos Santos Fernandes

* [#7445](https://github.com/FirebirdSQL/firebird/pull/7445): Fix problem with client-only build requiring _btyacc's_ generated files present  
  Contributor(s): Adriano dos Santos Fernandes

* [#7387](https://github.com/FirebirdSQL/firebird/issues/7387): Unreliable replication behaviour in Linux Classic  
  Contributor(s): Dmitry Yemanov

* [#7233](https://github.com/FirebirdSQL/firebird/pull/7233): Postfix for #5385 (CORE-5101): Fix slow database restore when Classic server mode is used  
  Contributor(s): Ilya Eremin


# v5.0 Beta 1 (27-Mar-2023)

## New features

* [#7447](https://github.com/FirebirdSQL/firebird/pull/7447): Parallel sweeping and index creation inside the engine  
  Reference(s): [/doc/README.parallel_features](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.parallel_features)  
  Contributor(s): Vlad Khorsun

* [#7397](https://github.com/FirebirdSQL/firebird/pull/7397): Inline minor ODS upgrade  
  Contributor(s): Dmitry Yemanov

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

* [#1783](https://github.com/FirebirdSQL/firebird/issues/1783): New GBAK switch to backup / restore tables/indexes in parallel  
  Reference(s): [/doc/README.gbak](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.gbak)  
  Contributor(s): Vlad Khorsun

## Improvements

* [#7441](https://github.com/FirebirdSQL/firebird/pull/7441): More cursor-related details in the plan output  
  Contributor(s): Dmitry Yemanov

* [#7437](https://github.com/FirebirdSQL/firebird/issues/7437): Update _zlib_ to 1.2.13  
  Contributor(s): Vlad Khorsun

* [#7411](https://github.com/FirebirdSQL/firebird/issues/7411): Unify display of system procedures & packages with other system objects  
  Contributor(s): Alexander Peshkov

* [#7399](https://github.com/FirebirdSQL/firebird/pull/7399): Simplify client library build  
  Contributor(s): Adriano dos Santos Fernandes

* [#7382](https://github.com/FirebirdSQL/firebird/issues/7382): Performance improvement for BLOB copying  
  Contributor(s): Adriano dos Santos Fernandes

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

* [#281](https://github.com/FirebirdSQL/firebird/pull/281): `RDB$BLOB_UTIL` system package  
  Contributor(s): Adriano dos Santos Fernandes

## Bugfixes

* [#7388](https://github.com/FirebirdSQL/firebird/issues/7388): Different invariants optimization between views and CTEs  
  Contributor(s): Dmitry Yemanov

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
