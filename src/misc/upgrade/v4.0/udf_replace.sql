/*
 *	PROGRAM:		Firebird functions migration.
 *	MODULE:			udf_replace.sql
 *	DESCRIPTION:	UDFs were deprecated in FB4 - first of all due to security reasons.
 *					This script alters UDFs from if_udf & fbudf to appropriate PSQL or UDR functions.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2019 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

set term ^;

execute block as begin

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'DIV')) then execute statement
'alter function div (
    n1 integer,
    n2 integer
) returns double precision
    external name ''udf_compat!UC_div''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'FRAC')) then execute statement
'alter function frac (
    val double precision
) returns double precision
    external name ''udf_compat!UC_frac''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'UDF_FRAC')) then execute statement
'alter function udf_frac (
    val double precision
) returns double precision
    external name ''udf_compat!UC_frac''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'DOW')) then execute statement
'alter function dow (
    val timestamp
) returns varchar(53) character set none
    external name ''udf_compat!UC_dow''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SDOW')) then execute statement
'alter function sdow (
    val timestamp
) returns varchar(13) character set none
    external name ''udf_compat!UC_sdow''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'GETEXACTTIMESTAMPUTC')) then execute statement
'alter function getExactTimestampUTC
	returns timestamp
    external name ''udf_compat!UC_getExactTimestampUTC''
    engine udr';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ISLEAPYEAR')) then begin
execute statement
'create or alter function booleanIsLeapYear (
    val timestamp
) returns boolean
    external name ''udf_compat!UC_isLeapYear''
    engine udr';
execute statement
'alter function isLeapYear (
    val timestamp
) returns int
as
begin
	if (booleanIsLeapYear(val)) then return 1;
	return 0;
end
';
end

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'LTRIM')) then execute statement
'
alter function ltrim (
	val varchar(255)
) returns varchar(255)
as
begin
	return trim(leading from val);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'RTRIM')) then execute statement
'
alter function rtrim (
	val varchar(255)
) returns varchar(255)
as
begin
	return trim(trailing from val);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SUBSTR')) then execute statement
'
alter function substr (
	val varchar(255),
	vfrom smallint,
	vto smallint
) returns varchar(255)
as
begin
	if (vto < vfrom) then
		return '''';
	return substring(val from vfrom for (vto + 1 - vfrom));
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SUBSTRLEN')) then execute statement
'
alter function substrlen (
	val varchar(255),
	vfrom smallint,
	vfor smallint
) returns varchar(255)
as
begin
	return substring(val from vfrom for vfor);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'STRLEN')) then execute statement
'
alter function strlen (
	val varchar(32764) character set none
) returns integer
as
begin
	return octet_length(val);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'INVL')) then execute statement
'
alter function invl (
	v1 int,
	v2 int
) returns int
as
begin
	return coalesce(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'I64NVL')) then execute statement
'
alter function i64nvl (
	v1 numeric(18,0),
	v2 numeric(18,0)
) returns numeric(18,0)
as
begin
	return coalesce(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'DNVL')) then execute statement
'
alter function dnvl (
	v1 double precision,
	v2 double precision
) returns double precision
as
begin
	return coalesce(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SNVL')) then execute statement
'
alter function snvl (
	v1 varchar(100),
	v2 varchar(100)
) returns varchar(100)
as
begin
	return coalesce(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'INULLIF')) then execute statement
'
alter function inullif (
	v1 int,
	v2 int
) returns int
as
begin
	return nullif(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'DNULLIF')) then execute statement
'
alter function dnullif (
	v1 double precision,
	v2 double precision
) returns double precision
as
begin
	return nullif(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'I64NULLIF')) then execute statement
'
alter function i64nullif (
	v1 numeric(18,4),
	v2 numeric(18,4)
) returns numeric(18,4)
as
begin
	return nullif(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SNULLIF')) then execute statement
'
alter function snullif (
	v1 varchar(100),
	v2 varchar(100)
) returns varchar(100)
as
begin
	return nullif(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'SRIGHT')) then execute statement
'
alter function sright (
	v1 varchar(100),
	v2 smallint
) returns varchar(100)
as
begin
	return right(v1, v2);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDDAY')) then execute statement
'
alter function addDay (
	v timestamp,
	ndays int
) returns timestamp
as
begin
	return dateadd(ndays day to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDDAY2')) then execute statement
'
alter function addDay2 (
	v timestamp,
	ndays int
) returns timestamp
as
begin
	return dateadd(ndays day to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDWEEK')) then execute statement
'
alter function addWeek (
    v timestamp,
    nweeks int
) returns timestamp
as
begin
	return dateadd(nweeks week to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDMONTH')) then execute statement
'
alter function addMonth (
    v timestamp,
    nmonths int
) returns timestamp
as
begin
	return dateadd(nmonths month to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDYEAR')) then execute statement
'
alter function addYear (
    v timestamp,
    nyears int
) returns timestamp
as
begin
	return dateadd(nyears year to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDMILLISECOND')) then execute statement
'
alter function addMilliSecond (
    v timestamp,
    nMilliseconds int
) returns timestamp
as
begin
	return dateadd(nMilliseconds millisecond to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDSECOND')) then execute statement
'
alter function addSecond (
    v timestamp,
    nseconds int
) returns timestamp
as
begin
	return dateadd(nseconds second to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDMINUTE')) then execute statement
'
alter function addMinute (
    v timestamp,
    nminutes int
) returns timestamp
as
begin
	return dateadd(nminutes minute to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ADDHOUR')) then execute statement
'
alter function addHour (
    v timestamp,
    nhours int
) returns timestamp
as
begin
	return dateadd(nhours hour to v);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'TRUNCATE')) then execute statement
'
alter function Truncate (
	v numeric(9, 2)
) returns integer
as
begin
	return cast (v - 0.5 as integer);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'I64TRUNCATE')) then execute statement
'
alter function i64Truncate (
	v numeric(18, 4)
) returns numeric(18)
as
begin
	return cast (v - 0.5 as bigint);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'ROUND')) then execute statement
'
alter function Round (
	v numeric(9, 2)
) returns integer
as
begin
	return cast (v as integer);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'I64ROUND')) then execute statement
'
alter function i64Round (
	v numeric(18, 4)
) returns numeric(18)
as
begin
	return cast (v as bigint);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'DPOWER')) then execute statement
'
alter function dPower (
	b double precision, p double precision
) returns double precision
as
begin
	return power(b, p);
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'STRING2BLOB')) then execute statement
'
alter function string2blob (
	s varchar(300)
) returns blob
as
begin
	return s;
end
';

if (exists(select * from RDB$FUNCTIONS where RDB$MODULE_NAME is not null and RDB$FUNCTION_NAME = 'GETEXACTTIMESTAMP')) then execute statement
'
alter function getExactTimestamp
returns timestamp
as
begin
	return localtimestamp;
end
';

end^

set term ;^

commit;
