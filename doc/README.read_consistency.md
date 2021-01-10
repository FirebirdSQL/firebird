# Commits order as a way to define database snapshot

## Traditional way to define database snapshot using copy of TIP.

State of every transaction in database is recorded at **Transaction Inventory Pages (TIP)**.  
It allows any active transaction to know state of another transaction, which created record version 
that active transaction going to read or change. If other transaction is committed, then active 
transaction is allowed to see given record version.

**Snapshot (concurrency)** transaction at own start takes *copy of TIP* and keeps it privately 
until commit (rollback). This private copy of TIP allows to see any record in database as it 
was at the moment of transaction start. I.e. it defines database *snapshot*.

**Read-committed** transaction not uses stable snapshot view of database and keeps no own TIP copy.
Instead, it ask for *most current* state of another transaction at the TIP. SuperServer have shared 
TIP cache to optimize access to the TIP for read-committed transactions. 

## Another way to define database snapshot.

The main idea: it is enough to know *order of commits* to know state of any transaction at 
moment when snapshot is created. 

### Define **commits order** for database
 - per-database counter: **Commit Number (CN)**
 - it is initialized when database is started
 - when any transaction is committed, database Commit Number is incremented and 
   its value is associated with this transaction and could be queried later.  
   Let call it **"transaction commit number"**, or **transaction CN**.
 - there is special values of CN for active and dead transactions.

### Possible values of transaction Commit Number
 - Transaction is active
   - CN_ACTIVE = 0
 - Transactions committed before database started (i.e. older than OIT)
   - CN_PREHISTORIC = 1
 - Transactions committed while database works:
   - CN_PREHISTORIC < CN < CN_DEAD
 - Dead transaction
   - CN_DEAD = MAX_TRA_NUM - 2
 - Transaction is in limbo
   - CN_LIMBO = MAX_TRA_NUM - 1

**Database snapshot** is defined by value of global Commit Number at moment when database snapshot 
is created. To create database snapshot it is enough to get (and keep) global Commit Number value 
at given moment. 

### The record version visibility rule
 - let **database snapshot** is current snapshot used by current transaction,
 - let **other transaction** is transaction that created given record version, 
 - if other transaction's state is **active, dead or in limbo**:  
   record version is **not visible** to the current transaction
 - if other transaction's state is **committed** - consider *when it was committed*:
   - **before** database snapshot was created:  
     record version is **visible** to the current transaction
   - **after** database snapshot was created:  
     record version is **not visible** to the current transaction

Therefore it is enough to compare CN of other transaction with CN of database snapshot to decide 
if given record version is visible at the scope of database snapshot. Also it is necessary to 
maintain list of all known transactions with associated Commit Numbers.

### Implementation details

List of all known transactions with associated Commit Numbers is maintained in shared memory.
It is implemented as array where index is transaction number and item value is corresponding
Commit Number. Whole array is split on blocks of fixed size. Array contains CN's for all 
transactions between OIT and Next markers, thus new block is allocated when Next moves out of 
scope of higher block, and old block is released when OIT moves out of lower block.

Block size could be set in firebird.conf using new setting **TipCacheBlockSize**. Default value is
4MB and it could keep 512K transactions.

**CONCURRENCY** transactions now uses **database snapshot** described above. Thus instead of taking 
a private copy of TIP at own start it just keeps value of global Commit Number at a moment.

# Statement level read consistency for read-committed transactions

## Not consistent read problem

Current implementation of read-committed transactions suffers from one important problem - single 
statement (such as SELECT) could see the different view of the same data during execution.  
For example, imagine two concurrent transactions:
 - first is inserting 1000 rows and commits, 
 - second run SELECT COUNT(*) against the same table. 

If second transaction is read-committed its result is hard to predict, it could be any of:
 - number of rows in table before first transaction starts, or
 - number of rows in table after first transaction commits, or
 - any number between two numbers above.

