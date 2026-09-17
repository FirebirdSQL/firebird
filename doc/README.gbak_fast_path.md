# gbak FAST_PATH mode

`FAST_PATH` is an optional, non-default backup and restore mode available through the Services Manager. It is intended to make
data-heavy `gbak` operations faster by moving table data directly between the `gbak` service and the database engine internal layers,
with less per-record processing.

The backup format is unchanged. A backup created with `FAST_PATH` can be restored normally, and an ordinary backup can be
restored with `FAST_PATH`. Database creation and metadata restore continue to use the normal restore process.

## When to use it

### Speeding up large operations

`FAST_PATH` is most useful for large backups or restores where moving and processing many rows is significant. It can
reduce CPU use and elapsed time, but the improvement depends on the database, data types, storage, and number of
parallel workers. Normal restore already has batch and bulk paths, so measure `FAST_PATH` against the current normal
restore with a representative backup or restore before making it the default for all jobs.

The mode is particularly suitable for data-heavy operations on persistent user tables. For restore, use a trusted backup
and plan appropriate post-restore data-quality checks, because the direct data path bypasses per-row validation performed
by some normal restore paths.

### Recovering data when normal restore fails

`FAST_PATH` restore can also be worth trying when a normal restore fails and the goal is to salvage the data. This works
only for a specific class of failures: normal restore rejects rows through its statement machinery — per-row `NOT NULL`
and domain validation expressions, and request/BLR compilation for the statement path. A backup whose rows no longer
satisfy the currently defined rules (for example, taken before a `NOT NULL` or domain constraint was added, or loaded
while enforcement was disabled) can therefore abort a normal restore while the stored values themselves are intact.

`FAST_PATH` skips that validation pass and the statement machinery: it converts each backed-up message directly into an
engine record with the same conversion routines normal storage uses, then buffers it through bulk insert. Rows rejected
only by validation can load this way.

Limits to keep in mind: the direct path still checks message lengths and field mappings and still converts data types,
and it is fail-fast — a conversion or engine error aborts the restore rather than skipping the row. It will not rescue
a corrupt or truncated backup stream, missing tables or columns, or unreadable blobs. Treat the recovered database as
containing potentially rule-violating rows: validation metadata itself is restored normally, so later DML is still
checked, but the loaded rows are not retrospectively validated. Inspect the data, fix or remove offending rows, and run
explicit post-restore checks before putting the database into production.

## Restore semantics

`FAST_PATH` changes the table-data transfer path, not the backup format or the normal metadata restore process. For eligible
persistent user tables, restore converts backed-up values directly into engine records (using the same conversion routines as
normal record storage) and buffers them through the engine's bulk-insert mechanism, bypassing normal restore statement
execution and its per-row validation processing.

This must not be confused with replaying application `INSERT` statements. Ordinary `gbak` restore also restores explicit
stored values rather than regenerating column defaults or identity values, and does not replay user DML trigger side effects.
Table `CHECK` constraints implemented through triggers are not a per-row validation guarantee of ordinary restore either.

The validation difference depends on the normal restore path being compared. Statement-based stores can evaluate field
validation expressions; the normal bulk-insert path has an explicit `NOT NULL` check but not the general store
validation-expression pass. `FAST_PATH` bypasses that `NOT NULL` check as well: the direct writer does not evaluate per-row
`NOT NULL` or domain validation expressions.

Unless affected by other restore options, validation metadata is restored normally. Retaining that metadata does not
retrospectively validate rows loaded through the direct writer.

Do not treat either restore mode as a complete audit of application data rules. If additional assurance is required, run
explicit post-restore checks for the relevant domain rules, table constraints, and application invariants. Ordinary restore
is not a way to replay trigger side effects.

The direct path still checks message lengths and field mappings and performs required data-type conversions; it is not a
complete backup-integrity validator.

