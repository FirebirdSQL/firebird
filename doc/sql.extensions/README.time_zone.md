# Time Zone support (FB 4.0)

Time zone support consists of `TIME WITH TIME ZONE` and `TIMESTAMP WITH TIME ZONE` data types,
expressions and statements to work with time zones and conversion between data types without/with time zones.

The first important thing to understand is that `TIME WITHOUT TIME ZONE`, `TIMESTAMP WITHOUT TIME ZONE` and `DATE`
data types are defined to use the session time zone when converting from or to a `TIME WITH TIME ZONE` or `TIMESTAMP WITH TIME ZONE`.
`TIME` and `TIMESTAMP` are synonymous to theirs respectively `WITHOUT TIME ZONE` data types.

The session time zone, as the name implies, can be a different one for each database attachment.
It can be set (with this priority) using `isc_dpb_session_time_zone` DPB, the client's `firebird.conf`
parameter `DefaultTimeZone` and the server's `firebird.conf` parameter `DefaultTimeZone`. If none of these are set,
it starts using the same time zone used by the Firebird engine OS process.
A change in `DefaultTimeZone` configuration or the OS time zone does not changes the default of a running Firebird process.

The session time zone can be changed with `SET TIME ZONE` statement to a given time zone or reset to its original value
with `SET TIME ZONE LOCAL`.

The original time zone value is initially defined equal to the current time zone in session initialization and cannot
be changed manually. But the original time zone is internally changed when a routine (function, procedure or trigger)
is called to the value of the current time zone and restored to its previous value at routine exit. That means that
a routine that changes the current time zone and later run `SET TIME ZONE LOCAL` will restore the current time zone
to its initially received value.

A time zone may be a string with a time zone region (for example, `America/Sao_Paulo`) or a hours:minutes displacement
(for example, `-03:00` or `+3:0`) from GMT.

A time/timestamp with time zone is considered equal to another time/timestamp with time zone if their conversion to UTC
are equal, for example, `time '10:00 -02:00' = time '09:00 -03:00'`, since both are the same as `time '12:00 GMT'`.
This is also valid in the context of `UNIQUE` constraints and for sorting purposes.

Some timestamps does not exist (DST starting) or repeats twice (DST ending). For the first case, when DST starts
in America/New_York, 2:30 AM on March 12, 2017 does not exist and is interpreted as 2:30 AM UTC-05 (equivalent to
3:30 AM UTC-04). For the second case, when DST ends in America/New_York, 1:30 AM on November 5, 2017 repeats twice and
is interpreted as 1:30 AM UTC-04 instead of 1:30 AM UTC-05.

`EXTENDED TIME/TIMESTAMP WITH TIME ZONE` are intended for use only when communicating with clients,
they solve a problem of representing correct time on clients missing ICU library. One can't use extended
datatypes in tables, procedures, etc. The only way to use that datatypes is datatype coercion including
SET BIND statement (see [README.set_bind](./README.set_bind.md) for further details).


## Data types

```
TIME [ { WITH | WITHOUT } TIME ZONE ]

TIMESTAMP [ { WITH | WITHOUT } TIME ZONE ]

EXTENDED { TIME | TIMESTAMP } WITH TIME ZONE
```

## Region-based `TIME WITH TIME ZONE` semantics

By definition region-based time zones depends on a moment (date and time - or timestamp) to
know its UTC offset in relation to GMT.
But Firebird also supports region-based time zones in `TIME WITH TIME ZONE` values.

When constructing a `TIME WITH TIME ZONE` value from a literal or conversion its UTC value must
be computed and cannot be changed, so the current date may not be used. In this case the fixed date
`2020-01-01` is used. So when comparing `TIME WITH TIME ZONE` with different time zones the
comparation is done in a manner similar to they being `TIMESTAMP WITH TIME ZONE` values in the
given date.

