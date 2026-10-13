# gfix: -SKIP_DATA / -INCLUDE_DATA / -SKIP_SCHEMA_DATA / -INCLUDE_SCHEMA_DATA

Sometimes a damaged database can't be fully validated or repaired with `gfix -v -full` or `gfix -mend`
because of broken records in some unimportant tables. The switches below allow to exclude records of
such tables from validation, so the rest of the database can still be checked and repaired.

The switches are similar to the same `gbak` switches and accept a regular expression in SQL syntax
(see `SIMILAR TO`). Matching is case insensitive.

| Switch                    | Description                                                        |
|---------------------------|--------------------------------------------------------------------|
| `-SKIP_D(ATA)`            | skip records validation of tables with matching names              |
| `-INCLUDE(_DATA)`         | validate records of tables with matching names only                |
| `-SKIP_SCHEMA_D(ATA)`     | skip records validation of tables in schemas with matching names   |
| `-INCLUDE_SCHEMA_D(ATA)`  | validate records of tables in schemas with matching names only     |

The rules to decide whether records of a table are validated are the same as in `gbak`: records are skipped
if the table is filtered out either by the table name switches or by the schema name switches. If a table
matches both `-SKIP_DATA` and `-INCLUDE_DATA` (or a schema matches both `-SKIP_SCHEMA_DATA` and
`-INCLUDE_SCHEMA_DATA`), its records are skipped.

The filters are applied to user tables only, records of system tables are always validated.

Records of skipped tables are not read at all, including big records (fragmented across several pages)
and blobs, so `-mend` doesn't change them. Pointer pages, data pages and index pages of skipped tables
are still walked and checked as without `-full`: the checks of full validation (record version chains,
index entries against records, consistency of index tree levels) are not performed for them.
Search for orphan pages is not performed when records of any table were skipped, as pages of big
records and blobs of skipped tables were not walked.

The switches require `-validate` together with `-full` (or `-mend`, which implies both).

Examples:
```shell
# Validate all records except records of table T1 of any schema
gfix -v -full -skip_data T1 database.fdb

# Repair the database, skipping records of tables with names starting with LOG_
gfix -mend -skip_data "LOG\_%" database.fdb

# Validate records of tables of the schema S1 only
gfix -v -full -include_schema_data S1 database.fdb

# Validate records of table T1 of the schema S1 only
gfix -v -full -include_schema_data S1 -include_data T1 database.fdb
```

## Services API

The same filters are available in `isc_action_svc_repair` with the following string SPB items:

| SPB item                          | fbsvcmgr option            | Equivalent                  |
|-----------------------------------|----------------------------|-----------------------------|
| `isc_spb_rpr_skip_data`           | `rpr_skip_data`            | `gfix -skip_data`           |
| `isc_spb_rpr_include_data`        | `rpr_include_data`         | `gfix -include_data`        |
| `isc_spb_rpr_skip_schema_data`    | `rpr_skip_schema_data`     | `gfix -skip_schema_data`    |
| `isc_spb_rpr_include_schema_data` | `rpr_include_schema_data`  | `gfix -include_schema_data` |

The items require `isc_spb_rpr_validate_db` and `isc_spb_rpr_full` (or `isc_spb_rpr_mend_db`) in `isc_spb_options`,
the order of the items in SPB doesn't matter.

```shell
fbsvcmgr service_mgr action_repair dbname database.fdb rpr_validate_db rpr_full rpr_skip_data T1
```

## DPB

`gfix` passes the filters to the engine using the following string DPB items, used together with `isc_dpb_verify`:
`isc_dpb_verify_skip_data`, `isc_dpb_verify_include_data`, `isc_dpb_verify_skip_schema_data`,
`isc_dpb_verify_include_schema_data`.
