# Time Zone support (FB 4.0)

Time zone support consists of `TIME WITH TIME ZONE` and `TIMESTAMP WITH TIME ZONE` data types,
expressions and statements to work with time zones and conversion between data types without/with time zones.

The first important thing to understand is that `TIME WITHOUT TIME ZONE`, `TIMESTAMP WITHOUT TIME ZONE` and `DATE`
data types are defined to use the session time zone when converting from or to a `TIME WITH TIME ZONE` or `TIMESTAMP WITH TIME ZONE`.
`TIME` and `TIMESTAMP` are synonymous to theirs respectively `WITHOUT TIME ZONE` data types.

The session time zone, as the name implies, can be a different one for each database attachment.
It can be set with the isc_dpb_session_time_zone DPB, and if not, it starts by default defined to be the
`firebird.conf` parameter `DefaultTimeZone` or the same time zone used by the Firebird OS process when the parameter is not defined.
A change in `DefaultTimeZone` configuration or the OS time zone does not changes the default of a running Firebird process.

It can then be changed with `SET TIME ZONE` statement to a given time zone or reset to its original value with `SET TIME ZONE LOCAL`.

The original time zone value is initially defined equal to the current time zone in session initialization and cannot
be changed manually. But the original time zone is internally changed when a routine (function, procedure or trigger)
is called to the value of the current time zone and restored to its previous value at routine exit. That means that
a routine that changes the current time zone and later run `SET TIME ZONE LOCAL` will restore the current time zone
to its initially received value.

A time zone may be a string with a time zone region (for example, `America/Sao_Paulo`) or a hours:minutes displacement
(for example, `-03:00`) from GMT.

A time/timestamp with time zone is considered equal to another time/timestamp with time zone if their conversion to UTC
are equal, for example, `time '10:00 -02' = time '09:00 -03'`, since both are the same as `time '12:00 GMT'`.
This is also valid in the context of `UNIQUE` constraints and for sorting purposes.

Some timestamps does not exist (DST starting) or repeats twice (DST ending). For the first case, when DST starts
in America/New_York, 2:30 AM on March 12, 2017 does not exist and is interpreted as 2:30 AM UTC-05 (equivalent to
3:30 AM UTC-04). For the second case, when DST ends in America/New_York, 1:30 AM on November 5, 2017 repeats twice and
is interpreted as 1:30 AM UTC-04 instead of 1:30 AM UTC-05.

`EXTENDED TIME/TIMESTAMP WITH TIME ZONE` are intended for use only when communicating with a cliens,
they solve a problem of representing correct time on clients missing ICU library. One can't use extended
datatypes in tables, procedures, etc. The only way to use that datatypes is datatype coercion including
SET BIND statement (see [README.set_bind](./README.set_bind.md) for further details).


## Data types

```
TIME [ { WITH | WITHOUT } TIME ZONE ]

TIMESTAMP [ { WITH | WITHOUT } TIME ZONE ]

EXTENDED { TIME | TIMESTAMP } WITH TIME ZONE
```

### Storage

TIME/TIMESTAMP WITH TIME ZONE has respectively the same storage of TIME/TIMESTAMP WITHOUT TIME ZONE
plus 2 bytes for the time zone identifier or displacement.

The time/timestamp parts are stored in UTC (translated from the informed time zone).

Time zone identifiers (from regions) are put directly in the time_zone field.
They start from 65535 (which is the GMT code) and are decreasing as new time zones were/are added.

Time zone displacements (+/- HH:MM) are encoded with `(sign * (HH * 60 + MM)) + 1439`.
For example, a `00:00` displacement is encoded as `(1 * (0 * 60 + 0)) + 1439 = 1439` and `-02:00` as `(-1 * (2 * 60 + 0)) + 1439 = 1319`.

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
    [+/-] <hour displacement> [: <minute displacement>]
```

Examples:

- `'America/Sao_Paulo'`
- `'-02:00'`
- `'+04'`
- `'04:00'`
- `'04:30'`

## `TIME WITH TIME ZONE` and `TIMESTAMP WITH TIME ZONE` literals

```
<time with time zone literal> ::=
    time '<time> <time zone>'

<timestamp with time zone literal> ::=
    timestamp '<timestamp> <time zone>'