`NO_VALIDITY` (`-no_validity`) is separate from `FAST_PATH`. It omits restored column and domain nullability flags and domain
validation BLR and source; it is not merely a temporary instruction to skip checks during data loading. Those omitted rules are
consequently unavailable for later enforcement unless recreated. `FAST_PATH` alone does not remove those definitions. Conversely,
`NO_VALIDITY` is not a blanket removal of all constraints: table `CHECK` trigger metadata and index-backed primary-key, unique,
and foreign-key constraints follow their normal restore paths.

## Usage

`FAST_PATH` requires the `-service` option (Services Manager). It works both via `gbak -service ... -fast` (or `-fast_path`) and via
`fbsvcmgr` / Services API options.

With `fbsvcmgr`, use `bkp_fast_path` for backup and `res_fast_path` for restore. Paths in these examples are server-side.
For example:

```text
fbsvcmgr localhost:service_mgr \
  -user SYSDBA -password <password> \
  action_backup dbname /path/source.fdb \
  bkp_file /path/source.fbk bkp_fast_path
```

```text
fbsvcmgr localhost:service_mgr \
  -user SYSDBA -password <password> \
  action_restore bkp_file /path/source.fbk \
  dbname /path/restored.fdb res_fast_path
```

The same mode is available through `gbak -service`, which forwards its switches via the service command line:

```text
gbak -service localhost:service_mgr -user SYSDBA -password <password> \
  -backup /path/source.fdb /path/source.fbk -fast_path
```

```text
gbak -service localhost:service_mgr -user SYSDBA -password <password> \
  -create_database /path/source.fbk /path/restored.fdb -fast_path
```

Services API clients set the corresponding bit in the service-start `isc_spb_options` value:

| Operation | `fbsvcmgr` option | Services API option bit |
|-----------|-------------------|-------------------------|
| Backup | `bkp_fast_path` | `isc_spb_bkp_fast_path` (`0x040000`) |
| Restore | `res_fast_path` | `isc_spb_res_fast_path` (`0x040000`; backup and restore option bits live in separate per-action namespaces) |

The option can be combined with the usual backup, restore, and parallel worker options. `FAST_PATH` does not change the normal
table-data selection rules. With `METADATA_ONLY`, backup omits table rows, and restore consumes but does not load any table rows
present in the backup; the direct row-transfer path is therefore not exercised.

## Scope and eligibility

The direct data path targets persistent user tables. Normal `gbak` selection happens before `FAST_PATH` eligibility is tested:
system relations with `RDB$SYSTEM_FLAG = 1` are excluded from the backed-up relation list, views do not have their rows backed up,
and external-table rows are omitted unless external-table conversion is requested. Temporary-table contents are not persistent
database data.

## Architecture

`FAST_PATH` selects the in-process engine provider. Direct VIO access needs an engine `JAttachment`, which the
Services Manager yvalve handle is not, so backup workers always use a private engine attachment - including with a
single worker. Those worker transactions join the main backup snapshot with `isc_tpb_at_snapshot_number` (the same
mechanism as `-parallel` workers).
The reader scans records with `VIO_next_record()` and fills the normal backup message layout; downstream backup serialization
remains unchanged.

Restore retains normal metadata processing, backup decoding, and blob and array handling. Eligible table-data workers convert each
message into an engine record and buffer it through the transaction's bulk-insert page images, the same mechanism the normal
restore batch path uses to reserve and fill data pages. The direct path is fail-fast: a conversion or engine error aborts the restore
rather than skipping a row. This is also true when `-incremental` is specified; normal-mode relations retain their existing
incremental error handling. Rows buffered in the bulk-insert images are flushed at table end and at commit. In-memory batches use an aligned
stride distinct from the serialized message length so packed records keep field alignment; the backup format itself is unpadded.

## Verification

`FAST_PATH` is a trade-off for trusted backups. Restore-test backups, check relevant data rules with application-specific queries, and
benchmark representative scalar-heavy, blob-heavy, and array-heavy workloads with one and multiple workers before enabling the option in
production jobs. CPU, elapsed time, and peak memory should all be compared, since the direct path changes allocation and batching behaviour
as well as per-row CPU.
