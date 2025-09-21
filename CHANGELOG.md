# v5.0.3

## Improvements

* [#8649](https://github.com/FirebirdSQL/firebird/issues/8649): AV when `ON CONNECT` triggers use `EXECUTE STATEMENT ON EXTERNAL`  
  Contributor(s): Vlad Khorsun

* [#8598](https://github.com/FirebirdSQL/firebird/issues/8598): Don't fire referential integrity triggers if primary or unique keys haven't changed  
  Contributor(s): Vlad Khorsun

* [#8523](https://github.com/FirebirdSQL/firebird/issues/8523): Avoid internal exception in _fbclient_ during first `isc_attach_database`  
  Contributor(s): Vlad Khorsun

* [#8522](https://github.com/FirebirdSQL/firebird/issues/8522): Avoid internal exception in _fbclient_ during `isc_detach_database`  
  Contributor(s): Vlad Khorsun

* [#8513](https://github.com/FirebirdSQL/firebird/pull/8513): Makes `MON$COMPILED_STATEMENTS` and `MON$STATEMENTS` share blobs with text and plan content of the same statement  
  Contributor(s): Vlad Khorsun

* [#8447](https://github.com/FirebirdSQL/firebird/pull/8447): Avoid index scan for lower/upper bounds containing NULL keys  
  Contributor(s): Dmitry Yemanov

* [#8433](https://github.com/FirebirdSQL/firebird/pull/8433): Improve code of class `BePlusTree`   
  Contributor(s): Vlad Khorsun

* [#8421](https://github.com/FirebirdSQL/firebird/pull/8421): Add pointers tree to `TempSpace` class   
  Contributor(s): Andrey Kravchenko, Vlad Khorsun

* [#8318](https://github.com/FirebirdSQL/firebird/pull/8318): Send small blobs inline  
  Contributor(s): Vlad Khorsun

* [#8278](https://github.com/FirebirdSQL/firebird/issues/8278): Avoid index lookup for a NULL key if the condition is known to always be `FALSE` in this case  
  Contributor(s): Dmitry Yemanov

* [#6413](https://github.com/FirebirdSQL/firebird/issues/6413): Data pages of newly _gbak_ restored databases should marked as "swept"  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#8628](https://github.com/FirebirdSQL/firebird/issues/8592): Incorrect join order for `JOIN LATERAL` with `UNION` referencing the outer stream(s) via its select list  
  Contributor(s): Dmitry Yemanov

* [#8592](https://github.com/FirebirdSQL/firebird/issues/8592): Presence of 'ROWS <n_limit>' causes garbage in error message when string conversion problem raises  
  Contributor(s): Adriano dos Santos Fernandes

* [#8590](https://github.com/FirebirdSQL/firebird/issues/8590): Line "BLR to Source mapping:" may look broken when `procedures.rdb$debug_info` is queried using remote protocol  
  Contributor(s): Vlad Khorsun

* [#8589](https://github.com/FirebirdSQL/firebird/issues/8589): `PERCENT_RANK` may return _NaN_ instead of 0  
  Contributor(s): Adriano dos Santos Fernandes

* [#8588](https://github.com/FirebirdSQL/firebird/issues/8588): Error doing a backup when database file name has extended ASCII chars  
  Contributor(s): Alexander Peshkov

* [#8554](https://github.com/FirebirdSQL/firebird/issues/8554): Vulnerability GHSA-7qp6-hqxj-pjjp / ZDI-CAN-26486  
  Contributor(s): Alexander Peshkov

* [#8524](https://github.com/FirebirdSQL/firebird/issues/8524): _ISQL_ truncates lines longer than 255 characters when pasting  
  Contributor(s): Vlad Khorsun

* [#8520](https://github.com/FirebirdSQL/firebird/issues/8520): Error in `iTransaction.getInfo()` on embedded connection  
  Contributor(s): Alexander Peshkov

* [#8509](https://github.com/FirebirdSQL/firebird/issues/8509): "Error creating private namespace" message in _firebird.log_  
  Contributor(s): Vlad Khorsun

* [#8508](https://github.com/FirebirdSQL/firebird/issues/8508): Conversion error with `old.field` inside `UPDATE OR INSERT`  
  Contributor(s): Adriano dos Santos Fernandes

* [#8488](https://github.com/FirebirdSQL/firebird/issues/8488): `MIN/MAX` aggregates may badly affect the join order in queries with mixed INNER/LEFT joins  
  Contributor(s): Dmitry Yemanov

* [#8487](https://github.com/FirebirdSQL/firebird/issues/8487): Unexpected _SetFilePointer_ error when remapping the shared memory  
  Contributor(s): Vlad Khorsun

* [#8485](https://github.com/FirebirdSQL/firebird/issues/8485): Segfault/AV on incorrect _databases.conf_ starting with subconfig (without line `alias=database_path`)  
  Contributor(s): Alexander Peshkov

* [#8477](https://github.com/FirebirdSQL/firebird/issues/8477): Inheritance of `WINDOW` does not work  
  Contributor(s): Adriano dos Santos Fernandes

* [#8449](https://github.com/FirebirdSQL/firebird/issues/8449): Races when server is closed during forced database shutdown  
  Contributor(s): Alexander Peshkov

* [#8440](https://github.com/FirebirdSQL/firebird/issues/8440): Firebird 5.0.2 - wrong result for `MINVALUE/MAXVALUE` with string arguments  
  Contributor(s): Dmitry Yemanov

* [#8437](https://github.com/FirebirdSQL/firebird/issues/8437): Segmentation fault when running query with `PARTITION BY` and subquery  
  Contributor(s): Adriano dos Santos Fernandes

* [#8432](https://github.com/FirebirdSQL/firebird/pull/8432): Fix server hang when trying to stop a server with active connections with event notifications  
  Contributor(s): TreeHunter

* [#8431](https://github.com/FirebirdSQL/firebird/pull/8431): Make sure only one error will be sent to not-started Service  
  Contributor(s): Alexander Peshkov

* [#8430](https://github.com/FirebirdSQL/firebird/issues/8430): Unstable error messages in services with trace enabled  
  Contributor(s): Artyom Abakumov

* [#8320](https://github.com/FirebirdSQL/firebird/pull/8320): Do not allow run concurrent sweep instances  
  Contributor(s): Alexander Peshkov

* [#8296](https://github.com/FirebirdSQL/firebird/issues/8296): Crash in `TipCache::findStates`  
  Contributor(s): Vlad Khorsun

* [#8194](https://github.com/FirebirdSQL/firebird/issues/8194): Internal consistency check (page in use during flush) with small number of _DefaultDbCachePages_  
  Contributor(s): Vlad Khorsun

* [#8182](https://github.com/FirebirdSQL/firebird/issues/8182): `IN` predicate incorrectly handles single parenthesized subquery as IN-list, instead of table subquery  
  Contributor(s): Mark Rotteveel

* [#8139](https://github.com/FirebirdSQL/firebird/issues/8139): Conflict resolution code uses constraint name instead of index name  
  Contributor(s): Andrey Kravchenko, Vlad Khorsun


# v5.0.2

## Improvements

* [#8323](https://github.com/FirebirdSQL/firebird/pull/8323): Add `AUTO RELEASE TEMP BLOBID` transaction option  
  Contributor(s): Ilya Eremin

* [#8356](https://github.com/FirebirdSQL/firebird/issues/8356): Make trace use HEX representation for parameter values ​​of types `[VAR]CHAR CHARACTER SET OCTETS` and `[VAR]BINARY`  
  Contributor(s): Vlad Khorsun

* [#8353](https://github.com/FirebirdSQL/firebird/issues/8353): Report unique usernames for `isc_info_user_names`  
  Contributor(s): Vlad Khorsun

* [#8310](https://github.com/FirebirdSQL/firebird/pull/8310): Collect network statistics and make it available for the user applications  
  Contributor(s): Vlad Khorsun

* [#8307](https://github.com/FirebirdSQL/firebird/pull/8307): Wire protocol improvement: prefetch blob info and some data when open blob  
  Contributor(s): Vlad Khorsun

* [#8291](https://github.com/FirebirdSQL/firebird/issues/8291): NULLs should be skipped during index navigation when there's no lower bound and matched conditions are known to ignore NULLs  
  Contributor(s): Dmitry Yemanov

* [#8273](https://github.com/FirebirdSQL/firebird/pull/8273): Reorganize public headers  
  Contributor(s): Adriano dos Santos Fernandes

* [#8256](https://github.com/FirebirdSQL/firebird/issues/8256): Win_SSPI plugin uses NTLM  
  Contributor(s): Vlad Khorsun

* [#8197](https://github.com/FirebirdSQL/firebird/issues/8197): Add generated files for OO API for C language to distribution  
  Contributor(s): Alexander Peshkov

* [#8161](https://github.com/FirebirdSQL/firebird/issues/8161): Cardinality estimation should use primary record versions only  
  Contributor(s): Vlad Khorsun

* [#7269](https://github.com/FirebirdSQL/firebird/issues/7269): Database restore must make every effort on activating deferred indexes  
  Contributor(s): Vlad Khorsun, Dima

## Bugfixes

* [#8429](https://github.com/FirebirdSQL/firebird/issues/8429): Segfault when already destroyed callback interface is used  
  Contributor(s): Alexander Peshkov

* [#8417](https://github.com/FirebirdSQL/firebird/issues/8417): Negative statement ID in trace output  
  Contributor(s): Dmitry Yemanov

* [#8413](https://github.com/FirebirdSQL/firebird/pull/8413): Fix incorrect maximum size when reading dbb parameter values in `SHOW DATABASE`  
  Contributor(s): Artyom Ivanov

* [#8408](https://github.com/FirebirdSQL/firebird/issues/8408): Add option to disable install of _MSVCRT_ runtime libraries via a scripted install  
  Contributor(s): Paul Reeves

* [#8407](https://github.com/FirebirdSQL/firebird/issues/8407): _InnoSetup_ based installer deletes _msiexec_ log of runtime libraries install  
  Contributor(s): Paul Reeves

* [#8403](https://github.com/FirebirdSQL/firebird/pull/8403): Fix potential deadlock when starting the encryption thread  
  Contributor(s): Alexander Peshkov

* [#8390](https://github.com/FirebirdSQL/firebird/issues/8390): Deadlock might happen when database is shutting down with internal worker attachments  
  Contributor(s): Vlad Khorsun

* [#8389](https://github.com/FirebirdSQL/firebird/issues/8389): Unnecessary reload of the encryption plugin in the _SuperServer_ architecture  
  Contributor(s): 

* [#8386](https://github.com/FirebirdSQL/firebird/issues/8386): Crash when creating index on table that uses UDR and `ParallelWorkers > 1`  
  Contributor(s): Vlad Khorsun

* [#8380](https://github.com/FirebirdSQL/firebird/pull/8380): Fix race condition in `shutdownThread` start that can cause server to be unable to stop  
  Contributor(s): Artyom Ivanov

* [#8379](https://github.com/FirebirdSQL/firebird/issues/8379): Incorrect cardinality estimation for retrievals with multiple compound indices having common set of fields  
  Contributor(s): Dmitry Yemanov

* [#8350](https://github.com/FirebirdSQL/firebird/issues/8350): Missing records in replicated database  
  Contributor(s): Vlad Khorsun

* [#8341](https://github.com/FirebirdSQL/firebird/pull/8341): Cleanup batches inside the engine if they were not released explicity before disconnection  
  Contributor(s): Dmitry Yemanov

* [#8336](https://github.com/FirebirdSQL/firebird/issues/8336): Error: "Invalid clumplet buffer structure: buffer end before end of clumplet - clumplet too long (77779)" when using trusted auth  
  Contributor(s): Vlad Khorsun

* [#8334](https://github.com/FirebirdSQL/firebird/issues/8334): _MacOS ARM_ version requires _Rosetta_  
  Contributor(s): Adriano dos Santos Fernandes

* [#8327](https://github.com/FirebirdSQL/firebird/pull/8327): Fix use `:@` characters and add sub-section to configure username and password for `sync_replica`  
  Contributor(s): Andrey Kravchenko

* [#8324](https://github.com/FirebirdSQL/firebird/pull/8324): Make asynchronous replica re-initialization reliable  
  Contributor(s): Dmitry Yemanov

* [#8319](https://github.com/FirebirdSQL/firebird/pull/8319): Use 64-bit counter for records written during backup  
  Contributor(s): Dmitry Starodubov

* [#8315](https://github.com/FirebirdSQL/firebird/issues/8315): Crash at database restore due to failed system call  
  Contributor(s): Vlad Khorsun

* [#8304](https://github.com/FirebirdSQL/firebird/issues/8304): Wrong results using `minvalue/maxvalue` in join condition  
  Contributor(s): Adriano dos Santos Fernandes

* [#8292](https://github.com/FirebirdSQL/firebird/issues/8292): `run_all PDB` fails with "Error calling COPY_XTRA"   
  Contributor(s): Vlad Khorsun

* [#8290](https://github.com/FirebirdSQL/firebird/issues/8290): "Unique scan" is incorrectly reported in the explained plan for unique index and `IS NULL` predicate  
  Contributor(s): Dmitry Yemanov

* [#8288](https://github.com/FirebirdSQL/firebird/issues/8288): _GPRE_ generated code is incompatible with _GCC 14.2_  
  Contributor(s): Adriano dos Santos Fernandes

* [#8283](https://github.com/FirebirdSQL/firebird/issues/8283): Assert in `~thread_db()` due to not released page buffer  
  Contributor(s): Vlad Khorsun

* [#8268](https://github.com/FirebirdSQL/firebird/pull/8268): Fix refetch header data from delta when database in backup lock  
  Contributor(s): Andrey Kravchenko

* [#8265](https://github.com/FirebirdSQL/firebird/issues/8265): Nested `IN/EXISTS` subqueries should not be converted into semi-joins if the outer context is a sub-query which wasn't unnested  
  Contributor(s): Dmitry Yemanov

* [#8263](https://github.com/FirebirdSQL/firebird/issues/8263): _gbak_ on Classic with `ParallelWorkers > 1` doesn't restore indices, giving a cryptic error message  
  Contributor(s): Vlad Khorsun

* [#8255](https://github.com/FirebirdSQL/firebird/pull/8255): Catch possible stack overflow when preparing and compiling user statements  
  Contributor(s): Vlad Khorsun

* [#8253](https://github.com/FirebirdSQL/firebird/issues/8253): Incorrect handling of non-ASCII object names in `CREATE MAPPING` statement  
  Contributor(s): Vlad Khorsun

* [#8252](https://github.com/FirebirdSQL/firebird/issues/8252): Incorrect subquery unnesting with complex dependencies (`SubQueryConversion = true`)  
  Contributor(s): Dmitry Yemanov

* [#8250](https://github.com/FirebirdSQL/firebird/issues/8250): Bad performance on simple two joins query on tables with composed index - minutes on Firebird 5 compared to Firebird 3 miliseconds  
  Contributor(s): Dmitry Yemanov

* [#8243](https://github.com/FirebirdSQL/firebird/pull/8243): Fix a bug where the shutdown handler could be called again  
  Contributor(s): Alexander Zhdanov

* [#8238](https://github.com/FirebirdSQL/firebird/pull/8238): Fix using macro with regex in path parameter in _fbtrace.conf_  
  Contributor(s): Alexander Peshkov, Artyom Ivanov

* [#8237](https://github.com/FirebirdSQL/firebird/issues/8237): Database access error when _nbackup_ is starting  
  Contributor(s): Alexander Peshkov

* [#8236](https://github.com/FirebirdSQL/firebird/issues/8236): Avoid "hangs" in `clock_gettime()` in tomcrypt's `PRNG`  
  Contributor(s): Alexander Peshkov

* [#8233](https://github.com/FirebirdSQL/firebird/issues/8233): `SubQueryConversion = true` --multiple rows in singleton select  
  Contributor(s): Dmitry Yemanov

* [#8224](https://github.com/FirebirdSQL/firebird/issues/8224): `SubQueryConversion = true` -- wrong resultset with FIRST/SKIP clauses inside the outer query  
  Contributor(s): Dmitry Yemanov

* [#8223](https://github.com/FirebirdSQL/firebird/issues/8223): `SubQueryConversion = true` -- error "no current record for fetch operation" with complex joins  
  Contributor(s): Dmitry Yemanov

* [#8222](https://github.com/FirebirdSQL/firebird/pull/8222): Fix a case of deleted memory modification  
  Contributor(s): 

* [#8221](https://github.com/FirebirdSQL/firebird/issues/8221): Crash when `MAKE_DBKEY()` is called with 0 or 1 arguments  
  Contributor(s): Vlad Khorsun

* [#8219](https://github.com/FirebirdSQL/firebird/issues/8219): Database creation in 3.0.12, 4.0.5 and 5.0.1 slower than in previous releases  
  Contributor(s): Adriano dos Santos Fernandes

* [#8215](https://github.com/FirebirdSQL/firebird/issues/8215): Rare sporadic segfaults in test for _core-6142_ on Windows  
  Contributor(s): Alexander Peshkov

* [#8214](https://github.com/FirebirdSQL/firebird/issues/8214): Incorrect result of index list scan for a composite index, the second segment of which is a text field with `COLLATE UNICODE_CI`  
  Contributor(s): Dmitry Yemanov

* [#8211](https://github.com/FirebirdSQL/firebird/issues/8211): `DATEADD` truncates milliseconds for month and year deltas  
  Contributor(s): Adriano dos Santos Fernandes

* [#8203](https://github.com/FirebirdSQL/firebird/issues/8203): Function `MAKE_DBKEY` may produce random errors if used with relation name  
  Contributor(s): Vlad Khorsun

* [#8109](https://github.com/FirebirdSQL/firebird/issues/8109): Plan/performance regression when using special construct for `IN` in FB5.x compared to FB3.x  
  Contributor(s): Dmitry Yemanov

* [#8069](https://github.com/FirebirdSQL/firebird/pull/8069): Add missing synchronization to cached vectors of known pages  
  Contributor(s): Dmitry Yemanov, Vlad Khorsun


# v5.0.1

## Improvements

* [#8181](https://github.com/FirebirdSQL/firebird/pull/8181): Ensure the standalone CS listener on Linux uses the _SO_REUSEADDR_ socket option   
  Contributor(s): Dmitry Yemanov

* [#8165](https://github.com/FirebirdSQL/firebird/pull/8165): Added shutdown handler for _Classic Server_   
  Contributor(s): Alexander Zhdanov

* [#8104](https://github.com/FirebirdSQL/firebird/issues/8104): More efficient evaluation of expressions like `RDB$DB_KEY <= ?` after mass delete   
  Contributor(s): Vlad Khorsun

* [#8066](https://github.com/FirebirdSQL/firebird/issues/8066): Make protocol schemes case-insensitive  
  Contributor(s): Vlad Khorsun

* [#8061](https://github.com/FirebirdSQL/firebird/pull/8061): Unnest `IN/ANY/EXISTS` subqueries and optimize them using semi-join algorithm  
  Contributor(s): Dmitry Yemanov

* [#8042](https://github.com/FirebirdSQL/firebird/issues/8042): Improve conflict resolution on replica when table have both primary and unique keys  
  Contributor(s): Vlad Khorsun

* [#8030](https://github.com/FirebirdSQL/firebird/issues/8030): Better cardinality estimation when empty data pages exist  
  Contributor(s): Vlad Khorsun

* [#8010](https://github.com/FirebirdSQL/firebird/issues/8010): Remove _gfix -cache_ option  
  Contributor(s): Vlad Khorsun

* [#7978](https://github.com/FirebirdSQL/firebird/issues/7978): Update Windows distributions with _zlib_ version 1.3.1  
  Contributor(s): Vlad Khorsun

* [#7928](https://github.com/FirebirdSQL/firebird/issues/7928): Make _TempCacheLimit_ setting to be per-database (not per-attachment) for _SuperClassic_  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#8189](https://github.com/FirebirdSQL/firebird/issues/8189): Slow connection times with a lot of simultaneous connections and active trace session present  
  Contributor(s): Alex Peshkoff

* [#8186](https://github.com/FirebirdSQL/firebird/issues/8186): Fixed a few issues with IPC used by remote profiler  
  Contributor(s): Vlad Khorsun

* [#8185](https://github.com/FirebirdSQL/firebird/issues/8185): SIGSEGV in Firebird 5.0.0.1306 Embedded during update on cursor  
  Contributor(s): Adriano dos Santos Fernandes, Dmitry Yemanov

* [#8180](https://github.com/FirebirdSQL/firebird/issues/8180): Sometimes a system trace session is terminated spontaneously  
  Contributor(s): Artyom Abakumov

* [#8178](https://github.com/FirebirdSQL/firebird/pull/8178): Fix boolean conversion to string inside `DataTypeUtil::makeFromList()`  
  Contributor(s): Dmitry Yemanov

* [#8176](https://github.com/FirebirdSQL/firebird/issues/8176): Firebird 5 hangs after starting remote profiling session  
  Contributor(s): Vlad Khorsun

* [#8172](https://github.com/FirebirdSQL/firebird/issues/8172): File _include/firebird/impl/iberror_c.h_ is missing from the Linux x64 tar archive  
  Contributor(s): Adriano dos Santos Fernandes

* [#8171](https://github.com/FirebirdSQL/firebird/issues/8171): Trace plugin unloaded if called method is not implemented  
  Contributor(s): Vlad Khorsun

* [#8168](https://github.com/FirebirdSQL/firebird/issues/8168): `MAKE_DBKEY` bug after backup/restore  
  Contributor(s): Vlad Khorsun

* [#8156](https://github.com/FirebirdSQL/firebird/issues/8156): Can not specify concrete IPv6 address in ES/EDS connection string.  
  Contributor(s): Vlad Khorsun

* [#8151](https://github.com/FirebirdSQL/firebird/issues/8151): Deadlock happens when run 'List Trace Sessions' service and there are many active trace sessions  
  Contributor(s): Vlad Khorsun

* [#8150](https://github.com/FirebirdSQL/firebird/issues/8150): Process could attach to the deleted instance of shared memory  
  Contributor(s): Alexander Peshkov, Vlad Khorsun

* [#8149](https://github.com/FirebirdSQL/firebird/issues/8149): The hung or crash could happen when connection fires _TRACE_EVENT_DETACH_ event and new trace session created concurrently  
  Contributor(s): Vlad Khorsun

* [#8138](https://github.com/FirebirdSQL/firebird/issues/8138): Bugcheck when replicator state is changed concurrently  
  Contributor(s): Vlad Khorsun

* [#8136](https://github.com/FirebirdSQL/firebird/issues/8136): Server crashes with `IN (dbkey1, dbkey2, ...)` condition  
  Contributor(s): Dmitry Yemanov

* [#8123](https://github.com/FirebirdSQL/firebird/issues/8123): Procedure manipulation can lead to wrong dependencies removal  
  Contributor(s): Adriano dos Santos Fernandes

* [#8120](https://github.com/FirebirdSQL/firebird/issues/8120): Cast dies with numeric value is out of range error  
  Contributor(s): Vlad Khorsun

* [#8115](https://github.com/FirebirdSQL/firebird/issues/8115): Unexpected results using `LEFT JOIN` with `WHEN` function  
  Contributor(s): Dmitry Yemanov

* [#8114](https://github.com/FirebirdSQL/firebird/issues/8114): Segfault in connections pool during server shutdown  
  Contributor(s): Vlad Khorsun

* [#8112](https://github.com/FirebirdSQL/firebird/issues/8112): Error _isc_read_only_trans (335544361)_ should report _SQLSTATE 25006_  
  Contributor(s): Adriano dos Santos Fernandes

* [#8110](https://github.com/FirebirdSQL/firebird/issues/8110): Firebird 5 crashes on Android API level 34  
  Contributor(s): Vlad Khorsun

* [#8108](https://github.com/FirebirdSQL/firebird/issues/8108): ICU 63.1 suppresses conversion errors  
  Contributor(s): Dmitry Kovalenko

* [#8101](https://github.com/FirebirdSQL/firebird/issues/8101): Firebird crashes if a plugin factory returns `nullptr` and no error in status  
  Contributor(s): Vlad Khorsun, Dimitry Sibiryakov

* [#8100](https://github.com/FirebirdSQL/firebird/issues/8100): The `isc_array_lookup_bounds` function returns invalid values for low and high array bounds  
  Contributor(s): Adriano dos Santos Fernandes

* [#8094](https://github.com/FirebirdSQL/firebird/issues/8094): Creation index error when restore with parallels workers  
  Contributor(s): Vlad Khorsun

* [#8089](https://github.com/FirebirdSQL/firebird/issues/8089): AV when attaching database while low of free memory  
  Contributor(s): Vlad Khorsun

* [#8087](https://github.com/FirebirdSQL/firebird/issues/8087): AV when preparing a query with `IN` list that contains both literals and sub-query  
  Contributor(s): Vlad Khorsun

* [#8086](https://github.com/FirebirdSQL/firebird/issues/8086): `IN` predicate with string-type elements is evaluated wrongly against a numeric field  
  Contributor(s): Dmitry Yemanov

* [#8085](https://github.com/FirebirdSQL/firebird/issues/8085): Memory leak when executing a lot of different queries and _StatementTimeout > 0_  
  Contributor(s): Vlad Khorsun

* [#8084](https://github.com/FirebirdSQL/firebird/issues/8084): Partial index uniqueness violation  
  Contributor(s): Vlad Khorsun

* [#8083](https://github.com/FirebirdSQL/firebird/issues/8083): AV when writting into internal trace log  
  Contributor(s): Vlad Khorsun

* [#8079](https://github.com/FirebirdSQL/firebird/issues/8079): Engine could crash when executing some trigger(s) while another attachment modifies them  
  Contributor(s): Vlad Khorsun

* [#8078](https://github.com/FirebirdSQL/firebird/issues/8078): `SIMILAR TO` with constant pattern using ‘|’, ‘*’, ‘?’ or ‘{0,N}’ doesn't work as expected  
  Contributor(s): Adriano dos Santos Fernandes

* [#8077](https://github.com/FirebirdSQL/firebird/issues/8077): Error "Too many recursion levels" does not stop execution of code that uses `ON DISCONNECT` trigger  
  Contributor(s): Alexander Peshkov, Vlad Khorsun

* [#8063](https://github.com/FirebirdSQL/firebird/issues/8063): `(VAR)CHAR` variables/parameters assignments fail in stored procedures with subroutines  
  Contributor(s): Adriano dos Santos Fernandes

* [#8058](https://github.com/FirebirdSQL/firebird/issues/8058): DDL changes in replication does not set the correct grantor  
  Contributor(s): Dmitry Yemanov

* [#8056](https://github.com/FirebirdSQL/firebird/issues/8056): "Too many temporary blobs" with blob_append when select a stored procedue using rows-clause  
  Contributor(s): Vlad Khorsun

* [#8040](https://github.com/FirebirdSQL/firebird/issues/8040): Bugcheck 183 (wrong record length) could happen on replica database after UK violation on insert   
  Contributor(s): Vlad Khorsun

* [#8039](https://github.com/FirebirdSQL/firebird/issues/8039): Segfault when opening damaged (last TIP is missing in _RDB$PAGES_, user's FW was OFF) database  
  Contributor(s): Alexander Peshkov

* [#8037](https://github.com/FirebirdSQL/firebird/issues/8037): Remove directory entries from debug symbols tarbal  
  Contributor(s): Alexander Peshkov

* [#8034](https://github.com/FirebirdSQL/firebird/issues/8034): (re)set owner/group in tarbal of non-root builds  
  Contributor(s): Alexander Peshkov

* [#8033](https://github.com/FirebirdSQL/firebird/issues/8033): Invalid result when string compared with indexed `NUMERIC(x,y)` field `where x > 18 and y != 0`  
  Contributor(s): Alexander Peshkov

* [#8027](https://github.com/FirebirdSQL/firebird/issues/8027): Broken gbak statistics  
  Contributor(s): Alexander Peshkov

* [#8026](https://github.com/FirebirdSQL/firebird/issues/8026): Crash LI-V5.0.0.1306 in _libEngine13.so_  
  Contributor(s): Alexander Peshkov

* [#8016](https://github.com/FirebirdSQL/firebird/pull/8016): Free memory issued for _isql_ command list but has never been freed on output file write  
  Contributor(s): 

* [#8011](https://github.com/FirebirdSQL/firebird/issues/8011): `DECFLOAT` error working with `INT128` in UDR  
  Contributor(s): Alexander Peshkov

* [#8006](https://github.com/FirebirdSQL/firebird/issues/8006): `INT128` datatype not supported in _FB_MESSAGE_ macro   
  Contributor(s): Alexander Peshkov

* [#8003](https://github.com/FirebirdSQL/firebird/issues/8003): _gbak_ can't backup database in ODS < 13  
  Contributor(s): Vlad Khorsun

* [#7998](https://github.com/FirebirdSQL/firebird/issues/7998): Сrash during partial index checking if the condition raises a conversion error  
  Contributor(s): Dmitry Yemanov

* [#7997](https://github.com/FirebirdSQL/firebird/issues/7997): Unexpected results when comparing integer with string containing value out of range of that integer datatype  
  Contributor(s): Alexander Peshkov

* [#7996](https://github.com/FirebirdSQL/firebird/issues/7996): _gbak_ terminates/crashes when a read error occurs during restore  
  Contributor(s): Vlad Khorsun

* [#7995](https://github.com/FirebirdSQL/firebird/issues/7995): Unexpected results after creating partial index  
  Contributor(s): Dmitry Yemanov

* [#7993](https://github.com/FirebirdSQL/firebird/issues/7993): Unexpected results when using `CASE WHEN` with `RIGHT JOIN`  
  Contributor(s): Dmitry Yemanov

* [#7992](https://github.com/FirebirdSQL/firebird/issues/7992): Assertion _(space > 0)_ failure during restore  
  Contributor(s): Vlad Khorsun

* [#7985](https://github.com/FirebirdSQL/firebird/issues/7985): Hang in case of error when sweep thread is attaching to database (CS case)  
  Contributor(s): Alexander Peshkov

* [#7979](https://github.com/FirebirdSQL/firebird/issues/7979): Hang when database with disconnect trigger using MON$ tables is shutting down  
  Contributor(s): Alexander Peshkov

* [#7976](https://github.com/FirebirdSQL/firebird/issues/7976): False validation error for short unpacked records  
  Contributor(s): Dmitry Yemanov

* [#7974](https://github.com/FirebirdSQL/firebird/issues/7974): Restore of wide table can fail with "adjusting an invalid decompression length from <N> to <M>"  
  Contributor(s): Vlad Khorsun

* [#7969](https://github.com/FirebirdSQL/firebird/issues/7969): Characters are garbled when replicating fields with type `BLOB SUB_TYPE TEXT` if the character set of the connection and the field are different  
  Contributor(s): Dmitry Yemanov

* [#7962](https://github.com/FirebirdSQL/firebird/issues/7962): System procedure/function inconsistency between `SHOW FUNCTIONS` and `SHOW PROCEDURES` in _isql_  
  Contributor(s): 

* [#7950](https://github.com/FirebirdSQL/firebird/issues/7950): Unable to restore database when .fbk was created on host with other ICU  
  Contributor(s): Alexander Peshkov

* [#7942](https://github.com/FirebirdSQL/firebird/issues/7942): Error: database file appears corrupted after restore from backup  
  Contributor(s): Vlad Khorsun

* [#7937](https://github.com/FirebirdSQL/firebird/issues/7937): Inner join raises error "no current record for fetch operation" if a stored procedure depends on some table via input parameter and also has an indexed relationship with another table  
  Contributor(s): Dmitry Yemanov

* [#7927](https://github.com/FirebirdSQL/firebird/issues/7927): Some default values is set incorrectly for SC/CS architectures  
  Contributor(s): Vlad Khorsun

* [#7921](https://github.com/FirebirdSQL/firebird/issues/7921): FB5 uses PK for ordered plan even if less count of fields matching index exists  
  Contributor(s): Dmitry Yemanov

* [#7899](https://github.com/FirebirdSQL/firebird/issues/7899): Inconsistent state of master-detail occurs after RE-connect + 'SET AUTODDL OFF' + 'drop <FK>' which is ROLLED BACK  
  Contributor(s): Vlad Khorsun

* [#7896](https://github.com/FirebirdSQL/firebird/issues/7896): replication.log remains empty (and without any error in firebird.log) until concurrent FB instance is running under different account and generates segments on its master. Significant delay required after stop concurrent FB it in order allow first one to write in its replication log.  
  Contributor(s): Vlad Khorsun

* [#7873](https://github.com/FirebirdSQL/firebird/issues/7873): Wrong memory buffer alignment and I/O buffer size when working in direct I/O mode  
  Contributor(s): Vlad Khorsun

* [#7869](https://github.com/FirebirdSQL/firebird/issues/7869): _GBAK_ can write uninitialized data into `RDB$RETURN_ARGUMENT` and `RDB$ARGUMENT_POSITION` fields  
  Contributor(s): Dmitry Kovalenko

* [#7863](https://github.com/FirebirdSQL/firebird/issues/7863): Non-correlated sub-query is evaluated multiple times if it is based on a VIEW rather than on appropriate derived table  
  Contributor(s): Dmitry Yemanov

* [#7394](https://github.com/FirebirdSQL/firebird/issues/7394): autoconf 2.72  
  Contributor(s): Alexander Peshkov


# v5.0 Final Release

(no changes)

# v5.0 Release Candidate 2

## Improvements

* [#7918](https://github.com/FirebirdSQL/firebird/issues/7918): Allow to configure Firebird in posix using relative directories with options --with-fb*  
  Contributor(s): Adriano dos Santos Fernandes

* [#7910](https://github.com/FirebirdSQL/firebird/issues/7910): Add backward compatibility option that disables joins transformation  
  Contributor(s): Dmitry Yemanov

* [#7854](https://github.com/FirebirdSQL/firebird/issues/7854): Performance issue with time zones  
  Contributor(s): Adriano dos Santos Fernandes, Vlad Khorsun

* [#7819](https://github.com/FirebirdSQL/firebird/issues/7819): Difficulty returning the product version with the legacy connection  
  Contributor(s): Vlad Khorsun

* [#7818](https://github.com/FirebirdSQL/firebird/issues/7818): Extend rdb$get_context('SYSTEM', '***') with other info from MON$ATTACHMENT  
  Contributor(s): Vlad Khorsun

* [#7814](https://github.com/FirebirdSQL/firebird/issues/7814): Don't update database-level statistics on every page cache operation  
  Contributor(s): Vlad Khorsun

* [#7810](https://github.com/FirebirdSQL/firebird/issues/7810): Improve SKIP LOCKED implementation  
  Contributor(s): Vlad Khorsun

* [#7755](https://github.com/FirebirdSQL/firebird/issues/7755): Update Windows distribution with new zlib version 1.3 (released 2023-08-18)  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#7917](https://github.com/FirebirdSQL/firebird/issues/7917): Hang in a case of error when the sweep thread is attaching the database  
  Contributor(s): Alexander Peshkov

* [#7905](https://github.com/FirebirdSQL/firebird/issues/7905): Segfault during TIP cache initialization  
  Contributor(s): Alexander Peshkov

* [#7904](https://github.com/FirebirdSQL/firebird/issues/7904): FB5 bad plan for query  
  Contributor(s): Dmitry Yemanov

* [#7903](https://github.com/FirebirdSQL/firebird/issues/7903): Unexpected Results when Using CASE-WHEN with LEFT JOIN  
  Contributor(s): Dmitry Yemanov

* [#7885](https://github.com/FirebirdSQL/firebird/issues/7885): Unstable error messages in services due to races related with service status vector  
  Contributor(s): Alexander Peshkov

* [#7879](https://github.com/FirebirdSQL/firebird/issues/7879): Unexpected Results when Using Natural Right Join  
  Contributor(s): Dmitry Yemanov

* [#7867](https://github.com/FirebirdSQL/firebird/issues/7867): Error "wrong page type" during garbage collection on v4.0.4  
  Contributor(s): Ilya Eremin

* [#7860](https://github.com/FirebirdSQL/firebird/issues/7860): Crash potentially caused by BETWEEN Operator  
  Contributor(s): Vlad Khorsun

* [#7853](https://github.com/FirebirdSQL/firebird/issues/7853): Do not consider non-deterministic expressions as invariants in pre-filters  
  Contributor(s): Dmitry Yemanov

* [#7851](https://github.com/FirebirdSQL/firebird/issues/7851): [FB1+, GBAK, Restore] The skip of att_functionarg_field_precision does not check RESTORE_format  
  Contributor(s): Dmitry Kovalenko

* [#7846](https://github.com/FirebirdSQL/firebird/issues/7846): FB4 can't backup/restore int128-array  
  Contributor(s): Dmitry Kovalenko

* [#7844](https://github.com/FirebirdSQL/firebird/issues/7844): Removing first column with SET WIDTH crashes ISQL  
  Contributor(s): Adriano dos Santos Fernandes

* [#7839](https://github.com/FirebirdSQL/firebird/issues/7839): Potential bug in BETWEEN Operator  
  Contributor(s): Vlad Khorsun

* [#7832](https://github.com/FirebirdSQL/firebird/issues/7832): Firebird 5 and 6 crash on "... RETURNING * " without INTO in PSQL  
  Contributor(s): Adriano dos Santos Fernandes

* [#7831](https://github.com/FirebirdSQL/firebird/issues/7831): Incorrect type of UDF-argument with array  
  Contributor(s): Dmitry Kovalenko

* [#7827](https://github.com/FirebirdSQL/firebird/issues/7827): Problem using python firebird-driver with either intel or m1 Mac buiilds with version 4.0.3 or 5.0+  
  Contributor(s): Adriano dos Santos Fernandes

* [#7817](https://github.com/FirebirdSQL/firebird/issues/7817): Memory leak is possible for UDF array arguments  
  Contributor(s): Dmitry Yemanov

* [#7812](https://github.com/FirebirdSQL/firebird/issues/7812): Service backup does not work in multiple engines configuration  
  Contributor(s): Alexander Peshkov

* [#7800](https://github.com/FirebirdSQL/firebird/issues/7800): Default publication status is not preserved after backup/restore  
  Contributor(s): Dmitry Yemanov

* [#7795](https://github.com/FirebirdSQL/firebird/issues/7795): NOT IN <list> returns incorrect result if NULLs are present inside the value list  
  Contributor(s): Dmitry Yemanov

* [#7779](https://github.com/FirebirdSQL/firebird/issues/7779): Firebird 4.0.3 is constantly crashing with the same symptoms (fbclient.dll) (incl. DMP File Analysis)  
  Contributor(s): Vlad Khorsun

* [#7772](https://github.com/FirebirdSQL/firebird/issues/7772): Blob corruption in FB 4.0.3 (embedded)  
  Contributor(s): Vlad Khorsun

* [#7770](https://github.com/FirebirdSQL/firebird/issues/7770): restore takes 25% more time vs 4.0.0  
  Contributor(s): Vlad Khorsun

* [#7767](https://github.com/FirebirdSQL/firebird/issues/7767): Slow drop trigger command execution under FB5.0  
  Contributor(s): Dmitry Yemanov

* [#7762](https://github.com/FirebirdSQL/firebird/issues/7762): Crash on "Operating system call pthread_mutex_destroy failed. Error code 16" in log  
  Contributor(s): Alexander Peshkov

* [#7761](https://github.com/FirebirdSQL/firebird/issues/7761): Regression when displaying line number of errors in ISQL scripts  
  Contributor(s): Adriano dos Santos Fernandes

* [#7760](https://github.com/FirebirdSQL/firebird/issues/7760): Parameters inside the IN list may cause a string truncation error  
  Contributor(s): Dmitry Yemanov

* [#7759](https://github.com/FirebirdSQL/firebird/issues/7759): Routine calling overhead increased by factor 6 vs Firebird 4.0.0  
  Contributor(s): Adriano dos Santos Fernandes

* [#7461](https://github.com/FirebirdSQL/firebird/issues/7461): Differencies in field metadata descriptions between Firebird 2.5 and Firebird 4  
  Contributor(s): Dmitry Yemanov


# v5.0 Release Candidate 1

## New features

* [#7682](https://github.com/FirebirdSQL/firebird/issues/7682): Use _ParallelWorkers_ setting from firebird.conf as default for all parallelised operations  
  Contributor(s): Vlad Khorsun

* [#7469](https://github.com/FirebirdSQL/firebird/pull/7469): Make Android port (client / embedded) work inside apps  
  Contributor(s): Adriano dos Santos Fernandes

* [#5959](https://github.com/FirebirdSQL/firebird/issues/5959): Add support for `QUARTER` to `EXTRACT`, `FIRST_DAY` and `LAST_DAY`  
  Contributor(s): Adriano dos Santos Fernandes

## Improvements

* [#7752](https://github.com/FirebirdSQL/firebird/issues/7752): Avoid the access path information inside the `PLG$PROF_RECORD_SOURCES` table from being truncated to 255 characters  
  Contributor(s): Adriano dos Santos Fernandes

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
