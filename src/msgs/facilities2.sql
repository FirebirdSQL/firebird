/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2021-05-11 14:10:00', 'JRD', 0, 955)
('2015-03-17 18:33:00', 'QLI', 1, 533)
('2018-03-17 12:00:00', 'GFIX', 3, 136)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
('2017-02-05 20:37:00', 'DSQL', 7, 41)
('2018-06-22 11:46:00', 'DYN', 8, 309)
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
('2021-02-04 11:21:00', 'GBAK', 12, 405)
('2019-04-13 21:10:00', 'SQLERR', 13, 1047)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2018-02-27 14:50:31', 'JRD_BUGCHK', 15, 307)
('2016-05-26 13:53:45', 'ISQL', 17, 197)
('2010-07-10 10:50:30', 'GSEC', 18, 105)
('2019-10-19 12:52:29', 'GSTAT', 21, 63)
('2021-02-04 10:32:00', 'FBSVCMGR', 22, 62)
('2009-07-18 12:12:12', 'UTL', 23, 2)
('2020-12-20 15:40:00', 'NBACKUP', 24, 82)
('2009-07-20 07:55:48', 'FBTRACEMGR', 25, 41)
('2015-07-27 00:00:00', 'JAYBIRD', 26, 1)
('2020-11-27 00:00:00', 'R2DBC_FIREBIRD', 27, 1)
stop

COMMIT WORK;
