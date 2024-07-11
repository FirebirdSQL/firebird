--------------------
BLOB_APPEND function
--------------------

Regular operator `||` (concatenation) with BLOB arguments creates temporary BLOB per every pair of arguments
with BLOB. This could lead to the excessive memory consumption and growth of database file. The `BLOB_APPEND` function is designed to concatenate BLOBs without creating intermediate BLOBs.

In order to achieve this, the resulting BLOB is left open for writing instead of been closed immediately after it is filled with data. I.e. such blob could be appended as many times as required. Engine marks such blob with new internal flag `BLB_close_on_read` and closes it automatically when necessary.

*Available in*: DSQL, PSQL.

Syntax:

`BLOB_APPEND( <value1>, ... <valueN> )`


*Return type*: BLOB, temporary, not closed (i.e. open for writing), marked by flag `BLB_close_on_read`.


Input Arguments:

- For the first argument, depending on its value, the following function behavior is defined:

	- NULL: a new empty non-closed BLOB will be created
	- permanent BLOB (from table) or temporary already closed BLOB:
	a new empty non-closed BLOB will be created and the contents the first BLOB will be added to it
	- temporary non-closed BLOB: it will be used later
	- other data types are converted to a string, a temporary non-closed BLOB will be created with the contents of this string

- Other arguments can be of any type. The following behavior is defined for them:
    - NULLs are ignored
    - non-BLOBs are converted to string (as usual) and appended to the content of the result
    - BLOBs, if necessary, are transliterated to the character set of the first argument and their contents are appended to the result

The `BLOB_APPEND` function returns a temporary non-closed BLOB as output.
This is either a new BLOB or the same one that was in the first argument. Thus, a series of operations of the form
`blob = BLOB_APPEND(blob, ...)` will result in at most one BLOB being created (unless trying to append the BLOB to itself).
This BLOB will be automatically closed by the engine when it is attempted to be read by a client, written to a table, or used in other expressions that require the content to be read.

Note:

Testing a BLOB for a NULL value with the `IS [NOT] NULL` operator does not read it, and therefore a temporary open BLOB will not be closed by such tests.

```
execute block
returns (b blob sub_type text)
as
begin
  -- will create a new temporary not closed BLOB 
  -- and will write to it the string from the 2nd argument
  b = blob_append(null, 'Hello ');
  -- adds two strings to the temporary BLOB without closing it 
  b = blob_append(b, 'World', '!');
  -- comparing a BLOB with a string will close it, because for this you need to read the BLOB
  if (b = 'Hello World!') then
  begin
  -- ...
  end
  -- will create a temporary closed BLOB by adding a string to it
  b = b || 'Close';
  suspend;
end
```

Tip:

Use the `LIST` and` BLOB_APPEND` functions to concatenate BLOBs. This will save memory consumption, disk I/O,
and prevent database growth due to the creation of many temporary BLOBs when using concatenation operators.

Example:

Let's say you need to build JSON on the server side. We have a PSQL package JSON_UTILS with a set of functions for converting primitive data types to JSON notation.
Then the JSON building using the `BLOB_APPEND` function will look like this:

```
EXECUTE BLOCK
RETURNS (
    JSON_STR BLOB SUB_TYPE TEXT CHARACTER SET UTF8)
AS
  DECLARE JSON_M BLOB SUB_TYPE TEXT CHARACTER SET UTF8;
BEGIN
  FOR
      SELECT
          HORSE.CODE_HORSE,
          HORSE.NAME,
          HORSE.BIRTHDAY
      FROM HORSE
      WHERE HORSE.CODE_DEPARTURE = 15
      FETCH FIRST 1000 ROW ONLY
      AS CURSOR C
  DO
  BEGIN
    SELECT
      LIST(
          '{' ||
          JSON_UTILS.NUMERIC_PAIR('age', MEASURE.AGE) ||
          ',' ||
          JSON_UTILS.NUMERIC_PAIR('height', MEASURE.HEIGHT_HORSE) ||
          ',' ||
          JSON_UTILS.NUMERIC_PAIR('length', MEASURE.LENGTH_HORSE) ||
          ',' ||
          JSON_UTILS.NUMERIC_PAIR('chestaround', MEASURE.CHESTAROUND) ||
          ',' ||
          JSON_UTILS.NUMERIC_PAIR('wristaround', MEASURE.WRISTAROUND) ||
          ',' ||
          JSON_UTILS.NUMERIC_PAIR('weight', MEASURE.WEIGHT_HORSE) ||
          '}'
      ) AS JSON_M
    FROM MEASURE
    WHERE MEASURE.CODE_HORSE = :C.CODE_HORSE
    INTO JSON_M;

    JSON_STR = BLOB_APPEND(
      JSON_STR,
      IIF(JSON_STR IS NULL, '[', ',' || ascii_char(13)),
      '{',
      JSON_UTILS.INTEGER_PAIR('code_horse', C.CODE_HORSE),
      ',',
      JSON_UTILS.STRING_PAIR('name', C.NAME),
      ',',
      JSON_UTILS.TIMESTAMP_PAIR('birthday', C.BIRTHDAY),
      ',',
      JSON_UTILS.STRING_VALUE('measures') || ':[', JSON_M, ']',
      '}'
    );
  END
  JSON_STR = BLOB_APPEND(JSON_STR, ']');
  SUSPEND;
END
```

A similar example using the usual concatenation operator `||` is an order of magnitude slower and does 1000 times more disk writes.
