gbak enhancements in Firebird v4.
---------------------------------

A new switch was added to gbak: -INCLUDE(_DATA).

Author: Dimitry Sibiryakov <sd at ibphoenix com>

It takes one parameter which is "similar like" pattern matching
table names in a case-insensitive way.

This switch, if provided, limit tables for which data is stored
or restored in/from backup file.

Interaction between -INCLUDE_DATA and -SKIP_DATA switches for
a table is following:
+--------------------------------------------------+
|           |             INCLUDE_DATA             |
|           |--------------------------------------|
| SKIP_DATA |  NOT SET   |   MATCH    | NOT MATCH  |
+-----------+------------+------------+------------+
|  NOT SET  |  included  |  included  |  excluded  |
|   MATCH   |  excluded  |  excluded  |  excluded  |
| NOT MATCH |  included  |  included  |  excluded  |
+-----------+------------+------------+------------+



gbak enhancements in Firebird v5.
---------------------------------

1. Parallel execution.

Author: Vladyslav Khorsun <hvlad at users sourceforge net>

a) gbak backup 

Backup could read source database tables using multiple threads in parallel.

New switch 
-PAR(ALLEL)           parallel workers

set number of workers that should be used for backup process. Default is 1.
Every additional worker creates own thread and own new connection used to read
data in parallel with other workers. All worker connections shares same database
snapshot to ensure consistent data view across all of its. Workers are created
and managed by gbak itself. Note, metadata still reads by single thread.

b) gbak restore

Restore could put data into user tables using multiple threads in parallel.

New switch 
-PAR(ALLEL)           parallel workers

set number of workers that should be used for restore process. Default is 1.
Every additional worker creates own thread and own new connection used to load
data in parallel with other workers. Metadata is still created using single
thread. Also, "main" connection uses DPB tag isc_dpb_parallel_workers to pass
the value of switch -PARALLEL to the engine - it allows to use engine ability
to build indices in parallel. If -PARALLEL switch is not used gbak will load
data using single thread and will not use DPB tag isc_dpb_parallel_workers. In
this case engine will use value of ParallelWorkers setting when building
indices, i.e. this phase could be run in parallel by the engine itself. To 
fully avoid parallel operations when restoring database, use -PARALLEL 1.

  Note, gbak not uses firebird.conf by itself and ParallelWorkers setting does
not affect its operations.


Examples.

  Set in firebird.conf ParallelWorkers = 4, MaxParallelWorkers = 8 and restart
Firebird server.

a) backup using 2 parallel workers

	gbak -b <database> <backup> -parallel 2

  Here gbak will read user data using 2 connections and 2 threads.


b) restore using 2 parallel workers

	gbak -r <backup> <database> -parallel 2

  Here gbak will put user data using 2 connections and 2 threads. Also, 
engine will build indices using 2 connections and 2 threads.

c) restore using no parallel workers but let engine to decide how many worker
should be used to build indices

	gbak -r <backup> <database>

  Here gbak will put user data using single connection. Eengine will build 
indices using 4 connections and 4 threads as set by ParallelWorkers.

d) restore using no parallel workers and not allow engine build indices in
parallel

	gbak -r <backup> <database> -par 1


2. Direct IO for backup files.

New switch
-D(IRECT_IO)          direct IO for backup file(s)

instruct gbak to open\create backup file(s) in direct IO (or unbuferred) mode.
It allows to not consume file system cache memory for backup files. Usually
backup is read (by restore) or write (by backup) just once and there is no big
use from caching it contents. Performance should not suffer as gbak uses 
sequential IO with relatively big chunks.
  Direct IO mode is silently ignored if backup file is redirected into standard
input\output, i.e. if "stdin"\"stdout" is used as backup file name.
