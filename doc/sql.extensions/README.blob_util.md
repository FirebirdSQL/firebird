# `RDB$BLOB_UTIL` package (FB 5.0)

Before Firebird 5 BLOBs could be created appending data using the concatenate (`||`) operator with another BLOB.

However there is two big problems with this approach:
- BLOBs are always created in database page space, even if that is not required (not going to a persistent table).
- Every time a concatenation happens, a new BLOB is created, filling the database with garbage.

`RDB$BLOB_UTIL` package now exists to fix that problems.

It allows BLOB creation in temporary page space (as well in the persistent page space) and has a workflow so that a BLOB is created and could have its data incrementally appended before it's closed and be available as a BLOB value. This is much like the BLOB client API works, but now available in PSQL.

## Function `NEW`

`RDB$BLOB_UTIL.NEW` is used to create a new BLOB. It returns a handle (an integer bound to the transaction) that should be used with the others functions of the package.

Input parameter:
 - `SEGMENTED` type `BOOLEAN NOT NULL`
 - `TEMP_STORAGE` type `BOOLEAN NOT NULL`

Return type: `INTEGER NOT NULL`.

## Function `OPEN_BLOB`

`RDB$BLOB_UTIL.OPEN_BLOB` is used to open an existing BLOB for read. It returns a handle that should be used with the others functions of the package.

Input parameter:
 - `BLOB` type `BLOB NOT NULL`

Return type: `INTEGER NOT NULL`.

## Procedure `APPEND`

`RDB$BLOB_UTIL.APPEND` is used to append chunks of data to a BLOB handle created with `RDB$BLOB_UTIL.NEW`.

Input parameters:
 - `HANDLE` type `INTEGER NOT NULL`
 - `DATA` type `VARBINARY(32767) NOT NULL`

## Function `READ`

`RDB$BLOB_UTIL.READ` is used to read chunks of data of a BLOB handle opened with `RDB$BLOB_UTIL.OPEN_BLOB`. When the BLOB is fully read and there is no more data, it returns `NULL`.

If `LENGTH` is passed with a positive number, it returns a VARBINARY with its maximum length.

If `LENGTH` is `NULL` it returns just a segment of the BLOB with a maximum length of 32765.

Input parameters:
 - `HANDLE` type `INTEGER NOT NULL`
 - `LENGTH` type `INTEGER`

Return type: `VARBINARY(32767)`.

## Function `SEEK`

`RDB$BLOB_UTIL.SEEK` is used to set the position for the next `READ`. It returns the new position.

`MODE` may be 0 (from the start), 1 (from current position) or 2 (from end).

When `MODE` is 2, `OFFSET` should be negative.

Input parameter:
 - `HANDLE` type `INTEGER NOT NULL`
 - `MODE` type `INTEGER NOT NULL`
 - `OFFSET` type `INTEGER NOT NULL`

Return type: `INTEGER NOT NULL`.

## Procedure `CANCEL`

`RDB$BLOB_UTIL.CANCEL` is used to release a BLOB handle opened with `RDB$BLOB_UTIL.OPEN_BLOB` or discard one created with `RDB$BLOB_UTIL.NEW`.

Input parameter:
 - `HANDLE` type `INTEGER NOT NULL`

## Function `MAKE_BLOB`

`RDB$BLOB_UTIL.MAKE_BLOB` is used to create a BLOB from a BLOB handle created with `NEW` followed by its content added with `APPEND`. After `MAKE_BLOB` is called the handle is destroyed and should not be used with the others functions.

Input parameter:
 - `HANDLE` type `INTEGER NOT NULL`

Return type: `BLOB NOT NULL`.

# Examples

- Example 1: Create a BLOB and return it in `EXECUTE BLOCK`:

```
execute block returns (b blob)
as
    declare bhandle integer;
begin
    -- Create a BLOB handle in the temporary space.
    bhandle = rdb$blob_util.new(false, true);

    -- Add chunks of data.
    execute procedure rdb$blob_util.append(bhandle, '12345');
    execute procedure rdb$blob_util.append(bhandle, '67');

    -- Create the BLOB and return it.
    b = rdb$blob_util.make_blob(bhandle);
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

    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    -- Here EOF is found, so it returns NULL.
    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    -- Close the BLOB handle.
    execute procedure rdb$blob_util.cancel(bhandle);
end
```

- Example 3: Seek in a blob.

```
create table t(b blob);

set term !;

execute block returns (s varchar(10))
as
    declare bhandle integer;
    declare b blob;
begin
    -- Create a stream BLOB handle.
    bhandle = rdb$blob_util.new(false, true);

    -- Add data.
    execute procedure rdb$blob_util.append(bhandle, '0123456789');

    -- Create the BLOB.
    insert into t (b) values (rdb$blob_util.make_blob(:bhandle)) returning b into b;

    -- Open the BLOB.
    bhandle = rdb$blob_util.open_blob(b);

    -- Seek to 5 since the start.
    rdb$blob_util.seek(bhandle, 0, 5);
    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    -- Seek to 2 since the start.
    rdb$blob_util.seek(bhandle, 0, 2);
    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    -- Advance 2.
    rdb$blob_util.seek(bhandle, 1, 2);
    s = rdb$blob_util.read(bhandle, 3);
    suspend;

    -- Seek to -1 since the end.
    rdb$blob_util.seek(bhandle, 2, -1);
    s = rdb$blob_util.read(bhandle, 3);
    suspend;
end!

set term ;!
```