However when converting between `TIMESTAMP` types to `TIME WITH TIME ZONE` that fixed date is
not used, otherwise some weird conversions may be seen when the current date has a different
offset (due to DST changes) than one in `2020-01-01`. In this case when converting
a `TIME WITH TIME ZONE` to `TIMESTAMP WITH TIME ZONE` the time portion is maintained
(if possible). For example, if current date is `2020-05-03` the effective offset in time zone
`America/Los_Angeles` is `-420` while its effective offset in `2020-01-01` is `-480`, but
`cast(time '10:00:00 America/Los_Angeles' as timestamp with time zone)` will result in
`2020-05-03 10:00:00.0000 America/Los_Angeles` instead of having the time portion adjusted.

But in a date when DST starts there is a missing hour, for example in `America/Los_Angeles`
in `2021-03-14` which there is no `02:00:00` to `02:59:59` hours. In this case the conversion
is done like constructing a literal and the hour is adjusted to its next valid value.
For example, in `2021-03-14` a `cast(time '02:10:00 America/Los_Angeles' as timestamp with time zone)`
will result in `2021-03-14 03:10:00.0000 America/Los_Angeles`.

### Storage

TIME/TIMESTAMP WITH TIME ZONE has respectively the same storage of TIME/TIMESTAMP WITHOUT TIME ZONE
plus 2 bytes for the time zone identifier or displacement.

The time/timestamp parts are stored in UTC (translated from the informed time zone).

Time zone identifiers (from regions) are put directly in the time_zone field.
They start from 65535 (which is the GMT code) and are decreasing as new time zones were/are added.

Time zone displacements (+/- hours:minutes) are encoded with `(sign * (hours * 60 + minutes)) + 1439`.
For example, a `+00:00` displacement is encoded as `(1 * (0 * 60 + 0)) + 1439 = 1439` and `-02:00` as `(-1 * (2 * 60 + 0)) + 1439 = 1319`.

EXTENDED TIME/TIMESTAMP WITH TIME ZONE have additionally more 2 bytes always containing absolute
time zone offset in minutes.

### API structs

```
struct ISC_TIME_TZ
{
    ISC_TIME utc_time;
    ISC_USHORT time_zone;
};

struct ISC_TIMESTAMP_TZ
{
    ISC_TIMESTAMP utc_timestamp;
    ISC_USHORT time_zone;
};

struct ISC_TIME_TZ_EX
{
    ISC_TIME utc_time;
    ISC_USHORT time_zone;
    ISC_SHORT ext_offset;
};

struct ISC_TIMESTAMP_TZ_EX
{
    ISC_TIMESTAMP utc_timestamp;
    ISC_USHORT time_zone;
    ISC_SHORT ext_offset;
};
```

### API functions (FirebirdInterface.idl - IUtil interface)

```
void decodeTimeTz(
    Status status,
    const ISC_TIME_TZ* timeTz,
    uint* hours,
    uint* minutes,
    uint* seconds,
    uint* fractions,
    uint timeZoneBufferLength,
    string timeZoneBuffer
);

void decodeTimeStampTz(
    Status status,
    const ISC_TIMESTAMP_TZ* timeStampTz,
    uint* year,
    uint* month,
    uint* day,
    uint* hours,
    uint* minutes,
    uint* seconds,
    uint* fractions,
    uint timeZoneBufferLength,
    string timeZoneBuffer
);

void encodeTimeTz(
    Status status,
    ISC_TIME_TZ* timeTz,
    uint hours,
    uint minutes,
    uint seconds,
    uint fractions,
    const string timeZone
);

void encodeTimeStampTz(
    Status status,
    ISC_TIMESTAMP_TZ* timeStampTz,
    uint year,
    uint month,
    uint day,
    uint hours,
    uint minutes,
    uint seconds,
    uint fractions,
    const string timeZone
);

void decodeTimeTzEx(
    Status status,
    const ISC_TIME_TZ_EX* timeTzEx,
    uint* hours,
    uint* minutes,
    uint* seconds,
    uint* fractions,
    uint timeZoneBufferLength,
    string timeZoneBuffer
);

void decodeTimeStampTzEx(
    Status status,
    const ISC_TIMESTAMP_TZ_EX* timeStampTzEx,
    uint* year,
    uint* month,
    uint* day,
    uint* hours,
    uint* minutes,
    uint* seconds,
    uint* fractions,
    uint timeZoneBufferLength,
    string timeZoneBuffer
);

```

