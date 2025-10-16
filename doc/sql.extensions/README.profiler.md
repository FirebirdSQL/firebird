# Profiler (FB 5.0)

The profiler allows users to measure performance cost of SQL and PSQL code. It's implemented with a system package in the engine passing data to a profiler plugin.

This documentation treats the engine and plugin parts as a single thing, in the way the default profiler (`Default_Profiler`) is going to be used.

The `RDB$PROFILER` package can profile execution of PSQL code, collecting statistics of how many times each line was executed along with its minimum, maximum and accumulated execution times (with nanoseconds precision), as well as open and fetch statistics of implicit and explicit SQL cursors.

To collect profile data, a user must first start a profile session with `RDB$PROFILER.START_SESSION`. This function returns a profile session ID which is later stored in the profiler snapshot tables to be queried and analyzed by the user. A profiler session may be local (same attachment) or remote (another attachment).

Remote profiling just forwards commands to the remote attachment. So, it's possible that a client profiles multiple attachments simultaneously. It's also possible that a locally or remotely started profile session have commands issued by another attachment.

Remotely issued commands require that the target attachment is in an idle state, i.e. not executing others requests. When the target attachment is not idle, the call blocks waiting for that state.

If the remote attachment is from a different user, the calling user must have the system privilege `PROFILE_ANY_ATTACHMENT`.

After a session is started, PSQL and SQL statements statistics are collected in memory. A profile session collects data only of statements executed in the same attachment associated with the session. Data is aggregated and stored per requests (i.e. a statement execution). When querying snapshot tables, the user may do extra aggregation per statement, or use the auxiliary views that do that automatically.

A session may be paused to temporarily disable statistics collecting. It may be resumed later to return statistics collection in the same session.

A new session may be started when a session is already active. In that case, it has the same semantics of finishing the current session with `RDB$PROFILER.FINISH_SESSION(FALSE)`, so snapshots tables are not updated.

To analyze the collected data, the user must flush the data to the snapshot tables, which can be done by finishing or pausing a session (with `FLUSH` parameter set to `TRUE`), or calling `RDB$PROFILER.FLUSH`. Data is flushed using an autonomous transaction (a transaction started and finished for the specific purpose of profiler data update).

## Important

When the profiler is active, there is an overhead that makes everything slower.
This overhead varies depending on OS, kernel version and CPU hardware and it's difficult to predict.

But sometimes this overhead may be very high, say, greater than 100%.

If this happens in Linux, you may see what clock source it's using with this command:

```
cat /sys/devices/system/clocksource/clocksource0/current_clocksource
```

If result is different than `tsc`, that may be the cause of this problem.

