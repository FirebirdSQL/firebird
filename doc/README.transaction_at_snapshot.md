# Transaction at defined snapshot number

With this feature it's possible to create parallel (via different attachments) processes reading consistent data from a database.

For example, a backup process may create multiple threads paralleling reading data from the database.

Or a web service may dispatch distributed sub services paralleling doing some processing.

That is accomplished creating a transaction with `SET TRANSACTION SNAPSHOT [ AT NUMBER <snapshot number> ]` or `isc_tpb_at_snapshot_number <snapshot number length> <snapshot number>`.

The `snapshot number` from the first transaction may be obtained with `RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER')` or transaction info call with `fb_info_tra_snapshot_number`.

Note that the `snapshot number` passed to the new transaction must be a snapshot of an active transaction.


Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>