When `decodeTimeTz` / `decodeTimeStampTz` is called with non-null `timeZoneBuffer` and ICU
could not be loaded in the client, `timeZoneBuffer` returns the string `GMT*` and the others fields
receives the timestamp GMT values.

When `decodeTimeTzEx` / `decodeTimeStampTzEx` is called with non-null `timeZoneBuffer` and ICU
could not be loaded in the client, `timeZoneBuffer` returns the string `+/-HH:MM` and the other
fields are set using specified `ext_offset`.

## Time zone string syntax

```
<time zone string> ::=
    '<time zone>'

<time zone> ::=
    <time zone region> |
    {+ | -} <hours displacement> : <minutes displacement>
```

Examples:

- `'America/Sao_Paulo'`
- `'-02:00'`
- `'+04:00'`
- `'+4:0'`
- `'-04:30'`

## `TIME WITH TIME ZONE` and `TIMESTAMP WITH TIME ZONE` literals

```
<time with time zone literal> ::=
    time '<time> <time zone>'

<timestamp with time zone literal> ::=
    timestamp '<timestamp> <time zone>'
```

Examples:

- `time '10:00 America/Los_Angeles'`
- `time '10:00:00.5 +08:00'`
- `timestamp '2018-01-01 10:00 America/Los_Angeles'`
- `timestamp '2018-01-01 10:00:00.5 +08:00'`

## Statements and expressions

### `SET TIME ZONE` statement

Changes the session time zone.

#### Syntax

```
SET TIME ZONE { <time zone string> | LOCAL }
```

#### Examples

```
set time zone '-02:00';

set time zone 'America/Sao_Paulo';

set time zone local;
```

### `AT` expression

Translates a time/timestamp value to its correspondent value in another time zone.

If `LOCAL` is used, the value is converted to the session time zone.

#### Syntax

```
<at expr> ::=
    <expr> AT { TIME ZONE <time zone string> | LOCAL }
```

#### Examples

```
select time '12:00 GMT' at time zone '-03:00'
  from rdb$database;

select current_timestamp at time zone 'America/Sao_Paulo'
  from rdb$database;

select timestamp '2018-01-01 12:00 GMT' at local
  from rdb$database;
```

### `EXTRACT` expressions

Two new `EXTRACT` expressions has been added:
- `TIMEZONE_HOUR`: extracts the time zone hours displacement
- `TIMEZONE_MINUTE`: extracts the time zone minutes displacement
- `TIMEZONE_NAME`: extracts the time zone region or displacement string

#### Examples

```
select extract(timezone_hour from current_time)
  from rdb$database;

select extract(timezone_minute from current_timestamp)
  from rdb$database;

select extract(timezone_name from current_timestamp)
  from rdb$database;
```

### `LOCALTIME` expression

Returns the current time as a `TIME WITHOUT TIME ZONE`, i.e., in the session time zone.

#### Example

```
select localtime
  from rdb$database;
```

### `LOCALTIMESTAMP` expression

Returns the current timestamp as a `TIMESTAMP WITHOUT TIME ZONE`, i.e., in the session time zone.

#### Example

```
select localtimestamp
  from rdb$database;
```

### `SESSION_TIMEZONE` context variable

`RDB$GET_CONTEXT('SYSTEM', 'SESSION_TIMEZONE')` could be used to obtain the session current time zone.

#### Examples

```
set time zone 'america/sao_paulo';
select rdb$get_context('SYSTEM', 'SESSION_TIMEZONE') from rdb$database;
-- Result: America/Sao_Paulo

set time zone '-3:00';
-- Result: -03:00
```

# Changes in `CURRENT_TIME` and `CURRENT_TIMESTAMP`