What case will take place depends on how both transactions interact with each other:
 - second transaction finished counting before first transaction commits  
   - second transaction see no records inserted by first transaction
   (as no new records was committed)
 - happens if second transaction start to see new records after first transaction commits  
   - second transaction sees all records inserted (and committed) by first transaction
 - happens in any other case  
   - second transaction could see some but not all records inserted (and committed) by first 
     transaction

This is the problem of not consistent read at *statement level*.

It is important to speak about *statement level* - because, by definition, each *statement* in 
*read-committed* transaction is allowed to see own view of database. The problem of current 
implementation is that this view is not stable and could be changed while statement is executed. 
*Snapshot* transactions have no this problem as it uses the same stable database snapshot for all 
executed statements. Different statements within read-committed transaction could see different 
view of database, of course.

## Solution for not consistent read problem

The obvious solution to not consistent read problem is to make read-committed transaction to use 
stable database snapshot while statement is executed. Each new top-level statement create own 
database snapshot to see data committed recently. With snapshots based on commit order it is very 
cheap operation. Let name this snapshot as **statement-level snapshot** further. Nested statements
(triggers, nested stored procedures and functions, dynamic statements, etc) uses same 
statement-level snapshot created by top-level statement. 

To support this solution new transaction isolation level is introduced: **READ COMMITTED READ 
CONSISTENCY**

Old read-committed isolation modes (**RECORD VERSION** and **NO RECORD VERSION**) are still 
allowed, works as before (i.e. not using statement-level snapshots) and could be considered 
as legacy in the future versions of Firebird. 

So, there are three kinds of read-committed transactions now:
 - READ COMMITTED READ CONSISTENCY
 - READ COMMITTED NO RECORD VERSION
 - READ COMMITTED RECORD VERSION

### Update conflicts handling

When statement executed within READ COMMITTED READ CONSISTENCY transaction its database view is 
not changed (similar to snapshot transaction). Therefore it is useless to wait for commit of 
concurrent transaction in the hope to re-read new committed record version. On read, behavior is 
similar to READ COMMITTED *RECORD VERSION* transaction - do not wait for active transaction and
walk backversions chain looking for record version visible to the current snapshot.

For READ COMMITTED *READ CONSISTENCY* mode handling of update conflicts by the engine is changed 
significantly. 
When update conflict is detected the following is performed:  
a) transaction isolation mode temporarily switched to the READ COMMITTED *NO RECORD VERSION MODE*
b) engine put write lock on conflicted record 
c) engine continue to evaluate remaining records of update\delete cursor and put write locks 
   on it too
d) when there is no more records to fetch, engine start to undo all actions performed since
   top-level statement execution starts and preserve already taken write locks for every 
   updated\deleted\locked record, all inserted records are removed
e) then engine restores transaction isolation mode as READ COMMITTED *READ CONSISTENCY*, creates
   new statement-level snapshot and restart execution of top-level statement.

Such algorithm allows to ensure that after restart already updated records remains locked, 
will be visible to the new snapshot, and could be updated again with no further conflicts. 
Also, because of read consistency mode, set of modified records remains consistent.

Notes:
- restart algorithm above is applied to the UPDATE, DELETE, SELECT WITH LOCK and MERGE statements, 
  with and without RETURNING clause, executing directly by user applicaiton or as a part of some 
  PSQL object (stored procedure\function, trigger, EXECUTE BLOCK, etc)
- if UPDATE\DELETE statement is positioned on some explicit cursor (WHERE CURRENT OF) then engine 
  skip step (c) above, i.e. not fetches and not put write locks on remaining records of cursor
- if top-level statement is SELECT'able and update conflict happens after one or more records was 
  returned to the application, then update conflict error is reported as usual and restart is not 
  initiated
- restart is not initiated for statements in autonomous blocks (IN AUTONOMOUS TRANSACTION DO ...)
- after 10 attempts engine aborts restart algorithm, releases all write locks, restores transaction 
  isolation mode as READ COMMITTED *READ CONSISTENCY* and report update conflict
