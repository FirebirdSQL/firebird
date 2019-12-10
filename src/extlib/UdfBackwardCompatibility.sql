-- Create functions in current DB
create function div (
    n1 integer,
    n2 integer
) returns double precision
    external name 'udf_compat!UC_div'
    engine udr;

create function frac (
    val double precision
) returns double precision
    external name 'udf_compat!UC_frac'
    engine udr;

create function dow (
    val timestamp
) returns varchar(53) character set none
    external name 'udf_compat!UC_dow'
    engine udr;

create function sdow (
    val timestamp
) returns varchar(13) character set none
    external name 'udf_compat!UC_sdow'
    engine udr;

create function getExactTimestampUTC
	returns timestamp
    external name 'udf_compat!UC_getExactTimestampUTC'
    engine udr;

create function isLeapYear (
    val timestamp
) returns boolean
    external name 'udf_compat!UC_isLeapYear'
    engine udr;

-- Run minimum test
select 25, 3, div(25, 3) from rdb$database;
select pi(), frac(pi()) from rdb$database;
select current_date, dow(current_date), sdow(current_date) from rdb$database;
select current_timestamp, getExactTimestampUTC() from rdb$database;
select current_date, isLeapYear(current_date) from rdb$database;