In version 4.0, `CURRENT_TIME` and `CURRENT_TIMESTAMP` are changed to return `TIME WITH TIME ZONE` and `TIMESTAMP WITH TIME ZONE` (with time zone set to the session time zone), different than previous versions, that returned the types without time zone.

To make transition easier, `LOCALTIME` and `LOCALTIMESTAMP` was added in v3.0.4, so applications can be adjusted in v3 and migrated to v4 without funcional changes.

## Virtual table `RDB$TIME_ZONES`

This virtual table lists time zones supported in the engine.

Columns:
- `RDB$TIME_ZONE_ID` type `INTEGER`
- `RDB$TIME_ZONE_NAME` type `CHAR(63)`

## Package `RDB$TIME_ZONE_UTIL`

This package has time zone utility functions and procedures.

### Function `DATABASE_VERSION`

`RDB$TIME_ZONE_UTIL.DATABASE_VERSION` returns the time zone database version.

Return type: `VARCHAR(10) CHARACTER SET ASCII`

```
select rdb$time_zone_util.database_version()
  from rdb$database;
```

Returns:
```
DATABASE_VERSION
================
2020d
```

### Procedure `TRANSITIONS`

`RDB$TIME_ZONE_UTIL.TRANSITIONS` returns the set of rules between the start and end timestamps.

Input parameters:
 - `RDB$TIME_ZONE_NAME` type `CHAR(63)`
 - `RDB$FROM_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE`
 - `RDB$TO_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE`

 Output parameters:
 - `RDB$START_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - the transition' start timestamp
 - `RDB$END_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - the transition's end timestamp
 - `RDB$ZONE_OFFSET` type `SMALLINT` - number of minutes related to the zone's offset
 - `RDB$DST_OFFSET` type `SMALLINT` - number of minutes related to the zone's DST offset
 - `RDB$EFFECTIVE_OFFSET` type `SMALLINT` - effective offset (`ZONE_OFFSET + DST_OFFSET`)

```
select *
  from rdb$time_zone_util.transitions(
    'America/Sao_Paulo',
    timestamp '2017-01-01',
    timestamp '2019-01-01');
```

Returns:

```
         RDB$START_TIMESTAMP            RDB$END_TIMESTAMP RDB$ZONE_OFFSET RDB$DST_OFFSET RDB$EFFECTIVE_OFFSET
============================ ============================ =============== ============== ====================
2016-10-16 03:00:00.0000 GMT 2017-02-19 01:59:59.9999 GMT            -180             60                 -120
2017-02-19 02:00:00.0000 GMT 2017-10-15 02:59:59.9999 GMT            -180              0                 -180
2017-10-15 03:00:00.0000 GMT 2018-02-18 01:59:59.9999 GMT            -180             60                 -120
2018-02-18 02:00:00.0000 GMT 2018-10-21 02:59:59.9999 GMT            -180              0                 -180
2018-10-21 03:00:00.0000 GMT 2019-02-17 01:59:59.9999 GMT            -180             60                 -120
```

# Updating the time zone database

Firebird uses the [IANA time zone database](http://www.iana.org/time-zones) through the ICU library.

When a Firebird version is released it's released with the most up-to-date time zone database but with the time it may become outdated.

An updated database can be found in [this Firebird's github page](https://github.com/FirebirdSQL/firebird/tree/master/extern/icu/tzdata). `le.zip` stands for little-endian and is the necessary file for most computer architectures (Intel/AMD compatible x86 or x64). `be.zip` stands for big-endian architectures.

The content of the zip file must be extracted in the `tzdata` subdirectory of Firebird's root, overwriting the others `*.res` files of the old database.

Note: `<firebird root>/tzdata` is the default directory where Firebird looks for the database. It could be overriden with the `ICU_TIMEZONE_FILES_DIR` environment variable.

Important note: Firebird stores `WITH TIME ZONE` values translated to UTC time. If a value is created with one time zone database and later that database is updated and the update changes the information in the range of a stored value, when reading that value it will be returned as a different value than the one initially stored.


Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>
