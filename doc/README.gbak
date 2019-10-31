In Firebird 4.0 a new switch was added to gbak: -INCLUDE(_DATA).

It takes one parameter which is "similar like" pattern matching
table names in a case-insensitive way.

This switch, if provided, limit tables for which data is stored
or restored in/from backup file.

Interaction between -INCLUDE_DATA and -SKIP_DATA switches for
a table is following:
+--------------------------------------------------+
|           |             INCLUDE_DATA             |
|           |--------------------------------------|
| SKIP_DATA |  NOT SET   |   MATCH    | NOT MATCH  |
+-----------+------------+------------+------------+
|  NOT SET  |  included  |  included  |  excluded  |
|   MATCH   |  excluded  |  excluded  |  excluded  |
| NOT MATCH |  included  |  included  |  excluded  |
+-----------+------------+------------+------------+
