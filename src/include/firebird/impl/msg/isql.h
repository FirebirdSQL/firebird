FB_IMPL_MSG_SYMBOL(ISQL, 0, GEN_ERR, "Statement failed, SQLSTATE = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 1, USAGE, "usage:    isql [options] [<database>]")
FB_IMPL_MSG_SYMBOL(ISQL, 2, SWITCH, "Unknown switch: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 3, NO_DB, "Use CONNECT or CREATE DATABASE to specify a database")
FB_IMPL_MSG_SYMBOL(ISQL, 4, FILE_OPEN_ERR, "Unable to open @1")
FB_IMPL_MSG_SYMBOL(ISQL, 5, COMMIT_PROMPT, "Commit current transaction (y/n)?")
FB_IMPL_MSG_SYMBOL(ISQL, 6, COMMIT_MSG, "Committing.")
FB_IMPL_MSG_SYMBOL(ISQL, 7, ROLLBACK_MSG, "Rolling back work.")
FB_IMPL_MSG_SYMBOL(ISQL, 8, CMD_ERR, "Command error: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 9, ADD_PROMPT, "Enter data or NULL for each column.  RETURN to end.")
FB_IMPL_MSG_SYMBOL(ISQL, 10, VERSION, "ISQL Version: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 11, USAGE_ALL, "	-a(ll)                  extract metadata incl. legacy non-SQL tables")
FB_IMPL_MSG_SYMBOL(ISQL, 12, NUMBER_PAGES, "Number of DB pages allocated = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 13, SWEEP_INTERV, "Sweep interval = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 14, NUM_WAL_BUFF, "Number of wal buffers = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 15, WAL_BUFF_SIZE, "Wal buffer size = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 16, CKPT_LENGTH, "Check point length = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 17, CKPT_INTERV, "Check point interval = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 18, WAL_GRPC_WAIT, "Wal group commit wait = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 19, BASE_LEVEL, "Base level = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 20, LIMBO, "Transaction in limbo = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 21, HLP_FRONTEND, "Frontend commands:")
FB_IMPL_MSG_SYMBOL(ISQL, 22, HLP_BLOBED, "BLOBVIEW <blobid>          -- view BLOB in text editor")
FB_IMPL_MSG_SYMBOL(ISQL, 23, HLP_BLOBDMP, "BLOBDUMP <blobid> <file>   -- dump BLOB to a file")
FB_IMPL_MSG_SYMBOL(ISQL, 24, HLP_EDIT, "EDIT     [<filename>]      -- edit SQL script file and execute")
FB_IMPL_MSG_SYMBOL(ISQL, 25, HLP_INPUT, "INput    <filename>        -- take input from the named SQL file")
FB_IMPL_MSG_SYMBOL(ISQL, 26, HLP_OUTPUT, "OUTput   [<filename>]      -- write output to named file")
FB_IMPL_MSG_SYMBOL(ISQL, 27, HLP_SHELL, "SHELL    <command>         -- execute Operating System command in sub-shell")
FB_IMPL_MSG_SYMBOL(ISQL, 28, HLP_HELP, "HELP                       -- display this menu")
FB_IMPL_MSG_SYMBOL(ISQL, 29, HLP_SETCOM, "Set commands:")
FB_IMPL_MSG_SYMBOL(ISQL, 30, HLP_SET, "    SET                    -- display current SET options")
FB_IMPL_MSG_SYMBOL(ISQL, 31, HLP_SETAUTO, "    SET AUTOddl            -- toggle autocommit of DDL statements")
FB_IMPL_MSG_SYMBOL(ISQL, 32, HLP_SETBLOB, "    SET BLOB [ALL|<n>]     -- display BLOBS of subtype <n> or ALL")
FB_IMPL_MSG_SYMBOL(ISQL, 33, HLP_SETCOUNT, "    SET COUNT              -- toggle count of selected rows on/off")
FB_IMPL_MSG_SYMBOL(ISQL, 34, HLP_SETECHO, "    SET ECHO               -- toggle command echo on/off")
FB_IMPL_MSG_SYMBOL(ISQL, 35, HLP_SETSTAT, "    SET STATs              -- toggle display of performance statistics")
FB_IMPL_MSG_SYMBOL(ISQL, 36, HLP_SETTERM, "    SET TERM <string>      -- change statement terminator string")
FB_IMPL_MSG_SYMBOL(ISQL, 37, HLP_SHOW, "SHOW     <object> [<name>] -- display system information")
FB_IMPL_MSG_SYMBOL(ISQL, 38, HLP_OBJTYPE, "    <object> = CHECK, COLLATION, DATABASE, DOMAIN, EXCEPTION, FILTER, FUNCTION,")
FB_IMPL_MSG_SYMBOL(ISQL, 39, HLP_EXIT, "EXIT                       -- exit and commit changes")
FB_IMPL_MSG_SYMBOL(ISQL, 40, HLP_QUIT, "QUIT                       -- exit and roll back changes")
FB_IMPL_MSG_SYMBOL(ISQL, 41, HLP_ALL, "All commands may be abbreviated to letters in CAPitals")
FB_IMPL_MSG_SYMBOL(ISQL, 42, HLP_SETSCHEMA, "	SET SCHema/DB <db name> -- changes current database")
FB_IMPL_MSG_SYMBOL(ISQL, 43, YES_ANS, "Yes")
FB_IMPL_MSG_SYMBOL(ISQL, 44, REPORT1, "Current memory = !c\nDelta memory = !d\nMax memory = !x\nElapsed time = !e sec\n")
FB_IMPL_MSG_SYMBOL(ISQL, 45, REPORT2, "Cpu = !u sec\nBuffers = !b\nReads = !r\nWrites = !w\nFetches = !f")
FB_IMPL_MSG_SYMBOL(ISQL, 46, BLOB_SUBTYPE, "BLOB display set to subtype @1. This BLOB: subtype = @2")
FB_IMPL_MSG_SYMBOL(ISQL, 47, BLOB_PROMPT, "BLOB: @1, type 'edit' or filename to load>")
FB_IMPL_MSG_SYMBOL(ISQL, 48, DATE_PROMPT, "Enter @1 as Y/M/D>")
FB_IMPL_MSG_SYMBOL(ISQL, 49, NAME_PROMPT, "Enter @1>")
FB_IMPL_MSG_SYMBOL(ISQL, 50, DATE_ERR, "Bad date @1")
FB_IMPL_MSG_SYMBOL(ISQL, 51, CON_PROMPT, "CON> ")
FB_IMPL_MSG_SYMBOL(ISQL, 52, HLP_SETLIST, "    SET LIST               -- toggle column or table display format")
FB_IMPL_MSG_SYMBOL(ISQL, 53, NOT_FOUND, "@1 not found")
FB_IMPL_MSG_SYMBOL(ISQL, 54, COPY_ERR, "Errors occurred (possibly duplicate domains) in creating @1 in @2")
FB_IMPL_MSG_SYMBOL(ISQL, 55, SERVER_TOO_OLD, "Server version too old to support the isql command")
FB_IMPL_MSG_SYMBOL(ISQL, 56, REC_COUNT, "Records affected: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 57, UNLICENSED, "Unlicensed for database \"@1\"")
FB_IMPL_MSG_SYMBOL(ISQL, 58, HLP_SETWIDTH, "    SET WIDTH <col> [<n>]  -- set/unset print width to <n> for column <col>")
FB_IMPL_MSG_SYMBOL(ISQL, 59, HLP_SETPLAN, "    SET PLAN               -- toggle display of query access plan")
FB_IMPL_MSG_SYMBOL(ISQL, 60, HLP_SETTIME, "    SET TIME               -- toggle display of timestamp with DATE values")
FB_IMPL_MSG_SYMBOL(ISQL, 61, HLP_EDIT2, "EDIT                       -- edit current command buffer and execute")
FB_IMPL_MSG_SYMBOL(ISQL, 62, HLP_OUTPUT2, "OUTput                     -- return output to stdout")
FB_IMPL_MSG_SYMBOL(ISQL, 63, HLP_SETNAMES, "    SET NAMES <csname>     -- set name of runtime character set")
FB_IMPL_MSG_SYMBOL(ISQL, 64, HLP_OBJTYPE2, "               GENERATOR, GRANT, INDEX, PACKAGE, PROCEDURE, ROLE, SQL DIALECT,")
FB_IMPL_MSG_SYMBOL(ISQL, 65, HLP_SETBLOB2, "    SET BLOB               -- turn off BLOB display")
FB_IMPL_MSG_SYMBOL(ISQL, 66, HLP_SET_ROOT, "SET      <option>          -- (Use HELP SET for complete list)")
FB_IMPL_MSG_SYMBOL(ISQL, 67, NO_TABLES, "There are no tables in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 68, NO_TABLE, "There is no table @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 69, NO_VIEWS, "There are no views in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 70, NO_VIEW, "There is no view @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 71, NO_INDICES_ON_REL, "There are no indices on table @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 72, NO_REL_OR_INDEX, "There is no table or index @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 73, NO_INDICES, "There are no indices in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 74, NO_DOMAIN, "There is no domain @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 75, NO_DOMAINS, "There are no domains in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 76, NO_EXCEPTION, "There is no exception @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 77, NO_EXCEPTIONS, "There are no exceptions in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 78, NO_FILTER, "There is no filter @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 79, NO_FILTERS, "There are no filters in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 80, NO_FUNCTION, "There is no user-defined function @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 81, NO_FUNCTIONS, "There are no user-defined functions in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 82, NO_GEN, "There is no generator @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 83, NO_GENS, "There are no generators in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 84, NO_GRANT_ON_REL, "There is no privilege granted on table @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 85, NO_GRANT_ON_PROC, "There is no privilege granted on stored procedure @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 86, NO_REL_OR_PROC, "There is no table or stored procedure @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 87, NO_PROC, "There is no stored procedure @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 88, NO_PROCS, "There are no stored procedures in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 89, NO_TRIGGERS_ON_REL, "There are no triggers on table @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 90, NO_REL_OR_TRIGGER, "There is no table or trigger @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 91, NO_TRIGGERS, "There are no triggers in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 92, NO_CHECKS_ON_REL, "There are no check constraints on table @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 93, REPORT2_WINDOWS_ONLY, "Buffers = !b\nReads = !r\nWrites !w\nFetches = !f")
FB_IMPL_MSG_SYMBOL(ISQL, 94, BUFFER_OVERFLOW, "Single isql command exceeded maximum buffer size")
FB_IMPL_MSG_SYMBOL(ISQL, 95, NO_ROLES, "There are no roles in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 96, NO_OBJECT, "There is no metadata object @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 97, NO_GRANT_ON_ROL, "There is no membership privilege granted on @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 98, UNEXPECTED_EOF, "Expected end of statement, encountered EOF")
FB_IMPL_MSG_SYMBOL(ISQL, 101, TIME_ERR, "Bad TIME: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 102, HLP_OBJTYPE3, "               SYSTEM, TABLE, TRIGGER, VERSION, USERS, VIEW, WIRE_STATISTICS")
FB_IMPL_MSG_SYMBOL(ISQL, 103, NO_ROLE, "There is no role @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 104, USAGE_BAIL, "	-b(ail)                 bail on errors (set bail on)")
FB_IMPL_MSG_NO_SYMBOL(ISQL, 105, "Incomplete string in @1")
FB_IMPL_MSG_SYMBOL(ISQL, 106, HLP_SETSQLDIALECT, "    SET SQL DIALECT <n>    -- set sql dialect to <n>")
FB_IMPL_MSG_SYMBOL(ISQL, 107, NO_GRANT_ON_ANY, "There is no privilege granted in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 108, HLP_SETPLANONLY, "    SET PLANONLY           -- toggle display of query plan without executing")
FB_IMPL_MSG_SYMBOL(ISQL, 109, HLP_SETHEADING, "    SET HEADING            -- toggle display of query column titles")
FB_IMPL_MSG_SYMBOL(ISQL, 110, HLP_SETBAIL, "    SET BAIL               -- toggle bailing out on errors in non-interactive mode")
FB_IMPL_MSG_SYMBOL(ISQL, 111, USAGE_CACHE, "	-c(ache) <num>          number of cache buffers")
FB_IMPL_MSG_SYMBOL(ISQL, 112, TIME_PROMPT, "Enter @1 as H:M:S>")
FB_IMPL_MSG_SYMBOL(ISQL, 113, TIMESTAMP_PROMPT, "Enter @1 as Y/MON/D H:MIN:S[.MSEC]>")
FB_IMPL_MSG_SYMBOL(ISQL, 114, TIMESTAMP_ERR, "Bad TIMESTAMP: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 115, NO_COMMENTS, "There are no comments for objects in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 116, ONLY_FIRST_BLOBS, "Printing only the first @1 blobs.")
FB_IMPL_MSG_SYMBOL(ISQL, 117, MSG_TABLES, "Tables:")
FB_IMPL_MSG_SYMBOL(ISQL, 118, MSG_FUNCTIONS, "Functions:")
FB_IMPL_MSG_SYMBOL(ISQL, 119, EXACTLINE, "At line @1 in file @2")
FB_IMPL_MSG_SYMBOL(ISQL, 120, AFTERLINE, "After line @1 in file @2")
FB_IMPL_MSG_SYMBOL(ISQL, 121, NO_TRIGGER, "There is no trigger @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 122, USAGE_CHARSET, "	-ch(arset) <charset>    connection charset (set names)")
FB_IMPL_MSG_SYMBOL(ISQL, 123, USAGE_DATABASE, "	-d(atabase) <database>  database name to put in script creation")
FB_IMPL_MSG_SYMBOL(ISQL, 124, USAGE_ECHO, "	-e(cho)                 echo commands (set echo on)")
FB_IMPL_MSG_SYMBOL(ISQL, 125, USAGE_EXTRACT, "	-ex(tract)              extract metadata")
FB_IMPL_MSG_SYMBOL(ISQL, 126, USAGE_INPUT, "	-i(nput) <file>         input file (set input)")
FB_IMPL_MSG_SYMBOL(ISQL, 127, USAGE_MERGE, "	-m(erge)                merge standard error")
FB_IMPL_MSG_SYMBOL(ISQL, 128, USAGE_MERGE2, "	-m2                     merge diagnostic")
FB_IMPL_MSG_SYMBOL(ISQL, 129, USAGE_NOAUTOCOMMIT, "	-n(oautocommit)         no autocommit DDL (set autoddl off)")
FB_IMPL_MSG_SYMBOL(ISQL, 130, USAGE_NOWARN, "	-now(arnings)           do not show warnings")
FB_IMPL_MSG_SYMBOL(ISQL, 131, USAGE_OUTPUT, "	-o(utput) <file>        output file (set output)")
FB_IMPL_MSG_SYMBOL(ISQL, 132, USAGE_PAGE, "	-pag(elength) <size>    page length")
FB_IMPL_MSG_SYMBOL(ISQL, 133, USAGE_PASSWORD, "	-p(assword) <password>  connection password")
FB_IMPL_MSG_SYMBOL(ISQL, 134, USAGE_QUIET, "	-q(uiet)                do not show the message \"Use CONNECT...\"")
FB_IMPL_MSG_SYMBOL(ISQL, 135, USAGE_ROLE, "	-r(ole) <role>          role name")
FB_IMPL_MSG_SYMBOL(ISQL, 136, USAGE_ROLE2, "	-r2 <role>              role (uses quoted identifier)")
FB_IMPL_MSG_SYMBOL(ISQL, 137, USAGE_SQLDIALECT, "	-s(qldialect) <dialect> SQL dialect (set sql dialect)")
FB_IMPL_MSG_SYMBOL(ISQL, 138, USAGE_TERM, "	-t(erminator) <term>    command terminator (set term)")
FB_IMPL_MSG_SYMBOL(ISQL, 139, USAGE_USER, "	-u(ser) <user>          user name")
FB_IMPL_MSG_SYMBOL(ISQL, 140, USAGE_XTRACT, "	-x                      extract metadata")
FB_IMPL_MSG_SYMBOL(ISQL, 141, USAGE_VERSION, "	-z                      show program and server version")
FB_IMPL_MSG_SYMBOL(ISQL, 142, USAGE_NOARG, "missing argument for switch \"@1\"")
FB_IMPL_MSG_SYMBOL(ISQL, 143, USAGE_NOTINT, "argument \"@1\" for switch \"@2\" is not an integer")
FB_IMPL_MSG_SYMBOL(ISQL, 144, USAGE_RANGE, "value \"@1\" for switch \"@2\" is out of range")
FB_IMPL_MSG_SYMBOL(ISQL, 145, USAGE_DUPSW, "switch \"@1\" or its equivalent used more than once")
FB_IMPL_MSG_SYMBOL(ISQL, 146, USAGE_DUPDB, "more than one database name: \"@1\", \"@2\"")
FB_IMPL_MSG_SYMBOL(ISQL, 147, NO_DEPENDENCIES, "No dependencies for @1 were found")
FB_IMPL_MSG_SYMBOL(ISQL, 148, NO_COLLATION, "There is no collation @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 149, NO_COLLATIONS, "There are no user-defined collations in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 150, MSG_COLLATIONS, "Collations:")
FB_IMPL_MSG_SYMBOL(ISQL, 151, NO_SECCLASS, "There are no security classes for @1")
FB_IMPL_MSG_SYMBOL(ISQL, 152, NO_DB_WIDE_SECCLASS, "There is no database-wide security class")
FB_IMPL_MSG_SYMBOL(ISQL, 153, CANNOT_GET_SRV_VER, "Cannot get server version without database connection")
FB_IMPL_MSG_SYMBOL(ISQL, 154, USAGE_NODBTRIGGERS, "	-nod(btriggers)         do not run database triggers")
FB_IMPL_MSG_SYMBOL(ISQL, 155, USAGE_TRUSTED, "	-tr(usted)              use trusted authentication")
FB_IMPL_MSG_SYMBOL(ISQL, 156, BULK_PROMPT, "BULK> ")
FB_IMPL_MSG_SYMBOL(ISQL, 157, NO_CONNECTED_USERS, "There are no connected users")
FB_IMPL_MSG_SYMBOL(ISQL, 158, USERS_IN_DB, "Users in the database")
FB_IMPL_MSG_SYMBOL(ISQL, 159, OUTPUT_TRUNCATED, "Output was truncated")
FB_IMPL_MSG_SYMBOL(ISQL, 160, VALID_OPTIONS, "Valid options are:")
FB_IMPL_MSG_SYMBOL(ISQL, 161, USAGE_FETCH, "	-f(etch_password)       fetch password from file")
FB_IMPL_MSG_SYMBOL(ISQL, 162, PASS_FILE_OPEN, "could not open password file @1, errno @2")
FB_IMPL_MSG_SYMBOL(ISQL, 163, PASS_FILE_READ, "could not read password file @1, errno @2")
FB_IMPL_MSG_SYMBOL(ISQL, 164, EMPTY_PASS, "empty password file @1")
FB_IMPL_MSG_SYMBOL(ISQL, 165, HLP_SETMAXROWS, "    SET MAXROWS [<n>]      -- limit select stmt to <n> rows, zero is no limit")
FB_IMPL_MSG_SYMBOL(ISQL, 166, NO_PACKAGE, "There is no package @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 167, NO_PACKAGES, "There are no packages in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 168, NO_SCHEMA, "There is no schema @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 169, NO_SCHEMAS, "There are no schemas in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 170, MAXROWS_INVALID, "Unable to convert @1 to a number for MAXROWS option")
FB_IMPL_MSG_SYMBOL(ISQL, 171, MAXROWS_OUTOF_RANGE, "Value @1 for MAXROWS is out of range. Max value is @2")
FB_IMPL_MSG_SYMBOL(ISQL, 172, MAXROWS_NEGATIVE, "The value (@1) for MAXROWS must be zero or greater")
FB_IMPL_MSG_SYMBOL(ISQL, 173, HLP_SETEXPLAIN, "    SET EXPLAIN            -- toggle display of query access plan in the explained form")
FB_IMPL_MSG_SYMBOL(ISQL, 174, NO_GRANT_ON_GEN, "There is no privilege granted on generator @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 175, NO_GRANT_ON_XCP, "There is no privilege granted on exception @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 176, NO_GRANT_ON_FLD, "There is no privilege granted on domain @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 177, NO_GRANT_ON_CS, "There is no privilege granted on character set @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 178, NO_GRANT_ON_COLL, "There is no privilege granted on collation @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 179, NO_GRANT_ON_PKG, "There is no privilege granted on package @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 180, NO_GRANT_ON_FUN, "There is no privilege granted on function @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 181, REPORT_NEW1, "Current memory = !\nDelta memory = !\nMax memory = !\nElapsed time = ~ sec\n")
FB_IMPL_MSG_SYMBOL(ISQL, 182, REPORT_NEW2, "Cpu = ~ sec\n")
FB_IMPL_MSG_SYMBOL(ISQL, 183, REPORT_NEW3, "Buffers = !\nReads = !\nWrites = !\nFetches = !")
FB_IMPL_MSG_SYMBOL(ISQL, 184, NO_MAP, "There is no mapping @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 185, NO_MAPS, "There are no mappings in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 186, INVALID_TERM_CHARS, "Invalid characters for SET TERMINATOR are @1")
FB_IMPL_MSG_SYMBOL(ISQL, 187, REC_DISPLAYCOUNT, "Records displayed: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 188, COLUMNS_HIDDEN, "Full NULL columns hidden due to RecordBuff: @1")
FB_IMPL_MSG_SYMBOL(ISQL, 189, HLP_SETRECORDBUF, "    SET RECORDBuf          -- toggle limited buffering and trimming of columns")
FB_IMPL_MSG_SYMBOL(ISQL, 190, NUMBER_USED_PAGES, "Number of DB pages used = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 191, NUMBER_FREE_PAGES, "Number of DB pages free = @1")
FB_IMPL_MSG_SYMBOL(ISQL, 192, DATABASE_CRYPTED, "Database encrypted")
FB_IMPL_MSG_SYMBOL(ISQL, 193, DATABASE_NOT_CRYPTED, "Database not encrypted")
FB_IMPL_MSG_SYMBOL(ISQL, 194, DATABASE_CRYPT_PROCESS, "crypt thread not complete")
FB_IMPL_MSG_SYMBOL(ISQL, 195, MSG_ROLES, "Roles:")
FB_IMPL_MSG_SYMBOL(ISQL, 196, NO_TIMEOUTS, "Timeouts are not supported by server")
FB_IMPL_MSG_SYMBOL(ISQL, 197, HLP_SETKEEPTRAN, "    SET KEEP_TRAN_params   -- toggle to keep or not to keep text of following successful SET TRANSACTION statement")
FB_IMPL_MSG_SYMBOL(ISQL, 198, HLP_SETPERTAB, "    SET PER_TABle_stats    -- toggle display of detailed per-table statistics")
FB_IMPL_MSG_SYMBOL(ISQL, 199, BAD_STMT_TYPE, "Statement type is not recognized")
FB_IMPL_MSG_SYMBOL(ISQL, 200, MSG_PACKAGES, "Packages:")
FB_IMPL_MSG_SYMBOL(ISQL, 201, NO_PUBLICATION, "There is no publication @1 in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 202, NO_PUBLICATIONS, "There is no publications in this database")
FB_IMPL_MSG_SYMBOL(ISQL, 203, MSG_PUBLICATIONS, "Publications:")
FB_IMPL_MSG_SYMBOL(ISQL, 204, MSG_PROCEDURES, "Procedures:")
FB_IMPL_MSG_SYMBOL(ISQL, 205, HLP_EXPLAIN, "EXPLAIN                    -- explain a query access plan")
FB_IMPL_MSG_SYMBOL(ISQL, 206, USAGE_AUTOTERM, "	-autot(erm)             use auto statement terminator (set autoterm on)")
FB_IMPL_MSG_SYMBOL(ISQL, 207, AUTOTERM_NOT_SUPPORTED, "SET AUTOTERM ON is not supported in engine/server and has been disabled")
FB_IMPL_MSG_SYMBOL(ISQL, 208, HLP_SETAUTOTERM, "    SET AUTOTERM           -- toggle auto statement terminator")
FB_IMPL_MSG_SYMBOL(ISQL, 209, HLP_SETWIRESTATS, "    SET WIRE_stats         -- toggle display of wire (network) statistics")
