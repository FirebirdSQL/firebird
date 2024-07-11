# `RDB$BLOB_UTIL` package (FB 5.0)

This package exists to manipulate BLOBs in a way that standard Firebird functions, like `BLOB_APPEND` and `SUBSTRING` cannot do it or is very slow.

These routines operates on binary data directly, even for text BLOBs.

## Function `NEW_BLOB`

`RDB$BLOB_UTIL.NEW_BLOB` is used to create a new BLOB. It returns a BLOB suitable for data appending, like `BLOB_APPEND` does.

The advantage over `BLOB_APPEND` is that it's possible to set custom `SEGMENTED` and `TEMP_STORAGE` options.

`BLOB_APPEND` always creates BLOB in temporary storage. That may not be the best approach if the created BLOB is going to be stored in a permanent table, as it will require copy.

Returned BLOB from this function, even when `TEMP_STORAGE = FALSE` may be used with `BLOB_APPEND` for appending data.

Input parameter:
 - `SEGMENTED` type `BOOLEAN NOT NULL`
 - `TEMP_STORAGE` type `BOOLEAN NOT NULL`

Return type: `BLOB NOT NULL`.

## Function `OPEN_BLOB`

`RDB$BLOB_UTIL.OPEN_BLOB` is used to open an existing BLOB for read. It returns a handle (an integer bound to the transaction) suitable for use with others functions of this package, like `SEEK`, `READ_DATA` and `CLOSE_HANDLE`.

Input parameter:
 - `BLOB` type `BLOB NOT NULL`

Return type: `INTEGER NOT NULL`.

## Function `IS_WRITABLE`

`RDB$BLOB_UTIL.IS_WRITABLE` returns `TRUE` when BLOB is suitable for data appending without copying using `BLOB_APPEND`.

Input parameter:
 - `BLOB` type `BLOB NOT NULL`

Return type: `BOOLEAN NOT NULL`.

## Function `READ_DATA`

`RDB$BLOB_UTIL.READ_DATA` is used to read chunks of data of a BLOB handle opened with `RDB$BLOB_UTIL.OPEN_BLOB`. When the BLOB is fully read and there is no more data, it returns `NULL`.

If `LENGTH` is passed with a positive number, it returns a VARBINARY with its maximum length.

If `LENGTH` is `NULL` it returns just a segment of the BLOB with a maximum length of 32765.

Input parameters:
 - `HANDLE` type `INTEGER NOT NULL`
 - `LENGTH` type `INTEGER`

Return type: `VARBINARY(32767)`.

## Function `SEEK`

`RDB$BLOB_UTIL.SEEK` is used to set the position for the next `READ_DATA`. It returns the new position.

`MODE` may be 0 (from the start), 1 (from current position) or 2 (from end).

When `MODE` is 2, `OFFSET` should be zero or negative.

Input parameter:
 - `HANDLE` type `INTEGER NOT NULL`
 - `MODE` type `INTEGER NOT NULL`
 - `OFFSET` type `INTEGER NOT NULL`

Return type: `INTEGER NOT NULL`.

## Procedure `CANCEL_BLOB`

`RDB$BLOB_UTIL.CANCEL_BLOB` is used to immediately release a temporary BLOB, like one created with `BLOB_APPEND`.

Note that if the same BLOB is used after cancel, using the same variable or another one with the same BLOB id reference, invalid blob id error will be raised.

## Procedure `CLOSE_HANDLE`

`RDB$BLOB_UTIL.CLOSE_HANDLE` is used to close a BLOB handle opened with `RDB$BLOB_UTIL.OPEN_BLOB`.

Not closed handles are closed automatically only in the transaction end.

Input parameter:
 - `HANDLE` type `INTEGER NOT NULL`

# Examples

- Example 1: Create a BLOB in temporary space and return it in `EXECUTE BLOCK`:

```
execute block returns (b blob)
as
begin
    -- Create a BLOB handle in the temporary space.
    b = rdb$blob_util.new_blob(false, true);

    -- Add chunks of data.
    b = blob_append(b, '12345');
    b = blob_append(b, '67');

    suspend;
end
```

- Example 2: Open a BLOB and return chunks of it with `EXECUTE BLOCK`:

```
execute block returns (s varchar(10))
as
    declare b blob = '1234567';
    declare bhandle integer;
begin
    -- Open the BLOB and get a BLOB handle.
    bhandle = rdb$blob_util.open_blob(b);

    -- Get chunks of data as string and return.

    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    -- Here EOF is found, so it returns NULL.
    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    -- Close the BLOB handle.
    execute procedure rdb$blob_util.close_handle(bhandle);
end
```

- Example 3: Seek in a blob.

```
set term !;

execute block returns (s varchar(10))
as
    declare b blob;
    declare bhandle integer;
begin
    -- Create a stream BLOB handle.
    b = rdb$blob_util.new_blob(false, true);

    -- Add data.
    b = blob_append(b, '0123456789');

    -- Open the BLOB.
    bhandle = rdb$blob_util.open_blob(b);

    -- Seek to 5 since the start.
    rdb$blob_util.seek(bhandle, 0, 5);
    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    -- Seek to 2 since the start.
    rdb$blob_util.seek(bhandle, 0, 2);
    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    -- Advance 2.
    rdb$blob_util.seek(bhandle, 1, 2);
    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;

    -- Seek to -1 since the end.
    rdb$blob_util.seek(bhandle, 2, -1);
    s = rdb$blob_util.read_data(bhandle, 3);
    suspend;
end!

set term ;!
```

- Example 4: Check if blobs are writable:

```
create table t(b blob);

set term !;

execute block returns (bool boolean)
as
    declare b blob;
begin
    b = blob_append(null, 'writable');
    bool = rdb$blob_util.is_writable(b);
    suspend;

    insert into t (b) values ('not writable') returning b into b;
    bool = rdb$blob_util.is_writable(b);
    suspend;
end!

set term ;!
```
