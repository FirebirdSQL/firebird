# Firebird replication

## Concept

Firebird 4 offers built-in support for uni-directional \(aka master-slave\) logical replication. Logical here means record-level replication, as opposed to physical \(page-level\) replication. Implementation is primarily focused on HA \(high-availability\) solutions, but it can be used for other tasks as well.

Events that are tracked include: inserted/updated/deleted records, sequence changes, DDL statements. Replication is transactional, commit order is preserved. Replication can track changes in either all tables, or in some customized subset of tables. Tables to be replicated are required to have either a primary key or at least a unique key.

There are two replication modes available: synchronous and asynchronous.

In synchronous replication, the primary database is permanently connected to the replica database\(s\) and changes are being replicated immediately \(in fact, some recent uncommitted changes may be buffered, but they are transmitted at the commit time\). This effectively means that databases are in sync after every commit. However, this may impact performance due to additional network traffic and round-trips. Multiple synchronous replicas can be configured, if necessary.

In asynchronous replication, changes are being written into the local journal files that are transferred over the wire and applied to the replica database. This impacts the performance much less, but introduces the delay \(known as replication lag\) when changes are not yet applied to the replica database, i.e. the replica database is always "catching up" the master database.

There are two access modes for replica databases: read-only and read-write. Read-only replica allows to execute any query that does not modify data \(global temporary tables can be modified as they are not replicated\), modifications are limited to the replication process only. Read-write replica allows to execute any query, possible conflicts must be resolved by users.

## Journalling

Asynchronous replication is based on journalling. Replicated changes are written into the journal which consists of multiple files \(known as replication segments\). Firebird server writes segments continuously, one after one. Every segment has a unique number which is generated sequentially. This number \(known as segment sequence\), combined with the database UUID, provide globally unique identification of journal segments. The global sequence counter is stored inside the replicated database and it's never reset \(until the database is restored from backup\).

Segments are regularly rotated, this process is controlled by either maximum segment size or timeout, both thresholds are configurable. Once the active segment reaches the threshold, it's marked as "full" and writing switches to the next available segment. Full segments are archived and then reused for subsequent writes. Archiving basically means copying the segment with a purpose of transferring it to the replica host and applying there. Copying can be done by Firebird server itself or, alternatively, by custom \(user-specified\) command.

On the replica side, journal segments are applied in the replication sequence order. Firebird server periodically scans for the new segments appearing in the configured directory. Once the next segment is found, it gets replicated. Replication state is stored in the local file named {UUID} \(per every replication source\) and contains the following markers: latest segment sequence \(LSS\), oldest segment sequence \(OSS\) and list of active transactions started between OSS and LSS. LSS means the last replicated segment. OSS means the segment that started the earliest transaction that wasn't finished at the time LSS was processed. These markers control two things: \(1\) what segment must be replicated next and \(2\) when segments can be safely deleted. Segments with numbers between OSS and LSS are preserved for replaying the journal after the replicator disconnects from the replica database \(e.g. due to replication error or idle timeout\). If there are no active transactions pending and LSS was processed without errors, all segments up to \(and including\) LSS are deleted. In the case of any critical error, replication is temporarily suspended and re-attempted after timeout.

## Error reporting

All replication errors and warnings \(e.g. detected conflicts\) are written into the replication.log file stored in the Firebird log directory \(by default this is the root directory of the Firebird installation\). This file may also include the detailed description of the operations performed by the replicator.

## Setting up the master side

Replication is configured using a single configuration file: replication.conf. It allows to define global settings as well as per-database settings. All the possible options are listed inside replication.conf, descriptions are provided as comments there. For per-database configuration, full database name must be specified \(aliases or wildcards are not allowed\) inside the {database} section.

Inside the database, replication should be enabled via the DDL command:
ALTER DATABASE ENABLE PUBLICATION

Also, the replication set (aka publication) should be defined. It includes tables that should be replicated. This is also done via the DDL command:

-- to replicate all tables (including the ones created later): 
ALTER DATABASE INCLUDE ALL TO PUBLICATION 
-- to replicate specific tables: 
ALTER DATABASE INCLUDE TABLE T1, T2, T3 TO PUBLICATION 

Tables may later be excluded from the replication set:

-- to disable replication of all tables (including the ones created later): 
ALTER DATABASE EXCLUDE ALL FROM PUBLICATION 
-- to disable replication of specific tables: 
ALTER DATABASE EXCLUDE TABLE T1, T2, T3 FROM PUBLICATION 

