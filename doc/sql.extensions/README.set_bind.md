# SQL Language Extension: SET BIND

##	Implements capability to setup columns coercion rules in current session.


### Author:

	Alex Peshkoff <peshkoff@mail.ru>


### Syntax is:

```sql
SET BIND OF { type-from | TIME ZONE } TO { type-to | LEGACY | EXTENDED | NATIVE };
```

### Description:

Makes it possible to define rules of describing types of returned to the client columns in non-standard way -
`type-from` is replaced with `type-to`, automatic data coercion takes place.

When incomplete type definition is used (i.e. `CHAR` instead `CHAR(n)`) in left part of `SET BIND` coercion
will take place for all `CHAR` columns, not only default `CHAR(1)`. Special incomplete type `TIME ZONE`
stands for all types (namely TIME & TIMESTAMP) WITH TIME ZONE.
When incomplete type definiton is used in right side of the statement (TO part) firebird engine will define missing
details about that type automatically based on source column.

Changing bind of any NUMERIC/DECIMAL does not affect appropriate underlying integer type. On contrary,
changing bind of integer datatype also affects appropriate NUMERICs/DECIMALs.

Special `TO` part format `LEGACY` is used when datatype, missing in previous FB version, should be represented in
a way, understandable by old client software (may be with some data losses). The following coercions are done for
legacy datatypes:

| Native datatype          | Legacy datatype             |
|--------------------------|-----------------------------|
| BOOLEAN                  | CHAR(5)                     |
| DECFLOAT                 | DOUBLE PRECISION            |
| INT128                   | BIGINT                      |
| TIME WITH TIME ZONE      | TIME WITHOUT TIME ZONE      |
| TIMESTAMP WITH TIME ZONE | TIMESTAMP WITHOUT TIME ZONE |

Using `EXTENDED` in the `TO` part directs firebird engine to coerce to extended form of `FROM` datatype.
Currently it works only for TIME/TIMESTAMP WITH TIME ZONE - they are coerced to EXTENDED TIME/TIMESTAMP WITH TIME ZONE
appropriately.

Setting `NATIVE` means `type` will be used as if there were no previous rules for it.

Except SQL-statement there are two more ways to specify data coercion - tag `isc_dpb_set_bind` in DPB
and `DataTypeCompatibility` parameter in firebird.conf & databases.conf. The later the rule is introduced
(.conf->DPB->SQL) the higher priotiy it has.
I.e. one can override .conf in any other way and DPB from SQL statement.

Value of clumplet with `isc_dpb_set_bind` tag in DPB should be specified as a set of partially
formed SET BIND statements, i.e. with prefix SET BIND OF is omitted, separated by semicolons.
For example:
```c++
dpb->insertString(&status, isc_dpb_set_bind, "decfloat to char; int128 to char");
```

`DataTypeCompatibility` is minor firebird version for which we want to provide some compatibility
regarding data types. That compatibility may be not absolute - for example SET BIND can't care about type
of particular SQL functions. The following types will be described in legacy form when `DataTypeCompatibility=3.0`:
DECFLOAT, INT128 and TIME(STAMP) WITH TIME ZONE. When `DataTypeCompatibility=2.5` in addition to this list
BOOLEAN will be described as legacy type as well.

Only fields returned by database engine in regular messages are modified according to SET BIND rules.
Variables returned by getting slice of an array are not affected by SET BIND statement.


### SQL Samples:

```sql
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;	--native
```
```
                   CAST
=======================
                 123.45
```

```sql
SET BIND OF DECFLOAT TO DOUBLE PRECISION;
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;	--double
```
```
                   CAST
=======================
      123.4500000000000
```

```sql
SET BIND OF DECFLOAT(34) TO CHAR;
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;	--still double
```
```
                   CAST
=======================
      123.4500000000000
```

```sql
SELECT CAST('123.45' AS DECFLOAT(34)) FROM RDB$DATABASE;	--text
```
```
CAST
==========================================
123.45
```

In a case of missing ICU on client:
```sql
SELECT CURRENT_TIMESTAMP FROM RDB$DATABASE;
```
```
                                        CURRENT_TIMESTAMP
=========================================================
2020-02-21 16:26:48.0230 GMT*
```
```sql
SET BIND OF TIME ZONE TO EXTENDED;
SELECT CURRENT_TIMESTAMP FROM RDB$DATABASE;
```
```
                                        CURRENT_TIMESTAMP
=========================================================
2020-02-21 19:26:55.6820 +03:00
```