```

Examples:

- `time '10:00 America/Los_Angeles'`
- `time '10:00:00.5 +08'`
- `timestamp '2018-01-01 10:00 America/Los_Angeles'`
- `timestamp '2018-01-01 10:00:00.5 +08'`

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
select time '12:00 GMT' at time zone '-03'
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

#### Examples

```
select extract(timezone_hour from current_time)
  from rdb$database;

select extract(timezone_minute from current_timestamp)
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
2017c
```

### Procedure `TRANSITIONS`

`RDB$TIME_ZONE_UTIL.TRANSITIONS` returns the set of rules between the start and end timestamps.

Input parameters:
 - `TIME_ZONE_NAME` type `CHAR(63)`
 - `FROM_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE`
 - `TO_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE`

 Output parameters:
 - `START_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - the transition' start timestamp
 - `END_TIMESTAMP` type `TIMESTAMP WITH TIME ZONE` - the transition's end timestamp
 - `ZONE_OFFSET` type `SMALLINT` - number of minutes related to the zone's offset
 - `DST_OFFSET` type `SMALLINT` - number of minutes related to the zone's DST offset
 - `EFFECTIVE_OFFSET` type `SMALLINT` - effective offset (`ZONE_OFFSET + DST_OFFSET`)

```
select *
  from rdb$time_zone_util.transitions(
    'America/Sao_Paulo',
    timestamp '2017-01-01',
    timestamp '2019-01-01');
```

Returns:

```
             START_TIMESTAMP                END_TIMESTAMP ZONE_OFFSET DST_OFFSET EFFECTIVE_OFFSET
============================ ============================ =========== ========== ================
2016-10-16 03:00:00.0000 GMT 2017-02-19 01:59:59.9999 GMT        -180         60             -120
2017-02-19 02:00:00.0000 GMT 2017-10-15 02:59:59.9999 GMT        -180          0             -180
2017-10-15 03:00:00.0000 GMT 2018-02-18 01:59:59.9999 GMT        -180         60             -120
2018-02-18 02:00:00.0000 GMT 2018-10-21 02:59:59.9999 GMT        -180          0             -180
2018-10-21 03:00:00.0000 GMT 2019-02-17 01:59:59.9999 GMT        -180         60             -120
```

# Updating the time zone database

