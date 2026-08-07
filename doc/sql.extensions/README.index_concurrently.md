# Concurrent index creation

Currently, when index is build, any modifications of table data is not allowed. 
This is required to create correct index not missing new keys inserted during 
the build.

The concurrent index build algorithm allows to significantly shorten time when 
such read lock is required. While it not fully eliminates need to block concurrent 
modifications of table data, time of such blocking is much less comparing with 
traditional non-concurrent algorithm.
This is especially useful in production environment where  blocking of users
activity for long periods is impractical or impossible.  
Due to concurrent load and additional complexity the concurrent index creation 
could take longer than traditional approach.

## Syntax

The new optional non-reserved keyword `CONCURRENTLY` used to specify concurrent 
index creation.

### Create new index

	CREATE [UNIQUE] [ASC[ENDING] | DESC[ENDING]]  
	  INDEX indexname ON tablename  
	  {(col [, col ...]) | COMPUTED BY (<expression>)}  
	  [WHERE <search_condition>]  
	  [CONCURRENTLY]

### Activate existing index

    ALTER INDEX indexname {ACTIVE | INACTIVE}
	  [CONCURRENTLY]

Note, `CONCURRENTLY` can be used with `ACTIVE` only.

### Uniqueness validation

It is hard to impossible to check index uniqueness while index is built concurrently.
Therefore when unique index is created (or activated) concurrently, it is immediately 
marked as *NOT VALIDATED* in system catalogue. The index uniqueness is validated after
the index build is complete, when index contains keys for all records of table.  
If uniqueness check succeed *NOT VALIDATED* mark is cleared.  
If uniqueness check failed, index **remains active** and marked as *NOT VALIDATED*. 
Later user can find and fix duplicates and validate uniquenes again or drop the index 
if appropriate.  
Note, *NOT VALIDATED* unique index still not allows to enter duplicates into table.

To validate unique index, run

	ALTER INDEX indexname VALIDATE UNIQUE

This statement performs index scan to find duplicate keys and check corresponding records.
If no duplicates found, the *NOT VALIDATED* mark is cleared and index considered as validated.
Else *NOT VALIDATED* mark remains in place and index could be validated later again.


isql command `SHOW INDEX` now shows *NOT VALIDATED* mark, if present.

### Table constraints

Concurrent creation of indices for table constraints, such as `PRIMARY|UNIQUE|FOREIGN KEY`
is not supported yet. It is possible to support `PRIMARY|UNIQUE KEY` in the future, but is
unlikely for `FOREIGN KEY`.

## Examples

Prepare table with some data

	create table tab (
	  id int not null,
	  f1 varchar(64)
	);

	insert into tab values (1, '1');
	insert into tab values (1, '2');
	commit;

create index using `CONCURRENTLY` clause

	create index idx_tab_f1 on tab(f1) concurrently;

create unique index on column with duplicates

	create unique index idx_tab_id on tab(id);

it trows an error:

	Statement failed, SQLSTATE = 23000
	attempt to store duplicate value (visible to active transactions) in unique index "IDX_TAB_ID"
	-Problematic key value is ("ID" = 1)

create same unique index using `CONCURRENTLY`

    create unique index idx_tab_id on tab(id) concurrently;

it gives a warning:

	Validation of unique index "IDX_TAB_ID" failed.
	-attempt to store duplicate value (visible to active transactions) in unique index "IDX_TAB_ID"
	-Problematic key value is ("ID" = 1)

check if index exists:

	show index idx_tab_id;

	IDX_TAB_ID UNIQUE NOT VALIDATED INDEX ON (ID)

note *NOT VALIDATED* mark.  

Validate index:

	alter index idx_tab_id validate unique;

	Statement failed, SQLSTATE = 23000
	unsuccessful metadata update
	-Validation of unique index "IDX_TAB_ID" failed.
	-attempt to store duplicate value (visible to active transactions) in unique index "IDX_TAB_ID"
	-Problematic key value is ("ID" = 1)

Fix duplicate record and validate again

	update tab set id = 2 where f1 = '2';
	commit;
	alter index idx_tab_id validate unique;

no error.  
Check index state:

	show index idx_tab_id;

	IDX_TAB_ID UNIQUE INDEX ON (ID)