Tables enabled for replicated can be additionally filtered using two settings in the configuration file: include\_filter and exclude\_filter. They are regular expressions that are applied to table names and define rules for inclusion table\(s\) into the replication set or excluding them from the replication set.

Synchronous replication can be turned on using the sync\_replica setting \(multiple entries are allowed\). It must specify a connection string to the replica database, prefixed with username/password. In SuperServer and SuperClassic architectures, replica database is being internally attached when the first user gets connected to the master database and detached when the last user disconnects from the master database. In Classic Server architecture, every server process keeps an active connection to the replica database.

Asynchronous replication requires setting up the journalling mechanism. The primary parameter is journal\_directory which defines location of the replication journal files (_aka_ segments). Once this location is specified, asynchronous replication is turned on and Firebird server starts producing the journal segments.

Minimal configuration looks like this:

database = /data/mydb.fdb  
{  
    journal\_directory = /journal/mydb/  
    journal\_archive\_directory = /archive/mydb/  
}

Archiving is performed by copying the segments from /journal/mydb/ to /archive/mydb/, Firebird server copies the segments itself.

The same with user-defined archiving:

database = /data/mydb.fdb  
{  
    journal\_directory = /journal/mydb/  
    journal\_archive\_directory = /archive/mydb/  
    journal\_archive\_command = "test ! -f $\(archivepathname\) && cp $\(pathname\) $\(archivepathname\)"  
}

Where $\(pathname\) and $\(archivepathname\) are built-in macros that provide the custom shell command with real file names.

Custom archiving \(journal\_archive\_command setting\) allows to use any system shell command \(including scripts / batch files\) to deliver segments to the replica side. It could use compression, FTP, or whatever else available on the server. Actual transport implementation is up to DBA, Firebird just produces segments on the master side and expects them to appear at the replica side. If the replica storage can be remotely attached to the master host, it becomes just a matter of copying the segments. In other cases, some transport solution is required.

The same with archiving performed every 10 seconds:

database = /data/mydb.fdb  
{  
    journal\_directory = /journal/mydb/  
    journal\_archive\_directory = /archive/mydb/  
    journal\_archive\_command = "test ! -f $\(archivepathname\) && cp $\(pathname\) $\(archivepathname\)"  
    journal\_archive\_timeout = 10  
}

Read replication.conf for other possible settings.

To apply the changed master-side settings, all users must be reconnected.

## Setting up the replica side

The same replication.conf file is used. Setting journal\_source\_directory specifies the location that Firebird server scans for the transmitted segments. Additionally, DBA may explicitly specify what source database is accepted for replication. Setting source\_guid is used for that purpose.

Sample configuration looks like this:

database = /data/mydb.fdb  
{  
    journal\_source\_directory = /incoming/  
    source\_guid = "{6F9619FF-8B86-D011-B42D-00CF4FC964FF}"  
}

Read replication.conf for other possible settings.

To apply the changed replica-side settings, Firebird server must be restarted.

## Creating the replica database

In the Beta 1 release, any physical copying method can be used:

* File-level copy when Firebird server is shutdown
* ALTER DATABASE BEGIN BACKUP + file-level copy + ALTER DATABASE END BACKUP
* nbackup -l + file-level copy + nbackup -n
* nbackup -b 0

Then the replica mode must be activated for the database copy. Two options are possible:

* gfix -replica read\_only &lt;database&gt; -- set up database as read-only replica
* gfix -replica read\_write &lt;database&gt; -- set up database as read-write replica

Read-only replica means that only the replicator connection can modify the database. This is mostly indended for high availability solutions as the replica database is guaranteed to match the master one and can be used for fast recovery. Regular user connections may perform any operations allowed for read-only transactions: select from tables, execute read-only procedures, write into global temporary tables, etc. Database maintenance such as sweeping, shutdown, monitoring is also allowed. This can be used for moving read-only load \(analytics, etc\) to the replica database. However, read-only connections may potentially conflict with the replication if some DDL statements \(those requiring an exclusing metadata lock\) are performed on the master database.

Read-write replicas allow both the replicator connection and regular user connections to modify the database concurrently. This does not guarantee the replica database to be in sync with the master one, so it's not recommended to use this mode for high availability, unless replica-side user connections modify only tables excluded from replication.

## Converting the replica to a regular database

As simple as this:

* gfix -replica none &lt;database&gt;

This isn't strictly required for read-write replicas, but recommended to avoid unexpected replication flow.

