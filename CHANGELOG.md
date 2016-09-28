# v4.0 Alpha 1 (unreleased)

## Bugfixes

* [CORE-5237](http://tracker.firebirdsql.org/browse/CORE-5237): Invalid handling of dot (.) and asterisk (*) in config file name and path for include clause  
  Contributor(s): Dimitry Sibiryakov

* [CORE-5223](http://tracker.firebirdsql.org/browse/CORE-5223): Double dots are prohibited in file names if access is restricted to a list of directories  
  Contributor(s): Dimitry Sibiryakov

* [CORE-5141](http://tracker.firebirdsql.org/browse/CORE-5141): Field definition allows several NOT NULL clauses  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4985](http://tracker.firebirdsql.org/browse/CORE-4985): Non-privileged user can implicitly count records in a restricted table  
  Contributor(s): Dmitry Yemanov

* [CORE-4701](http://tracker.firebirdsql.org/browse/CORE-4701): Index and blob garbage collection don't take into accout data in undo log  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4483](http://tracker.firebirdsql.org/browse/CORE-4483): Changed data not visible in WHEN-section if exception occured inside SP that has been called from this code  
  Contributor(s): Dimitry Sibiryakov

* [CORE-4424](http://tracker.firebirdsql.org/browse/CORE-4424): Rollback to wrong savepoint if several exception handlers on the same level are executed  
  Contributor(s): Dimitry Sibiryakov

## Improvements

* [CORE-5119](http://tracker.firebirdsql.org/browse/CORE-5119): Support autocommit mode in SET TRANSACTION statement  
  Contributor(s): Dmitry Yemanov

* [CORE-2557](http://tracker.firebirdsql.org/browse/CORE-2557): Grants on MON$ tables  
  Contributor(s): Alex Peshkoff

* [CORE-2192](http://tracker.firebirdsql.org/browse/CORE-2192): Extend maximum database page size to 32KB  
  Contributor(s): Dmitry Yemanov

* [CORE-749](http://tracker.firebirdsql.org/browse/CORE-749): Increase maximum length of object names to 63 characters  
  Contributor(s): Adriano dos Santos Fernandes

## New features

* [CORE-5346](http://tracker.firebirdsql.org/browse/CORE-5346): Named windows
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-5343](http://tracker.firebirdsql.org/browse/CORE-5343): Allow particular DBA privileges to be transferred to regular users  
  Contributor(s): Alex Peshkoff

* [CORE-3647](http://tracker.firebirdsql.org/browse/CORE-3647): Frames for window functions
  Contributor(s): Adriano dos Santos Fernandes

* [CORE-2762](http://tracker.firebirdsql.org/browse/CORE-2762): New built-in function to check whether some role is implicitly active  
  Contributor(s): Roman Simakov

* [CORE-1815](http://tracker.firebirdsql.org/browse/CORE-1815): Ability to grant role to another role  
  Contributor(s): Roman Simakov

* [CORE-1688](http://tracker.firebirdsql.org/browse/CORE-1688): More ANSI SQL:2003 window functions (PERCENK_RANK, CUME_DIST, NTILE)  
  Contributor(s): Hajime Nakagami, Adriano dos Santos Fernandes

* [CORE-751](http://tracker.firebirdsql.org/browse/CORE-751): Implicitly active roles (and their permissions summarized)  
  Contributor(s): Roman Simakov