You can see [here](https://access.redhat.com/solutions/18627) how to change clocksource, but you must understand
it may have others consequences.

Another possible source of slowdown in Linux is [this bug](https://bugzilla.kernel.org/show_bug.cgi?id=198961).

## Example usage

Below is a sample profile session and queries for data analysis.

```
-- Preparation - create table and routines that will be analyzed

create table tab (
    id integer not null,
    val integer not null
);

set term !;

create or alter function mult(p1 integer, p2 integer) returns integer
as
begin
    return p1 * p2;
end!

create or alter procedure ins
as
    declare n integer = 1;
begin
    while (n <= 1000)
    do
    begin
        if (mod(n, 2) = 1) then
            insert into tab values (:n, mult(:n, 2));
        n = n + 1;
    end
end!

set term ;!

-- Start profiling

select rdb$profiler.start_session('Profile Session 1', null, null, null, 'DETAILED_REQUESTS') from rdb$database;

set term !;

execute block
as
begin
    execute procedure ins;
    delete from tab;
end!

set term ;!

execute procedure rdb$profiler.finish_session(true);

execute procedure ins;

select rdb$profiler.start_session('Profile Session 2') from rdb$database;

select mod(id, 5),
       sum(val)
  from tab
  where id <= 50
  group by mod(id, 5)
  order by sum(val);

execute procedure rdb$profiler.finish_session(true);

-- Data analysis

set search_path to plg$profiler, public, system;

set transaction read committed;

select * from plg$prof_sessions;

select * from plg$prof_psql_stats_view;

select * from plg$prof_record_source_stats_view;

select preq.*
  from plg$prof_requests preq
  join plg$prof_sessions pses
    on pses.profile_id = preq.profile_id and
       pses.description = 'Profile Session 1';

select pstat.*
  from plg$prof_psql_stats pstat
  join plg$prof_sessions pses
    on pses.profile_id = pstat.profile_id and
       pses.description = 'Profile Session 1'
  order by pstat.profile_id,
           pstat.request_id,
           pstat.line_num,
           pstat.column_num;

select pstat.*
  from plg$prof_record_source_stats pstat
  join plg$prof_sessions pses
    on pses.profile_id = pstat.profile_id and
       pses.description = 'Profile Session 2'
  order by pstat.profile_id,
           pstat.request_id,
           pstat.cursor_id,
           pstat.record_source_id;
```

## Function `START_SESSION`

`RDB$PROFILER.START_SESSION` starts a new profiler session, makes it the current session (of the given `ATTACHMENT_ID`) and returns its identifier.

If `FLUSH_INTERVAL` is different than `NULL`, auto-flush is setup in the same way as manually calling `RDB$PROFILER.SET_FLUSH_INTERVAL`.

If `PLUGIN_NAME` is `NULL` (the default), it uses the database configuration `DefaultProfilerPlugin`.

`PLUGIN_OPTIONS` are plugin specific options and currently could be `NULL` or the string `DETAILED_REQUESTS` for `Default_Profiler` plugin.

When `DETAILED_REQUESTS` is used, `PLG$PROF_REQUESTS` will store detailed requests data, i.e., one record per each invocation of a statement. This may generate a lot of records, causing `RDB$PROFILER.FLUSH` to be slow.

When `DETAILED_REQUESTS` is not used (the default), `PLG$PROF_REQUESTS` stores an aggregated record per statement, using `REQUEST_ID = 0`.

Input parameters:
 - `DESCRIPTION` type `VARCHAR(255) CHARACTER SET UTF8` default `NULL`
 - `FLUSH_INTERVAL` type `INTEGER` default `NULL`
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)
 - `PLUGIN_NAME` type `VARCHAR(255) CHARACTER SET UTF8` default `NULL`
 - `PLUGIN_OPTIONS` type `VARCHAR(255) CHARACTER SET UTF8` default `NULL`

Return type: `BIGINT NOT NULL`.

## Procedure `PAUSE_SESSION`

`RDB$PROFILER.PAUSE_SESSION` pauses the current profiler session (of the given `ATTACHMENT_ID`), so the next executed statements statistics are not collected.

If `FLUSH` is `TRUE`, the snapshot tables are updated with data up to the current moment, otherwise data remains only in memory for later update.

Calling `RDB$PROFILER.PAUSE_SESSION(TRUE)` has the same semantics of calling `RDB$PROFILER.PAUSE_SESSION(FALSE)` followed by `RDB$PROFILER.FLUSH` (using the same `ATTACHMENT_ID`).

Input parameters:
 - `FLUSH` type `BOOLEAN NOT NULL` default `FALSE`
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `RESUME_SESSION`

`RDB$PROFILER.RESUME_SESSION` resumes the current profiler session (of the given `ATTACHMENT_ID`), if it was paused, so the next executed statements statistics are collected again.

Input parameters:
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `FINISH_SESSION`

`RDB$PROFILER.FINISH_SESSION` finishes the current profiler session (of the given `ATTACHMENT_ID`).

If `FLUSH` is `TRUE`, the snapshot tables are updated with data of the finished session (and old finished sessions not yet present in the snapshot), otherwise data remains only in memory for later update.

Calling `RDB$PROFILER.FINISH_SESSION(TRUE)` has the same semantics of calling `RDB$PROFILER.FINISH_SESSION(FALSE)` followed by `RDB$PROFILER.FLUSH` (using the same `ATTACHMENT_ID`).

Input parameters:
 - `FLUSH` type `BOOLEAN NOT NULL` default `TRUE`
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `CANCEL_SESSION`

`RDB$PROFILER.CANCEL_SESSION` cancels the current profiler session (of the given `ATTACHMENT_ID`).

All session data present in the profiler plugin is discarded and will not be flushed.

Data already flushed is not deleted automatically.

Input parameters:
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `DISCARD`

`RDB$PROFILER.DISCARD` removes all sessions (of the given `ATTACHMENT_ID`) from memory, without flushing them.

If there is a active session, it is cancelled.

Input parameters:
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `FLUSH`

`RDB$PROFILER.FLUSH` updates the snapshot tables with data from the profile sessions (of the given `ATTACHMENT_ID`) in memory.

After flushing, the data is stored in tables `PLG$PROF_SESSIONS`, `PLG$PROF_STATEMENTS`, `PLG$PROF_RECORD_SOURCES`, `PLG$PROF_REQUESTS`, `PLG$PROF_PSQL_STATS` and `PLG$PROF_RECORD_SOURCE_STATS` and may be read and analyzed by the user.

Data is updated using an autonomous transaction, so if the procedure is called in a snapshot transaction, data will not be directly readable in the same transaction.

Once flush happens, finished sessions are removed from memory.

Input parameters:
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

## Procedure `SET_FLUSH_INTERVAL`

`RDB$PROFILER.SET_FLUSH_INTERVAL` turns periodic auto-flush on (when `FLUSH_INTERVAL` is greater than 0) or off (when `FLUSH_INTERVAL` is equal to 0).

`FLUSH_INTERVAL` is interpreted as number of seconds.

Input parameters:
 - `FLUSH_INTERVAL` type `INTEGER NOT NULL`
 - `ATTACHMENT_ID` type `BIGINT` default `NULL` (meaning `CURRENT_CONNECTION`)

# Snapshot tables

The profiler schema, snapshot tables, views and sequence are automatically created in the first usage of the profiler.

They are owned by the database owner, with usage/read/write permissions for the RDB$PROFILER role, granted by default to `PUBLIC`.

When a session is deleted, the related data in other profiler snapshot tables are automatically deleted too through foreign keys with `DELETE CASCADE` option.

Below is the list of tables that stores profile data.

## Table `PLG$PROF_SESSIONS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `ATTACHMENT_ID` type `BIGINT` - Attachment ID
 - `USER_NAME` type `CHAR(63) CHARACTER SET UTF8` - User name
 - `DESCRIPTION` type `VARCHAR(255) CHARACTER SET UTF8` - Description passed in `RDB$PROFILER.START_SESSION`
 - `START_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - Moment the profile session was started
 - `FINISH_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - Moment the profile session was finished (NULL when not finished)
 - Primary key: `PROFILE_ID`

## Table `PLG$PROF_STATEMENTS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `PARENT_STATEMENT_ID` type `BIGINT` - Parent statement ID - related to sub routines
 - `STATEMENT_TYPE` type `VARCHAR(20) CHARACTER SET UTF8` - BLOCK, FUNCTION, PROCEDURE or TRIGGER
 - `SCHEMA_NAME` type `CHAR(63) CHARACTER SET UTF8` - Schema name of FUNCTION, PROCEDURE or TRIGGER
 - `PACKAGE_NAME` type `CHAR(63) CHARACTER SET UTF8` - Package of FUNCTION or PROCEDURE
 - `ROUTINE_NAME` type `CHAR(63) CHARACTER SET UTF8` - Routine name of FUNCTION, PROCEDURE or TRIGGER
 - `SQL_TEXT` type `BLOB subtype TEXT CHARACTER SET UTF8` - SQL text for BLOCK
 - Primary key: `PROFILE_ID, STATEMENT_ID`

## Table `PLG$PROF_CURSORS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `CURSOR_ID` type `INTEGER` - Cursor ID
 - `NAME` type `CHAR(63) CHARACTER SET UTF8` - Name of explicit cursor
 - `LINE_NUM` type `INTEGER` - Line number of the cursor
 - `COLUMN_NUM` type `INTEGER` - Column number of the cursor
 - Primary key: `PROFILE_ID, STATEMENT_ID, CURSOR_ID`

## Table `PLG$PROF_RECORD_SOURCES`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `CURSOR_ID` type `INTEGER` - Cursor ID
 - `RECORD_SOURCE_ID` type `INTEGER` - Record source ID
 - `PARENT_RECORD_SOURCE_ID` type `INTEGER` - Parent record source ID
 - `LEVEL` type `INTEGER` - Indentation level for the record source
 - `ACCESS_PATH` type `VARCHAR(255) CHARACTER SET UTF8` - Access path for the record source
 - Primary key: `PROFILE_ID, STATEMENT_ID, CURSOR_ID, RECORD_SOURCE_ID`

## Table `PLG$PROF_REQUESTS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `REQUEST_ID` type `BIGINT` - Request ID
 - `CALLER_STATEMENT_ID` type `BIGINT` - Caller statement ID
 - `CALLER_REQUEST_ID` type `BIGINT` - Caller request ID
 - `START_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - Moment this request was first gathered profile data
 - `FINISH_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - Moment this request was finished
 - `TOTAL_ELAPSED_TIME` type `BIGINT` - Accumulated elapsed time (in nanoseconds) of the request
 - Primary key: `PROFILE_ID, STATEMENT_ID, REQUEST_ID`

## Table `PLG$PROF_PSQL_STATS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `REQUEST_ID` type `BIGINT` - Request ID
 - `LINE_NUM` type `INTEGER` - Line number of the statement
 - `COLUMN_NUM` type `INTEGER` - Column number of the statement
 - `COUNTER` type `BIGINT` - Number of executed times of the line/column
 - `MIN_ELAPSED_TIME` type `BIGINT` - Minimal elapsed time (in nanoseconds) of a line/column execution
 - `MAX_ELAPSED_TIME` type `BIGINT` - Maximum elapsed time (in nanoseconds) of a line/column execution
 - `TOTAL_ELAPSED_TIME` type `BIGINT` - Accumulated elapsed time (in nanoseconds) of the line/column executions
 - Primary key: `PROFILE_ID, STATEMENT_ID, REQUEST_ID, LINE_NUM, COLUMN_NUM`

## Table `PLG$PROF_RECORD_SOURCE_STATS`

 - `PROFILE_ID` type `BIGINT` - Profile session ID
 - `STATEMENT_ID` type `BIGINT` - Statement ID
 - `REQUEST_ID` type `BIGINT` - Request ID
 - `CURSOR_ID` type `INTEGER` - Cursor ID
 - `RECORD_SOURCE_ID` type `INTEGER` - Record source ID
 - `OPEN_COUNTER` type `BIGINT` - Number of open times of the record source
 - `OPEN_MIN_ELAPSED_TIME` type `BIGINT` - Minimal elapsed time (in nanoseconds) of a record source open
 - `OPEN_MAX_ELAPSED_TIME` type `BIGINT` - Maximum elapsed time (in nanoseconds) of a record source open
 - `OPEN_TOTAL_ELAPSED_TIME` type `BIGINT` - Accumulated elapsed time (in nanoseconds) of the record source openings
 - `FETCH_COUNTER` type `BIGINT` - Number of fetch times of the record source
 - `FETCH_MIN_ELAPSED_TIME` type `BIGINT` - Minimal elapsed time (in nanoseconds) of a record source fetch
 - `FETCH_MAX_ELAPSED_TIME` type `BIGINT` - Maximum elapsed time (in nanoseconds) of a record source fetch
 - `FETCH_TOTAL_ELAPSED_TIME` type `BIGINT` - Accumulated elapsed time (in nanoseconds) of the record source fetches
 - Primary key: `PROFILE_ID, STATEMENT_ID, REQUEST_ID, CURSOR_ID, RECORD_SOURCE_ID`

# Auxiliary views

These views help profile data extraction aggregated at statement level.

They should be the preferred way to analyze the collected data. They can also be used together with the tables to get additional data not present on the views.

After hotspots are found, one can drill down in the data at the request level through the tables.

## View `PLG$PROF_STATEMENT_STATS_VIEW`
```
select req.profile_id,
       req.statement_id,
       sta.statement_type,
       sta.schema_name,
       sta.package_name,
       sta.routine_name,
       sta.parent_statement_id,
       sta_parent.statement_type parent_statement_type,
       sta_parent.routine_name parent_routine_name,
       (select sql_text
          from plg$prof_statements
          where profile_id = req.profile_id and
                statement_id = coalesce(sta.parent_statement_id, req.statement_id)
       ) sql_text,
       count(*) counter,
       min(req.total_elapsed_time) min_elapsed_time,
       max(req.total_elapsed_time) max_elapsed_time,
       cast(sum(req.total_elapsed_time) as bigint) total_elapsed_time,
       cast(sum(req.total_elapsed_time) / count(*) as bigint) avg_elapsed_time
  from plg$prof_requests req
  join plg$prof_statements sta
    on sta.profile_id = req.profile_id and
       sta.statement_id = req.statement_id
  left join plg$prof_statements sta_parent
    on sta_parent.profile_id = sta.profile_id and
       sta_parent.statement_id = sta.parent_statement_id
  group by req.profile_id,
           req.statement_id,
           sta.statement_type,
           sta.schema_name,
           sta.package_name,
           sta.routine_name,
           sta.parent_statement_id,
           sta_parent.statement_type,
           sta_parent.routine_name
  order by sum(req.total_elapsed_time) desc
```

## View `PLG$PROF_PSQL_STATS_VIEW`
```
select pstat.profile_id,
       pstat.statement_id,
       sta.statement_type,
       sta.schema_name,
       sta.package_name,
       sta.routine_name,
       sta.parent_statement_id,
       sta_parent.statement_type parent_statement_type,
       sta_parent.routine_name parent_routine_name,
       (select sql_text
          from plg$prof_statements
          where profile_id = pstat.profile_id and
                statement_id = coalesce(sta.parent_statement_id, pstat.statement_id)
       ) sql_text,
       pstat.line_num,
       pstat.column_num,
       cast(sum(pstat.counter) as bigint) counter,
       min(pstat.min_elapsed_time) min_elapsed_time,
       max(pstat.max_elapsed_time) max_elapsed_time,
       cast(sum(pstat.total_elapsed_time) as bigint) total_elapsed_time,
       cast(sum(pstat.total_elapsed_time) / nullif(sum(pstat.counter), 0) as bigint) avg_elapsed_time
  from plg$prof_psql_stats pstat
  join plg$prof_statements sta
    on sta.profile_id = pstat.profile_id and
       sta.statement_id = pstat.statement_id
  left join plg$prof_statements sta_parent
    on sta_parent.profile_id = sta.profile_id and
       sta_parent.statement_id = sta.parent_statement_id
  group by pstat.profile_id,
           pstat.statement_id,
           sta.statement_type,
           sta.schema_name,
           sta.package_name,
           sta.routine_name,
           sta.parent_statement_id,
           sta_parent.statement_type,
           sta_parent.routine_name,
           pstat.line_num,
           pstat.column_num
  order by sum(pstat.total_elapsed_time) desc
```

## View `PLG$PROF_RECORD_SOURCE_STATS_VIEW`
```
select rstat.profile_id,
       rstat.statement_id,
       sta.statement_type,
       sta.schema_name,
       sta.package_name,
       sta.routine_name,
       sta.parent_statement_id,
       sta_parent.statement_type parent_statement_type,
       sta_parent.routine_name parent_routine_name,
       (select sql_text
          from plg$prof_statements
          where profile_id = rstat.profile_id and
                statement_id = coalesce(sta.parent_statement_id, rstat.statement_id)
       ) sql_text,
       rstat.cursor_id,
       cur.name cursor_name,
       cur.line_num cursor_line_num,
       cur.column_num cursor_column_num,
       rstat.record_source_id,
       recsrc.parent_record_source_id,
       recsrc.level,
       recsrc.access_path,
       cast(sum(rstat.open_counter) as bigint) open_counter,
       min(rstat.open_min_elapsed_time) open_min_elapsed_time,
       max(rstat.open_max_elapsed_time) open_max_elapsed_time,
       cast(sum(rstat.open_total_elapsed_time) as bigint) open_total_elapsed_time,
       cast(sum(rstat.open_total_elapsed_time) / nullif(sum(rstat.open_counter), 0) as bigint) open_avg_elapsed_time,
       cast(sum(rstat.fetch_counter) as bigint) fetch_counter,
       min(rstat.fetch_min_elapsed_time) fetch_min_elapsed_time,
       max(rstat.fetch_max_elapsed_time) fetch_max_elapsed_time,
       cast(sum(rstat.fetch_total_elapsed_time) as bigint) fetch_total_elapsed_time,
       cast(sum(rstat.fetch_total_elapsed_time) / nullif(sum(rstat.fetch_counter), 0) as bigint) fetch_avg_elapsed_time,
       cast(coalesce(sum(rstat.open_total_elapsed_time), 0) + coalesce(sum(rstat.fetch_total_elapsed_time), 0) as bigint) open_fetch_total_elapsed_time
  from plg$prof_record_source_stats rstat
  join plg$prof_cursors cur
    on cur.profile_id = rstat.profile_id and
       cur.statement_id = rstat.statement_id and
       cur.cursor_id = rstat.cursor_id
  join plg$prof_record_sources recsrc
    on recsrc.profile_id = rstat.profile_id and
       recsrc.statement_id = rstat.statement_id and
       recsrc.cursor_id = rstat.cursor_id and
       recsrc.record_source_id = rstat.record_source_id
  join plg$prof_statements sta
    on sta.profile_id = rstat.profile_id and
       sta.statement_id = rstat.statement_id
  left join plg$prof_statements sta_parent
    on sta_parent.profile_id = sta.profile_id and
       sta_parent.statement_id = sta.parent_statement_id
  group by rstat.profile_id,
           rstat.statement_id,
           sta.statement_type,
           sta.schema_name,
           sta.package_name,
           sta.routine_name,
           sta.parent_statement_id,
           sta_parent.statement_type,
           sta_parent.routine_name,
           rstat.cursor_id,
           cur.name,
           cur.line_num,
           cur.column_num,
           rstat.record_source_id,
           recsrc.parent_record_source_id,
           recsrc.level,
           recsrc.access_path
  order by coalesce(sum(rstat.open_total_elapsed_time), 0) + coalesce(sum(rstat.fetch_total_elapsed_time), 0) desc
```