- any not handled error at step (c) above stops restart algorithm and engine continue processing 
  in usual way, for example error could be catched and handled by PSQL WHEN block or reported to 
  the application if not handled
- UPDATE\DELETE triggers will fire multiply times for the same record if statement execution was
  restarted and record is updated\deleted again
- statement restart usually fully transparent to the applications and no special actions should 
  be taken by developers to handle it in any way. The only exception is the code with side effects 
  that is out of transactional control, such as:
  - usage of external tables, sequences or context variables;
  - sending e-mails using UDF;
  - committed autonomous transactions or external queries, and so on
  
  Take into account that such code could be executed more than once if update conflict happens
- there is no special tools to detect restart but it could be easy done using code with side 
  effects as described above, for example - using context variable
- by historical reasons isc_update_conflict reported as secondary error code with primary error 
  code isc_deadlock.


### Read-committed read only transactions

READ COMMITTED *READ ONLY* transactions marked as committed immediately when transaction started. 
Also such transactions do not inhibit regular garbage collection and not delays advance of OST
marker. READ CONSISTENCY READ ONLY transactions still marked as committed on start but, to not
let regular garbage collection to break future statement-level snapshots, it delays movement of 
OST marker in the same way as SNAPSHOT transactions. Note, this delays *regular* (traditional)
garbage collection only, *intermediate* GC (see below) is not affected.

### Support for new READ COMMITTED READ CONSISTENCY isolation level
#### SQL syntax  

New isolation level is supported at SQL level:

*SET TRANSACTION READ COMMITTED READ CONSISTENCY*

#### API level
To start read-committed read consistency transaction using ISC API use new constant in Transaction 
Parameter Buffer (TPB):

*isc_tpb_read_consistency*

#### Configuration setting
It is recommended to use READ COMMITTED READ CONSISTENCY mode whenever read-committed isolation 
is feasible. To help test existing applications with new READ COMMITTED READ CONSISTENCY isolation 
level new configuration setting is introduced:   

*ReadConsistency*  

If ReadConsistency set to 1 (by default) engine ignores [NO] RECORD VERSION flags and makes all 
read-committed transactions READ COMMITTED READ CONSISTENCY.

If ReadConsistency is set to 0 - flags [NO] RECORD VERSION takes effect as in previous Firebird 
versions. READ COMMITTED READ CONSISTENCY isolation level should be specified explicitly by 
application - in TPB or using SQL syntax.

The setting is per-database.

# Garbage collection of intermediate record versions

Lets see how garbage collection should be done with commit order based database snapshots.

From the *Record version visibility rule* can be derived following:  
 - If snapshot CN could see some record version then all snapshots with numbers greater than CN also 
   could see same record version.
 - If *all existing snapshots* could see some record version then all it backversions could be removed, 
   or
 - If *oldest active snapshot* could see some record version then all it backversions could be removed.

The last statement is exact copy of well known rule for garbage collection!

This rule allows to remove record versions at the *tail of versions chain*, starting from some "mature" 
record version. Rule allows to find that "mature" record version and cut the whole tail after it.

Commit order based database snapshots allows also to remove some record version placed at the 
*intermediate positions* in the versions chain. To do it, mark every record versions in the chain by 
value of *oldest active snapshot* which could see *given* record version. If few consecutive versions 
in a chain got the same mark then all of them after the first one could be removed. This allows to 
keep versions chains short.

To make it work, engine maintains list of all active database snapshots. This list is kept in shared
memory. The initial size of shared memory block could be set in firebird.conf using new setting
**SnapshotsMemSize**. Default value is 64KB. It could grow automatically, when necessary.

When engine needs to find "*oldest active snapshot* which could see *given* record version" it just 
searches for CN of transaction that created given record version in the sorted array of active
snapshots.

Garbage collection of intermediate record versions run by:
 - sweep
 - background garbage collector in SuperServer
 - every user attachment after update or delete record
 - table scan at index creation

Traditional way of garbage collection (regular GC) is not changed and still works the same way
as in previous Firebird versions. 