Firebird uses the [IANA time zone database](http://www.iana.org/time-zones) through the ICU library.

When a Firebird version is released it's released with the most up-to-date time zone database but with the time it may become outdated.

An updated database can be found in [this Firebird's github page](https://github.com/FirebirdSQL/firebird/tree/master/tzdata). `le.zip` stands for little-endian and is the necessary file for most computer architectures (Intel/AMD compatible x86 or x64). `be.zip` stands for big-endian architectures.

The content of the zip file must be extracted in the `tzdata` subdirectory of Firebird's root, overwriting the others `*.res` files of the old database.

Note: `<firebird root>/tzdata` is the default directory where Firebird looks for the database. It could be overriden with the `ICU_TIMEZONE_FILES_DIR` environment variable.

Important note: Firebird stores `WITH TIME ZONE` values translated to UTC time. If a value is created with one time zone database and later that database is updated and the update changes the information in the range of a stored value, when reading that value it will be returned as a different value than the one initially stored.


# Appendix: time zone regions

Below is the list of time zone region names and IDs.

- GMT (65535)
- ACT (65534)
- AET (65533)
- AGT (65532)
- ART (65531)
- AST (65530)
- Africa/Abidjan (65529)
- Africa/Accra (65528)
- Africa/Addis_Ababa (65527)
- Africa/Algiers (65526)
- Africa/Asmara (65525)
- Africa/Asmera (65524)
- Africa/Bamako (65523)
- Africa/Bangui (65522)
- Africa/Banjul (65521)
- Africa/Bissau (65520)
- Africa/Blantyre (65519)
- Africa/Brazzaville (65518)
- Africa/Bujumbura (65517)
- Africa/Cairo (65516)
- Africa/Casablanca (65515)
- Africa/Ceuta (65514)
- Africa/Conakry (65513)
- Africa/Dakar (65512)
- Africa/Dar_es_Salaam (65511)
- Africa/Djibouti (65510)
- Africa/Douala (65509)
- Africa/El_Aaiun (65508)
- Africa/Freetown (65507)
- Africa/Gaborone (65506)
- Africa/Harare (65505)
- Africa/Johannesburg (65504)
- Africa/Juba (65503)
- Africa/Kampala (65502)
- Africa/Khartoum (65501)
- Africa/Kigali (65500)
- Africa/Kinshasa (65499)
- Africa/Lagos (65498)
- Africa/Libreville (65497)
- Africa/Lome (65496)
- Africa/Luanda (65495)
- Africa/Lubumbashi (65494)
- Africa/Lusaka (65493)
- Africa/Malabo (65492)
- Africa/Maputo (65491)
- Africa/Maseru (65490)
- Africa/Mbabane (65489)
- Africa/Mogadishu (65488)
- Africa/Monrovia (65487)
- Africa/Nairobi (65486)
- Africa/Ndjamena (65485)
- Africa/Niamey (65484)
- Africa/Nouakchott (65483)
- Africa/Ouagadougou (65482)
- Africa/Porto-Novo (65481)
- Africa/Sao_Tome (65480)
- Africa/Timbuktu (65479)
- Africa/Tripoli (65478)
- Africa/Tunis (65477)
- Africa/Windhoek (65476)
- America/Adak (65475)
- America/Anchorage (65474)
- America/Anguilla (65473)
- America/Antigua (65472)
- America/Araguaina (65471)
- America/Argentina/Buenos_Aires (65470)
- America/Argentina/Catamarca (65469)
- America/Argentina/ComodRivadavia (65468)
- America/Argentina/Cordoba (65467)
- America/Argentina/Jujuy (65466)
- America/Argentina/La_Rioja (65465)
- America/Argentina/Mendoza (65464)
- America/Argentina/Rio_Gallegos (65463)
- America/Argentina/Salta (65462)
- America/Argentina/San_Juan (65461)
- America/Argentina/San_Luis (65460)
- America/Argentina/Tucuman (65459)
- America/Argentina/Ushuaia (65458)
- America/Aruba (65457)
- America/Asuncion (65456)
- America/Atikokan (65455)
- America/Atka (65454)
- America/Bahia (65453)
- America/Bahia_Banderas (65452)
- America/Barbados (65451)
- America/Belem (65450)
- America/Belize (65449)
- America/Blanc-Sablon (65448)
- America/Boa_Vista (65447)
- America/Bogota (65446)
- America/Boise (65445)
- America/Buenos_Aires (65444)
- America/Cambridge_Bay (65443)
- America/Campo_Grande (65442)
- America/Cancun (65441)
- America/Caracas (65440)
- America/Catamarca (65439)
- America/Cayenne (65438)
- America/Cayman (65437)
- America/Chicago (65436)
- America/Chihuahua (65435)
- America/Coral_Harbour (65434)
- America/Cordoba (65433)
- America/Costa_Rica (65432)
- America/Creston (65431)
- America/Cuiaba (65430)
- America/Curacao (65429)
- America/Danmarkshavn (65428)
- America/Dawson (65427)
- America/Dawson_Creek (65426)
- America/Denver (65425)
- America/Detroit (65424)
- America/Dominica (65423)
- America/Edmonton (65422)
- America/Eirunepe (65421)
- America/El_Salvador (65420)
- America/Ensenada (65419)
- America/Fort_Nelson (65418)
- America/Fort_Wayne (65417)
- America/Fortaleza (65416)
- America/Glace_Bay (65415)
- America/Godthab (65414)
- America/Goose_Bay (65413)
- America/Grand_Turk (65412)
- America/Grenada (65411)
- America/Guadeloupe (65410)
- America/Guatemala (65409)
- America/Guayaquil (65408)
- America/Guyana (65407)
- America/Halifax (65406)
- America/Havana (65405)
- America/Hermosillo (65404)
- America/Indiana/Indianapolis (65403)
- America/Indiana/Knox (65402)
- America/Indiana/Marengo (65401)
- America/Indiana/Petersburg (65400)
- America/Indiana/Tell_City (65399)
- America/Indiana/Vevay (65398)
- America/Indiana/Vincennes (65397)
- America/Indiana/Winamac (65396)
- America/Indianapolis (65395)
- America/Inuvik (65394)
- America/Iqaluit (65393)
- America/Jamaica (65392)
- America/Jujuy (65391)
- America/Juneau (65390)
- America/Kentucky/Louisville (65389)
- America/Kentucky/Monticello (65388)
- America/Knox_IN (65387)
- America/Kralendijk (65386)
- America/La_Paz (65385)
- America/Lima (65384)
- America/Los_Angeles (65383)
- America/Louisville (65382)
- America/Lower_Princes (65381)
- America/Maceio (65380)
- America/Managua (65379)
- America/Manaus (65378)
- America/Marigot (65377)
- America/Martinique (65376)
- America/Matamoros (65375)
- America/Mazatlan (65374)
- America/Mendoza (65373)
- America/Menominee (65372)
- America/Merida (65371)
- America/Metlakatla (65370)
- America/Mexico_City (65369)
- America/Miquelon (65368)
- America/Moncton (65367)
- America/Monterrey (65366)
- America/Montevideo (65365)
- America/Montreal (65364)
- America/Montserrat (65363)
- America/Nassau (65362)
- America/New_York (65361)
- America/Nipigon (65360)
- America/Nome (65359)
- America/Noronha (65358)
- America/North_Dakota/Beulah (65357)
- America/North_Dakota/Center (65356)
- America/North_Dakota/New_Salem (65355)
- America/Ojinaga (65354)
- America/Panama (65353)
- America/Pangnirtung (65352)
- America/Paramaribo (65351)
- America/Phoenix (65350)
- America/Port-au-Prince (65349)
- America/Port_of_Spain (65348)
- America/Porto_Acre (65347)
- America/Porto_Velho (65346)
- America/Puerto_Rico (65345)
- America/Punta_Arenas (65344)
- America/Rainy_River (65343)
- America/Rankin_Inlet (65342)
- America/Recife (65341)
- America/Regina (65340)
- America/Resolute (65339)
- America/Rio_Branco (65338)
- America/Rosario (65337)
- America/Santa_Isabel (65336)
- America/Santarem (65335)
- America/Santiago (65334)
- America/Santo_Domingo (65333)
- America/Sao_Paulo (65332)
- America/Scoresbysund (65331)
- America/Shiprock (65330)
- America/Sitka (65329)
- America/St_Barthelemy (65328)
- America/St_Johns (65327)
- America/St_Kitts (65326)
- America/St_Lucia (65325)
- America/St_Thomas (65324)
- America/St_Vincent (65323)
- America/Swift_Current (65322)
- America/Tegucigalpa (65321)
- America/Thule (65320)
- America/Thunder_Bay (65319)
- America/Tijuana (65318)
- America/Toronto (65317)
- America/Tortola (65316)
- America/Vancouver (65315)
- America/Virgin (65314)
- America/Whitehorse (65313)
- America/Winnipeg (65312)
- America/Yakutat (65311)
- America/Yellowknife (65310)
- Antarctica/Casey (65309)
- Antarctica/Davis (65308)
- Antarctica/DumontDUrville (65307)
- Antarctica/Macquarie (65306)
- Antarctica/Mawson (65305)
- Antarctica/McMurdo (65304)
- Antarctica/Palmer (65303)
- Antarctica/Rothera (65302)
- Antarctica/South_Pole (65301)
- Antarctica/Syowa (65300)
- Antarctica/Troll (65299)
- Antarctica/Vostok (65298)
- Arctic/Longyearbyen (65297)
- Asia/Aden (65296)
- Asia/Almaty (65295)
- Asia/Amman (65294)
- Asia/Anadyr (65293)
- Asia/Aqtau (65292)
- Asia/Aqtobe (65291)
- Asia/Ashgabat (65290)
- Asia/Ashkhabad (65289)
- Asia/Atyrau (65288)
- Asia/Baghdad (65287)
- Asia/Bahrain (65286)
- Asia/Baku (65285)
- Asia/Bangkok (65284)
- Asia/Barnaul (65283)
- Asia/Beirut (65282)
- Asia/Bishkek (65281)
- Asia/Brunei (65280)
- Asia/Calcutta (65279)
- Asia/Chita (65278)
- Asia/Choibalsan (65277)
- Asia/Chongqing (65276)
- Asia/Chungking (65275)
- Asia/Colombo (65274)
- Asia/Dacca (65273)
- Asia/Damascus (65272)
- Asia/Dhaka (65271)
- Asia/Dili (65270)
- Asia/Dubai (65269)
- Asia/Dushanbe (65268)
- Asia/Famagusta (65267)
- Asia/Gaza (65266)
- Asia/Harbin (65265)
- Asia/Hebron (65264)
- Asia/Ho_Chi_Minh (65263)
- Asia/Hong_Kong (65262)
- Asia/Hovd (65261)
- Asia/Irkutsk (65260)
- Asia/Istanbul (65259)
- Asia/Jakarta (65258)
- Asia/Jayapura (65257)
- Asia/Jerusalem (65256)
- Asia/Kabul (65255)
- Asia/Kamchatka (65254)
- Asia/Karachi (65253)
- Asia/Kashgar (65252)
- Asia/Kathmandu (65251)
- Asia/Katmandu (65250)
- Asia/Khandyga (65249)
- Asia/Kolkata (65248)
- Asia/Krasnoyarsk (65247)
- Asia/Kuala_Lumpur (65246)
- Asia/Kuching (65245)
- Asia/Kuwait (65244)
- Asia/Macao (65243)
- Asia/Macau (65242)
- Asia/Magadan (65241)
- Asia/Makassar (65240)
- Asia/Manila (65239)
- Asia/Muscat (65238)
- Asia/Nicosia (65237)
- Asia/Novokuznetsk (65236)
- Asia/Novosibirsk (65235)
- Asia/Omsk (65234)
- Asia/Oral (65233)
- Asia/Phnom_Penh (65232)
- Asia/Pontianak (65231)
- Asia/Pyongyang (65230)
- Asia/Qatar (65229)
- Asia/Qyzylorda (65228)
- Asia/Rangoon (65227)
- Asia/Riyadh (65226)
- Asia/Saigon (65225)
- Asia/Sakhalin (65224)
- Asia/Samarkand (65223)
- Asia/Seoul (65222)
- Asia/Shanghai (65221)
- Asia/Singapore (65220)
- Asia/Srednekolymsk (65219)
- Asia/Taipei (65218)
- Asia/Tashkent (65217)
- Asia/Tbilisi (65216)
- Asia/Tehran (65215)
- Asia/Tel_Aviv (65214)
- Asia/Thimbu (65213)
- Asia/Thimphu (65212)
- Asia/Tokyo (65211)
- Asia/Tomsk (65210)
- Asia/Ujung_Pandang (65209)
- Asia/Ulaanbaatar (65208)
- Asia/Ulan_Bator (65207)
- Asia/Urumqi (65206)
- Asia/Ust-Nera (65205)
- Asia/Vientiane (65204)
- Asia/Vladivostok (65203)
- Asia/Yakutsk (65202)
- Asia/Yangon (65201)
- Asia/Yekaterinburg (65200)
- Asia/Yerevan (65199)
- Atlantic/Azores (65198)
- Atlantic/Bermuda (65197)
- Atlantic/Canary (65196)
- Atlantic/Cape_Verde (65195)
- Atlantic/Faeroe (65194)
- Atlantic/Faroe (65193)
- Atlantic/Jan_Mayen (65192)
- Atlantic/Madeira (65191)
- Atlantic/Reykjavik (65190)
- Atlantic/South_Georgia (65189)
- Atlantic/St_Helena (65188)
- Atlantic/Stanley (65187)
- Australia/ACT (65186)
- Australia/Adelaide (65185)
- Australia/Brisbane (65184)
- Australia/Broken_Hill (65183)
- Australia/Canberra (65182)
- Australia/Currie (65181)
- Australia/Darwin (65180)
- Australia/Eucla (65179)
- Australia/Hobart (65178)
- Australia/LHI (65177)
- Australia/Lindeman (65176)
- Australia/Lord_Howe (65175)
- Australia/Melbourne (65174)
- Australia/NSW (65173)
- Australia/North (65172)
- Australia/Perth (65171)
- Australia/Queensland (65170)
- Australia/South (65169)
- Australia/Sydney (65168)
- Australia/Tasmania (65167)
- Australia/Victoria (65166)
- Australia/West (65165)
- Australia/Yancowinna (65164)
- BET (65163)
- BST (65162)
- Brazil/Acre (65161)
- Brazil/DeNoronha (65160)
- Brazil/East (65159)
- Brazil/West (65158)
- CAT (65157)
- CET (65156)
- CNT (65155)
- CST (65154)
- CST6CDT (65153)
- CTT (65152)
- Canada/Atlantic (65151)
- Canada/Central (65150)
- Canada/East-Saskatchewan (65149)
- Canada/Eastern (65148)
- Canada/Mountain (65147)
- Canada/Newfoundland (65146)
- Canada/Pacific (65145)
- Canada/Saskatchewan (65144)
- Canada/Yukon (65143)
- Chile/Continental (65142)
- Chile/EasterIsland (65141)
- Cuba (65140)
- EAT (65139)
- ECT (65138)
- EET (65137)
- EST (65136)
- EST5EDT (65135)
- Egypt (65134)
- Eire (65133)
- Etc/GMT (65132)
- Etc/GMT+0 (65131)
- Etc/GMT+1 (65130)
- Etc/GMT+10 (65129)
- Etc/GMT+11 (65128)
- Etc/GMT+12 (65127)
- Etc/GMT+2 (65126)
- Etc/GMT+3 (65125)
- Etc/GMT+4 (65124)
- Etc/GMT+5 (65123)
- Etc/GMT+6 (65122)
- Etc/GMT+7 (65121)
- Etc/GMT+8 (65120)
- Etc/GMT+9 (65119)
- Etc/GMT-0 (65118)
- Etc/GMT-1 (65117)
- Etc/GMT-10 (65116)
- Etc/GMT-11 (65115)
- Etc/GMT-12 (65114)
- Etc/GMT-13 (65113)
- Etc/GMT-14 (65112)
- Etc/GMT-2 (65111)
- Etc/GMT-3 (65110)
- Etc/GMT-4 (65109)
- Etc/GMT-5 (65108)
- Etc/GMT-6 (65107)
- Etc/GMT-7 (65106)
- Etc/GMT-8 (65105)
- Etc/GMT-9 (65104)
- Etc/GMT0 (65103)
- Etc/Greenwich (65102)
- Etc/UCT (65101)
- Etc/UTC (65100)
- Etc/Universal (65099)
- Etc/Zulu (65098)
- Europe/Amsterdam (65097)
- Europe/Andorra (65096)
- Europe/Astrakhan (65095)
- Europe/Athens (65094)
- Europe/Belfast (65093)
- Europe/Belgrade (65092)
- Europe/Berlin (65091)
- Europe/Bratislava (65090)
- Europe/Brussels (65089)
- Europe/Bucharest (65088)
- Europe/Budapest (65087)
- Europe/Busingen (65086)
- Europe/Chisinau (65085)
- Europe/Copenhagen (65084)
- Europe/Dublin (65083)
- Europe/Gibraltar (65082)
- Europe/Guernsey (65081)
- Europe/Helsinki (65080)
- Europe/Isle_of_Man (65079)
- Europe/Istanbul (65078)
- Europe/Jersey (65077)
- Europe/Kaliningrad (65076)
- Europe/Kiev (65075)
- Europe/Kirov (65074)
- Europe/Lisbon (65073)
- Europe/Ljubljana (65072)
- Europe/London (65071)
- Europe/Luxembourg (65070)
- Europe/Madrid (65069)
- Europe/Malta (65068)
- Europe/Mariehamn (65067)
- Europe/Minsk (65066)
- Europe/Monaco (65065)
- Europe/Moscow (65064)
- Europe/Nicosia (65063)
- Europe/Oslo (65062)
- Europe/Paris (65061)
- Europe/Podgorica (65060)
- Europe/Prague (65059)
- Europe/Riga (65058)
- Europe/Rome (65057)
- Europe/Samara (65056)
- Europe/San_Marino (65055)
- Europe/Sarajevo (65054)
- Europe/Saratov (65053)
- Europe/Simferopol (65052)
- Europe/Skopje (65051)
- Europe/Sofia (65050)
- Europe/Stockholm (65049)
- Europe/Tallinn (65048)
- Europe/Tirane (65047)
- Europe/Tiraspol (65046)
- Europe/Ulyanovsk (65045)
- Europe/Uzhgorod (65044)
- Europe/Vaduz (65043)
- Europe/Vatican (65042)
- Europe/Vienna (65041)
- Europe/Vilnius (65040)
- Europe/Volgograd (65039)
- Europe/Warsaw (65038)
- Europe/Zagreb (65037)
- Europe/Zaporozhye (65036)
- Europe/Zurich (65035)
- Factory (65034)
- GB (65033)
- GB-Eire (65032)
- GMT+0 (65031)
- GMT-0 (65030)
- GMT0 (65029)
- Greenwich (65028)
- HST (65027)
- Hongkong (65026)
- IET (65025)
- IST (65024)
- Iceland (65023)
- Indian/Antananarivo (65022)
- Indian/Chagos (65021)
- Indian/Christmas (65020)
- Indian/Cocos (65019)
- Indian/Comoro (65018)
- Indian/Kerguelen (65017)
- Indian/Mahe (65016)
- Indian/Maldives (65015)
- Indian/Mauritius (65014)
- Indian/Mayotte (65013)
- Indian/Reunion (65012)
- Iran (65011)
- Israel (65010)
- JST (65009)
- Jamaica (65008)
- Japan (65007)
- Kwajalein (65006)
- Libya (65005)
- MET (65004)
- MIT (65003)
- MST (65002)
- MST7MDT (65001)
- Mexico/BajaNorte (65000)
- Mexico/BajaSur (64999)
- Mexico/General (64998)
- NET (64997)
- NST (64996)
- NZ (64995)
- NZ-CHAT (64994)
- Navajo (64993)
- PLT (64992)
- PNT (64991)
- PRC (64990)
- PRT (64989)
- PST (64988)
- PST8PDT (64987)
- Pacific/Apia (64986)
- Pacific/Auckland (64985)
- Pacific/Bougainville (64984)
- Pacific/Chatham (64983)
- Pacific/Chuuk (64982)
- Pacific/Easter (64981)
- Pacific/Efate (64980)
- Pacific/Enderbury (64979)
- Pacific/Fakaofo (64978)
- Pacific/Fiji (64977)
- Pacific/Funafuti (64976)
- Pacific/Galapagos (64975)
- Pacific/Gambier (64974)
- Pacific/Guadalcanal (64973)
- Pacific/Guam (64972)
- Pacific/Honolulu (64971)
- Pacific/Johnston (64970)
- Pacific/Kiritimati (64969)
- Pacific/Kosrae (64968)
- Pacific/Kwajalein (64967)
- Pacific/Majuro (64966)
- Pacific/Marquesas (64965)
- Pacific/Midway (64964)
- Pacific/Nauru (64963)
- Pacific/Niue (64962)
- Pacific/Norfolk (64961)
- Pacific/Noumea (64960)
- Pacific/Pago_Pago (64959)
- Pacific/Palau (64958)
- Pacific/Pitcairn (64957)
- Pacific/Pohnpei (64956)
- Pacific/Ponape (64955)
- Pacific/Port_Moresby (64954)
- Pacific/Rarotonga (64953)
- Pacific/Saipan (64952)
- Pacific/Samoa (64951)
- Pacific/Tahiti (64950)
- Pacific/Tarawa (64949)
- Pacific/Tongatapu (64948)
- Pacific/Truk (64947)
- Pacific/Wake (64946)
- Pacific/Wallis (64945)
- Pacific/Yap (64944)
- Poland (64943)
- Portugal (64942)
- ROC (64941)
- ROK (64940)
- SST (64939)
- Singapore (64938)
- SystemV/AST4 (64937)
- SystemV/AST4ADT (64936)
- SystemV/CST6 (64935)
- SystemV/CST6CDT (64934)
- SystemV/EST5 (64933)
- SystemV/EST5EDT (64932)
- SystemV/HST10 (64931)
- SystemV/MST7 (64930)
- SystemV/MST7MDT (64929)
- SystemV/PST8 (64928)
- SystemV/PST8PDT (64927)
- SystemV/YST9 (64926)
- SystemV/YST9YDT (64925)
- Turkey (64924)
- UCT (64923)
- US/Alaska (64922)
- US/Aleutian (64921)
- US/Arizona (64920)
- US/Central (64919)
- US/East-Indiana (64918)
- US/Eastern (64917)
- US/Hawaii (64916)
- US/Indiana-Starke (64915)
- US/Michigan (64914)
- US/Mountain (64913)
- US/Pacific (64912)
- US/Pacific-New (64911)
- US/Samoa (64910)
- UTC (64909)
- Universal (64908)
- VST (64907)
- W-SU (64906)
- WET (64905)
- Zulu (64904)


Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>
