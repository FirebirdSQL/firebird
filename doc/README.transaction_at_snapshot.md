# Transaction at defined snapshot number

With this feature it's possible to create parallel (via different attachments) processes reading consistent data from a database.

For example, a backup process may create multiple threads paralleling reading data from the database.

Or a web service may dispatch distributed sub services paralleling doing some processing.

That is accomplished creating a transaction with `SET TRANSACTION SNAPSHOT [ AT NUMBER <snapshot number> ]` or `isc_tpb_at_snapshot_number <snapshot number length> <snapshot number>`.

The `snapshot number` from the first transaction may be obtained with `RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER')` or transaction info call with `fb_info_tra_snapshot_number`.

Note that the `snapshot number` passed to the new transaction must be a snapshot of an active transaction.

## Global temporary tables

Data of a global temporary table (GTT) is private to an attachment, so it is not a part of the shared snapshot. A transaction started `AT NUMBER` reads the GTT data of its own attachment only.

If the attachment has rows in a GTT created with `ON COMMIT PRESERVE ROWS`, the new transaction reads them with the given `snapshot number`. The engine keeps the record versions that this snapshot needs from the moment when the new transaction starts. Record versions that were garbage collected in this attachment before that moment are not restored.

As a result, a GTT row can be not visible to the new transaction if both conditions are true:
- the row was updated or deleted in this attachment by a transaction that committed after the snapshot was taken;
- another transaction of this attachment read the row after that commit and before the new transaction started.

Do not rely on such GTT data in a transaction started `AT NUMBER`.


Author:
    Adriano dos Santos Fernandes <adrianosf at gmail.com>
