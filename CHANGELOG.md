# v4.0 Beta 1 (unreleased)

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
