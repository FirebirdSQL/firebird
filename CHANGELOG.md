# v4.0.7

## New features

* [#8761](https://github.com/FirebirdSQL/firebird/issues/8761): Add API method `Util::convert`  
  Contributor(s): Adriano dos Santos Fernandes

## Improvements

* [#8922](https://github.com/FirebirdSQL/firebird/issues/8922): Update Windows distributions with _zlib_ version 1.3.2  
  Contributor(s): Vlad Khorsun

* [#8598](https://github.com/FirebirdSQL/firebird/issues/8598): Don't fire referential integrity triggers if primary or unique keys haven't changed  
  Contributor(s): Vlad Khorsun

* [#8553](https://github.com/FirebirdSQL/firebird/pull/8553): Get the modification time of a config file with a higher precision to fix cases when it's not reloaded after modification/replacement  
  Contributor(s): Ilya Eremin

## Bugfixes

* [#8965](https://github.com/FirebirdSQL/firebird/issues/8965): Wrong `PERCENT_RANK` computation  
  Contributor(s): Denis Simonov

* [#8926](https://github.com/FirebirdSQL/firebird/issues/8926): Indexes may not be used for multiple ORed conditions containing both field and non-field references  
  Contributor(s): Dmitry Yemanov

* [#8903](https://github.com/FirebirdSQL/firebird/issues/8903): Use of some switches with parameter in _gbak_'s command line before name of database in `-SE` mode breaks access to databases with non-default security database  
  Contributor(s): Alexander Peshkov

* [#8857](https://github.com/FirebirdSQL/firebird/issues/8857): Replication stops if `GRANT` was issued by non-DBA user who has the `RDB$ADMIN` role and appropriate object (table, etc) belongs to another user  
  Contributor(s): Dmitry Yemanov

* [#8851](https://github.com/FirebirdSQL/firebird/issues/8851): 'Internal error' when calling outer procedure after deleting unused result variable from inner procedure  
  Contributor(s): Vlad Khorsun

* [#8850](https://github.com/FirebirdSQL/firebird/issues/8850): `KERNEL32.GetThreadId` import breaks Windows XP  
  Contributor(s): Vlad Khorsun

* [#8826](https://github.com/FirebirdSQL/firebird/pull/8826): Fixed potential endless loop inside `MET_scan_relation`  
  Contributor(s): Vlad Khorsun

* [#8817](https://github.com/FirebirdSQL/firebird/issues/8817): Fatal lock manager error: invalid lock id  
  Contributor(s): Vlad Khorsun

* [#8799](https://github.com/FirebirdSQL/firebird/issues/8799): BUGCHECK "decompression overran buffer (179)" when `WITH LOCK` clause is used  
  Contributor(s): Ilya Eremin

* [#8766](https://github.com/FirebirdSQL/firebird/pull/8766): File truncation errors may happen during replication in Windows CS  
  Contributor(s): Dmitry Yemanov

* [#8755](https://github.com/FirebirdSQL/firebird/issues/8755): Replicator could produce log segments with duplicated segment numbers  
  Contributor(s): Vlad Khorsun

* [#8739](https://github.com/FirebirdSQL/firebird/issues/8739): Wrong `SQLSTATE` in case of table alias conflict  
  Contributor(s): Mark Rotteveel

* [#8726](https://github.com/FirebirdSQL/firebird/issues/8726): Replication error is raised for `CREATE TABLE ... EXTERNAL` if the specified disk does not exist on the replica host  
  Contributor(s): Dmitry Yemanov

* [#8690](https://github.com/FirebirdSQL/firebird/issues/8690): On Windows 7 _isql_ exits silently right after the start  
  Contributor(s): Vlad Khorsun

* [#8674](https://github.com/FirebirdSQL/firebird/pull/8674): Fix crash during shutdown after unsuccessful `ping()` call  
  Contributor(s): Dmitry Yemanov

* [#8666](https://github.com/FirebirdSQL/firebird/pull/8666): Fixed crash after calling incorrectly parametrized request  
  Contributor(s): Vlad Khorsun

* [#8665](https://github.com/FirebirdSQL/firebird/issues/8665): `SHOW DEPENDENCIES` command terminates unexpectedly if there are packages in the dependencies  
  Contributor(s): Adriano dos Santos Fernandes

* [#8644](https://github.com/FirebirdSQL/firebird/issues/8644): Connection error via `Loopback` provider if it's the first in the `Providers` parameter  
  Contributor(s): Alexander Peshkov

* [#8541](https://github.com/FirebirdSQL/firebird/issues/8541): Deadlock update conflict on replica server  
  Contributor(s): Vlad Khorsun

* [#8105](https://github.com/FirebirdSQL/firebird/issues/8105): Master database could miss replication segments on Windows Classic Server  
  Contributor(s): Vlad Khorsun

* [GHSA-7pxc-h3rv-r257](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-7pxc-h3rv-r257): Path traversal when declaring external routine (CVE-2026-40342)  
  Contributor(s): VladimirEliTokarev, Alexander Peshkov

* [GHSA-g99w-prq5-29c6](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-g99w-prq5-29c6): DoS via malicious slice descriptor in the slice packet (CVE-2026-35215)  
  Contributor(s): Artyom Ivanov, Alexander Peshkov

* [GHSA-7jq3-6j3c-5cm2](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-7jq3-6j3c-5cm2): DoS via `op_response` packet from client (CVE-2026-34232)  
  Contributor(s): Artyom Ivanov

* [GHSA-89mq-229g-x47p](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-89mq-229g-x47p): Buffer overflow on parsing corrupted slice packet (CVE-2026-33337)  
  Contributor(s): Artyom Ivanov

* [GHSA-xrcw-wpjx-pr9](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-xrcw-wpjx-pr9): CryptCallback DoS (CVE-2026-28224)  
  Contributor(s): Ev3nt

* [GHSA-6crx-4g37-7j49](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-6crx-4g37-7j49): Pre-Auth DoS (CVE-2026-27890)  
  Contributor(s): Artyom Ivanov

* [GHSA-7cq5-994r-jhrf](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-7cq5-994r-jhrf): Server hangs when using specific clumplet on batch creation (CVE-2026-28214)  
  Contributor(s): Artyom Ivanov

* [GHSA-9884-9qm3-hqch](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-9884-9qm3-hqch): One packet (`op_slice`) DoS (CVE-2026-28212)  
  Contributor(s): Alexey Mochalov

* [GHSA-mfpr-9886-xjhg](https://github.com/FirebirdSQL/firebird/security/advisories/GHSA-mfpr-9886-xjhg): Information leak vulnerability in firebird3 client when used with newer (>= 4) server (CVE-2025-65104)  
  Contributor(s): Alexander Peshkov

# v4.0.6

## Improvements

* [#8433](https://github.com/FirebirdSQL/firebird/pull/8433): Improve code of class `BePlusTree`   
  Contributor(s): Vlad Khorsun

* [#8353](https://github.com/FirebirdSQL/firebird/issues/8353): Report unique usernames for `isc_info_user_names`  
  Contributor(s): Vlad Khorsun

* [#8256](https://github.com/FirebirdSQL/firebird/issues/8256): `Win_sspi` plugin uses NTLM  
  Contributor(s): Vlad Khorsun

* [#8161](https://github.com/FirebirdSQL/firebird/issues/8161): Cardinality estimation should use primary record versions only  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#8592](https://github.com/FirebirdSQL/firebird/issues/8592): Presence of `ROWS <n_limit>` causes garbage in error message when string conversion problem raises  
  Contributor(s): Adriano dos Santos Fernandes

* [#8590](https://github.com/FirebirdSQL/firebird/issues/8590): Line "BLR to Source mapping:" may look broken when `procedures.rdb$debug_info` is queried using remote protocol  
  Contributor(s): Vlad Khorsun

* [#8589](https://github.com/FirebirdSQL/firebird/issues/8589): `PERCENT_RANK` may return _NaN_ instead of 0  
  Contributor(s): Adriano dos Santos Fernandes

* [#8588](https://github.com/FirebirdSQL/firebird/issues/8588): Error doing a backup when database file name has extended ASCII chars  
  Contributor(s): Alexander Peshkov

* [#8554](https://github.com/FirebirdSQL/firebird/issues/8554): Vulnerability GHSA-7qp6-hqxj-pjjp / ZDI-CAN-26486  
  Contributor(s): Alexander Peshkov

* [#8524](https://github.com/FirebirdSQL/firebird/issues/8524): _ISQL_ will truncate lines longer than 255 characters when pasting  
  Contributor(s): Vlad Khorsun

* [#8520](https://github.com/FirebirdSQL/firebird/issues/8520): Error in `iTransaction.getInfo()` on embedded connection  
  Contributor(s): Alexander Peshkov

* [#8509](https://github.com/FirebirdSQL/firebird/issues/8509): "Error creating private namespace" message in _firebird.log_  
  Contributor(s): Vlad Khorsun

* [#8487](https://github.com/FirebirdSQL/firebird/issues/8487): Unexpected _SetFilePointer_ error when remapping the shared memory  
  Contributor(s): Vlad Khorsun

* [#8485](https://github.com/FirebirdSQL/firebird/issues/8485): Segfault on incorrect _databases.conf_ starting with sub-config (without line `alias=database_path`)  
  Contributor(s): Alexander Peshkov

* [#8477](https://github.com/FirebirdSQL/firebird/issues/8477): Inheritance of `WINDOW` does not work  
  Contributor(s): Adriano dos Santos Fernandes

* [#8449](https://github.com/FirebirdSQL/firebird/issues/8449): Races when server is closed during forced database shutdown  
  Contributor(s): Alexander Peshkov

* [#8440](https://github.com/FirebirdSQL/firebird/issues/8440): Wrong result for `MINVALUE/MAXVALUE` with string arguments  
  Contributor(s): Dmitry Yemanov

* [#8437](https://github.com/FirebirdSQL/firebird/issues/8437): Segfault when running query with `PARTITION BY` and subquery  
  Contributor(s): Adriano dos Santos Fernandes

* [#8431](https://github.com/FirebirdSQL/firebird/pull/8431): Make sure only one error will be sent to not-started service  
  Contributor(s): Alexander Peshkov

* [#8430](https://github.com/FirebirdSQL/firebird/issues/8430): Unstable error messages in services with trace enabled  
  Contributor(s): Artyom Abakumov

* [#8429](https://github.com/FirebirdSQL/firebird/issues/8429): Segfault when already destroyed callback interface is used  
  Contributor(s): Alexander Peshkov

* [#8417](https://github.com/FirebirdSQL/firebird/issues/8417): Negative statement ID in trace output  
  Contributor(s): Dmitry Yemanov

* [#8403](https://github.com/FirebirdSQL/firebird/pull/8403): Fix potential deadlock when starting the encryption thread  
  Contributor(s): Alexander Peshkov

* [#8389](https://github.com/FirebirdSQL/firebird/issues/8389): Unnecessary reload of the encryption plugin in the _SuperServer_ architecture  
  Contributor(s): Alexey Mochalov

* [#8350](https://github.com/FirebirdSQL/firebird/issues/8350): Missing records inside a replicated database  
  Contributor(s): Vlad Khorsun

* [#8336](https://github.com/FirebirdSQL/firebird/issues/8336): Error: "Invalid clumplet buffer structure: buffer end before end of clumplet - clumplet too long (77779)" when using trusted auth  
  Contributor(s): Vlad Khorsun

* [#8324](https://github.com/FirebirdSQL/firebird/pull/8324): Make asynchronous replica re-initialization reliable  
  Contributor(s): Dmitry Yemanov

* [#8304](https://github.com/FirebirdSQL/firebird/issues/8304): Wrong results using `MINVALUE/MAXVALUE` in join condition  
  Contributor(s): Adriano dos Santos Fernandes

* [#8283](https://github.com/FirebirdSQL/firebird/issues/8283): Assert in `~thread_db()` due to not released page buffer  
  Contributor(s): Vlad Khorsun

* [#8268](https://github.com/FirebirdSQL/firebird/pull/8268): Fix refetch header data from delta when database in backup lock  
  Contributor(s): Andrey Kravchenko

* [#8262](https://github.com/FirebirdSQL/firebird/pull/8262): Make server to correctly handle case when `accept()` returns both success and data for client  
  Contributor(s): Vlad Khorsun

* [#8255](https://github.com/FirebirdSQL/firebird/pull/8255): Catch possible stack overflow when preparing and compiling user statements  
  Contributor(s): Vlad Khorsun

* [#8253](https://github.com/FirebirdSQL/firebird/issues/8253): Incorrect handling of non-ASCII object names in `CREATE MAPPING` statement  
  Contributor(s): Vlad Khorsun

* [#8243](https://github.com/FirebirdSQL/firebird/pull/8243): Fix a bug where the shutdown handler could be called again  
  Contributor(s): Alexander Zhdanov

* [#8238](https://github.com/FirebirdSQL/firebird/pull/8238): Fix using macro with regex in path parameter in _fbtrace.conf_  
  Contributor(s): Alexander Peshkov, Artyom Ivanov

* [#8237](https://github.com/FirebirdSQL/firebird/issues/8237): Database access error when _nbackup_ is starting  
  Contributor(s): Alexander Peshkov

* [#8236](https://github.com/FirebirdSQL/firebird/issues/8236): Avoid "hangs" in `clock_gettime()` in tomcrypt's PRNG  
  Contributor(s): Alexander Peshkov

* [#8222](https://github.com/FirebirdSQL/firebird/pull/8222): Fix a case of deleted memory modification  
  Contributor(s): Ilya Eremin

* [#8221](https://github.com/FirebirdSQL/firebird/issues/8221): Crash when `MAKE_DBKEY()` is called with 0 or 1 arguments  
  Contributor(s): Vlad Khorsun

* [#8219](https://github.com/FirebirdSQL/firebird/issues/8219): Database creation in 3.0.12, 4.0.5 and 5.0.1 is slower than in previous releases  
  Contributor(s): Adriano dos Santos Fernandes

* [#8215](https://github.com/FirebirdSQL/firebird/issues/8215): Rare sporadic segfaults in test for core-6142 on Windows  
  Contributor(s): Alexander Peshkov

* [#8182](https://github.com/FirebirdSQL/firebird/issues/8182): `IN` predicate incorrectly handles single parenthesized subquery as IN-list, instead of table subquery  
  Contributor(s): Mark Rotteveel

* [#8139](https://github.com/FirebirdSQL/firebird/issues/8139): Conflict resolution code uses constraint name instead of index name  
  Contributor(s): Andrey Kravchenko, Vlad Khorsun

* [#8069](https://github.com/FirebirdSQL/firebird/pull/8069): Add missing synchronization to cached vectors of known pages  
  Contributor(s): Dmitry Yemanov, Vlad Khorsun


# v4.0.5

## Improvements

* [#8181](https://github.com/FirebirdSQL/firebird/pull/8181): Ensure the standalone CS listener on Linux uses the _SO_REUSEADDR_ socket option  
  Contributor(s): Dmitry Yemanov

* [#8165](https://github.com/FirebirdSQL/firebird/pull/8165): Added shutdown handler for _Classic Server_  
  Contributor(s): Alexander Zhdanov

* [#8104](https://github.com/FirebirdSQL/firebird/issues/8104): More efficient evaluation of expressions like `RDB$DB_KEY <= ?` after mass delete   
  Contributor(s): Vlad Khorsun

* [#8066](https://github.com/FirebirdSQL/firebird/issues/8066): Make protocol schemes case-insensitive  
  Contributor(s): Vlad Khorsun

* [#8042](https://github.com/FirebirdSQL/firebird/issues/8042): Improve conflict resolution on replica when table have both primary and unique keys  
  Contributor(s): Vlad Khorsun

* [#8030](https://github.com/FirebirdSQL/firebird/issues/8030): Better cardinality estimation when empty data pages exist  
  Contributor(s): Vlad Khorsun

* [#8010](https://github.com/FirebirdSQL/firebird/issues/8010): Remove `gfix -cache` option  
  Contributor(s): Vlad Khorsun

* [#7978](https://github.com/FirebirdSQL/firebird/issues/7978): Update Windows distributions with _zlib_ version 1.3.1  
  Contributor(s): Vlad Khorsun

* [#7928](https://github.com/FirebirdSQL/firebird/issues/7928):  Make `TempCacheLimit` setting to be per-database, (not per-attachment) for _SuperClassic_  
  Contributor(s): Vlad Khorsun

* [#7854](https://github.com/FirebirdSQL/firebird/issues/7854): Performance issue with time zones  
  Contributor(s): Adriano dos Santos Fernandes, Vlad Khorsun

## Bugfixes

* [#8189](https://github.com/FirebirdSQL/firebird/issues/8189): Slow connection times with a lot of simultaneous connections and active trace session present  
  Contributor(s): Alexander Peshkov, Vlad Khorsun

* [#8180](https://github.com/FirebirdSQL/firebird/issues/8180): Sometimes a system trace session is terminated spontaneously  
  Contributor(s): Artyom Abakumov

* [#8178](https://github.com/FirebirdSQL/firebird/pull/8178): Fix boolean conversion to string inside `DataTypeUtil::makeFromList()`  
  Contributor(s): Dmitry Yemanov

* [#8171](https://github.com/FirebirdSQL/firebird/issues/8171): Trace plugin unloaded if called method is not implemented  
  Contributor(s): Vlad Khorsun

* [#8156](https://github.com/FirebirdSQL/firebird/issues/8156): Can not specify concrete IPv6 address in ES/EDS connection string  
  Contributor(s): Vlad Khorsun

* [#8151](https://github.com/FirebirdSQL/firebird/issues/8151): Deadlock happens when run 'List Trace Sessions' service and there are many active trace sessions  
  Contributor(s): Vlad Khorsun

* [#8150](https://github.com/FirebirdSQL/firebird/issues/8150): Process could attach to the deleted instance of shared memory  
  Contributor(s): Alexander Peshkov, Vlad Khorsun

* [#8149](https://github.com/FirebirdSQL/firebird/issues/8149): The hung or crash could happen when connection fires _TRACE_EVENT_DETACH_ event and new trace session created concurrently  
  Contributor(s): Vlad Khorsun

* [#8138](https://github.com/FirebirdSQL/firebird/issues/8138): Bugcheck when replicator state is changed concurrently  
  Contributor(s): Vlad Khorsun

* [#8120](https://github.com/FirebirdSQL/firebird/issues/8120): `CAST` dies with numeric value is out of range error  
  Contributor(s): Vlad Khorsun

* [#8114](https://github.com/FirebirdSQL/firebird/issues/8114): Segfault in connections pool during server shutdown  
  Contributor(s): Vlad Khorsun

* [#8110](https://github.com/FirebirdSQL/firebird/issues/8110): Firebird crashes on Android API level 34  
  Contributor(s): Vlad Khorsun

* [#8108](https://github.com/FirebirdSQL/firebird/issues/8108): ICU 63.1 suppresses conversion errors  
  Contributor(s): Dmitry Kovalenko

* [#8101](https://github.com/FirebirdSQL/firebird/issues/8101): Firebird crashes if a plugin factory returns _nullptr_ and no error in status  
  Contributor(s): Vlad Khorsun, Dimitry Sibiryakov

* [#8089](https://github.com/FirebirdSQL/firebird/issues/8089): AV when attaching database while low of free memory  
  Contributor(s): Vlad Khorsun

* [#8085](https://github.com/FirebirdSQL/firebird/issues/8085): Memory leak when executing a lot of different queries and `StatementTimeout > 0`  
  Contributor(s): Vlad Khorsun

* [#8083](https://github.com/FirebirdSQL/firebird/issues/8083): AV when writting into internal trace log  
  Contributor(s): Vlad Khorsun

* [#8079](https://github.com/FirebirdSQL/firebird/issues/8079): Engine could crash when executing some trigger(s) while another attachment modifies them  
  Contributor(s): Vlad Khorsun

* [#8077](https://github.com/FirebirdSQL/firebird/issues/8077): Error "Too many recursion levels" does not stop execution of code that uses `ON DISCONNECT` trigger  
  Contributor(s): Alexander Peshkov, Vlad Khorsun

* [#8058](https://github.com/FirebirdSQL/firebird/issues/8058): Replicated DDL changes do not set the correct grantor  
  Contributor(s): Dmitry Yemanov

* [#8056](https://github.com/FirebirdSQL/firebird/issues/8056): Error "Too many temporary blobs" with blob_append when select a stored procedue using rows-clause  
  Contributor(s): Vlad Khorsun

* [#8040](https://github.com/FirebirdSQL/firebird/issues/8040): Bugcheck 183 (wrong record length) could happen on replica database after UK violation on insert   
  Contributor(s): Vlad Khorsun

* [#8039](https://github.com/FirebirdSQL/firebird/issues/8039): Segfault when opening damaged (last TIP is missing in `RDB$PAGES`, user's FW was OFF) database  
  Contributor(s): Alexander Peshkov

* [#8027](https://github.com/FirebirdSQL/firebird/issues/8027): Broken _gbak_ statistics  
  Contributor(s): Alexander Peshkov

* [#8011](https://github.com/FirebirdSQL/firebird/issues/8011): DECFLOAT error working with `INT128` in UDR  
  Contributor(s): Alexander Peshkov

* [#8006](https://github.com/FirebirdSQL/firebird/issues/8006): Int128 datatype not supported in FB_MESSAGE macro   
  Contributor(s): Alexander Peshkov

* [#8003](https://github.com/FirebirdSQL/firebird/issues/8003): _gbak_ v4 can't backup database in _ODS < 13_  
  Contributor(s): Vlad Khorsun

* [#7997](https://github.com/FirebirdSQL/firebird/issues/7997): Unexpected results when comparing integer with string containing value out of range of that integer datatype  
  Contributor(s): Alexander Peshkov

* [#7993](https://github.com/FirebirdSQL/firebird/issues/7993): Unexpected results when using `CASE WHEN` with `RIGHT JOIN`  
  Contributor(s): Dmitry Yemanov

* [#7985](https://github.com/FirebirdSQL/firebird/issues/7985): Hang in case of error when sweep thread is attaching to database (_Classic Server_)
  Contributor(s): Alexander Peshkov

* [#7979](https://github.com/FirebirdSQL/firebird/issues/7979): Hang when database with disconnect trigger using `MON$` tables is shutting down  
  Contributor(s): Alexander Peshkov

* [#7969](https://github.com/FirebirdSQL/firebird/issues/7969): Characters are garbled when replicating fields with type `BLOB SUB_TYPE TEXT` if the character set of the connection and the field are different  
  Contributor(s): Dmitry Yemanov

* [#7950](https://github.com/FirebirdSQL/firebird/issues/7950): Unable to restore database when .fbk was created on host with other ICU  
  Contributor(s): Alexander Peshkov

* [#7927](https://github.com/FirebirdSQL/firebird/issues/7927): Some default values is set incorrectly for SC/CS architectures  
  Contributor(s): Vlad Khorsun

* [#7917](https://github.com/FirebirdSQL/firebird/issues/7917): Hang in case of error when sweep thread is attaching to database  
  Contributor(s): Alexander Peshkov

* [#7905](https://github.com/FirebirdSQL/firebird/issues/7905): Segfault during TPC initialization  
  Contributor(s): Alexander Peshkov

* [#7899](https://github.com/FirebirdSQL/firebird/issues/7899): Inconsistent state of master-detail occurs after RE-connect + 'SET AUTODDL OFF' + 'drop <FK>' which is ROLLED BACK  
  Contributor(s): Vlad Khorsun

* [#7896](https://github.com/FirebirdSQL/firebird/issues/7896): _replication.log_ remains empty (and without any error in firebird.log) until concurrent FB instance is running under different account and generates segments on its master. Significant delay required after stop concurrent FB it in order allow first one to write in its replication log.  
  Contributor(s): Vlad Khorsun

* [#7885](https://github.com/FirebirdSQL/firebird/issues/7885): Unstable error messages in services due to races related with service status vector  
  Contributor(s): Alexander Peshkov

* [#7873](https://github.com/FirebirdSQL/firebird/issues/7873): Wrong memory buffer alignment and I/O buffer size when working in direct I/O mode  
  Contributor(s): Vlad Khorsun

* [#7869](https://github.com/FirebirdSQL/firebird/issues/7869): _gbak_ can write uninitialized data into `RDB$RETURN_ARGUMENT` and `RDB$ARGUMENT_POSITION` fields  
  Contributor(s): Dmitry Kovalenko

* [#7867](https://github.com/FirebirdSQL/firebird/issues/7867): Error "wrong page type" during garbage collection on v4.0.4  
  Contributor(s): Ilya Eremin

* [#7860](https://github.com/FirebirdSQL/firebird/issues/7860): Crash potentially caused by `BETWEEN` operator  
  Contributor(s): Vlad Khorsun

* [#7851](https://github.com/FirebirdSQL/firebird/issues/7851): The skip of `att_functionarg_field_precision` does not check `RESTORE_format`  
  Contributor(s): Dmitry Kovalenko

* [#7846](https://github.com/FirebirdSQL/firebird/issues/7846): FB4 can't backup/restore `INT128`-array  
  Contributor(s): Dmitry Kovalenko

* [#7844](https://github.com/FirebirdSQL/firebird/issues/7844): Removing first column with `SET WIDTH` crashes _isql_  
  Contributor(s): Adriano dos Santos Fernandes

* [#7839](https://github.com/FirebirdSQL/firebird/issues/7839): Potential bug in `BETWEEN` operator  
  Contributor(s): Vlad Khorsun

* [#7831](https://github.com/FirebirdSQL/firebird/issues/7831): Incorrect type of UDF-argument with array  
  Contributor(s): Dmitry Kovalenko

* [#7827](https://github.com/FirebirdSQL/firebird/issues/7827): Problem using Python _firebird-driver_ with either Intel or M1 Mac builds with version 4.0.3 or 5.0+  
  Contributor(s): Adriano dos Santos Fernandes

* [#7809](https://github.com/FirebirdSQL/firebird/issues/7809): Crash "Fatal lock manager error: Process disappeared in LockManager::acquire_shmem"  
  Contributor(s): Alexander Peshkov

* [#7800](https://github.com/FirebirdSQL/firebird/issues/7800): Default publication status is not preserved after backup/restore  
  Contributor(s): Dmitry Yemanov

* [#7461](https://github.com/FirebirdSQL/firebird/issues/7461): Differences in field metadata descriptions between Firebird 2.5 and Firebird 4  
  Contributor(s): Dmitry Yemanov


# v4.0.4

## Improvements

* [#7818](https://github.com/FirebirdSQL/firebird/issues/7818): Extend `RDB$GET_CONTEXT('SYSTEM', '***')` with other info from `MON$ATTACHMENTS`  
  Contributor(s): Vlad Khorsun

* [#7755](https://github.com/FirebirdSQL/firebird/issues/7755): Update Windows distribution with new _zlib_ version 1.3 (released 2023-08-18)  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#7817](https://github.com/FirebirdSQL/firebird/issues/7817): Memory leak is possible for UDF array arguments  
  Contributor(s): Dmitry Yemanov

* [#7812](https://github.com/FirebirdSQL/firebird/issues/7812): Service backup does not work in multiple engines configuration  
  Contributor(s): Alexander Peshkov

* [#7779](https://github.com/FirebirdSQL/firebird/issues/7779): Firebird 4.0.3 is constantly crashing with the same symptoms (fbclient.dll) 
  Contributor(s): Vlad Khorsun

* [#7772](https://github.com/FirebirdSQL/firebird/issues/7772): Blob corruption in FB4.0.3 (embedded)  
  Contributor(s): Vlad Khorsun

* [#7770](https://github.com/FirebirdSQL/firebird/issues/7770): Restore takes 25% more time vs 4.0.0  
  Contributor(s): Vlad Khorsun

* [#7766](https://github.com/FirebirdSQL/firebird/issues/7766): Firebird 4 Windows Installer omits DLLs during custom installation mode  
  Contributor(s): Paul Reeves

* [#7762](https://github.com/FirebirdSQL/firebird/issues/7762): Crash on "Operating system call pthread_mutex_destroy failed. Error code 16" in log  
  Contributor(s): Alexander Peshkov

* [#7761](https://github.com/FirebirdSQL/firebird/issues/7761): Regression when displaying line number of errors in _ISQL_ scripts  
  Contributor(s): Adriano dos Santos Fernandes

* [#7759](https://github.com/FirebirdSQL/firebird/issues/7759): Routine calling overhead increased by factor 6 vs Firebird 4.0.0  
  Contributor(s): Adriano dos Santos Fernandes

* [#7747](https://github.com/FirebirdSQL/firebird/pull/7747): Fix an issue where the garbage collection in indexes and blobs is not performed in _VIO_backout_  
  Contributor(s): Ilya Eremin

* [#7745](https://github.com/FirebirdSQL/firebird/issues/7745): Error restoring database which has system domains in user table with BLOBs using embedded connection  
  Contributor(s): Alexander Peshkov

* [#7738](https://github.com/FirebirdSQL/firebird/issues/7738): Crash on multiple connections/disconnections  
  Contributor(s): Alexander Peshkov

* [#7737](https://github.com/FirebirdSQL/firebird/pull/7737): Fix cases where the precedence relationship between a record page and a blob page is not set  
  Contributor(s): Ilya Eremin

* [#7731](https://github.com/FirebirdSQL/firebird/issues/7731): Display length of `TIMESTAMP WITH TIME ZONE` is wrong in dialect 1  
  Contributor(s): Alexander Peshkov

* [#7730](https://github.com/FirebirdSQL/firebird/issues/7730): Server ignores the size of `VARCHAR` when performing `SET BIND ... TO VARCHAR(N)`  
  Contributor(s): Alexander Peshkov

* [#7729](https://github.com/FirebirdSQL/firebird/issues/7729): `SET BIND OF TS WITH TZ TO VARCHAR(128)` uses the date format of dialect 1  
  Contributor(s): Alexander Peshkov

* [#7727](https://github.com/FirebirdSQL/firebird/issues/7727): Index for integer column cannot be used when `INT128/DECFLOAT` value is being searched  
  Contributor(s): Dmitry Yemanov

* [#7723](https://github.com/FirebirdSQL/firebird/issues/7723): Wrong error message on login if the user doesn't exist and _WireCrypt_ is disabled  
  Contributor(s): Alexander Peshkov

* [#7713](https://github.com/FirebirdSQL/firebird/issues/7713): `FOR SELECT` statement can not see any changes made inside the `DO` block  
  Contributor(s): Vlad Khorsun

* [#7691](https://github.com/FirebirdSQL/firebird/issues/7691): `WITH CALLER PRIVILEGES` has no effect in triggers   
  Contributor(s): Alexander Peshkov

* [#7480](https://github.com/FirebirdSQL/firebird/issues/7480): Firebird server stops accepting new connections after some time  
  Contributor(s): Alexander Peshkov


# v4.0.3

## Improvements

* [#7542](https://github.com/FirebirdSQL/firebird/issues/7542): Compiler warnings raise when build cloop-generated _Firebird.pas_ in RAD Studio 11.3  
  Contributor(s): Vlad Khorsun

* [#7539](https://github.com/FirebirdSQL/firebird/issues/7539): `RDB$GET/SET_CONTEXT()`: enclosing in apostrophes or double quotes of a missed namespace/variable will made output more readable  
  Contributor(s): Vlad Khorsun

* [#7494](https://github.com/FirebirdSQL/firebird/issues/7494): Avoid non necessary index reads  
  Contributor(s): Vlad Khorsun

* [#7468](https://github.com/FirebirdSQL/firebird/issues/7468): Add switch to control in guardian timeout before killing firebird server process  
  Contributor(s): Alex Peshkoff

* [#7437](https://github.com/FirebirdSQL/firebird/issues/7437): Updated _zlib_ to version 1.2.13 (released 2022-10-13)  
  Contributor(s): Vlad Khorsun

* [#7425](https://github.com/FirebirdSQL/firebird/issues/7425): Add REPLICA MODE to the output of the _isql_ `SHOW DATABASE` command  
  Contributor(s): Dmitry Yemanov

* [#7418](https://github.com/FirebirdSQL/firebird/issues/7418): Improve reliability of plugin manager  
  Contributor(s): Alex Peshkoff

* [#7294](https://github.com/FirebirdSQL/firebird/issues/7294): Allow FB-known macros in replication.conf  
  Contributor(s): Dmitry Yemanov

* [#7259](https://github.com/FirebirdSQL/firebird/issues/7259): Remove TcpLoopbackFastPath and use of SIO_LOOPBACK_FAST_PATH  
  Contributor(s): Vlad Khorsun

* [#7186](https://github.com/FirebirdSQL/firebird/issues/7186): _Nbackup_ `RDB$BACKUP_HISTORY` cleanup  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#7683](https://github.com/FirebirdSQL/firebird/issues/7683): `rdb$time_zone_util.transitions` returns an infinite result set  
  Contributor(s): Adriano dos Santos Fernandes

* [#7670](https://github.com/FirebirdSQL/firebird/issues/7670): Cursor name can duplicate parameter and variable names in procedures and functions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7665](https://github.com/FirebirdSQL/firebird/issues/7665): Wrong result ordering in `LEFT JOIN` query  
  Contributor(s): Dmitry Yemanov

* [#7664](https://github.com/FirebirdSQL/firebird/issues/7664): `DROP TABLE` executed for a table with big records may lead to "wrong page type" or "end of file" error  
  Contributor(s): Ilya Eremin

* [#7662](https://github.com/FirebirdSQL/firebird/pull/7662): Peformance issues in prepare_update()  
  Contributor(s): Ilya Eremin

* [#7661](https://github.com/FirebirdSQL/firebird/issues/7661): FB CS rejects new connections  
  Contributor(s): Vlad Khorsun

* [#7651](https://github.com/FirebirdSQL/firebird/issues/7651): Unable to find savepoint in insert with nested query and returning clause in FB4  
  Contributor(s): Dmitry Yemanov

* [#7638](https://github.com/FirebirdSQL/firebird/issues/7638): `OVERRIDING USER VALUE` should be allowed for `GENERATED ALWAYS AS IDENTITY`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7627](https://github.com/FirebirdSQL/firebird/issues/7627): The size of the database with big records becomes bigger after backup/restore  
  Contributor(s): Ilya Eremin

* [#7626](https://github.com/FirebirdSQL/firebird/issues/7626): Segfault when new attachment is done to shutting down database  
  Contributor(s): Alex Peshkoff

* [#7611](https://github.com/FirebirdSQL/firebird/issues/7611): Can't backup/restore database from v3 to v4 with `SEC$USER_NAME` field longer than 10 characters  
  Contributor(s): Adriano dos Santos Fernandes

* [#7610](https://github.com/FirebirdSQL/firebird/issues/7610): Uninitialized/random value assigned to `RDB$ROLES.RDB$SYSTEM PRIVILEGES` when restoring from FB3 backup  
  Contributor(s): Adriano dos Santos Fernandes

* [#7605](https://github.com/FirebirdSQL/firebird/issues/7605): Disallow replication of `RDB$BACKUP_HISTORY`  
  Contributor(s): Dmitry Yemanov

* [#7604](https://github.com/FirebirdSQL/firebird/issues/7604): PSQL functions do not convert the output BLOB to the connection character set  
  Contributor(s): Adriano dos Santos Fernandes

* [#7603](https://github.com/FirebirdSQL/firebird/issues/7603): `BIN_SHR` on `INT128` does not apply sign extension  
  Contributor(s): Alex Peshkoff

* [#7599](https://github.com/FirebirdSQL/firebird/issues/7599): Conversion of text with '\0' to `DECFLOAT` without errors  
  Contributor(s): Alex Peshkoff

* [#7591](https://github.com/FirebirdSQL/firebird/issues/7591): `RELEASE SAVEPOINT ONLY` works incorrectly  
  Contributor(s): Dmitry Yemanov

* [#7582](https://github.com/FirebirdSQL/firebird/issues/7582): Missing _isc_info_end_ in _Firebird.pas_  
  Contributor(s): Alex Peshkoff

* [#7579](https://github.com/FirebirdSQL/firebird/issues/7579): Cannot _nbackup_ a Firebird 3.0 database in Firebird 4.0 service with _engine12_ setup in _Providers_  
  Contributor(s): Alex Peshkoff

* [#7556](https://github.com/FirebirdSQL/firebird/issues/7556): FB Classic can hang attempting to attach DB while it is starting to encrypt/decrypt  
  Contributor(s): Alex Peshkoff

* [#7555](https://github.com/FirebirdSQL/firebird/issues/7555): Invalid configuration for random fresh created database may be used after drop of another one with alias in _databases.conf_  
  Contributor(s): Alex Peshkoff

* [#7548](https://github.com/FirebirdSQL/firebird/issues/7548): `SET BIND OF TIMESTAMP WITH TIME ZONE TO CHAR` is not working with UTF8 connection charset  
  Contributor(s): Adriano dos Santos Fernandes

* [#7537](https://github.com/FirebirdSQL/firebird/issues/7537): Wrong name in error message when unknown namespace is passed into `RDB$SET_CONTEXT`  
  Contributor(s): Vlad Khorsun

* [#7535](https://github.com/FirebirdSQL/firebird/issues/7535): High CPU usage connect to Firebird 3 database using Firebird 4 Classic and SuperClassic service  
  Contributor(s): Vlad Khorsun

* [#7517](https://github.com/FirebirdSQL/firebird/issues/7517): Successful compiling of procedure with wrong PLAN(s) used by some of its statement(s)  
  Contributor(s): Dmitry Yemanov

* [#7514](https://github.com/FirebirdSQL/firebird/issues/7514): Segfault when detaching after deleting shadow on Classic  
  Contributor(s): Alex Peshkoff

* [#7510](https://github.com/FirebirdSQL/firebird/issues/7510): Firebird regularly crashes soon after unload of _udr_engine_ plugin  
  Contributor(s): Alex Peshkoff

* [#7501](https://github.com/FirebirdSQL/firebird/issues/7501): Precision of a standalone unit may differ from a packaged one in SQL dialect 1  
  Contributor(s): Vlad Khorsun

* [#7499](https://github.com/FirebirdSQL/firebird/issues/7499): Problem with restore (error: index cannot be used in the specified plan)  
  Contributor(s): Vlad Khorsun

* [#7488](https://github.com/FirebirdSQL/firebird/issues/7488): Invalid real to string cast  
  Contributor(s): Artyom Abakumov, Alex Peshkoff

* [#7484](https://github.com/FirebirdSQL/firebird/issues/7484): External engine `SYSTEM` not found  
  Contributor(s): Adriano dos Santos Fernandes

* [#7482](https://github.com/FirebirdSQL/firebird/issues/7482): Result of `blob_append(null, null) (literal '<null>')` is not shown  
  Contributor(s): Vlad Khorsun, Adriano dos Santos Fernandes

* [#7473](https://github.com/FirebirdSQL/firebird/issues/7473): Client application crash when processing callback requests from server during _attachDatabase_  
  Contributor(s): Alex Peshkoff

* [#7472](https://github.com/FirebirdSQL/firebird/issues/7472): Window functions may lead to crash interacting with others exceptions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7467](https://github.com/FirebirdSQL/firebird/issues/7467): Simple SQL crashes Firebird: `select cast(rdb$db_key as integer) from rdb$database`  
  Contributor(s): Alex Peshkoff

* [#7465](https://github.com/FirebirdSQL/firebird/issues/7465): Restore success illegally reported when _gbak_ was unable to activate all indices  
  Contributor(s): Alex Peshkoff

* [#7456](https://github.com/FirebirdSQL/firebird/issues/7456): Impossible drop function in package with name of PSQL-function  
  Contributor(s): Adriano dos Santos Fernandes

* [#7446](https://github.com/FirebirdSQL/firebird/issues/7446): Attempt to use data in destroyed transaction pool  
  Contributor(s): Alex Peshkoff

* [#7444](https://github.com/FirebirdSQL/firebird/issues/7444): _isql_ crashes while executing test from QA suite  
  Contributor(s): Alex Peshkoff

* [#7415](https://github.com/FirebirdSQL/firebird/issues/7415): _DbCrypt/KeyHolder_ plugins key changing issues on running server  
  Contributor(s): Alexey Mochalov

* [#7402](https://github.com/FirebirdSQL/firebird/issues/7402): Server crashes on startup error  
  Contributor(s): Alex Peshkoff

* [#7398](https://github.com/FirebirdSQL/firebird/issues/7398): Worst plan sort created to execute an indexed tables  
  Contributor(s): Dmitry Yemanov

* [#7393](https://github.com/FirebirdSQL/firebird/issues/7393): Access violation after double fault in _attachDatabase()_  
  Contributor(s): Alex Peshkoff

* [#7387](https://github.com/FirebirdSQL/firebird/issues/7387): Unreliable replication behaviour in Linux Classic  
  Contributor(s): Dmitry Yemanov

* [#7380](https://github.com/FirebirdSQL/firebird/issues/7380): Aliased blob variable with less restrictions makes text blob accept malformed string through `BLOB_APPEND`  
  Contributor(s): Vlad Khorsun

* [#7379](https://github.com/FirebirdSQL/firebird/issues/7379): `BLOB_APPEND` with existing blob accepts malformed string  
  Contributor(s): Vlad Khorsun, Adriano dos Santos Fernandes

* [#7371](https://github.com/FirebirdSQL/firebird/issues/7371): Various errors (fatal lock manager error, pthread_mutex_destroy failed) caused by races when opening/closing database  
  Contributor(s): Alex Peshkoff

* [#7370](https://github.com/FirebirdSQL/firebird/issues/7370): Segfault under OOM conditions  
  Contributor(s): Alex Peshkoff

* [#7369](https://github.com/FirebirdSQL/firebird/issues/7369): Build fails against _re2_ 20220601  
  Contributor(s): Adriano dos Santos Fernandes

* [#7365](https://github.com/FirebirdSQL/firebird/issues/7365): Client side aliases do not work in _databases.conf_  
  Contributor(s): Alex Peshkoff

* [#7361](https://github.com/FirebirdSQL/firebird/issues/7361): Broken compacting of trace config storage  
  Contributor(s): Alex Peshkoff

* [#7359](https://github.com/FirebirdSQL/firebird/issues/7359): Querying to list the running trace sessions can fail if two service API calls was done before it for a short time (Linux specifics)  
  Contributor(s): Alex Peshkoff

* [#7357](https://github.com/FirebirdSQL/firebird/issues/7357): Lock manager error when connect a Firebird 3.0 database more than once using SuperClassic Firebird 4.0 service  
  Contributor(s): Alex Peshkoff

* [#7349](https://github.com/FirebirdSQL/firebird/issues/7349): Contradictory licensing/distribution statements in several charset-support files  
  Contributor(s): Mark Rotteveel

* [#7314](https://github.com/FirebirdSQL/firebird/issues/7314): Multitreaded activating indices restarts server process  
  Contributor(s): Vlad Khorsun

* [#7298](https://github.com/FirebirdSQL/firebird/issues/7298): Unreliable info result parsing  
  Contributor(s): Alex Peshkoff

* [#7296](https://github.com/FirebirdSQL/firebird/issues/7296): During shutdown _op_disconnect_ may be sent to invalid handle  
  Contributor(s): Alex Peshkoff

* [#7295](https://github.com/FirebirdSQL/firebird/issues/7295): Unexpected message 'Error reading data from the connection' when _fbtracemgr_ is closed using Ctrl-C  
  Contributor(s): Alex Peshkoff

* [#7283](https://github.com/FirebirdSQL/firebird/issues/7283): Suspicious error message during install  
  Contributor(s): Alex Peshkoff

* [#7276](https://github.com/FirebirdSQL/firebird/issues/7276): Firebird 4 literal with `CONTAINING` crashes server  
  Contributor(s): Vlad Khorsun

* [#7271](https://github.com/FirebirdSQL/firebird/issues/7271): Sporadic server crash  
  Contributor(s): Vlad Khorsun

* [#7262](https://github.com/FirebirdSQL/firebird/issues/7262): Repeated _op_batch_create_ leaks the batch  
  Contributor(s): Alex Peshkoff

* [#7256](https://github.com/FirebirdSQL/firebird/issues/7256): Inconsistent conversion of non-TEXT blobs in `BLOB_APPEND`  
  Contributor(s): Vlad Khorsun

* [#7255](https://github.com/FirebirdSQL/firebird/issues/7255): `READ COMMITTED READ CONSISTENCY` mode is broken in Classic / SuperClassic on Linux  
  Contributor(s): Alex Peshkoff

* [#7233](https://github.com/FirebirdSQL/firebird/pull/7233): Slow database restore when Classic server mode is used  
  Contributor(s): Ilya Eremin

* [#7241](https://github.com/FirebirdSQL/firebird/issues/7241): MacOS installer - firebird user is not created correctly if group already exists  
  Contributor(s): Jonathan Frutos

* [#7239](https://github.com/FirebirdSQL/firebird/issues/7239): Connect using XNET protocol shows different exception (comparing to INET) if database is in the shutdown state  
  Contributor(s): Vlad Khorsun

* [#6941](https://github.com/FirebirdSQL/firebird/issues/6941): Dummy (always true) conditions may change the join order  
  Contributor(s): Dmitry Yemanov

* [#4729](https://github.com/FirebirdSQL/firebird/issues/4729): `GRANT` and `REVOKE UPDATE` (field)  
  Contributor(s): Alex Peshkoff


# v4.0.2

## New features

* [#6983](https://github.com/FirebirdSQL/firebird/pull/6983): New built-in function BLOB_APPEND  
  Reference(s): [/doc/sql.extensions/README.blob_append.md](https://github.com/FirebirdSQL/firebird/raw/v4.0-release/doc/sql.extensions/README.blob_append.md)  
  Contributor(s): Vlad Khorsun

## Improvements

* [#7208](https://github.com/FirebirdSQL/firebird/issues/7208): Trace: provide performance statistics for DDL statement  
  Contributor(s): Vlad Khorsun
  
* [#7194](https://github.com/FirebirdSQL/firebird/issues/7194): Make it possible to avoid _fbclient_ dependency in Pascal programs using _firebird.pas_  
  Contributor(s): Alex Peshkoff

* [#7168](https://github.com/FirebirdSQL/firebird/issues/7168): Ignore missing UDR libraries during restore  
  Contributor(s): Adriano dos Santos Fernandes

* [#7161](https://github.com/FirebirdSQL/firebird/issues/7161): Update _zlib_ to version 1.2.12  
  Contributor(s): Vlad Khorsun

* [#7093](https://github.com/FirebirdSQL/firebird/issues/7093): Improve indexed lookup speed of strings when the last keys characters are part of collated contractions  
  Contributor(s): Adriano dos Santos Fernandes

* [#7092](https://github.com/FirebirdSQL/firebird/issues/7092): Improve performance of `CURRENT_TIME`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7042](https://github.com/FirebirdSQL/firebird/issues/7042): `ON DISCONNECT` triggers are not executed during forced attachment shutdown  
  Contributor(s): Ilya Eremin

* [#7041](https://github.com/FirebirdSQL/firebird/issues/7041): Firebird port for Apple M1 architecture  
  Contributor(s): Jonathan Frutos, Alex Peshkoff, Adriano dos Santos Fernandes

* [#7038](https://github.com/FirebirdSQL/firebird/issues/7038): Improve performance of `STARTING WITH` with insensitive collations  
  Contributor(s): Adriano dos Santos Fernandes

* [#6730](https://github.com/FirebirdSQL/firebird/issues/6730): Trace: provide ability to see `STATEMENT RESTART` events (or their count)  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#7243](https://github.com/FirebirdSQL/firebird/issues/7243): Some _UNICODE_ characters can lead to wrong `CONTAINING` evaluation when lower/upper uses different number of bytes in its encoding  
  Contributor(s): Adriano dos Santos Fernandes

* [#7229](https://github.com/FirebirdSQL/firebird/issues/7229): `ALTER COLUMN DROP IDENTITY` does not reset generator name in metadata cache  
  Contributor(s): Adriano dos Santos Fernandes

* [#7222](https://github.com/FirebirdSQL/firebird/issues/7222): Dependencies of packaged functions are not tracked  
  Contributor(s): Adriano dos Santos Fernandes

* [#7217](https://github.com/FirebirdSQL/firebird/pull/7217): It's not allowed to execute DROP PACKAGE BODY for a package with a procedure even if a user has DROP ANY PACKAGE privilege  
  Contributor(s): Ilya Eremin

* [#7204](https://github.com/FirebirdSQL/firebird/issues/7204): Segfault in _GBAK_ when restoring a broken backup file over the wire  
  Contributor(s): Alex Peshkoff

* [#7202](https://github.com/FirebirdSQL/firebird/issues/7202): _ISQL -ch utf8_ (Windows only): either silently quits to OS or issues non-expected 'malformed string' when non-ASCII character occurs in the typed command  
  Contributor(s): Adriano dos Santos Fernandes

* [#7200](https://github.com/FirebirdSQL/firebird/issues/7200): `DROP DATABASE` leads to hang if it is issued while database encrypting/decrypting is in progress  
  Contributor(s): Alex Peshkoff

* [#7199](https://github.com/FirebirdSQL/firebird/issues/7199): Various errors (strange messages in firebird.log, segfaults) with high rate of attach/detach database operations  
  Contributor(s): Alex Peshkoff

* [#7197](https://github.com/FirebirdSQL/firebird/issues/7197): Segfault in Linux CS after successful detach from database  
  Contributor(s): Alex Peshkoff

* [#7194](https://github.com/FirebirdSQL/firebird/issues/7194): _GSTAT_ fails but returns 0 as error code if incorrect _databases.conf_ is used  
  Contributor(s): Alexey Mochalov

* [#7188](https://github.com/FirebirdSQL/firebird/issues/7188): Memory leak in _fbclient_ when a multi-database transaction is used  
  Contributor(s): Alex Peshkoff

* [#7184](https://github.com/FirebirdSQL/firebird/issues/7184): _GBAK_ output is not being flushed to disk  
  Contributor(s): Alex Peshkoff

* [#7179](https://github.com/FirebirdSQL/firebird/issues/7179): Wrong error message - "string right truncation. expected length 30, actual 30."  
  Contributor(s): Adriano dos Santos Fernandes

* [#7178](https://github.com/FirebirdSQL/firebird/issues/7178): DEFAULTed grants to PUBLIC must act as DEFAULTed to every user  
  Contributor(s): Roman Simakov

* [#7176](https://github.com/FirebirdSQL/firebird/issues/7176): Incorrect error "Invalid token. Malformed string." with UNION + blob + non-UTF8 varchar  
  Contributor(s): Adriano dos Santos Fernandes

* [#7167](https://github.com/FirebirdSQL/firebird/issues/7167): Incorrect transliteration of field names in constraint violation errors  
  Contributor(s): Adriano dos Santos Fernandes

* [#7160](https://github.com/FirebirdSQL/firebird/issues/7160): Missing checkout in the trace manager when performing user mapping may cause server hang  
  Contributor(s): Alex Peshkoff

* [#7150](https://github.com/FirebirdSQL/firebird/issues/7150): Replication not restarting after network failure  
  Contributor(s): Dmitry Yemanov

* [#7147](https://github.com/FirebirdSQL/firebird/issues/7147): Problems with use of big timeout (or no timeout at all) in the trace service  
  Contributor(s): Alex Peshkoff

* [#7141](https://github.com/FirebirdSQL/firebird/issues/7141): Services manager breaks long lines into 1023 bytes portions when using _isc_info_svc_line_ in _Service::query()_  
  Contributor(s): Adriano dos Santos Fernandes

* [#7140](https://github.com/FirebirdSQL/firebird/issues/7140): Wrong select result in case of special sort character  
  Contributor(s): Adriano dos Santos Fernandes

* [#7139](https://github.com/FirebirdSQL/firebird/issues/7139): With multiple trace sessions user may receive trace events related to engine's requests  
  Contributor(s): Alex Peshkoff

* [#7138](https://github.com/FirebirdSQL/firebird/issues/7138): Problems accessing FB4 database, copied from another host  
  Contributor(s): Alex Peshkoff

* [#7135](https://github.com/FirebirdSQL/firebird/issues/7135): Firebird engine randomly fails when delivering mapping clear signal to other processes  
  Contributor(s): Alex Peshkoff

* [#7134](https://github.com/FirebirdSQL/firebird/issues/7134): Database page errors directly after _GBAK_, dissappearing after some calls of _GFIX_  
  Contributor(s): Vlad Khorsun

* [#7129](https://github.com/FirebirdSQL/firebird/issues/7129): Cannot alter SQL SECURITY on package  
  Contributor(s): Alexey Mochalov

* [#7128](https://github.com/FirebirdSQL/firebird/issues/7128): Incorrect error message with _isc_sql_interprete()_  
  Contributor(s): Vlad Khorsun

* [#7124](https://github.com/FirebirdSQL/firebird/issues/7124): Inconsistent _RDB$USER_PRIVILEGES_ after dropping identity  
  Contributor(s): Adriano dos Santos Fernandes

* [#7123](https://github.com/FirebirdSQL/firebird/issues/7123): ISQL does not extract `INCREMENT BY` for IDENTITY column  
  Contributor(s): Adriano dos Santos Fernandes

* [#7122](https://github.com/FirebirdSQL/firebird/issues/7122): Invalid state of mapping cache after replacement of database  
  Contributor(s): Alex Peshkoff

* [#7121](https://github.com/FirebirdSQL/firebird/issues/7121): Mapping error when server tries to use mapping rules from database in full shutdown mode  
  Contributor(s): Alex Peshkoff

* [#7119](https://github.com/FirebirdSQL/firebird/issues/7119): Database statistics service could not find existing table(s)  
  Contributor(s): Vlad Khorsun

* [#7118](https://github.com/FirebirdSQL/firebird/issues/7118): Chained `JOIN .. USING` across the same column names may be optimized badly  
  Contributor(s): Dmitry Yemanov

* [#7113](https://github.com/FirebirdSQL/firebird/issues/7113): Wrong path in Object Pascal's readme.md  
  Contributor(s): Alex Peshkoff

* [#7112](https://github.com/FirebirdSQL/firebird/issues/7112): Avoid unload of plugins in MacOS due to problematic reload of them  
  Contributor(s): Adriano dos Santos Fernandes

* [#7108](https://github.com/FirebirdSQL/firebird/issues/7108): Firebird does not find an record when adding a foreign key  
  Contributor(s): Adriano dos Santos Fernandes

* [#7106](https://github.com/FirebirdSQL/firebird/issues/7106): Wrong detection of must-be-delimited user names  
  Contributor(s): Alex Peshkoff

* [#7103](https://github.com/FirebirdSQL/firebird/issues/7103): FB service hangs after several `DELETE FROM MON$STATEMENTS` being issued in order to stop ES/EDS which waits record for updating  
  Contributor(s): Vlad Khorsun

* [#7099](https://github.com/FirebirdSQL/firebird/issues/7099): Incomplete _op_batch_cs_ response with _TAG_MULTIERROR_  
  Contributor(s): Alex Peshkoff

* [#7096](https://github.com/FirebirdSQL/firebird/issues/7096): Client install on Windows is missing some files  
  Contributor(s): Paul Reeves

* [#7094](https://github.com/FirebirdSQL/firebird/issues/7094): Incorrect indexed lookup of strings when the last keys characters are part of collated contractions and there is multi-segment insensitive descending index  
  Contributor(s): Adriano dos Santos Fernandes

* [#7090](https://github.com/FirebirdSQL/firebird/issues/7090): Performance degradation with `CURRENT_DATE`, `LOCALTIME` and `LOCALTIMESTAMP`  
  Contributor(s): Adriano dos Santos Fernandes

* [#7088](https://github.com/FirebirdSQL/firebird/pull/7088): MacOS UDR and Legacy_UserManager plugins not working due to not exported entry point  
  Contributor(s): Adriano dos Santos Fernandes

* [#7084](https://github.com/FirebirdSQL/firebird/issues/7084): Creating unique constraints on MacOS fails on larger tables  
  Contributor(s): Adriano dos Santos Fernandes

* [#7080](https://github.com/FirebirdSQL/firebird/issues/7080): Executing batch crashes the server  
  Contributor(s): Alex Peshkoff

* [#6947](https://github.com/FirebirdSQL/firebird/issues/6947): Query to MON$ tables does not return data when database encryption/decryption is in progress  
  Contributor(s): Alex Peshkoff

* [#4085](https://github.com/FirebirdSQL/firebird/issues/4085): _RDB$INDICES_ information stored inconsistently after a `CREATE INDEX`  
  Contributor(s): Dmitry Yemanov

* [#3357](https://github.com/FirebirdSQL/firebird/issues/3357): Bad execution plan if some stream depends on multiple streams via a function  
  Contributor(s): Dmitry Yemanov


# v4.0.1

## New features

* [#6910](https://github.com/FirebirdSQL/firebird/issues/6910): Add way to retrieve statement BLR with Statement::getInfo and ISQL's SET EXEC_PATH_DISPLAY BLR  
  Contributor(s): Adriano dos Santos Fernandes

## Improvements

* [#6959](https://github.com/FirebirdSQL/firebird/issues/6959): Add IBatch::getInfo() method to the API  
  Contributor(s): Alex Peshkoff

* [#6954](https://github.com/FirebirdSQL/firebird/issues/6954): fb_info_protocol_version support  
  Contributor(s): Alex Peshkoff

* [#6929](https://github.com/FirebirdSQL/firebird/issues/6929): Add support of PKCS v.1.5 padding to RSA functions, needed for backward compatibility with old systems  
  Contributor(s): Alex Peshkoff

* [#6915](https://github.com/FirebirdSQL/firebird/issues/6915): Allow attribute DISABLE-COMPRESSIONS in UNICODE collations  
  Contributor(s): Adriano dos Santos Fernandes

* [#6903](https://github.com/FirebirdSQL/firebird/issues/6903): Make it possible to create ICU-based collations with locales keywords  
  Contributor(s): tkeinz, Adriano dos Santos Fernandes

* [#6872](https://github.com/FirebirdSQL/firebird/issues/6872): Faster indexed STARTING WITH execution with UNICODE collations  
  Contributor(s): Adriano dos Santos Fernandes

* [#6810](https://github.com/FirebirdSQL/firebird/issues/6810): Use precise limit of salt length when signing messages and verifying the sign  
  Contributor(s): Alex Peshkoff

* [#6809](https://github.com/FirebirdSQL/firebird/issues/6809): Integer hex-literal support for INT128  
  Contributor(s): Alex Peshkoff

## Bugfixes

* [#7070](https://github.com/FirebirdSQL/firebird/issues/7070): Error "BLOB is not found" while replication converts INSERT into UPDATE for a conflicting record  
  Contributor(s): Dmitry Yemanov

* [#7068](https://github.com/FirebirdSQL/firebird/issues/7068): Errors in the ChaCha plugin are displayed incorrectly  
  Contributor(s): Alex Peshkoff

* [#7067](https://github.com/FirebirdSQL/firebird/issues/7067): Deadlock when using not initialized security database  
  Contributor(s): Alex Peshkoff

* [#7066](https://github.com/FirebirdSQL/firebird/issues/7066): Server may send incorrect specific data to client when establishing encrypted connection  
  Contributor(s): Alex Peshkoff

* [#7065](https://github.com/FirebirdSQL/firebird/issues/7065): Connection hangs after delivery of 256GB of data  
  Contributor(s): Alex Peshkoff

* [#7064](https://github.com/FirebirdSQL/firebird/issues/7064): Linear regression functions aren't implemented correctly  
  Contributor(s): Adriano dos Santos Fernandes

* [#7062](https://github.com/FirebirdSQL/firebird/issues/7062): Creation of expression index does not release its statement correctly  
  Contributor(s): Adriano dos Santos Fernandes

* [#7060](https://github.com/FirebirdSQL/firebird/issues/7060): Deadlock when execute test for CORE-4337  
  Contributor(s): Alex Peshkoff

* [#7057](https://github.com/FirebirdSQL/firebird/issues/7057): Client-side positioned updates work wrongly with scrollable cursors  
  Contributor(s): Dmitry Yemanov

* [#7056](https://github.com/FirebirdSQL/firebird/issues/7056): Fetching from a scrollable cursor may overwrite user-specified buffer and corrupt memory  
  Contributor(s): Dmitry Yemanov

* [#7052](https://github.com/FirebirdSQL/firebird/issues/7052): Races between transactions on the primary side may cause update conflicts while applying journals to the replica  
  Contributor(s): Dmitry Yemanov

* [#7048](https://github.com/FirebirdSQL/firebird/issues/7048): Incorrect releasing of user savepoint (older savepoints become inaccessible)  
  Contributor(s): Dmitry Yemanov

* [#7043](https://github.com/FirebirdSQL/firebird/issues/7043): Wrong message when user has no access to /tmp/firebird  
  Contributor(s): Alex Peshkoff

* [#7037](https://github.com/FirebirdSQL/firebird/issues/7037): Build problem when using both --with-builtin-tommath and --with-builtin-tomcrypt  
  Contributor(s): Adriano dos Santos Fernandes

* [#7034](https://github.com/FirebirdSQL/firebird/issues/7034): Server crashes while fetching from a scrollable cursor in PSQL  
  Contributor(s): Dmitry Yemanov

* [#7033](https://github.com/FirebirdSQL/firebird/issues/7033): Replicator is missing sanity checks for user-supplied blocks  
  Contributor(s): Dmitry Yemanov

* [#7018](https://github.com/FirebirdSQL/firebird/issues/7018): Problems with windows frames  
  Contributor(s): Adriano dos Santos Fernandes

* [#7015](https://github.com/FirebirdSQL/firebird/issues/7015): Replication applier code may crash if the specified block contains no savepoint operations  
  Contributor(s): Dmitry Yemanov

* [#6998](https://github.com/FirebirdSQL/firebird/issues/6998): Problems with access to RDB$CONFIG table for non-privileged user when he has grant on execution of SP which has necessary access rights (created by SYSDBA with SQL DEFINER clause)  
  Contributor(s): Roman Simakov

* [#6995](https://github.com/FirebirdSQL/firebird/issues/6995): "String truncation error" is raised while restoring the database from a v2.5 backup  
  Contributor(s): Dmitry Yemanov

* [#6989](https://github.com/FirebirdSQL/firebird/issues/6989): Invalid message in replication.log (and possibly crash in the case of synchronous replication) when the target DB has no "replica" flag set  
  Contributor(s): Dmitry Yemanov

* [#6985](https://github.com/FirebirdSQL/firebird/issues/6985): Application could hang when using new Batch API with blobs over XNET  
  Contributor(s): Vlad Khorsun

* [#6979](https://github.com/FirebirdSQL/firebird/issues/6979): Windows x64 server installer installs an incomplete x86 client
  Contributor(s): Paul Reeves

* [#6978](https://github.com/FirebirdSQL/firebird/issues/6978): Firebird 4 fails to build on big endian platforms  
  Contributor(s): Alex Peshkoff

* [#6976](https://github.com/FirebirdSQL/firebird/issues/6976): Lack of proper clean up after failure to initialize shared memory  
  Contributor(s): Vlad Khorsun

* [#6975](https://github.com/FirebirdSQL/firebird/issues/6975): Crash or hang while shutting down the replica database if segments are being applied  
  Contributor(s): Dmitry Yemanov

* [#6968](https://github.com/FirebirdSQL/firebird/issues/6968): On Windows, engine may hang when works with corrupted database and read after the end of file  
  Contributor(s): Vlad Khorsun

* [#6966](https://github.com/FirebirdSQL/firebird/issues/6966): Status vector for ES(<EMPTY_STRING>) is unstable if another execute block with correct statement was executed before. Affects only LINUX builds in Classic mode  
  Contributor(s): Alex Peshkoff

* [#6963](https://github.com/FirebirdSQL/firebird/issues/6963): The REFERENCES permission does not work  
  Contributor(s): Alex Peshkoff

* [#6955](https://github.com/FirebirdSQL/firebird/issues/6955): fb_info_creation_timestamp_tz returns a corrupted buffer  
  Contributor(s): Vlad Khorsun

* [#6945](https://github.com/FirebirdSQL/firebird/issues/6945): Segfault in the batch interface when gbak restores a database with many blobs over the wire  
  Contributor(s): Alex Peshkoff

* [#6943](https://github.com/FirebirdSQL/firebird/issues/6943): Windows client install is missing some DLLs
  Contributor(s): Paul Reeves

* [#6935](https://github.com/FirebirdSQL/firebird/issues/6935): SQL SECURITY DEFINER has inconsistent behaviour if the object owner is non-privileged  
  Contributor(s): Dmitry Yemanov

* [#6935](https://github.com/FirebirdSQL/firebird/issues/6935): SQL SECURITY DEFINER does not affect the ownership of created DDL objects  
  Contributor(s): Dmitry Yemanov

* [#6930](https://github.com/FirebirdSQL/firebird/issues/6930): Segfault when calling cryptographic functions  
  Contributor(s): Alex Peshkoff

* [#6917](https://github.com/FirebirdSQL/firebird/issues/6917): Firebird 4 installer asks for SYSDBA password when doing client installation
  Contributor(s): Paul Reeves

* [#6913](https://github.com/FirebirdSQL/firebird/issues/6913): Buffer overflows in getInfo APIs  
  Contributor(s): Adriano dos Santos Fernandes

* [#6909](https://github.com/FirebirdSQL/firebird/issues/6909): Some updates may crash Firebird server on the replica side  
  Contributor(s): Dmitry Yemanov

* [#6907](https://github.com/FirebirdSQL/firebird/issues/6907): Failed DDL commands can be replicated  
  Contributor(s): Dmitry Yemanov

* [#6900](https://github.com/FirebirdSQL/firebird/issues/6900): IBatch::add() method fails silently when a memory limit is exceeded  
  Contributor(s): Alex Peshkoff

* [#6893](https://github.com/FirebirdSQL/firebird/issues/6893): Problem with replication of BLOB segments longer than 32KB  
  Contributor(s): Dmitry Yemanov

* [#6887](https://github.com/FirebirdSQL/firebird/issues/6887): Invalid SIMILAR TO patterns may lead memory read beyond string limits  
  Contributor(s): Adriano dos Santos Fernandes

* [#6886](https://github.com/FirebirdSQL/firebird/issues/6886): Differerent interfaces behaviour depending upon source of interface  
  Contributor(s): Alex Peshkoff

* [#6887](https://github.com/FirebirdSQL/firebird/issues/6887): Significant performance regression of SIMILAR TO and SUBSTRING(SIMILAR) when pattern is taken from a variable  
  Contributor(s): Adriano dos Santos Fernandes

* [#6874](https://github.com/FirebirdSQL/firebird/issues/6874): Literal 65536 (interpreted as int) cannot be multiplied by itself w/o cast if result is more than 2^63-1  
  Contributor(s): Alex Peshkoff

* [#6860](https://github.com/FirebirdSQL/firebird/issues/6860): Create user statement fails with SQLSTATE = HY000 when using DataTypeCompatibility  
  Contributor(s): Alex Peshkoff

* [#6856](https://github.com/FirebirdSQL/firebird/issues/6856): Permission error after replication of DDL commands  
  Contributor(s): Dmitry Yemanov

* [#6853](https://github.com/FirebirdSQL/firebird/issues/6853): Asynchronous replication leaks file handles  
  Contributor(s): Dmitry Yemanov

* [#6850](https://github.com/FirebirdSQL/firebird/issues/6850): Database-level and DDL triggers are executed at the replica side  
  Contributor(s): Dmitry Yemanov

* [#6849](https://github.com/FirebirdSQL/firebird/issues/6849): Conflicting INSERT propagated into a read-write replica may cause duplicate records to appear (and PK/UK violation to happen)  
  Contributor(s): Dmitry Yemanov

* [#6848](https://github.com/FirebirdSQL/firebird/issues/6848): Generator values may not replicate after commit  
  Contributor(s): Dmitry Yemanov

* [#6845](https://github.com/FirebirdSQL/firebird/issues/6845): Result type of AVG over BIGINT column results in type INT128  
  Contributor(s): Alex Peshkoff

* [#6838](https://github.com/FirebirdSQL/firebird/issues/6838): Deleting multiple rows from a view with triggers may cause triggers to fire just once  
  Contributor(s): Dmitry Yemanov

* [#6832](https://github.com/FirebirdSQL/firebird/issues/6832): Segfault when using COMMIT RETAINING with Global Temporary Tables  
  Contributor(s): Alex Peshkoff

* [#6801](https://github.com/FirebirdSQL/firebird/issues/6801): Error recompiling a package with some combination of nested functions  
  Contributor(s): Adriano dos Santos Fernandes


# v4.0 Release (01-Jun-2021)

## Improvements

* [#6806](https://github.com/FirebirdSQL/firebird/issues/6806): Rename RSA_SIGN and RSA_VERIFY  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt), [/doc/sql.extensions/README.keywords](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.keywords)  
  Contributor(s): Alex Peshkoff

* [#6786](https://github.com/FirebirdSQL/firebird/issues/6786): Add session time zone to system context  
  Reference(s): [/doc/sql.extensions/README.time_zone.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.time_zone.md), [/doc/sql.extensions/README.context_variables2](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.context_variables2)  
  Contributor(s): Adriano dos Santos Fernandes

* [#6769](https://github.com/FirebirdSQL/firebird/issues/6769): More efficient implementation of SUBSTRING for UTF8 character set  
  Contributor(s): Adriano dos Santos Fernandes

* [#6767](https://github.com/FirebirdSQL/firebird/issues/6767): Add ability for a replication plugin to refuse attachment  
  Contributor(s): Dmitry Yemanov

* [#6748](https://github.com/FirebirdSQL/firebird/issues/6748): Freeing a statement using DSQL_drop or DSQL_unprepare should send free packet immediately  
  Contributor(s): Vlad Khorsun

* [#6715](https://github.com/FirebirdSQL/firebird/issues/6715): Provide time zone ID constants in public headers  
  Contributor(s): Adriano dos Santos Fernandes

* [#6253](https://github.com/FirebirdSQL/firebird/issues/6253): RDB$GET_TRANSACTION_CN should work the same way in Super and Classic  
  Contributor(s): Vlad Khorsun

## Bugfixes

* [#6816](https://github.com/FirebirdSQL/firebird/issues/6816): Illegal output length in base64/hex encode/decode functions  
  Contributor(s): Alex Peshkoff

* [#6812](https://github.com/FirebirdSQL/firebird/issues/6812): BASE64_ENCODE and HEX_ENCODE can exceed maximum widths for VARCHAR  
  Contributor(s): Alex Peshkoff

* [#6808](https://github.com/FirebirdSQL/firebird/issues/6808): Segfault in encrypt/decrypt functions when their first argument is NULL  
  Contributor(s): Alex Peshkoff

* [#6805](https://github.com/FirebirdSQL/firebird/issues/6805): RDB$TYPES has incorrect entries for RDB$TYPE 28 and 29 in RDB$TYPE_NAME column  
  Contributor(s): Alex Peshkoff

* [#6804](https://github.com/FirebirdSQL/firebird/issues/6804): Assertion in tomcrypt when key length for RC4 is too small  
  Contributor(s): Alex Peshkoff

* [#6802](https://github.com/FirebirdSQL/firebird/issues/6802): When the statement timeout is set, it causes the lock manager to delay reporting deadlocks until timeout is expired  
  Contributor(s): Vlad Khorsun

* [#6800](https://github.com/FirebirdSQL/firebird/issues/6800): Client config DefaultTimeZone is not passed to server when isc_dpb_session_time_zone is not set  
  Contributor(s): Adriano dos Santos Fernandes

* [#6797](https://github.com/FirebirdSQL/firebird/issues/6797): Functions DECRYPT and RSA_DECRYPT return VARCHAR CHARACTER SET NONE instead of VARBINARY (VARCHAR CHARACTER SET OCTETS)  
  Contributor(s): Alex Peshkoff

* [#6796](https://github.com/FirebirdSQL/firebird/issues/6796): Buffer overflow when padding line with national characters causes ISQL to crash  
  Contributor(s): Adriano dos Santos Fernandes

* [#6795](https://github.com/FirebirdSQL/firebird/issues/6795): Replication gets stuck due to "Blob xxx.xx is not found for table xxx" error  
  Contributor(s): Dmitry Yemanov

* [#6790](https://github.com/FirebirdSQL/firebird/issues/6790): MON$ATTACHMENTS.MON$TIMESTAMP is incorrect when DefaultTimeZone is configured with time zone different from the server's default  
  Contributor(s): Adriano dos Santos Fernandes

* [#6787](https://github.com/FirebirdSQL/firebird/issues/6787): MON$ATTACHMENTS.MON$TIMESTAMP should use its session original time zone  
  Contributor(s): Adriano dos Santos Fernandes

* [#6785](https://github.com/FirebirdSQL/firebird/issues/6785): Problem when restoring the database on Firebird 4 RC1  
  Contributor(s): Vlad Khorsun

* [#6782](https://github.com/FirebirdSQL/firebird/issues/6782): Cannot get "records fetched" for selectable procedures in trace  
  Contributor(s): Vlad Khorsun

* [#6781](https://github.com/FirebirdSQL/firebird/issues/6781): Crashing (due to UDF exception) process hangs  
  Contributor(s): Vlad Khorsun

* [#6780](https://github.com/FirebirdSQL/firebird/issues/6780): firebird.msg is missing in Firebird Android builds  
  Contributor(s): Alex Peshkoff

* [#6778](https://github.com/FirebirdSQL/firebird/issues/6778): Inconsistent cursor-driven deletion  
  Contributor(s): Dmitry Yemanov

* [#6777](https://github.com/FirebirdSQL/firebird/issues/6777): AV when the engine shuts down and cancels an attachment waiting in the lock manager  
  Contributor(s): Vlad Khorsun

* [#6768](https://github.com/FirebirdSQL/firebird/issues/6768): Restore backup on RawDevice (error during "open O_CREAT" operation for file "/dev/sda1")  
  Contributor(s): Alex Peshkoff

* [#6766](https://github.com/FirebirdSQL/firebird/issues/6766): Replication plugin can crash engine returning nullptr from startTransaction() method  
  Contributor(s): Dmitry Yemanov

* [#6761](https://github.com/FirebirdSQL/firebird/issues/6761): Hash join cannot match records using some TIME ZONE / DECFLOAT keys  
  Contributor(s): Dmitry Yemanov

* [#6759](https://github.com/FirebirdSQL/firebird/issues/6759): Results of concatenation with blob has no info about collation of source columns (which are declared with such info)  
  Contributor(s): Vlad Khorsun

* [#6758](https://github.com/FirebirdSQL/firebird/issues/6758): COMPUTED BY column looses charset and collate of source field <F> when <F> is either of type BLOB or VARCHAR casted to BLOB  
  Contributor(s): Vlad Khorsun

* [#6756](https://github.com/FirebirdSQL/firebird/issues/6756): Error "no current record for fetch operation" when sorting by a international string  
  Contributor(s): Dmitry Yemanov

* [#6754](https://github.com/FirebirdSQL/firebird/issues/6754): Connect to database that contains broken pages can lead to FB crash  
  Contributor(s): Alex Peshkoff

* [#6753](https://github.com/FirebirdSQL/firebird/issues/6753): AV in the engine when StatementTimeout is active for user statement and some internal DSQL statement was executed as part of overall execution process  
  Contributor(s): Vlad Khorsun

* [#6752](https://github.com/FirebirdSQL/firebird/issues/6752): Segfaults in fbclient when receiving invalid / unexpected data from server  
  Contributor(s): Alex Peshkoff

* [#6751](https://github.com/FirebirdSQL/firebird/issues/6751): Various segfaults in fbclient  
  Contributor(s): Alex Peshkoff

* [#6750](https://github.com/FirebirdSQL/firebird/issues/6750): CAST of Infinity values to FLOAT doesn't work  
  Contributor(s): Alex Peshkoff

* [#6749](https://github.com/FirebirdSQL/firebird/issues/6749): Error "Invalid time zone (+08). Falling back to displacement" in firebird.log  
  Contributor(s): Adriano dos Santos Fernandes

* [#6747](https://github.com/FirebirdSQL/firebird/issues/6747): Wrong message when connecting to tiny trash database file  
  Contributor(s): Alex Peshkoff

* [#6746](https://github.com/FirebirdSQL/firebird/issues/6746): Regression: CREATE DATABASE fails with 'Token unknown' error when DB name is enclosed in double quotes and 'DEFAULT CHARACTER SET' is specified after DB name  
  Contributor(s): Adriano dos Santos Fernandes

* [#6745](https://github.com/FirebirdSQL/firebird/issues/6745): Protect included tomcrypt library from being overwritten by a system package  
  Contributor(s): Alex Peshkoff

* [#6738](https://github.com/FirebirdSQL/firebird/issues/6738): Segfault when GFIX requests for database page buffer more memory than available from OS  
  Contributor(s): Alex Peshkoff

* [#6734](https://github.com/FirebirdSQL/firebird/issues/6734): Provide same results for date arithmetics when date is changed by values near +/-max(bigint)  
  Contributor(s): Alex Peshkoff

* [#6733](https://github.com/FirebirdSQL/firebird/issues/6733): Attempt to create database with page_size >= 65536 makes DB with actual page size = 4KB or 8KB, but not 32KB as it should  
  Contributor(s): Alex Peshkoff

* [#6731](https://github.com/FirebirdSQL/firebird/issues/6731): Segfault when shutting down database which got encrypted by another process  
  Contributor(s): Alex Peshkoff

* [#6729](https://github.com/FirebirdSQL/firebird/issues/6729): Regression: GSTAT with switch -t executed via services fails with "found unknown switch" error  
  Contributor(s): Alex Peshkoff

* [#6727](https://github.com/FirebirdSQL/firebird/issues/6727): Synchronous replication to localhost hangs on disconnect  
  Contributor(s): Dmitry Yemanov

* [#6724](https://github.com/FirebirdSQL/firebird/issues/6724): Inconsistent translation "string->timestamp->string->timestamp" in Dialect 1  
  Contributor(s): Adriano dos Santos Fernandes

* [#6719](https://github.com/FirebirdSQL/firebird/issues/6719): User without ALTER ANY ROLE privilege can use COMMENT ON ROLE  
  Contributor(s): Alex Peshkoff

* [#6718](https://github.com/FirebirdSQL/firebird/issues/6718): Event delivery could be missed when local (XNET) protocol is used  
  Contributor(s): Vlad Khorsun

* [#6717](https://github.com/FirebirdSQL/firebird/issues/6717): FETCH ABSOLUTE and RELATIVE beyond bounds of cursor should always position immediately before-first or after-last  
  Contributor(s): Dmitry Yemanov

* [#6716](https://github.com/FirebirdSQL/firebird/issues/6716): FETCH RELATIVE has an off by one error for the first row  
  Contributor(s): Dmitry Yemanov

* [#6710](https://github.com/FirebirdSQL/firebird/issues/6710): COMMENT ON USER can only apply comment on user defined by the default user manager plugin  
  Contributor(s): Alex Peshkoff

* [#6709](https://github.com/FirebirdSQL/firebird/issues/6709): GBAK discards replica mode during backup/restore  
  Contributor(s): Dmitry Yemanov

* [#6708](https://github.com/FirebirdSQL/firebird/issues/6708): Rare race condition in Plugin Manager could lead to the server crash  
  Contributor(s): Vlad Khorsun

* [#6700](https://github.com/FirebirdSQL/firebird/issues/6700): Wire compression causes sporadic "Error reading data from the connection" errors  
  Contributor(s): Alex Peshkoff

* [#6698](https://github.com/FirebirdSQL/firebird/issues/6698): Comments before the first line of code are removed  
  Contributor(s): Adriano dos Santos Fernandes

* [#6679](https://github.com/FirebirdSQL/firebird/issues/6679): CLOOP envelopes are wrong regarding IStatus  
  Contributor(s): Alex Peshkoff

* [#6265](https://github.com/FirebirdSQL/firebird/issues/6265): Segfault when using expression index with complex expression  
  Contributor(s): Vlad Khorsun

* [#5784](https://github.com/FirebirdSQL/firebird/issues/5784): When 32-bit and 64-bit Firebird 3 servers run on a single Windows machine concurrently, Firebird services freeze several minutes after first disconnect  
  Contributor(s): Vlad Khorsun

* [#3810](https://github.com/FirebirdSQL/firebird/issues/3810): Wrong or missing IS NULL optimization (regression)  
  Contributor(s): Vlad Khorsun

* [#3106](https://github.com/FirebirdSQL/firebird/issues/3106): Many indexed reads in a compound index with NULLs  
  Contributor(s): Vlad Khorsun

* [#2469](https://github.com/FirebirdSQL/firebird/issues/2469): Stored procedure recursively called by calculated field fails after reconnect  
  Contributor(s): Vlad Khorsun

# v4.0 Release Candidate 1 (01-Feb-2021)

## New features

* [CORE-5832](http://tracker.firebirdsql.org/browse/CORE-5832): Add command to reset user session environment to its initial (default) state  
  Contributor(s): Vlad Khorsun

* [CORE-3708](http://tracker.firebirdsql.org/browse/CORE-3708): Add virtual table RDB$CONFIG that shows actual configuration settings  
  Contributor(s): Vlad Khorsun

## Improvements

* [CORE-6474](http://tracker.firebirdsql.org/browse/CORE-6474): Report replica mode through isc_database_info, MON$DATABASE and SYSTEM context  
  Contributor(s): Dmitry Yemanov

* [CORE-6469](http://tracker.firebirdsql.org/browse/CORE-6469): Ability to trace session management commands (e.g. ALTER SESSION RESET)  
  Contributor(s): Vlad Khorsun

* [CORE-6429](http://tracker.firebirdsql.org/browse/CORE-6429): Timezone offset in timestamp/time literal, CAST, SET TIME ZONE and AT TIME ZONE should follow SQL standard syntax only  
  Reference(s): [/doc/sql.extensions/README.time_zone.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.time_zone.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6413](http://tracker.firebirdsql.org/browse/CORE-6413): Remove PIDFile/-pidfile directive/option from firebird Super(Server/Classic) systemd unit  
  Contributor(s): Alex Peshkoff

* [CORE-6395](http://tracker.firebirdsql.org/browse/CORE-6395): Allow usage of time zone displacement in config DefaultTimeZone  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6366](http://tracker.firebirdsql.org/browse/CORE-6366): Check presence of tomcrypt & curses libraries before installing Firebird  
  Contributor(s): Alex Peshkoff

* [CORE-6342](http://tracker.firebirdsql.org/browse/CORE-6342): INT128 as explicit basic type for high precision numerics  
  Reference(s): [/doc/sql.extensions/README.data_types](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.data_types)  
  Contributor(s): Alex Peshkoff

* [CORE-6320](http://tracker.firebirdsql.org/browse/CORE-6320): Replace Util methods to get interface pointer by legacy handle with plain functions  
  Contributor(s): Alex Peshkoff

* [CORE-6308](http://tracker.firebirdsql.org/browse/CORE-6308): Make it possible to update list of time zones (names and ids) without source code recompilation  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6300](http://tracker.firebirdsql.org/browse/CORE-6300): Ability to retrieve next attachment id and next statement id  
  Reference(s): [/doc/README.monitoring_tables](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.monitoring_tables)  
  Contributor(s): Dmitry Yemanov

* [CORE-5085](http://tracker.firebirdsql.org/browse/CORE-5085): Allow to fixup (nbackup) database via Services API  
  Reference(s): [/doc/README.services_extension](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.services_extension)  
  Contributor(s): Alex Peshkoff

* [CORE-4841](http://tracker.firebirdsql.org/browse/CORE-4841): Report explicit message about missing password in CREATE (OR ALTER) USER  
  Contributor(s): Alex Peshkoff

* [CORE-2650](http://tracker.firebirdsql.org/browse/CORE-2650): Improve sorting performance when long VARCHARs are involved  
  Contributor(s): Dmitry Yemanov

## Bugfixes

* [CORE-6475](http://tracker.firebirdsql.org/browse/CORE-6475): Memory leak when running EXECUTE STATEMENT with named parameters  
  Contributor(s): Vlad Khorsun

* [CORE-6472](http://tracker.firebirdsql.org/browse/CORE-6472): Wrong byte order for UUIDs in gstat and monitoring output  
  Contributor(s): Dmitriy Sibiryakov

* [CORE-6460](http://tracker.firebirdsql.org/browse/CORE-6460): Incorrect query result when using named window  
  Contributor(s): Vlad Khorsun

* [CORE-6453](http://tracker.firebirdsql.org/browse/CORE-6453): EXECUTE STATEMENT fails on FB 4.x if containing time/timestamp with time zone parameters  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6450](http://tracker.firebirdsql.org/browse/CORE-6450): Races in the security databases cache could lead to the server crash  
  Contributor(s): Alex Peshkoff

* [CORE-6447](http://tracker.firebirdsql.org/browse/CORE-6447): Unexpectedly different text of message for parameterized expression starting from second run  
  Contributor(s): Vlad Khorsun

* [CORE-6441](http://tracker.firebirdsql.org/browse/CORE-6441): Srp plugin keeps connection after database has been removed for ~10 seconds  
  Contributor(s): Alex Peshkoff

* [CORE-6440](http://tracker.firebirdsql.org/browse/CORE-6440): Expression indexes containing COALESCE inside cannot be matched by the optimizer after migration from v2.5 to v3.0  
  Contributor(s): Dmitry Yemanov

* [CORE-6438](http://tracker.firebirdsql.org/browse/CORE-6438): ISQL: bad headers when text columns has >= 80 characters  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6437](http://tracker.firebirdsql.org/browse/CORE-6437): GFIX cannot set big value for page buffers  
  Contributor(s): Vlad Khorsun

* [CORE-6433](http://tracker.firebirdsql.org/browse/CORE-6433): Server could crash during a daily maintenance / set statistics index  
  Contributor(s): Alex Peshkoff

* [CORE-6432](http://tracker.firebirdsql.org/browse/CORE-6432): Possible buffer overflow in client library in Attachment::getInfo() call  
  Contributor(s): Alex Peshkoff

* [CORE-6427](http://tracker.firebirdsql.org/browse/CORE-6427): Whitespace as date separator causes conversion error  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6426](http://tracker.firebirdsql.org/browse/CORE-6426): Assertion when the batch is executed without a BLOB field  
  Contributor(s): Alex Peshkoff

* [CORE-6425](http://tracker.firebirdsql.org/browse/CORE-6425): Exception in client library in IAttachment::createBatch()  
  Contributor(s): Alex Peshkoff

* [CORE-6421](http://tracker.firebirdsql.org/browse/CORE-6421): Parameter in offset expression in LAG, LEAD, NTH_VALUE window functions requires explicit cast to BIGINT or INTEGER  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6419](http://tracker.firebirdsql.org/browse/CORE-6419): Truncation of strings to put in MON$ tables do not work correctly  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6415](http://tracker.firebirdsql.org/browse/CORE-6415): Error "malformed string' is raised instead of "expected: N, actual: M" when UTF-8 charset is used and default value is longer than the column length  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6414](http://tracker.firebirdsql.org/browse/CORE-6414): Error "expected length N, actual M" contains wrong value of M when UTF-8 charset is used in the field declaration  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6412](http://tracker.firebirdsql.org/browse/CORE-6412): Firebird was freezing when trying to manage users via triggers  
  Contributor(s): Alex Peshkoff

* [CORE-6408](http://tracker.firebirdsql.org/browse/CORE-6408): RETURNING clause in the MERGE statement cannot reference column in aliased target table using qualified reference (alias.column) if DELETE action present  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6403](http://tracker.firebirdsql.org/browse/CORE-6403): Some PSQL statements may lead to exceptions report wrong line/column  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6398](http://tracker.firebirdsql.org/browse/CORE-6398): Error converting string with hex representation of INTEGER to SMALLINT  
  Contributor(s): Alex Peshkoff

* [CORE-6397](http://tracker.firebirdsql.org/browse/CORE-6397):  Message length error with COALESCE and TIME/TIMESTAMP WITHOUT TIME ZONE and WITH TIME ZONE  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6389](http://tracker.firebirdsql.org/browse/CORE-6389): Using binary string literal to assign to user-defined blob sub-types yield conversion error  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6387](http://tracker.firebirdsql.org/browse/CORE-6387): Client process was aborting due to bugs inside the ChaCha plugin
  Contributor(s): Alex Peshkoff

* [CORE-6386](http://tracker.firebirdsql.org/browse/CORE-6386): ALTER SEQUENCE RESTART WITH <n> should not change the initial sequence START value  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6385](http://tracker.firebirdsql.org/browse/CORE-6385): Wrong line and column information after IF statement  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6379](http://tracker.firebirdsql.org/browse/CORE-6379): Bugcheck 179 (decompression overran buffer)  
  Contributor(s): Vlad Khorsun

* [CORE-6377](http://tracker.firebirdsql.org/browse/CORE-6377): Unable to restore database with tables using GENERATED ALWAYS AS IDENTITY columns  
  Contributor(s): Alex Peshkoff

* [CORE-6376](http://tracker.firebirdsql.org/browse/CORE-6376): IDENTITY column with explicit START WITH or INCREMENT BY starts with wrong value  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6357](http://tracker.firebirdsql.org/browse/CORE-6357): LEAD() and LAG() do not allow to specify 3rd argument of INT128 datatype  
  Contributor(s): Alex Peshkoff

* [CORE-6356](http://tracker.firebirdsql.org/browse/CORE-6356): ROUND() does not allow second argument >=1 when its first argument is more than MAX_BIGINT / 10  
  Contributor(s): Alex Peshkoff

* [CORE-6355](http://tracker.firebirdsql.org/browse/CORE-6355): TRUNC() does not accept second argument = -128 (but shows it as required boundary in error message)  
  Contributor(s): Alex Peshkoff

* [CORE-6353](http://tracker.firebirdsql.org/browse/CORE-6353): INT128 data type has problems with some PSQL objects  
  Contributor(s): Alex Peshkoff

* [CORE-6344](http://tracker.firebirdsql.org/browse/CORE-6344): Invalid return type for functions with INT128 / Numeric(38) argument  
  Contributor(s): Alex Peshkoff

* [CORE-6337](http://tracker.firebirdsql.org/browse/CORE-6337): Sub-type information is lost when calculating arithmetic expressions  
  Contributor(s): Alex Peshkoff

* [CORE-6336](http://tracker.firebirdsql.org/browse/CORE-6336): Error "Implementation of text subtype <NNNN> not located" on attempt to use some collations defined in fbintl.conf  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6335](http://tracker.firebirdsql.org/browse/CORE-6335): INSERT ... RETURNING does not require a SELECT privilege  
  Contributor(s): Dmitry Yemanov

* [CORE-6328](http://tracker.firebirdsql.org/browse/CORE-6328): FB4 Beta 2 may still be using the current date for TIME WITH TIME ZONE and extended wire protocol  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6325](http://tracker.firebirdsql.org/browse/CORE-6325): NTILE/RANK/PERCENT_RANK may cause problems in big/complex statements  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6318](http://tracker.firebirdsql.org/browse/CORE-6318): CAST('NOW' as TIME) raises a conversion error  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6316](http://tracker.firebirdsql.org/browse/CORE-6316): Unable to specify new 32KB page size in CREATE DATABASE statement  
  Contributor(s): Alex Peshkoff

* [CORE-6305](http://tracker.firebirdsql.org/browse/CORE-6305): Android port build failure  
  Contributor(s): Alex Peshkoff

* [CORE-6303](http://tracker.firebirdsql.org/browse/CORE-6303): Error writing to TIMESTAMP/TIME WITH TIME ZONE array  
  Contributor(s): Alex Peshkoff

* [CORE-6302](http://tracker.firebirdsql.org/browse/CORE-6302): Error writing an array of NUMERIC(24,6) to the database  
  Contributor(s): Alex Peshkoff

* [CORE-6084](http://tracker.firebirdsql.org/browse/CORE-6084): CREATE SEQUENCE START WITH has wrong initial value  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6023](http://tracker.firebirdsql.org/browse/CORE-6023): FB4 is unable to overwrite older ODS database  
  Contributor(s): Alex Peshkoff

* [CORE-5838](http://tracker.firebirdsql.org/browse/CORE-5838): Rotated trace files are locked by the engine  
  Contributor(s): Vlad Khorsun

* [CORE-4985](http://tracker.firebirdsql.org/browse/CORE-4985): A non-privileged user could implicitly count records in a restricted table  
  Contributor(s): Dmitry Yemanov

* [CORE-2274](http://tracker.firebirdsql.org/browse/CORE-2274): MERGE has a non-standard behaviour, accepts multiple matches  
  Contributor(s): Vlad Khorsun


# v4.0 Beta 2 (14-May-2020)

## New features

* [CORE-6287](http://tracker.firebirdsql.org/browse/CORE-6287): SET BIND OF type1 TO type2  
  Reference(s): [/doc/sql.extensions/README.set_bind.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.set_bind.md)  
  Contributor(s): Alex Peshkoff

* [CORE-6285](http://tracker.firebirdsql.org/browse/CORE-6285): SQL-level replication management  
  Reference(s): [/doc/README.replication.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.replication.md), [/doc/sql.extensions/README.ddl.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.ddl.txt)  
  Contributor(s): Dmitry Yemanov

* [CORE-6109](http://tracker.firebirdsql.org/browse/CORE-6109): Changed FLOAT to a SQL standard compliant FLOAT datatype  
  Reference(s): [/doc/sql.extensions/README.floating_point_types.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.floating_point_types.md)  
  Contributor(s): Mark Rotteveel

* [CORE-6018](http://tracker.firebirdsql.org/browse/CORE-6018): Make it possible to start multiple transactions (possibly in different attachments) using the same initial transaction snapshot  
  Reference(s): [/doc/README.transaction_at_snapshot.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.transaction_at_snapshot.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-4933](http://tracker.firebirdsql.org/browse/CORE-4933): Add better transaction control to ISQL  
  Reference(s): [/doc/README.isql_enhancements.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.isql_enhancements.txt)  
  Contributor(s): Vlad Khorsun

* [CORE-3435](http://tracker.firebirdsql.org/browse/CORE-3435): Lateral derived tables  
  Reference(s): [/doc/sql.extensions/README.derived_tables.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.derived_tables.txt)  
  Contributor(s): Dmitry Yemanov

## Improvements

* [CORE-6286](http://tracker.firebirdsql.org/browse/CORE-6286): Make usage of TIMESTAMP/TIME WITH TIME ZONE convenient for users when appropriate ICU library is not installed on the client side  
  Reference(s): [/doc/sql.extensions/README.time_zone.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.time_zone.md), [/doc/sql.extensions/README.set_bind.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.set_bind.md)   
  Contributor(s): Alex Peshkoff

* [CORE-6279](http://tracker.firebirdsql.org/browse/CORE-6279): Put options in user management statements in any order  
  Reference(s): [/doc/sql.extensions/README.user_management](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.user_management)  
  Contributor(s): Alex Peshkoff

* [CORE-6278](http://tracker.firebirdsql.org/browse/CORE-6278): Efficient table scans for DBKEY-based range conditions  
  Contributor(s): Dmitry Yemanov

* [CORE-6274](http://tracker.firebirdsql.org/browse/CORE-6274): Increase parse speed of long queries  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6268](http://tracker.firebirdsql.org/browse/CORE-6268): Add methods to set various names (field, relation, etc.) in metadata builder  
  Reference(s): [/doc/Using_OO_API.html](https://github.com/FirebirdSQL/firebird/raw/master/doc/Using_OO_API.html)  
  Contributor(s): Alex Peshkoff

* [CORE-6239](http://tracker.firebirdsql.org/browse/CORE-6239): Procedures and EXECUTE BLOCK without RETURNS should not be allowed to use SUSPEND  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6237](http://tracker.firebirdsql.org/browse/CORE-6237): Performance problem when using SRP plugin  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6220](http://tracker.firebirdsql.org/browse/CORE-6220): Enable delivery of known to the client key to any connection to the server  
  Contributor(s): Alex Peshkoff

* [CORE-6219](http://tracker.firebirdsql.org/browse/CORE-6219): Add support for special (inf/nan) values when sorting DECFLOAT values  
  Contributor(s): Alex Peshkoff

* [CORE-6095](http://tracker.firebirdsql.org/browse/CORE-6095): Extend trace record for COMMIT/ROLLBACK RETAINING to allow chaining of transaction ids  
  Note(s): Backported into v3.0.6  
  Contributor(s): Vlad Khorsun

* [CORE-6069](http://tracker.firebirdsql.org/browse/CORE-6069): Show OS-specific error when entrypont is not found in dynamic library  
  Contributor(s): Alex Peshkoff

* [CORE-6058](http://tracker.firebirdsql.org/browse/CORE-6058): Change behavior of skipped and repeated wall times within time zones  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6049](http://tracker.firebirdsql.org/browse/CORE-6049): Builtin functions converting binary string to hexadecimal representation and vice versa  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Alex Peshkoff

* [CORE-6048](http://tracker.firebirdsql.org/browse/CORE-6048): Provide ability to see current state of DB encryption  
  Reference(s): [/doc/README.monitoring_tables](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.monitoring_tables)  
  Contributor(s): Alex Peshkoff

* [CORE-6032](http://tracker.firebirdsql.org/browse/CORE-6032): Add DPB properties for time zone bind and decfloat configuration  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6017](http://tracker.firebirdsql.org/browse/CORE-6017): Add transaction info fb\_info\_tra\_snapshot\_number  
  Contributor(s): Vlad Khorsun

* [CORE-5658](http://tracker.firebirdsql.org/browse/CORE-5658): Execute statement with excess parameters  
  Reference(s): [/doc/sql.extensions/README.execute_statement2](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.execute_statement2)  
  Contributor(s): Vlad Khorsun

* [CORE-5538](http://tracker.firebirdsql.org/browse/CORE-5538): Add ability to backup/restore only those (several) tables which are enumerated as command line argument (pattern)  
  Reference(s): [/doc/README.gbak](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.gbak)  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4726](http://tracker.firebirdsql.org/browse/CORE-4726): Implement REcreate user <user_name> password <user_pwd>  
  Contributor(s): Alex Peshkoff

* [CORE-4579](http://tracker.firebirdsql.org/browse/CORE-4579): Distribute separate header files instead merged one  
  Contributor(s): Dimitry Sibiryakov

* [CORE-3983](http://tracker.firebirdsql.org/browse/CORE-3983): Ability to authenticate user in "EXECUTE STATEMENT ON EXTERNAL DATA SOURCE" by hash of the current password  
  Contributor(s): Alex Peshkoff

## Bugfixes

* [CORE-6290](http://tracker.firebirdsql.org/browse/CORE-6290): Hex number used at the end of statement may read invalid memory and produce wrong values or exceptions  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6283](http://tracker.firebirdsql.org/browse/CORE-6283): isNullable() in message metadata, returned by metadata builder, does not match with datatype set by setType() in metadata builder  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6282](http://tracker.firebirdsql.org/browse/CORE-6282): Change type of MON$ATTACHMENTS.MON$IDLE\_TIMER and MON$STATEMENTS.MON$STATEMENT\_TIMER to TIMESTAMP WITH TIME ZONE  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6281](http://tracker.firebirdsql.org/browse/CORE-6281): Invalid timestamp errors with RDB$TIME\_ZONE_UTIL.TRANSITIONS  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6280](http://tracker.firebirdsql.org/browse/CORE-6280): MERGE statement loses parameters in WHEN (NOT) MATCHED clause that will never be matched, crashes server in some situations  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6272](http://tracker.firebirdsql.org/browse/CORE-6272): Failed attach to a database is not traced  
  Contributor(s): Alex Peshkoff

* [CORE-6266](http://tracker.firebirdsql.org/browse/CORE-6266): Deleting records from MON$ATTACHMENTS using ORDER BY clause doesn't close the corresponding attachments  
  Note(s): Backported into v3.0.6  
  Contributor(s): Dmitry Yemanov

* [CORE-6265](http://tracker.firebirdsql.org/browse/CORE-6265): Mapping rules are destroyed by backup / restore  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6264](http://tracker.firebirdsql.org/browse/CORE-6264): GBAK with PIPE to stdout: invalid content if user '-se <host>:service_mgr' command switch  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6262](http://tracker.firebirdsql.org/browse/CORE-6262): SHOW DOMAIN/TABLE does not display character set of system objects  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6260](http://tracker.firebirdsql.org/browse/CORE-6260): Warnings are not always displayed in ISQL  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6254](http://tracker.firebirdsql.org/browse/CORE-6254): Server crashes when using SET TRANSACTION and ON TRANSACTION START trigger uses EXECUTE STATEMENT against current transaction  
  Note(s): Backported into v3.0.6  
  Contributor(s): Vlad Khorsun

* [CORE-6253](http://tracker.firebirdsql.org/browse/CORE-6253): Crash caused by the locked fb_lock file  
  Note(s): Backported into v3.0.6  
  Contributor(s): Vlad Khorsun

* [CORE-6252](http://tracker.firebirdsql.org/browse/CORE-6252): UNIQUE CONSTRAINT violation  
  Note(s): Backported into v3.0.6  
  Contributor(s): Vlad Khorsun

* [CORE-6251](http://tracker.firebirdsql.org/browse/CORE-6251): Crash when built-in function LEFT or RIGHT is missing its 2nd argument  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6250](http://tracker.firebirdsql.org/browse/CORE-6250): Signature mismatch when creating package body on identical packaged procedure header  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6248](http://tracker.firebirdsql.org/browse/CORE-6248): A number of errors when database name is longer than 255 symbols  
  Contributor(s): Alex Peshkoff

* [CORE-6243](http://tracker.firebirdsql.org/browse/CORE-6243): Regression: v4 Beta 1 rejects POSITION element of v2.5 defined SQL2003 CREATE TRIGGER syntax  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6241](http://tracker.firebirdsql.org/browse/CORE-6241): Values greater than number of days between 01.01.0001 and 31.12.9999 (=3652058) can be added or subtracted from DATE  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6238](http://tracker.firebirdsql.org/browse/CORE-6238): DECFLOAT: subtraction Num1 - Num2 leads to "Decimal float overflow" if Num2 is specified in scientific notation and less than max double (1.7976931348623157e308)  
  Contributor(s): Alex Peshkoff

* [CORE-6236](http://tracker.firebirdsql.org/browse/CORE-6236): RDB$TIME_ZONE_UTIL package has wrong privilege for PUBLIC  
  Contributor(s): Adriano dos Santos Fernandes, Dmitry Yemanov

* [CORE-6233](http://tracker.firebirdsql.org/browse/CORE-6233): Wrong dependencies of stored function on view after backup/restore  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6231](http://tracker.firebirdsql.org/browse/CORE-6231): Server crashes on shutdown of XNET connection to a local database when events have been registered  
  Note(s): Backported into v3.0.6  
  Contributor(s): Vlad Khorsun

* [CORE-6230](http://tracker.firebirdsql.org/browse/CORE-6230): Unable to connect with database if security.db reference is removed from databases.conf file  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6227](http://tracker.firebirdsql.org/browse/CORE-6227): isc\_info\_svc\_user\_dbpath always returns alias of main security database  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6224](http://tracker.firebirdsql.org/browse/CORE-6224): Server crash: re-destruction of the rem_port object  
  Note(s): Backported into v3.0.6  
  Contributor(s): Dmitry Kovalenko, Alex Peshkoff

* [CORE-6221](http://tracker.firebirdsql.org/browse/CORE-6221): Incorrect (throw-based) allocFunc for zlib1: memory leak is possible  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6218](http://tracker.firebirdsql.org/browse/CORE-6218): COUNT(DISTINCT <DECFLOAT_FIELD>) leads FB to crash when there are duplicate values of this field  
  Contributor(s): Alex Peshkoff

* [CORE-6217](http://tracker.firebirdsql.org/browse/CORE-6217): [inet.cpp] Dangerous work with pointer: delete ptr; ptr=new ;  
  Note(s): Backported into v3.0.6  
  Contributor(s): Dmitry Kovalenko, Alex Peshkoff

* [CORE-6214](http://tracker.firebirdsql.org/browse/CORE-6214): Update outdated tzdata version  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6212](http://tracker.firebirdsql.org/browse/CORE-6212): Authentication plugin on server may get garbage data from client instead empty packet  
  Contributor(s): Alex Peshkoff

* [CORE-6211](http://tracker.firebirdsql.org/browse/CORE-6211): Command "ISQL -X" can not extract ROLE name when use multi-byte charset for connection  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6208](http://tracker.firebirdsql.org/browse/CORE-6208): Grant lost in security.db after backup/restore cycle  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6207](http://tracker.firebirdsql.org/browse/CORE-6207): Modifications needed to complile Firebird.pas under FPC  
  Contributor(s): Alex Peshkoff

* [CORE-6206](http://tracker.firebirdsql.org/browse/CORE-6206): VARCHAR of insufficient length used for set bind of decfloat to varchar  
  Contributor(s): Vlad Khorsun

* [CORE-6205](http://tracker.firebirdsql.org/browse/CORE-6205): Generate proper error for UNION DISTINCT with more than 255 columns  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6186](http://tracker.firebirdsql.org/browse/CORE-6186): Original content of column which is involved into ENCRYPT() is displayed as distorted view after this call  
  Contributor(s): Alex Peshkoff

* [CORE-6181](http://tracker.firebirdsql.org/browse/CORE-6181): Operations when using "SET DECFLOAT BIND BIGINT,n" with result of 11+ digits, fail with "Decimal float invalid operation"  
  Contributor(s): Alex Peshkoff

* [CORE-6174](http://tracker.firebirdsql.org/browse/CORE-6174): ibase.h is missing from nightly builds  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6170](http://tracker.firebirdsql.org/browse/CORE-6170): Cannot install on CentOS 8 because version of LibTomMath and LibNCurses library  
  Contributor(s): Alex Peshkoff

* [CORE-6166](http://tracker.firebirdsql.org/browse/CORE-6166): Problems with long object names (> 255 bytes)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6160](http://tracker.firebirdsql.org/browse/CORE-6160): SUBSTRING of non-text/-blob is described to return NONE character set in DSQL  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6159](http://tracker.firebirdsql.org/browse/CORE-6159): SUBSTRING SIMILAR is described with wrong data type in DSQL  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6130](http://tracker.firebirdsql.org/browse/CORE-6130): Creating backup to stdout using service manager is broken  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-6116](http://tracker.firebirdsql.org/browse/CORE-6116): Metadata script extracted using ISQL of a database restored from a v2.5.9 backup is invalid when table has COMPUTED BY fields  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6110](http://tracker.firebirdsql.org/browse/CORE-6110): 64-bit transaction IDs are not stored properly in status vector  
  Contributor(s): Ilya Eremin

* [CORE-6080](http://tracker.firebirdsql.org/browse/CORE-6080): Attempt to drop existing user randomly fails with "336723990 : record not found for user"  
  Contributor(s): Vlad Khorsun

* [CORE-6071](http://tracker.firebirdsql.org/browse/CORE-6071): Restore of encrypted backup of database with SQL dialect 1 fails  
  Contributor(s): Alex Peshkoff

* [CORE-6061](http://tracker.firebirdsql.org/browse/CORE-6061): Building with --with-builtin-tommath doesn't work  
  Contributor(s): Alex Peshkoff

* [CORE-6056](http://tracker.firebirdsql.org/browse/CORE-6056): Overflow warnings when building some collations  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6046](http://tracker.firebirdsql.org/browse/CORE-6046): Incorrect time zone parsing reads garbage in memory  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6044](http://tracker.firebirdsql.org/browse/CORE-6044): ISQL issues with increased identifier length  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6034](http://tracker.firebirdsql.org/browse/CORE-6034): The original time zone should be set to the current time zone at routine invocation  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6033](http://tracker.firebirdsql.org/browse/CORE-6033): SUBSTRING(CURRENT_TIMESTAMP) does not work  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-6019](http://tracker.firebirdsql.org/browse/CORE-6019): Wire compression does not work without MSVC 2010 runtime installation  
  Contributor(s): Vlad Khorsun

* [CORE-5976](http://tracker.firebirdsql.org/browse/CORE-5976): GBAK multi-database file restore uses wrong minimum number of pages for first database file  
  Contributor(s): Mark Rotteveel

* [CORE-5972](http://tracker.firebirdsql.org/browse/CORE-5972): External engine trigger crashing server if table have computed field  
  Note(s): Backported into v3.0.6  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5957](http://tracker.firebirdsql.org/browse/CORE-5957): Bug in SIMILAR TO when adding numeric quantifier as bound for repetetion of expression leads to empty resultset  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5931](http://tracker.firebirdsql.org/browse/CORE-5931): SIMILAR TO does not return result when invalid pattern is used  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5892](http://tracker.firebirdsql.org/browse/CORE-5892): SQL SECURITY DEFINER context is not properly evaluated for monitoring tables  
  Contributor(s): Roman Simakov

* [CORE-5697](http://tracker.firebirdsql.org/browse/CORE-5697): Conversion from numeric literals to DECFLOAT adds precision that is not originally present  
  Contributor(s): Alex Peshkoff

* [CORE-5696](http://tracker.firebirdsql.org/browse/CORE-5696): Conversion from zero numeric literals to DECFLOAT results in incorrect value  
  Contributor(s): Alex Peshkoff

* [CORE-5691](http://tracker.firebirdsql.org/browse/CORE-5691): File description on Firebird executables should be specific  
  Contributor(s): Vlad Khorsun

* [CORE-5664](http://tracker.firebirdsql.org/browse/CORE-5664): SIMILAR TO is substantially (500-700x) slower than LIKE on trivial pattern matches with VARCHAR data  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5445](http://tracker.firebirdsql.org/browse/CORE-5445): Cannot install on Debian Stretch/Testing because version of LibTomMath library  
  Contributor(s): Alex Peshkoff

* [CORE-5364](http://tracker.firebirdsql.org/browse/CORE-5364): gfix -online normal <db> (executed in session #1) does not produce error when there is a SYSDBA-owned session #2  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

* [CORE-4893](http://tracker.firebirdsql.org/browse/CORE-4893): SIMILAR-TO leads FB to crash when matching blob with size > 2Gb to a string literal  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-4874](http://tracker.firebirdsql.org/browse/CORE-4874): Infinite "SIMILAR TO" matching  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-4739](http://tracker.firebirdsql.org/browse/CORE-4739): Accent insensitive comparison: diacritical letters with DIAGONAL crossing stroke pass only test on EQUALITY to their non-accented forms  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-3858](http://tracker.firebirdsql.org/browse/CORE-3858): Very poor performance of SIMILAR TO on some arguments   
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-3380](http://tracker.firebirdsql.org/browse/CORE-3380): Disable reading from the newly created BLOB  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-2251](http://tracker.firebirdsql.org/browse/CORE-2251): GBAK doesn't return error code  
  Note(s): Backported into v3.0.6  
  Contributor(s): Alex Peshkoff

# v4.0 Beta 1 (20-Feb-2019)

## New features

* [CORE-5990](http://tracker.firebirdsql.org/browse/CORE-5990): Pool of external connections  
  Reference(s): [/doc/sql.extensions/README.external_connections_pool](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.external_connections_pool)  
  Contributor(s): Vlad Khorsun

* [CORE-5970](http://tracker.firebirdsql.org/browse/CORE-5970): Built-in cryptographic functions  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Alex Peshkoff

* [CORE-5953](http://tracker.firebirdsql.org/browse/CORE-5953): Statement level read consistency in read-committed transactions  
  Reference(s): [/doc/README.read_consistency.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.read_consistency.md)  
  Contributor(s): Nickolay Samofatov, Roman Simakov, Vlad Khorsun

* [CORE-5951](http://tracker.firebirdsql.org/browse/CORE-5951): Add support of batch insert and update operations  
  Reference(s): [/doc/Using_OO_API.html](https://github.com/FirebirdSQL/firebird/raw/master/doc/Using_OO_API.html)  
  Contributor(s): Alex Peshkoff

* [CORE-5808](http://tracker.firebirdsql.org/browse/CORE-5808): Support backup of encrypted databases  
  Contributor(s): Alex Peshkoff

* [CORE-5768](http://tracker.firebirdsql.org/browse/CORE-5768): Implement FILTER-clause for aggregate functions  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5620](http://tracker.firebirdsql.org/browse/CORE-5620): Add builtin functions FIRST_DAY and LAST_DAY  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5536](http://tracker.firebirdsql.org/browse/CORE-5536): Connection compressed/encrypted status in MON$ATTACHMENTS table  
  Reference(s): [/doc/README.monitoring_tables](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.monitoring_tables)  
  Contributor(s): Alex Peshkoff

* [CORE-2022](http://tracker.firebirdsql.org/browse/CORE-2022): Built-in logical replication  
  Reference(s): [/doc/README.replication.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.replication.md)  
  Contributor(s): Dmitry Yemanov

* [CORE-909](http://tracker.firebirdsql.org/browse/CORE-909): Ability to retrieve current UTC/GMT timestamp  
  Reference(s): [/doc/sql.extensions/README.time_zone.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.time_zone.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-694](http://tracker.firebirdsql.org/browse/CORE-694): Support for time zones  
  Reference(s): [/doc/sql.extensions/README.time_zone.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.time_zone.md)  
  Contributor(s): Adriano dos Santos Fernandes

## Improvements

* [CORE-5973](http://tracker.firebirdsql.org/browse/CORE-5973): Handling FP overflow in DOUBLE PRECISION value when converting from DECFLOAT  
  Contributor(s): Alex Peshkoff

* [CORE-5954](http://tracker.firebirdsql.org/browse/CORE-5954): Garbage collection in intermediate record versions  
  Reference(s): [/doc/README.read_consistency.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.read_consistency.md)  
  Contributor(s): Nickolay Samofatov, Roman Simakov, Vlad Khorsun

* [CORE-5952](http://tracker.firebirdsql.org/browse/CORE-5952): Enhance restore performance of GBAK using batch API  
  Contributor(s): Alex Peshkoff

* [CORE-5948](http://tracker.firebirdsql.org/browse/CORE-5948): Make WIN_SSPI plugin produce keys for wirecrypt plugin  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5928](http://tracker.firebirdsql.org/browse/CORE-5928): Make it possible for AuthClient plugin to access authentication block from DPB  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5921](http://tracker.firebirdsql.org/browse/CORE-5921): Provide information about Global Commit Number, Commit Number of currently used database snapshot (if any) and Commit Numbers assigned to the committed transactions  
  Reference(s): [/doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Vlad Khorsun

* [CORE-5887](http://tracker.firebirdsql.org/browse/CORE-5887): Allow the use of management statements in PSQL blocks  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5883](http://tracker.firebirdsql.org/browse/CORE-5883): Services version 1 cleanup  
  Contributor(s): Alex Peshkoff

* [CORE-5874](http://tracker.firebirdsql.org/browse/CORE-5874): Provide name of read-only column incorrectly referenced in UPDATE ... SET xxx  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5779](http://tracker.firebirdsql.org/browse/CORE-5779): Add support for riscv64  
  Contributor(s): Richard Jones

* [CORE-5770](http://tracker.firebirdsql.org/browse/CORE-5770): User who is allowed to manage other users must have this ability WITHOUT need to grant him RDB$ADMIN role  
  Contributor(s): Alex Peshkoff

* [CORE-5741](http://tracker.firebirdsql.org/browse/CORE-5741): Replace word "fixing" with "adjusting" in GBAK output  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5718](http://tracker.firebirdsql.org/browse/CORE-5718): Make TempCacheLimit setting database-wise  
  Contributor(s): Dmitry Yemanov

* [CORE-5705](http://tracker.firebirdsql.org/browse/CORE-5705): Store precision of DECFLOAT in RDB$FIELDS  
  Contributor(s): Alex Peshkoff

* [CORE-5647](http://tracker.firebirdsql.org/browse/CORE-5647): Increase number of formats/versions of views from 255 to 32K  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5606](http://tracker.firebirdsql.org/browse/CORE-5606): Add expression index name to exception message if computation failed  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-4529](http://tracker.firebirdsql.org/browse/CORE-4529): Allow to use index when GROUP BY on field which has DESCENDING index  
  Contributor(s): Dmitry Yemanov

* [CORE-4409](http://tracker.firebirdsql.org/browse/CORE-4409): Enhancement in precision of calculations with NUMERIC/DECIMAL  
  Contributor(s): Alex Peshkoff

* [CORE-3808](http://tracker.firebirdsql.org/browse/CORE-3808): Provide ability to return all columns using RETURNING clause  
  Reference(s): [/doc/sql.extensions/README.returning](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.returning)  
  Contributor(s): Adriano dos Santos Fernandes

## Bugfixes

* [CORE-5995](http://tracker.firebirdsql.org/browse/CORE-5995): Creator user name is empty in user trace sessions  
  Note(s): Backported into v3.0.5  
  Contributor(s): Vlad Khorsun

* [CORE-5993](http://tracker.firebirdsql.org/browse/CORE-5993): When creation of audit log file fails, there is no error message in firebird.log  
  Note(s): Backported into v3.0.5  
  Contributor(s): Vlad Khorsun

* [CORE-5991](http://tracker.firebirdsql.org/browse/CORE-5991): Trace could not work correctly with quoted file names in trace configurations  
  Note(s): Backported into v3.0.5  
  Contributor(s): Vlad Khorsun

* [CORE-5989](http://tracker.firebirdsql.org/browse/CORE-5989): iconv / libiconv 1.15 vs libc / libiconv_open | common/isc_file.cpp  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5986](http://tracker.firebirdsql.org/browse/CORE-5986): Incorrect evaluation of NULL IS [NOT] {FALSE | TRUE}  
  Note(s): Backported into v3.0.5  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5985](http://tracker.firebirdsql.org/browse/CORE-5985): Regression: ROLE does not passed in ES/EDS (specifying it in the statement is ignored)  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5982](http://tracker.firebirdsql.org/browse/CORE-5982): Error "read permission for BLOB field", when it's an input/output procedure's parameter  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitriy Starodubov

* [CORE-5980](http://tracker.firebirdsql.org/browse/CORE-5980): Firebird crashes due to concurrent operations with expression indices  
  Note(s): Backported into v3.0.5  
  Contributor(s): Vlad Khorsun

* [CORE-5974](http://tracker.firebirdsql.org/browse/CORE-5974): Wrong result of select distinct with decfload/timezone/collated column  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5972](http://tracker.firebirdsql.org/browse/CORE-5972): External engine trigger crashing server if table have computed field  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5965](http://tracker.firebirdsql.org/browse/CORE-5965): FB3 optimizer chooses less efficient plan than FB2.5 optimizer  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-5959](http://tracker.firebirdsql.org/browse/CORE-5959): Firebird returns wrong time after changes of time zone  
  Note(s): Backported into v3.0.5  
  Contributor(s): Vlad Khorsun

* [CORE-5955](http://tracker.firebirdsql.org/browse/CORE-5955): Unable to init binreloc with ld >= 2.31  
  Note(s): Backported into v3.0.5  
  Contributor(s): Roman Simakov

* [CORE-5950](http://tracker.firebirdsql.org/browse/CORE-5950): Deadlock when attaching to bugchecked database  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5943](http://tracker.firebirdsql.org/browse/CORE-5943): Server crashes preparing a query with both DISTINCT/ORDER BY and non-field expression in the select list  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-5934](http://tracker.firebirdsql.org/browse/CORE-5934): gpre_boot fails to link using cmake, undefined reference 'dladdr' and 'dlerror'  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5930](http://tracker.firebirdsql.org/browse/CORE-5930): Bugcheck "incorrect snapshot deallocation - too few slots"  
  Contributor(s): Vlad Khorsun

* [CORE-5927](http://tracker.firebirdsql.org/browse/CORE-5927): With some non-standard authentication plugins providing correct crypt key wire anyway remains not encrypted  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5926](http://tracker.firebirdsql.org/browse/CORE-5926): Attempt to create mapping with non-ASCII user name which is encoded in SINGLE-BYTE codepage (WIN1251) leads to 'Malformed string' error  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5918](http://tracker.firebirdsql.org/browse/CORE-5918): Memory pool statistics is not accurate  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5907](http://tracker.firebirdsql.org/browse/CORE-5907): Regression: can not launch trace if its 'database' section contains regexp pattern with curvy brackets to enclose quantifier  
  Note(s): Backported into v3.0.5  
  Contributor(s): Alex Peshkoff

* [CORE-5896](http://tracker.firebirdsql.org/browse/CORE-5896): NOT NULL constraint is not synchronized after rename column  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5871](http://tracker.firebirdsql.org/browse/CORE-5871): Incorrect caching of the subquery result (procedure call) in independent queries  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5862](http://tracker.firebirdsql.org/browse/CORE-5862): Varchar computed column without explicit type does not populate RDB$CHARACTER_LENGTH  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5861](http://tracker.firebirdsql.org/browse/CORE-5861): GRANT OPTION is not checked for new object  
  Contributor(s): Roman Simakov

* [CORE-5855](http://tracker.firebirdsql.org/browse/CORE-5855): Firebird 4.0 cannot backup DB with generators which contains space in the names  
  Contributor(s): Alex Peshkoff

* [CORE-5800](http://tracker.firebirdsql.org/browse/CORE-5800): After backup/restore the indexes by expression on computed field are not working properly  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-5795](http://tracker.firebirdsql.org/browse/CORE-5795): ORDER BY clause on compound index may disable usage of other indices  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-5750](http://tracker.firebirdsql.org/browse/CORE-5750): Date-time parsing is very weak  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5728](http://tracker.firebirdsql.org/browse/CORE-5728): Field subtype of DEC_FIXED columns not returned by isc_info_sql_sub_type  
  Contributor(s): Alex Peshkoff

* [CORE-5726](http://tracker.firebirdsql.org/browse/CORE-5726): Unclear error message when inserting value exceeding max of DEC_FIXED decimal  
  Contributor(s): Alex Peshkoff

* [CORE-5717](http://tracker.firebirdsql.org/browse/CORE-5717): Reject non-constant date/time/timestamp literals  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5710](http://tracker.firebirdsql.org/browse/CORE-5710): Datatype declaration DECFLOAT without precision should use a default precision  
  Contributor(s): Alex Peshkoff

* [CORE-5700](http://tracker.firebirdsql.org/browse/CORE-5700): DECFLOAT underflow should yield zero instead of an error  
  Contributor(s): Alex Peshkoff

* [CORE-5699](http://tracker.firebirdsql.org/browse/CORE-5699): DECFLOAT should not throw exceptions when +/-NaN, +/-sNaN and +/-Infinity is used in comparisons  
  Contributor(s): Alex Peshkoff

* [CORE-5657](http://tracker.firebirdsql.org/browse/CORE-5657): Various UDF-related security vulnerabilities  
  Contributor(s): Alex Peshkoff

* [CORE-5646](http://tracker.firebirdsql.org/browse/CORE-5646): Parse error when compiling a statement causes memory leak until attachment is disconnected  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5639](http://tracker.firebirdsql.org/browse/CORE-5639): Mapping rule using WIN_SSPI plugin: windows user group conversion to firebird role does not work  
  Contributor(s): Alex Peshkoff

* [CORE-5637](http://tracker.firebirdsql.org/browse/CORE-5637): string right truncation on restore of security db  
  Contributor(s): Alex Peshkoff

* [CORE-5612](http://tracker.firebirdsql.org/browse/CORE-5612): Gradual slowdown of view operations (create, recreate or drop)  
  Contributor(s): Dmitry Yemanov

* [CORE-5611](http://tracker.firebirdsql.org/browse/CORE-5611): Higher memory consumption for prepared statements  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5593](http://tracker.firebirdsql.org/browse/CORE-5593): System function RDB$ROLE_IN_USE cannot take long role names  
  Contributor(s): Alex Peshkoff

* [CORE-5518](http://tracker.firebirdsql.org/browse/CORE-5518): Firebird UDF string2blob() may allow remote code execution  
  Contributor(s): Alex Peshkoff

* [CORE-5480](http://tracker.firebirdsql.org/browse/CORE-5480): SUBSTRING startposition smaller than 1 should be allowed  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5395](http://tracker.firebirdsql.org/browse/CORE-5395): Invalid data type for negation (minus operator)  
  Note(s): Backported into v3.0.5  
  Contributor(s): Adriano dos Santos Fernandes, Dmitry Yemanov

* [CORE-5118](http://tracker.firebirdsql.org/browse/CORE-5118): Indices on computed fields are broken after restore (all keys are NULL)  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-5070](http://tracker.firebirdsql.org/browse/CORE-5070): Compound index cannot be used for filtering in some ORDER/GROUP BY queries  
  Note(s): Backported into v3.0.5  
  Contributor(s): Dmitry Yemanov

* [CORE-1592](http://tracker.firebirdsql.org/browse/CORE-1592): Altering procedure parameters can lead to unrestorable database  
  Contributor(s): Adriano dos Santos Fernandes


# v4.0 Alpha 1 (23-Aug-2017)

## New features

* [CORE-5568](http://tracker.firebirdsql.org/browse/CORE-5568): SQL SECURITY feature  
  Reference(s): [/doc/sql.extensions/README.sql_security.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.sql_security.txt)  
  Contributor(s): Roman Simakov

* [CORE-5525](http://tracker.firebirdsql.org/browse/CORE-5525): Create high-precision floating point datatype named DECFLOAT  
  Reference(s): [/doc/sql.extensions/README.data_types](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.data_types)  
  Contributor(s): Alex Peshkoff

* [CORE-5488](http://tracker.firebirdsql.org/browse/CORE-5488): Timeouts for running SQL statements and idle connections  
  Reference(s): [/doc/README.session_idle_timeouts](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.session_idle_timeouts), [/doc/README.statement_timeouts](https://github.com/FirebirdSQL/firebird/raw/master/doc/README.statement_timeouts)  
  Contributor(s): Vlad Khorsun

* [CORE-5463](http://tracker.firebirdsql.org/browse/CORE-5463): Support DEFAULT context value in INSERT, UPDATE, MERGE and UPDATE OR INSERT statements  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5449](http://tracker.firebirdsql.org/browse/CORE-5449): Support GENERATED ALWAYS identity columns and OVERRIDE clause  
  Reference(s): [doc/sql.extensions/README.identity_columns.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.identity_columns.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5346](http://tracker.firebirdsql.org/browse/CORE-5346): Named windows  
  Reference(s): [doc/sql.extensions/README.window_functions.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.window_functions.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5343](http://tracker.firebirdsql.org/browse/CORE-5343): Allow particular DBA privileges to be transferred to regular users  
  Reference(s): [doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt), [doc/sql.extensions/README.ddl](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.ddl.txt)  
  Contributor(s): Alex Peshkoff

* [CORE-3647](http://tracker.firebirdsql.org/browse/CORE-3647): Frames for window functions  
  Reference(s): [doc/sql.extensions/README.window_functions.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.window_functions.md)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-2762](http://tracker.firebirdsql.org/browse/CORE-2762): New built-in function to check whether some role is implicitly active  
  Reference(s): [doc/sql.extensions/README.cumulative_roles.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.cumulative_roles.txt)  
  Contributor(s): Roman Simakov

* [CORE-1815](http://tracker.firebirdsql.org/browse/CORE-1815): Ability to grant role to another role  
  Reference(s): [doc/sql.extensions/README.cumulative_roles.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.cumulative_roles.txt)  
  Contributor(s): Roman Simakov

* [CORE-1688](http://tracker.firebirdsql.org/browse/CORE-1688): More ANSI SQL:2003 window functions (PERCENK_RANK, CUME_DIST, NTILE)  
  Reference(s): [doc/sql.extensions/README.window_functions.md](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.window_functions.md)  
  Contributor(s): Hajime Nakagami, Adriano dos Santos Fernandes

* [CORE-751](http://tracker.firebirdsql.org/browse/CORE-751): Implicitly active roles (and their permissions summarized)  
  Contributor(s): Roman Simakov

## Improvements

* [CORE-5431](http://tracker.firebirdsql.org/browse/CORE-5431): Support for DROP IDENTITY clause  
  Reference(s): [doc/sql.extensions/README.identity_columns.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.identity_columns.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5430](http://tracker.firebirdsql.org/browse/CORE-5430): Support for INCREMENT option in identity columns  
  Reference(s): [doc/sql.extensions/README.identity_columns.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.identity_columns.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5380](http://tracker.firebirdsql.org/browse/CORE-5380): Allow subroutines to call others subroutines and themself recursively  
  Reference(s): [doc/sql.extensions/README.subroutines.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.subroutines.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5238](http://tracker.firebirdsql.org/browse/CORE-5238): Replace xinetd support with the native listener
  Contributor(s): Alex Peshkoff

* [CORE-5119](http://tracker.firebirdsql.org/browse/CORE-5119): Support autocommit mode in SET TRANSACTION statement  
  Contributor(s): Dmitry Yemanov

* [CORE-5064](http://tracker.firebirdsql.org/browse/CORE-5064): Add datatypes (VAR)BINARY(n) and BINARY VARYING(n) as aliases for (VAR)CHAR(n) CHARACTER SET OCTETS  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4436](http://tracker.firebirdsql.org/browse/CORE-4436): Support for different hash algorithms in HASH system function  
  Reference(s): [doc/sql.extensions/README.builtin_functions.txt](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.builtin_functions.txt)  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-2557](http://tracker.firebirdsql.org/browse/CORE-2557): Grants on MON$ tables  
  Contributor(s): Alex Peshkoff

* [CORE-2216](http://tracker.firebirdsql.org/browse/CORE-2216): NBackup as online dump  
  Contributor(s): Roman Simakov, Vlad Khorsun

* [CORE-2192](http://tracker.firebirdsql.org/browse/CORE-2192): Extend maximum database page size to 32KB  
  Contributor(s): Dmitry Yemanov

* [CORE-2040](http://tracker.firebirdsql.org/browse/CORE-2040): Allow exception name and possibly exception text to be determined within a "WHEN ANY" error handling block  
  Reference(s): [doc/sql.extensions/README.context_variables](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.context_variables)  
  Contributor(s): Dmitry Yemanov

* [CORE-1132](http://tracker.firebirdsql.org/browse/CORE-1132): Exception context in PSQL exception handlers  
  Reference(s): [doc/sql.extensions/README.context_variables](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.context_variables)  
  Contributor(s): Dmitry Yemanov

* [CORE-749](http://tracker.firebirdsql.org/browse/CORE-749): Increase maximum length of object names to 63 characters  
  Contributor(s): Adriano dos Santos Fernandes

## Bugfixes

* [CORE-5545](http://tracker.firebirdsql.org/browse/CORE-5545): Wrong syntax with CREATE TRIGGER ... ON <table> used with POSITION  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5454](http://tracker.firebirdsql.org/browse/CORE-5454): INSERT into updatable view without explicit field list failed  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5408](http://tracker.firebirdsql.org/browse/CORE-5408): Result of boolean expression can not be concatenated with string literal  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5404](http://tracker.firebirdsql.org/browse/CORE-5404): Inconsistent column/line references when PSQL definitions return errors  
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5237](http://tracker.firebirdsql.org/browse/CORE-5237): Invalid handling of dot (.) and asterisk (*) in config file name and path for include clause  
  Contributor(s): Dimitry Sibiryakov

* [CORE-5223](http://tracker.firebirdsql.org/browse/CORE-5223): Double dots are prohibited in file names if access is restricted to a list of directories  
  Contributor(s): Dimitry Sibiryakov

* [CORE-5141](http://tracker.firebirdsql.org/browse/CORE-5141): Field definition allows several NOT NULL clauses  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4985](http://tracker.firebirdsql.org/browse/CORE-4985): Non-privileged user can implicitly count records in a restricted table  
  Contributor(s): Dmitry Yemanov

* [CORE-4701](http://tracker.firebirdsql.org/browse/CORE-4701): Index and blob garbage collection don't take into accout data in undo log  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4483](http://tracker.firebirdsql.org/browse/CORE-4483): Changed data not visible in WHEN-section if exception occured inside SP that has been called from this code  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4424](http://tracker.firebirdsql.org/browse/CORE-4424): Rollback to wrong savepoint if several exception handlers on the same level are executed  
  Contributor(s): Dimitry Sibiryakov
