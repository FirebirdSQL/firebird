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

Block size could be set in firebird.conf using new setting **TpcBlockSize**. Default value is
4MB and it could keep 512K transactions.

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
cheap operation. Nested statements (triggers, nested stored procedures and functions, dynamic 
statements, etc) uses same database snapshot created by top-level statement. 

To support this solution new transaction isolation level is introduced: **READ COMMITTED READ 
CONSISTENCY**

Old read-committed isolation modes (**RECORD VERSION** and **NO RECORD VERSION**) are still 
allowed but considered as legacy and not recommended to use.

So, there are three kinds of read-committed transactions now:
 - READ COMMITTED READ CONSISTENCY
 - READ COMMITTED NO RECORD VERSION
 - READ COMMITTED RECORD VERSION

### Update conflicts handling

When statement executed within read committed read consistency transaction its database view is 
not changed (similar to snapshot transaction). Therefore it is useless to wait for commit of 
concurrent transaction in the hope to re-read new committed record version. On read, behavior is 
similar to read committed *record version* transaction - do not wait for active transaction and
walk backversions chain looking for record version visible to the current snapshot.

When update conflict happens engine behavior is changed. If concurrent transaction is active, 
engine waits (according to transaction lock timeout) and if concurrent transaction still not
committed, update conflict error is returned. If concurrent transaction is committed, engine 
creates new snapshot and restart top-level statement execution. In both cases all work performed 
up to the top-level statement is undone.

This is the same logic as user applications uses for update conflict handling usually, but it
is a bit more efficient as it excludes network roundtrips to\from client host. This restart
logic is not applied to the selectable stored procedures if update conflict happens after any
record returned to the client application. In this case *isc_update_conflict* error is returned.

Note: by historical reasons isc_update_conflict reported as secondary error code with primary
error code isc_deadlock.

### No more precommitted transactions

*Read-committed read only* (RCRO) transactions currently marked as committed immediately when 
transaction started. This is correct if read-committed transaction needs no database snapshot. 
But it is not correct to mark RCRO transaction as committed as it can break statement-level 
snapshot consistency.

### Support for new READ COMMITTED READ CONSISTENCY isolation level
#### SQL syntax  

New isolation level is supported at SQL level:

*SET TRANSACTION READ COMMITTED READ CONSISTENCY*

#### API level
To start read-committed read consistency transaction using ISC API use new constant in Transaction 
Parameter Buffer (TPB):

*isc_tpb_read_consistency*

#### Configuration setting
In the future versions of Firebird old kinds of read-committed transactions could be removed.
To help test existing applications with new READ COMMITTED READ CONSISTENCY isolation level
new configuration setting is introduced:   

*ReadConsistency*  

If ReadConsistency set to 1 (by default) engine ignores [NO] RECORD VERSION flags and makes all 
read-committed transactions READ COMMITTED READ CONSISTENCY.

Set ReadConsistency to 0 to allow old engine behavior - flags [NO] RECORD VERSION takes effect as
in previous Firebird versions. READ COMMITTED READ CONSISTENCY isolation level should be specified
explicitly.


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

To make it works, engine maintains list of all active database snapshots. This list is kept in shared
memory. The initial size of shared memory block could be set in firebird.conf using new setting
**SnapshotsMemSize**. Default value is 64KB. It could grow automatically, when necessary.

When engine need to find "*oldest active snapshot* which could see *given* record version" it
just search for CN of transaction that created given record version at the sorted array of active
snapshots.

Garbage collection of intermediate record versions run by:
 - sweep
 - background garbage collector in SuperServer
 - every user attachment after update or delete record
